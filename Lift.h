#ifndef LIFT_H
#define LIFT_H
#include "DataType.h"
#include "zmotioncontrol.h"

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QSettings>
#include "DataType.h"
#include "DatabaseManager.h"

//视觉加六自由度平台-开始
#include"vision_source/dahengTwoCams_qt_vs.h"
#include"vision_source/arucoDetector.h"
#include "vision_source/udpcommunication.h"
#include"vision_source/lightsourcecontroller.h"
#include <opencv2/opencv.hpp>
#include "LRUdatadef.h"
//视觉加六自由度平台-结束

using namespace nsTaskMgr;

class Lift: public QObject
{
    Q_OBJECT
public:
    explicit Lift(ZMotionControl* zm,dahengTwoCams_qt_vs *dahengCamera,LightSourceController* lightcontrol,QObject *parent = nullptr);

    //力实时信息
    float Check_ForceInfo[6] = {0.0f};
    float LastCheck_ForceInfo[6] = {0.0f};
    bool  isforceOver = false;
    bool  istorqueOver = false;

    //阈值
    float Threshold[6] = {100.0f};

    //视觉加六自由度平台-开始
    UdpCommunication* m_SixDof;
    LightSourceController *m_lightcontroller;
    ArucoDetector *m_vision_detected;
    dahengTwoCams_qt_vs *m_dahengCamera;
    bool autolift = false;
    bool autodescent = false;
    bool detected_flag = false;
    //视觉加六自由度平台-结束

    bool m_isHalfAutoLiftUp = false;
    bool m_isAutoLiftDown = false;
    bool m_isManualLiftUp = false;
    bool m_isManualLiftDown = false;

    //任务调度
    eTaskStatus m_TaskStatus;

    QTimer* m_statusMonitorTimer;

    //判断是否下降，进行分离检测,并且每次上升重置这里标志
    bool isFirstLowering = true;

    QString overflowInfo;

    QString axisNames[6] = {"Fx", "Fy", "Fz", "Mx", "My", "Mz"};

    bool m_isDetectingDetach = false;

public slots:

    //对应调度系统任务信号的执行槽函数
    void ExeLiftTask(TaskInfo info);


    //信息交互
    void SetForceData(SixForeData* data);

    //碰撞检测
    void CheckCollision();

    void CheckDetach();

    //视觉加六自由度平台-开始
    bool StatusModifyLatte();
    bool auto_StatusModifyLatte();
    bool vision_detected();
    bool auto_vision_detected();
    bool auto_lift();
    void stop_auto_lift();
    void waitSixDof();
    bool auto_descent();

    void onParamsReceived(const LRUInnerParams &params);

    void setcollisionState(bool newState);
    //视觉加六自由度平台-结束

    // 获取子系统实时状态  电机12
    void getSubSystemState();
    // 处理状态转换
    void handleStateTransition(eTaskstate newState);
    // 状态监测定时器回调
    void monitorSubsystemStatus();

    //正向任务
    void ExeLiftTask1();

    //反向任务
    void ExeLiftTask2();

    //手动任务
    void IsManualLift();

    void collectMotorCurrents();
signals:
    void sigStatusFeedback(eTaskStatus);//任务完成信号
    void LiftUpdateUI(QString str);
    void lift_updateGapInfo(nsTaskMgr::DetectedDisplayData vision_data);
    // void lift_updateGapInfo(DetectedDisplayData& data);
    void isCollision(bool symbol,QString Info);
    void isDetach(bool symbol);
    void sendBrightnessValueToMainWindow(const int &value);
    void UpdateSystemInfo(QString str);

    void sigAutoLiftFinalSixDofCoordinates(
        float x,
        float y
        );

private:
    bool isComplete;

    //正运动
    ZMotionControl* m_zm;

    //视觉加六自由度平台-开始


    float m_angle;
    float m_gapwidth_x;
    float m_gapwidth_y;

    float x_gap_lift;
    float y_gap_lift;
    float final_z_lift;
    bool checkCollision_flag = false;
    //视觉加六自由度平台-结束

    //子系统状态判断
    eTaskstate m_lastState = eTaskstate::Ready;
    eTaskstate m_currentState = eTaskstate::Ready;
    QString m_errorInfo; // 存储错误信息

    //ini
    QSettings* iniReadThreshold;

    //抬升和下降定时器
    QTimer* UpTimer;
    QTimer* DownTimer;

    QDateTime m_taskStartTime;  //任务开始时间

    QTimer* m_currentCollectionTimer;
};

#endif // LIFT_H
