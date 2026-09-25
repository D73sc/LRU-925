#ifndef STEPPERMOTOR_H
#define STEPPERMOTOR_H


class StepperMotor
{
private:
    int step_pin_;
    int dir_pin_;
    int current_position_;

public:
    StepperMotor(int step, int dir);
    void move(int steps);
    int getPosition() const { return current_position_; }
};

//class SafetyMonitor {
//public:
//    bool checkMotorCurrent(StepperMotor& motor) {
//        // 监测电机电流是否异常
//        return true;
//    }

//    bool checkTimeout(const TimeStamp& start) {
//        // 检查调平操作是否超时
//        return false;
//    }
//};


#endif // STEPPERMOTOR_H
