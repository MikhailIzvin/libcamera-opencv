#include <camera.hh>

Camera::Camera(int width, int height, std::function<YUV420 &> func)
{
    width = width;
    height = height;
    func = func;

    cm = std::make_unique<libcamera::CameraManager>();
    cm->start();

    std::string cameraId = cm->cameras()[0]->id();
    camera = cm->get(cameraId);

    // grab semaphore
    camera->acquire();

    config = camera->generateConfiguration({libcamera::StreamRole::Viewfinder});

    libcamera::StreamConfiguration &streamConfig = config->at(0);

    // Set camera size params
    streamConfig.size.width = width;
    streamConfig.size.height = height;

    // default pixel format.
    streamConfig.pixelFormat = libcamera::formats::YUV420;

    config->validate();
    std::cout << "Validated viewfinder configuration is: " << streamConfig.toString() << std::endl;

    if (camera->configure(config.get()))
    {
        throw std::runtime_error("CONFIGURATION FAILED!");
    }
    else
    {
        streamConfig = config->at(0);
        stride = streamConfig.stride;
        width = streamConfig.size.width;
        height = streamConfig.size.height;
    }

    allocator = new libcamera::FrameBufferAllocator(camera);
    stream = streamConfig.stream();

    if (allocator->allocate(stream) < 0)
    {
        throw std::runtime_error("CANNOT ALLOCATE BUFFERS!");
    }

    // Get number of allocated buffers in dma
    // size_t allocated = allocator->buffers(stream).size();
    const std::vector<std::unique_ptr<libcamera::FrameBuffer>> &buffers = allocator->buffers(stream);
    for (unsigned int i = 0; i < buffers.size(); ++i)
    {
        // libcamera works with request to camera, and get info in callback
        std::unique_ptr<libcamera::Request> request = camera->createRequest();

        if (!request)
        {
            throw std::runtime_error("CANNOT CREATE REQUEST!");
        }

        const std::unique_ptr<libcamera::FrameBuffer> &buffer = buffers[i];

        // Map dma buffers to program memory
        for (unsigned j = 0; j < buffer->planes().size(); ++j)
        {
            const libcamera::FrameBuffer::Plane &plane = buffer->planes()[j];
            void *memory = mmap(nullptr, plane.length, PROT_READ | PROT_WRITE, MAP_SHARED, plane.fd.get(), 0);
            mapped_buffers_[buffer.get()].push_back(libcamera::Span<uint8_t>(static_cast<uint8_t *>(memory), plane.length));
        }

        if (request->addBuffer(stream, buffer.get()) < 0)
        {
            throw std::runtime_error("CANNOT SET BUFFER REQUEST!");
        }

        // Add controls to request (such as exposure, shutter, awb, etc ...)
        // There set default settings
        libcamera::ControlList &controls = request->controls();

        // Auto exposure
        controls.set(libcamera::controls::AeExposureMode, libcamera::controls::ExposureNormal);

        // Auto white balance
        controls.set(libcamera::controls::AwbMode, libcamera::controls::AwbAuto);

        requests.push_back(std::move(request));
    }

    // Reserve buffer for yuv
    YUV420 yuv = YUV420{
        std::vector<std::uint8_t>(width * height),
        std::vector<std::uint8_t>(width * height / 2),
        std::vector<std::uint8_t>(width * height / 2)};

    // Set callack for video frames
    camera->requestCompleted.connect(this, &Camera::camera_callback);

    camera->start();

    // Add requests to camera
    for (std::unique_ptr<libcamera::Request> &request : requests)
        camera->queueRequest(request.get());
}

Camera::~Camera()
{
    camera->stop();
    allocator->free(stream);
    delete allocator;
    camera->release();
    camera.reset();
    cm->stop();
}

void Camera::camera_callback(libcamera::Request *request)
{
    if (request->status() != libcamera::Request::RequestComplete)
        return;

    libcamera::FrameBuffer *buffer = request->buffers().find(stream)->second;
    std::vector<libcamera::Span<uint8_t>> const &mem = mapped_buffers_[buffer];

    unsigned w = width, h = height, s = stride;
    uint8_t *Y = (uint8_t *)mem[0].data();
    uint8_t *y_b = yuv.Y.data();
    for (unsigned int j = 0; j < h; j++)
    {
        std::memcpy(y_b + j * w, Y + j * s, w);
    }

    uint8_t *U = (uint8_t *)mem[1].data();
    uint8_t *u_b = yuv.U.data();
    h /= 2, w /= 2, s /= 2;
    for (unsigned int j = 0; j < h; j++)
    {
        std::memcpy(u_b + j * w, U + j * s, w);
    }

    uint8_t *V = (uint8_t *)mem[2].data();
    uint8_t *v_b = yuv.V.data();
    for (unsigned int j = 0; j < h; j++)
    {
        std::memcpy(v_b + j * w, V + j * s, w);
    }

    func(&yuv);

    request->reuse(libcamera::Request::ReuseBuffers);
    camera->queueRequest(request);
}

