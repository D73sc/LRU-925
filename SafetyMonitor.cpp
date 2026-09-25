#include "SafetyMonitor.h"

SafetyMonitor::SafetyMonitor()
{

}

bool SafetyMonitor::checkTimeout(const TimePoint &start,int Timeout)
{
   // constexpr int TIMEOUT_SEC = Timeout;//30;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start);
    return elapsed.count() > Timeout;
}

bool SafetyMonitor::checkMotorCurrent(StepperMotor &motor)
{
    // 监测电机电流是否异常
    return true;
}
