#include <functional>
#include <iostream>

using VF = std::function<void(int, int)>;

class Test
{
  private:
    int x, y;
    VF _func;

  public:
    Test(VF func)
    {
        _func = func;
        x = 1;
        y = 2;
    }

    void callback()
    {
        _func(x, y);
    }

    void print()
    {
        std::cout << x + y << std::endl;
    }
};

void ex(int x, int y)
{
    std::cout << x + y << std::endl;
}

int main()
{
    // Test test(ex);
    // test.callback();
    int ao = 480 & ~3;
    std::cout << ao << std::endl;
}

// #include <functional>
// #include <iostream>
// #include <string>

// typedef std::function<void(int)> FlyFunc;

// class Duck
// {
//   public:
//     Duck(FlyFunc flyFunc)
//     {
//         _flyFunc = flyFunc;
//         x = 33;
//     }
//     void run() { _flyFunc(x); }

//   private:
//     FlyFunc _flyFunc;
//     int x;
// };

// void fly(int x)
// {
//     std::cout << x << std::endl;
// }

// int main()
// {
//     Duck d(fly);
//     d.run();
// }