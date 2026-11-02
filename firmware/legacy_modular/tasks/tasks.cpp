#include "tasks/tasks.hpp"
#include "App.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace tasks {
    void initTasks(App& /*app*/) {
        // App lifecycle is driven cooperatively via loop() / App::update().
        // Additional dedicated FreeRTOS worker tasks (e.g. background logging)
        // can be instantiated here if desired.
    }
}
