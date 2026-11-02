#include "Logger.hpp"
#include <esp_log.h>

void Logger::init() {
    // Set the default ESP log level at runtime to avoid macro redefinition issues in ESP-IDF library builds.
    esp_log_level_set("*", ESP_LOG_INFO);
}

void Logger::info(const std::string &tag, const std::string &msg) {
    ESP_LOGI(tag.c_str(), "%s", msg.c_str());
}

void Logger::warn(const std::string &tag, const std::string &msg) {
    ESP_LOGW(tag.c_str(), "%s", msg.c_str());
}

void Logger::error(const std::string &tag, const std::string &msg) {
    ESP_LOGE(tag.c_str(), "%s", msg.c_str());
}
