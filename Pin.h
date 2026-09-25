#ifndef PIN_H
#define PIN_H
#include "DataType.h"
#include <QObject>
#include <QTimer>
#include <zmotioncontrol.h>


using namespace nsTaskMgr;

class Pin: public QObject
{
    Q_OBJECT
public:
    explicit Pin(ZMotionControl* zm, QObject *parent = nullptr);

    //任务调度
    eTaskStatus m_TaskStatus;

    QTimer* m_statusMonitorTimer;

public slots:

    //判断任务状态
    void JudgeTaskStatus(bool status);

    //对应调度系统任务信号的执行槽函数
    void ExePinTask(TaskInfo info);

    void emergencyStop();

signals:

    void sigStatusFeedback(eTaskStatus);//任务完成信号

private:

    ZMotionControl* m_zm;

    bool isComplete;


private slots:



};

#endif // PIN_H
