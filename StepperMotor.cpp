#include "StepperMotor.h"
#include <QDebug>


StepperMotor::StepperMotor(int step, int dir): step_pin_(step), dir_pin_(dir), current_position_(0)
{

}

void StepperMotor::move(int steps)
{
    if(steps == 0) return;

    // 设置方向
    bool direction = steps > 0;
    // digitalWrite(dir_pin_, direction);  // 实际GPIO控制

    // 模拟移动
    for(int i = 0; i < abs(steps); ++i)
    {
        // digitalWrite(step_pin_, HIGH);
        // delayMicroseconds(500);
        // digitalWrite(step_pin_, LOW);
        // delayMicroseconds(500);
    }

    current_position_ += steps;
    qDebug() << "Motor[" << step_pin_ << "] moved "
              << steps << " steps. Current pos: "
              << current_position_ ;
}
