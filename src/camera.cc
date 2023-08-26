#include <camera.hh>

Camera::Camera(int width, int height)
{
    width = width;
    height = height;

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

    // Reserve buffer for node
    node_buffer = new std::uint8_t[width * height * 3];

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