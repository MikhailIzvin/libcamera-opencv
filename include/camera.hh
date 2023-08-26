#include <libcamera/libcamera.h>
#include <memory>
#include <opencv2/opencv.hpp>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

class Camera
{
  public:
    Camera(const Napi::CallbackInfo &info);
    ~Camera();

  private:
    // Callback for recieved frames
    void camera_callback(libcamera::Request *);

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

    // buffers for opencv
  private:
    std::uint8_t *yuv420_buffer;
    std::uint8_t *rgb_buffer;
};