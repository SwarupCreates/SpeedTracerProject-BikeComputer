#include "Drivers/GPSDriver.hpp"

GPSDriver::GPSDriver() {}
GPSDriver::~GPSDriver() {}

bool GPSDriver::initUART(int /*uart_num*/, int /*baud*/) {
    // Stub: initialization would configure UART peripheral
    return true;
}

std::string GPSDriver::readLine() {
    // Stub: would block/read from UART and return a line
    return std::string();
}
