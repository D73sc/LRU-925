#ifndef LEVELINGSYSTEM_H
#define LEVELINGSYSTEM_H
#include <QObject>
#include <QMutex>
#include <QMutexLocker>
#include <QTimer>
#include <QDateTime>
#include <QMessageBox>
#include "zmotioncontrol.h"
#include "DataType.h"
#include <QVector>
#include <QSettings>
#include "DatabaseManager.h"


using namespace nsTaskMgr;

class LevelingSystem : public QObject
{
    Q_OBJECT

public:
    explicit LevelingSystem(ZMotionControl* zm,QSettings* iniRead,QObject *parent = nullptr);

    //判定当前调平阶段，用于暂停后的恢复逻辑
    enum class LevelingState {
        Idle,        // 空闲状态
        ForContact,  // 检测脚座触地
        ForBalance   // 执行调平
    };

    LevelingState CurrentLevelingState = LevelingState::Idle;

    bool m_isAutoBalance = false;//调平标志位
    bool m_isAutotToGround = false;//车身触地标志位
    bool m_isManualBalance = false;
    bool m_isManualToGround = false;

    float angleXforleveling;
    float angleYforleveling;
    float disforleveling;

    //阈值
    float AngleXThreshold;
    float AngleYThreshold;
    float FrontLeftFootCurrentThreshold;
    float FrontRightFootCurrentThreshold;
    float RearLeftFootCurrentThreshold;
    float RearRightFootCurrentThreshold;

    //平行度修正
    float Anglex;
    float Angley;


    //状态监测定时器
    QTimer* m_statusMonitorTimer;
    //调平监测定时器
    QTimer* m_balanceMonitorTimer;

    //任务调度
    eTaskStatus m_TaskStatus;

    //正向任务
    void ExeLevelTask1();

    //反向任务
    void ExeLevelTask2();



public slots:

    // AGV 控制函数
    void FeetToGround();
    void BodyToGround();
    void StartSpeedBodyUp();
    void StopSpeedBodyUp();
    void StartSpeedBodyDown();
    void StopSpeedBodyDown();
    void BalanceBody();
    void PosBodyUp();
    void PosBodyDown();

    //对应调度系统任务信号的执行槽函数
    void ExeLevellingTask(TaskInfo info);

    //急停
    void emergencyStop();

    //手动调平
    void IsManualBalance();
private:
    // enum MoveMode { Cancel = 2, Forward = 1, Backward = -1};

    ZMotionControl* m_zm;
    QMutex m_statusMutex;
    bool m_feetContact[4] = {false};
    QTimer* m_adjustLevelingTimer = nullptr;

    //子系统状态判断
    eTaskstate m_lastState = eTaskstate::Ready;
    eTaskstate m_currentState = eTaskstate::Ready;
    QString m_errorInfo; // 存储错误信息
    QTimer* detectionTimer;
    QTimer* restartTimer;
    //检测调平前的抬升是否完成
    bool ForLevelOK = false;

    //调平
    int adjustmentPhase = 0;
    QVector<int> currentMotors;

    // 采集电机电流写入MySQL数据库
    QTimer* m_currentCollectionTimer;

    //任务开始时间，用于计算任务耗时
    QDateTime m_taskStartTime;

    bool singleReached[4];
    float motorposi[4] = {0.0f};

    QSettings *LevelSetting = nullptr;

private slots:

    MoveMode getMotorDirection(int motorId, bool isUp);//isUp代表螺杆的升降

    //子系统状态判断
    void getSubSystemState();
    void handleStateTransition(eTaskstate newState);
    void monitorSubsystemStatus();

    //采集电流数据写入数据库
    void collectMotorCurrents();


signals:
    void sigStatusFeedback(eTaskStatus);//任务完成信号
    void sigSymbolChanged(LevelSymbol symbol);
    void UpdateSystemInfo(QString str);


};

#endif // LEVELINGSYSTEM_H
