#ifndef COVERPLATE_H
#define COVERPLATE_H
#include "DataType.h"
#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QSettings>
#include <QMessageBox>
#include <zmotioncontrol.h>
#include <CoverPlateFSM.h>
#include "DatabaseManager.h"


using namespace nsTaskMgr;

class CoverPlate: public QObject
{
    Q_OBJECT
public:
    explicit CoverPlate(ZMotionControl* zm, QObject *parent = nullptr);

    CoverPlateSymbol symbol;//标志位

    bool m_isManualOpen = false;
    bool m_isManualClose = false;
    bool m_isAutoOpen = false;
    bool m_isAutoClose = false;

    //任务调度
    eTaskStatus m_TaskStatus;

    //系统任务状态监测
    QTimer* m_statusMonitorTimer;

    DMDetected* dmcamera;//读码器

    QVector<float> currentVals;

    //状态机
    TakeFloorFSM* m_takeFloorFsm;      // 取封门状态机
    TakePlateFSM* m_takePlateFsm;      // 取盖板状态机
    CoverPlateFSM* m_coverPlateFsm;    // 盖盖板状态机
    CoverFloorFSM* m_coverFloorFsm;    // 盖封门状态机

    QSettings * iniReadPOS;

public slots:

    //对应调度系统任务信号的执行槽函数
    void ExeCoverPlateTask(TaskInfo info);

    void emergencyStop();

    void IsManualCoverPlate();

    // 正向任务
    void TakeSealDoor();   // 取封门
    void TakePlate();      // 取盖板

    // 反向任务
    void FixPlate();      // 盖盖板
    void FixSealDoor();   // 盖封门

    void CoverPlateTask1();//整体取封门+盖板
    void CoverPlateTask2(); // 整体盖封门+盖板

    // 禁用所有状态机动作
    void DisableAllFSMs();

    // 选择盖板
    void ChooseCoverPlate(bool symbol);
    void ChooseMiddleCoverPlate(bool symbol);
    void ResetChoose();

    void STOPAllFSMs();

    // 确认门流程的下一步
    void confirmDoorNextStep();

signals:

    void sigStatusFeedback(eTaskStatus);//任务完成信号
    void sigSymbolChanged(CoverPlateSymbol symbol);
    void UpdateSystemInfo(QString str);

    // takeDoor=true：取封门；false：盖封门
    void sigDoorStepConfirmationRequired(bool takeDoor,const QString& currentState,const QString& nextState);

private:

    ZMotionControl* m_zm;

    //子系统状态判断
    eTaskstate m_lastState = eTaskstate::Ready;
    eTaskstate m_currentState = eTaskstate::Ready;
    QString m_errorInfo; // 存储错误信息

    //ini
    QSettings* iniReadThreshold;

    //状态机激活标志，确保同一时间只运行一个状态机
    bool iscoverPlateFsmOpen = false;
    bool istakePlateFsmOpen = false;
    bool istakeFloorFsmOpen = false;
    bool iscoverFloorFsmOpen = false;

    QTimer* m_eventCheckTimer; // 事件检测定时器

    QDateTime m_taskStartTime;  // 任务开始时间

    QTimer* m_currentCollectionTimer;

    float motor5Pos;
    float motor6Pos;
    float motor7Pos;
    float motor8Pos;
    float motor9Pos;
    float motor10Pos;
    float motor5BorrowPos;
    float motor6BorrowPos;
    bool FingleCurrent9 = false;
    bool FingleCurrent10 = false;

private slots:

    // 获取子系统实时状态  电机5~11
    void getSubSystemState();
    // 处理状态转换
    void handleStateTransition(eTaskstate newState);
    // 状态监测定时器回调
    void monitorSubsystemStatus();

    // 状态机处理状态变化
    void onFsmStateChanged(const QString& stateName);

    // 间隔200ms检查是否触发状态机事件
    void checkEvents();

    //停止状态机
    void stopAllFSMs();

    void collectMotorCurrents();
};

#endif // COVERPLATE_H
