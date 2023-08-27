#include <functional>
#include <libcamera/libcamera.h>
#include <memory>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

struct YUV420
{
    std::vector<std::uint8_t> Y;
    std::vector<std::uint8_t> U;
    std::vector<std::uint8_t> V;
};

class Camera
{
  public:
    Camera(int width, int height, std::function<YUV420 *>);
    ~Camera();

  private:
    // Callback for recieved frames
    void camera_callback(libcamera::Request *);
    void yuv2rgb(std::vector<std::uint8_t> &buffer);

  private:
    // Camera settings part
    std::unique_ptr<libcamera::CameraManager> cm;
    std::shared_ptr<libcamera::Camera> camera;
    std::unique_ptr<libcamera::CameraConfiguration> config;
    std::map<libcamera::FrameBuffer *, std::vector<libcamera::Span<uint8_t>>> mapped_buffers_;
    std::vector<std::unique_ptr<libcamera::Request>> requests;
    libcamera::FrameBufferAllocator *allocator;
    libcamera::Stream *stream;
    int stride, width, height;

  private:
    YUV420 yuv;
    std::function<YUV420 *> func;
};