#ifndef MAINLIFT_H
#define MAINLIFT_H
#include "DataType.h"
#include "zmotioncontrol.h"
#include "vision_source/guangzilasersensor.h"
#include <QObject>
#include <QDateTime>
#include <QTimer>
#include <QSettings>
#include <QMessageBox>
#include "DatabaseManager.h"


using namespace nsTaskMgr;

class MainLift: public QObject
{
    Q_OBJECT
public:
     explicit MainLift(ZMotionControl* zm, QObject *parent = nullptr);

    //一级升降标志位
    bool m_isAutotUp = false;
    bool m_isAutoDown = false;
    bool m_isManualUp = false;
    bool m_isManualDown = false;

    //任务调度
    eTaskStatus m_TaskStatus;

    QTimer* m_statusMonitorTimer;

    guangziLaserSensor* laser;// 激光测距传感器

public slots:

    //对应调度系统任务信号的执行槽函数
    void ExeMainLiftTask(TaskInfo info);

    //急停
    void emergencyStop();

    //手动升降
    void IsManualLift();

    //正向任务
    void ExeMainLiftTask1();

    //反向任务
    void ExeMainLiftTask2();

signals:

    void sigStatusFeedback(eTaskStatus);//任务完成信号
    void UpdateSystemInfo(QString str);

private:

    ZMotionControl* m_zm;

    //子系统状态判断
    eTaskstate m_lastState = eTaskstate::Ready;
    eTaskstate m_currentState = eTaskstate::Ready;
    QString m_errorInfo; // 存储错误信息

    //ini
    QSettings* iniReadThreshold;

    QTimer* UpTimer;
    QTimer* DownTimer;

    QDateTime m_taskStartTime;  // 记录任务开始时间

    QTimer* m_currentCollectionTimer;

private slots:

    //子系统状态判断
    void getSubSystemState();
    void handleStateTransition(eTaskstate newState);
    void monitorSubsystemStatus();
    void collectMotorCurrents();

};

#endif // MAINLIFT_H
