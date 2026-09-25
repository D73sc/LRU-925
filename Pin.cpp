#include "Pin.h"

Pin::Pin(ZMotionControl* zm, QObject *parent) : QObject(parent), m_zm(zm)
{
    // m_TaskStatus.sys =eSubSystem::Pin;
    m_statusMonitorTimer = new QTimer(this);
}

void Pin::ExePinTask(TaskInfo info)
{
    m_TaskStatus.taskinfo.TaskType =info.TaskType;
    JudgeTaskStatus(isComplete);
}

void Pin::JudgeTaskStatus(bool status)
{
    QTimer *timer =new QTimer(this);
    connect(timer,&QTimer::timeout,this,[=](){

        if(status)
        {
            m_TaskStatus.state =eTaskstate::Complete;
            emit sigStatusFeedback(m_TaskStatus);
        }
    });
}

void Pin::emergencyStop()
{
    m_zm->EmergencyStop();
}
