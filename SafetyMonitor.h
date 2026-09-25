#ifndef SAFETYMONITOR_H
#define SAFETYMONITOR_H
#include <chrono>
#include <iostream>

#include "StepperMotor.h"

class SafetyMonitor
{
public:
    using TimePoint = std::chrono::steady_clock::time_point;
    SafetyMonitor();
    bool checkTimeout(const TimePoint& start,int Timeout=30);//second

    bool checkMotorCurrent(StepperMotor& motor);
};


//使用
/*

SafetyMonitor monitor;
    auto start_time = std::chrono::steady_clock::now();

    // 模拟时间流逝（实际中通过循环检测）
    // ...

    if (monitor.checkTimeout(start_time)) {
        std::cout << "Operation timeout!" << std::endl;
    }


// 转换为不同时间单位
auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
auto us = std::chrono::duration_cast<std::chrono::microseconds>(duration);

auto now = std::chrono::steady_clock::now();



// 记录操作开始时间
auto start = std::chrono::steady_clock::now();

// 在控制循环中检查超时
while (!operation_done) {
    if (safety_monitor.checkTimeout(start)) {
        emergencyStop();
        break;
    }
    // ... 其他操作 ...
}

*/

#endif // SAFETYMONITOR_H
