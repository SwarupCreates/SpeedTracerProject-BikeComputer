#pragma once
#include <string>

// Minimal LVGL stub used for initial UI scaffolding. Replace with real LVGL
// integration later.
namespace lvgl {
    inline void init() {}
    inline void tick_inc(int ms) {}
    inline void task_handler() {}
    inline void label_set_text(int /*label*/, const std::string & /*text*/) {}
}