void yuv2rgb(std::vector<std::uint8_t> &buffer)
{
    buffer.resize(width * height * 3);

    unsigned int y = 0;

    for (; y < height; y += 2)
    {
        const uint8_t *src_Y = yuv.Y.data() + y;
        const uint8_t *src_U = yuv.U.data() + y;
        const uint8_t *src_V = yuv.V.data() + y;
        const uint8_t *src_Y1 = src_Y0 + 1;
        uint8_t *dst0 = &output[y * width * 3];
        uint8_t *dst1 = dst0 + width * 3;
        unsigned int x = 0;
        for (; x < width; x += 4)
        {
            int Y0 = *(src_Y0++);
            int U0 = *(src_U++);
            int V0 = *(src_V++);
            int Y1 = *(src_Y0++);
            int Y2 = *(src_Y0++);
            int U2 = *(src_U++);
            int V2 = *(src_V++);
            int Y3 = *(src_Y0++);
            int Y4 = *(src_Y1++);
            int Y5 = *(src_Y1++);
            int Y6 = *(src_Y1++);
            int Y7 = *(src_Y1++);

            U0 -= 128;
            V0 -= 128;
            U2 -= 128;
            V2 -= 128;
            int U1 = U0;
            int V1 = V0;
            int U4 = U0;
            int V4 = V0;
            int U5 = U0;
            int V5 = V0;
            int U3 = U2;
            int V3 = V2;
            int U6 = U2;
            int V6 = V2;
            int U7 = U2;
            int V7 = V2;

            int R0 = Y0 + 1.402 * V0;
            int G0 = Y0 - 0.345 * U0 - 0.714 * V0;
            int B0 = Y0 + 1.771 * U0;
            int R1 = Y1 + 1.402 * V1;
            int G1 = Y1 - 0.345 * U1 - 0.714 * V1;
            int B1 = Y1 + 1.771 * U1;
            int R2 = Y2 + 1.402 * V2;
            int G2 = Y2 - 0.345 * U2 - 0.714 * V2;
            int B2 = Y2 + 1.771 * U2;
            int R3 = Y3 + 1.402 * V3;
            int G3 = Y3 - 0.345 * U3 - 0.714 * V3;
            int B3 = Y3 + 1.771 * U3;
            int R4 = Y4 + 1.402 * V4;
            int G4 = Y4 - 0.345 * U4 - 0.714 * V4;
            int B4 = Y4 + 1.771 * U4;
            int R5 = Y5 + 1.402 * V5;
            int G5 = Y5 - 0.345 * U5 - 0.714 * V5;
            int B5 = Y5 + 1.771 * U5;
            int R6 = Y6 + 1.402 * V6;
            int G6 = Y6 - 0.345 * U6 - 0.714 * V6;
            int B6 = Y6 + 1.771 * U6;
            int R7 = Y7 + 1.402 * V7;
            int G7 = Y7 - 0.345 * U7 - 0.714 * V7;
            int B7 = Y7 + 1.771 * U7;

            R0 = std::clamp(R0, 0, 255);
            G0 = std::clamp(G0, 0, 255);
            B0 = std::clamp(B0, 0, 255);
            R1 = std::clamp(R1, 0, 255);
            G1 = std::clamp(G1, 0, 255);
            B1 = std::clamp(B1, 0, 255);
            R2 = std::clamp(R2, 0, 255);
            G2 = std::clamp(G2, 0, 255);
            B2 = std::clamp(B2, 0, 255);
            R3 = std::clamp(R3, 0, 255);
            G3 = std::clamp(G3, 0, 255);
            B3 = std::clamp(B3, 0, 255);
            R4 = std::clamp(R4, 0, 255);
            G4 = std::clamp(G4, 0, 255);
            B4 = std::clamp(B4, 0, 255);
            R5 = std::clamp(R5, 0, 255);
            G5 = std::clamp(G5, 0, 255);
            B5 = std::clamp(B5, 0, 255);
            R6 = std::clamp(R6, 0, 255);
            G6 = std::clamp(G6, 0, 255);
            B6 = std::clamp(B6, 0, 255);
            R7 = std::clamp(R7, 0, 255);
            G7 = std::clamp(G7, 0, 255);
            B7 = std::clamp(B7, 0, 255);

            *(dst0++) = R0;
            *(dst0++) = G0;
            *(dst0++) = B0;
            *(dst0++) = R1;
            *(dst0++) = G1;
            *(dst0++) = B1;
            *(dst0++) = R2;
            *(dst0++) = G2;
            *(dst0++) = B2;
            *(dst0++) = R3;
            *(dst0++) = G3;
            *(dst0++) = B3;
            *(dst1++) = R4;
            *(dst1++) = G4;
            *(dst1++) = B4;
            *(dst1++) = R5;
            *(dst1++) = G5;
            *(dst1++) = B5;
            *(dst1++) = R6;
            *(dst1++) = G6;
            *(dst1++) = B6;
            *(dst1++) = R7;
            *(dst1++) = G7;
            *(dst1++) = B7;
        }
    }

    for (; y < height; y++)
    {
        const uint8_t *src_Y = yuv.Y.data() + y;
        const uint8_t *src_U = yuv.U.data() + y;
        const uint8_t *src_V = yuv.V.data() + y;
        uint8_t *dst0 = &output[y * width * 3];

        unsigned int x = 0;
        for (; x < width; x++)
        {
            int Y0 = *(src_Y0++);
            int U0 = *(src_U);
            int V0 = *(src_V);
            src_U += (x & 1);
            src_V += (x & 1);

            U0 -= 128;
            V0 -= 128;

            int R0 = Y0 + 1.402 * V0;
            int G0 = Y0 - 0.345 * U0 - 0.714 * V0;
            int B0 = Y0 + 1.771 * U0;

            R0 = std::clamp(R0, 0, 255);
            G0 = std::clamp(G0, 0, 255);
            B0 = std::clamp(B0, 0, 255);

            *(dst0++) = R0;
            *(dst0++) = G0;
            *(dst0++) = B0;
        }
    }
}