#pragma once
#include <string>

class GPSDriver {
public:
    GPSDriver();
    ~GPSDriver();
    bool initUART(int uart_num, int baud);
    std::string readLine();
};
