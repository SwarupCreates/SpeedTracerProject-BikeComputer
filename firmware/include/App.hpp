#pragma once
#include <memory>

class App {
public:
    App();
    ~App();

    void init();
    void update();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
