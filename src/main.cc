#include <iostream>

struct YUV420
{
    std::vector<std::uint8_t> Y;
    std::vector<std::uint8_t> U;
    std::vector<std::uint8_t> V;
};

int main()
{
    YUV420 yuv = YUV420{
        std::vector<std::uint8_t>(420),
        std::vector<std::uint8_t>(420),
        std::vector<std::uint8_t>(420)};

    std::cout << yuv.Y.size() << std::endl;
}
