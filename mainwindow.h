#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "motorcontroller.h"
#pragma execution_character_set("utf-8")

#include <QMainWindow>
#include <QLabel>
#include <QTime>
#include <QStateMachine>
#include <QSettings>
#include "QDebug"
#include "loghandler.h"
#include <QStandardItemModel>
#include <QStandardItem>
#include <QSignalMapper>
#include <vector>
#include "zmotion.h"
#include "zmcaux.h"
#include "zmotioncontrol.h"
#include "robotview/stlfileloader.h"
#include "ddr6robotwidget.h"
#include <QMessageBox>
#include <cstdlib>
#include <QRandomGenerator>
#include <QThread>
#include <Eigen/Dense>
#include <Eigen/Core>
#include <QFile>
#include <string>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QSerialPort>  // 必须包含
#include <QSpinBox>
#include <QThread>
#include <QQueue>
#include <QMutex>
#include <QDialog>
#include <QPushButton>
#include <QTcpSocket>
#include "SerialSensor.h"
#include "DataType.h"
#include "logmgr.h"
#include "TaskMgr.h"
#include "SCVehicle.h"
#include "LevelingSystem.h"
#include "MainLift.h"
#include "CoverPlate.h"
#include "Lift.h"
#include "Pin.h"
#include "lrumonitor.h"
#include "agvmonitor.h"
#include "levelmonitor.h"
#include "liftmonitor.h"
#include "coverplatemonitor.h"
#include "pinmonitor.h"
#include "vision_source/dahengTwoCams_qt_vs.h"
#include <cstdlib>
#include <QRandomGenerator>
#include <cmath>
#include <QtConcurrent>
#include "vision_source/dmdetected.h"
#include "vision_source/guangzilasersensor.h"
#include "DatabaseManager.h"
#include <QRegularExpression>
#include <QSpinBox>
#include <QStandardItem>
#include "sriSensorDataDefine.h"
#include "sriSensorManager.h"
#include "srisensorworker.h"
#include "uvc_camera.h"
#include <QCameraViewfinder>
#include <QDialog>
#include <QGridLayout>
#include <QSet>
#include <QVector>
#include "LRUdatadef.h"
#include "vision_source/guangzilasersensor.h"
#include "PMC_lib/PUSIControlLib.h"
#include "PMC_lib/PMC006xxLib.h"
#include <QSplineSeries>
#include <QValueAxis>
#include <QDateTimeAxis>
#include "STLModel/newform.h"
#include "SharedMemory/sharedmemorymanager.h"




//新增
using namespace nVehicleDataType;
using namespace nsTaskMgr;


class SCVehicle;

//extern ZMC_HANDLE g_handle;
using namespace Eigen;
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
    

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    //ZMC_HANDLE g_handle;    //定义连接控制器的连接句柄


protected:
    //重写关闭事件
    void closeEvent(QCloseEvent *event) override;


private slots:

    //新加，思博电机位置反馈
    void updateMotorPositionLabel(quint8 stationAddr, double position);
    void updateLightMotorPositionLabel(quint8 stationAddr, double position);
    void onFixtureMotorFault(quint8 stationAddr,bool fault,const QString &reason);

    void onDisMoveClicked(const QString& btnName);
    void onAbsMoveClicked(const QString& btnName);
    void onZeroClicked(const QString& btnName);
    void onInitClicked(const QString& btnName);
    void onDirButtonPressed(const QString& btnName, MoveMode dir);
    void onDirButtonReleased(const QString& btnName);
    void initMotorButtons();
    QPair<int, int> parseMotorNumbers(const QString& objName);


    void on_btnInitBus_clicked();

    void on_btnConnectSimulation_clicked();

    void on_btnConnect_clicked();

    void on_btnEnableAxis_clicked();

    void on_btnDisableAxis_clicked();

    void on_btnDisconnect_clicked();

    void UpdateUILog(QString str);

    void on_btnAxisSave_clicked();

    void on_cBoxAxisNum_editTextChanged(const QString &arg1);

    void onMvGetTimerOut();

    void on_btnInitAxis_clicked();

    void on_btnEmergencyStop_clicked();
    //获取串口数据
    void on_btnOpenForceCOM_clicked();
    void on_btnOpenAngleCOM_clicked();
    void on_btnOpenDistanceCOM_clicked();

    //获取电流数据
    void on_btn_ShowCurrent_clicked();

    // 串口通讯传感器槽函数
    void onForceDataReceived(const QVariantMap &data);
    void onAngleDataReceived(const QVariantMap &data);
    void onDistanceDataReceived(const QVariantMap &data);
    void onBatteryDataReceived(const QVariantMap &data); //更新第二块电池的信息

    void on_btn_ShowForce_clicked();
    void on_btn_ShowAngle_clicked();
    void on_btn_ShowDistance_clicked();


    //调平槽函数
    void on_btn_FeetToGround_clicked();
    void on_btn_BodyToGround_clicked();
    void on_btn_SpeedBodyUp_pressed();//车身上升
    void on_btn_SpeedBodyUp_released();
    void on_btn_SpeedBodyDown_pressed();//车身下降
    void on_btn_SpeedBodyDown_released();
    void on_btn_LevelRapidStop_clicked();
    void on_btn_BalanceBody_clicked();
    void on_btn_PosBodyUp_clicked();
    void on_btn_PosBodyDown_clicked();

    //六维力传感器
    void on_btn_GoZero_clicked();
    void on_btn_ShowForceInfo_clicked();
    void on_btn_CheckCollision_clicked();//碰撞检测

    //手动+距离模式调平
    void on_spinBox_Distance_valueChanged(double arg1);

    //六自由度平台***************开始
    void on_btn_PlatformOpen_clicked();

    void on_btn_PlatformClose_clicked();

    void on_btn_PlatformGo_clicked();

    void on_btn_PlatformStop_clicked();

    void on_btn_PlatformGoZero_clicked();

    void on_btn_PlatformReset_clicked();

    void on_btn_PlatformInitial_clicked();

    void update_six_platform(float x, float y, float z, float rx, float ry, float rz,
                             float gyroPitch, float gyroRoll, float gyroYaw,
                             bool isReachTarget,
                             bool emergencyStop,
                             bool lightFault,
                             bool heavyFault,
                             bool servoEnabled,
                             bool isMoving);  // 新增

    void onAutoLiftFinalSixDofCoordinates(
        float x,
        float y
        );
    //六自由度平台*********************结束
    //视觉模块****************开始
    void on_btn_secondary_lift_clicked();

    void on_btn_stop_secondry_clicked();

    void on_btn_CheckStatus_clicked();

    void on_btn_StatusModifyLatter_clicked();
    // 主线程接收信号后执行：更新UI显示
    void slot_updateGapInfo(nsTaskMgr::DetectedDisplayData vision_data);
    //新增的
    void onLight1ValueChanged(int value);
    void onLight2ValueChanged(int value);
    void lightbrightnessvaleinit(int value1,int value2);
    void onComboChanged(const QString &text);
    void on_btn_SelectLRUType_clicked();
    void on_distance_open_clicked();
    void updateDistanceLabels();  // 声明更新标签的槽函数


    void onXChanged(double val);
    void onYChanged(double val);
    void onRzChanged(double val);
    //视觉模块****************结束

    //AGV小车通讯
    void on_btn_CnntPushSvr_clicked();
    void on_btn_CnntNavigateSvr_clicked();
    void on_btn_CnntStatusSvr_clicked();
    void on_btn_CnntControlSvr_clicked();
    void on_btn_Send_clicked();
    void on_btn_CancleTask_clicked();
    void on_btn_AGVOpen_clicked();
    void on_btn_AGVReLocation_clicked();
    void on_btn_AGVPauseNavigate_clicked();
    void on_btn_AGVContinueNavigate_clicked();
    void on_btn_AGVCancelNavigate_clicked();
    void on_btn_MapLoad_clicked();


    //任务调度系统
    void on_btn_ExeVehTaskForward_clicked();
    void on_btn_ExeVehTaskBackward_clicked();
    void on_btn_ExeLevelTaskForward_clicked();
    void on_btn_ExeLevelTaskBackward_clicked();
    void on_btn_ExeMainLiftTaskForward_clicked();
    void on_btn_ExeMainLiftTaskBackward_clicked();
    void on_btn_ExeCoverPlateTaskForward_clicked();
    void on_btn_ExeCoverPlateTaskBackward_clicked();
    void on_btn_ExeLiftTaskForward_clicked();
    void on_btn_ExeLiftTaskBackward_clicked();
    void on_btn_ExePinTaskForward_clicked();
    void on_btn_StartTask_clicked();
    void on_btn_StopTask_clicked();
    void on_btn_PauseTask_clicked();
    void on_btn_ContinueTask_clicked();

    //更新标志位
    //调平
    void onLevelSymbolChanged(LevelSymbol symbol);

    //取盖板
    void onCoverPlateSymbolChanged(CoverPlateSymbol symbol);
    void resetCoverPlateSymbol();

    //自动和手动模式切换
    void onAutoModeToggled(bool checked);
    void onManualModeToggled(bool checked);
    void switchToAGV();
    void switchToLevelling();
    void switchToFirstLift();
    void switchToCoverPlate();
    void switchToSecondLift();
    void switchToPin();
    void switchToTaskMgr();
    void on_btn_LevelToAuto_clicked();
    void on_btn_AGVToAuto_clicked();
    void on_btn_AGVRapidStop_clicked();
    void on_comboBox_LRU_currentTextChanged(const QString &LRU_Name);

    //初始化刷新UI定时器
    void initUIUpdateTimer();

    //定时刷新UI
    void updateSensorUI();

    //更新系统状态
    void HandleSubsystemState(eSubSystem system, eTaskstate state, QString& baseStyle, QLabel* readyLbl,
                              QLabel* runningLbl, QLabel* completeLbl,QLabel* errorLbl);

    //更新任务类型和当前系统
    void HandleTaskTypeAndSystem(QLabel*SystemLbl,int taskType, QString& baseStyle,QLabel* forwardLbl,
                                 QLabel* backwardLbl);

    //取盖板机构
    void on_btn_OpenDoor_clicked();

    void on_btn_TakePlate_clicked();

    void on_btn_CloseDoor_clicked();

    void on_btn_CoverPlate_clicked();

    void on_btn_MainLiftToAuto_clicked();

    void on_btn_CoverPlateToAuto_clicked();

    //子系统状态监测
    void on_StartMonitor_triggered();
    void on_StartAGVMonitor_triggered();
    void on_StartLevelMonitor_triggered();
    void on_StartLiftMonitor_triggered();
    void on_StartCoverPlateMonitor_triggered();
    void on_StartPinMonitor_triggered();
    void on_StartCamera_trigger();
    void on_StartCoverPlate3D_triggered();


    void on_btn_MainLiftForward_clicked();

    void on_btn_MainLiftBackward_clicked();

    void on_btn_MainLiftRapidStop_clicked();

    void on_btn_CoverPlateRapidStop_clicked();

    void on_btn_LiftRapidStop_clicked();

    void on_btn_AllRapidStop_clicked();

    void on_btn_AGVClose_clicked();

    void on_btn_LiftToAuto_clicked();

    void on_btn_AutoTakePlate_clicked();

    void on_btn_AutoCoverPlate_clicked();

    void on_btnSixForceSave_clicked();

    void on_btnLevelSave_clicked();

    void ThresholdInit();//给UI控件初始化系统阈值参数

    void on_btn_AGV_Translate_clicked();

    void on_btn_AGV_Transverse_clicked();

    void on_btn_AGV_Turn_clicked();

    void on_btn_InitialAll_clicked();

    void on_btn_Motor78Forward_clicked();

    void on_btn_Motor78Backward_clicked();

    void on_comboBox_Station_currentTextChanged(const QString &arg1);

    void on_btnDbConnect_clicked();

    void on_btnDbDisconnect_clicked();

    void on_btnDbQuery_clicked();

    void on_btnDbAdd_clicked();

    void on_btnDbDelete_clicked();

    void on_btnDbShow_clicked();

    void on_btnDbSave_clicked();

    //MySQL数据库
    void refreshTableData(const QString& condition = QString()); // 刷新表格数据
    int getSelectedRowId(); // 获取当前选中行的ID
    //数据可视化
    void on_btnGenerateChart_clicked();

    void on_btn_AGVToTarget_clicked();

    void on_btn_AGVToOrigin_clicked();

    // 基于IO信号的安全保护措施
    void OnSafetyLimitTimerOut();

    void OnSafetyLimitTimerOut2();

    // 根据当前系统自动进行数据可视化
    void OnDataVisionTimerOut();

    // 调试代码
    void on_btn_Motor78Forward_2_clicked();

    void on_btn_Motor78Forward_3_clicked();

    void on_btn_Motor78Forward_4_clicked();


    void on_btn_Motor78Forward_10_pressed();

    void on_btn_Motor78Forward_10_released();

    void on_btn_Motor78Forward_9_pressed();

    void on_btn_Motor78Forward_9_released();

    void on_btn_Motor78Forward_5_pressed();

    void on_btn_Motor78Forward_5_released();

    void on_btn_Motor78Forward_6_pressed();

    void on_btn_Motor78Forward_6_released();

    void on_btn_Motor78Forward_7_pressed();

    void on_btn_Motor78Forward_7_released();

    void on_btn_Motor78Forward_8_pressed();

    void on_btn_Motor78Forward_8_released();

    void on_btn_Motor78Forward_11_pressed();

    void on_btn_Motor78Forward_11_released();

    void on_btn_Motor78Forward_12_pressed();

    void on_btn_Motor78Forward_12_released();

    void on_btn_Motor78Forward_13_pressed();

    void on_btn_Motor78Forward_13_released();

    void on_btn_Motor78Forward_14_pressed();

    void on_btn_Motor78Forward_14_released();

    void on_btn_Motor78Forward_15_pressed();

    void on_btn_Motor78Forward_15_released();

    void on_btn_Motor78Forward_16_pressed();

    void on_btn_Motor78Forward_16_released();

    void on_btn_Motor78Forward_17_clicked();

    void on_btn_PlatformLeveling_clicked();

    void on_btnTest_clicked();

    void on_btn_auto_descent_clicked();

    void on_btn_AGVPositionAdjust_clicked();

    void on_radioButton_leftPlate_toggled(bool checked);

    void on_radioButton_rightPlate_toggled(bool checked);

    //子线程执行初始化
    void on_EcatInitFinished(int result);

    //总初始化槽函数
    void initializeInitTasks();              // 初始化任务队列
    void startInitialization();              // 开始初始化
    void processNextInitTask();

    void on_btn_Battery2_clicked();

    //六维力传感器
    void onSixAxisSensorInitializationResult(bool success, const QString& message);
    void onSixAxisSensorDataReady(ForceSensorData* sensorData);

    //片架销子
    void on_btn_IOForward_pressed();

    void on_btn_IOForward_released();

    void on_btn_IOBackward_pressed();

    void on_btn_IOBackward_released();

    // void on_btn_IOForward_clicked();

    // void on_btn_IOBackward_clicked();

    //UVC相机
    void on_btn_FindCamera_clicked();

    void on_btn_CameraMonitor_clicked();

    void updateCameraPreviews(); // 定时更新预览图像

    bool selectMonitorCamera(int &cameraIndex, QString &cameraName);
    void addCameraMonitorView();
    void refreshCameraMonitorLayout();
    void closeCameraMonitor();

    //PMC电机，灯箱销子
    void on_btn_PMCConnect_clicked();

    void on_btn_PMCSpeedForward_pressed();

    void on_btn_PMCSpeedForward_released();

    void on_btn_PMCSpeedBackward_pressed();

    void on_btn_PMCSpeedBackward_released();

    void on_btn_PMCGetPos_clicked();

    void on_btn_PMCSetPos_clicked();

    void checkResult(PUSIResult res, const QString& tip);

    void on_btn_LightClamp_clicked();

    void on_btn_LightRelease_clicked();

    void on_btn_LightGuDing_clicked();

    void on_btn_LightSongKai_clicked();

    void on_btn_LightGuDing_pressed();

    void on_btn_LightGuDing_released();

    void on_btn_LightSongKai_pressed();

    void on_btn_LightSongKai_released();


    void on_btn_PMCSpeedBackward_2_clicked();

    void waitAGV();


    void on_btn_CheckDetach_clicked();

    void on_radioButton_middlePlate_toggled(bool checked);

    void on_radioButton_middlePlate_clicked();

    void on_radioButton_middlePlate_clicked(bool checked);

    void ChartInit();

    void onEmergencyStopPressed();

    void on_btnEnableMainLiftAxis_clicked();

    void on_btnDisableMainLiftAxis_clicked();

    void on_btn_CheckDetach_2_clicked();

    void on_btn_PianjiaConnect_clicked();

    void on_btn_EnablePianjia_clicked();

    void on_btn_IOForward_clicked();

    void on_btn_IOBackward_clicked();

    void on_btn_LightConnect_clicked();

    void on_btn_EnableLight_clicked();

    void on_btn_LightSpeedBackward_clicked();

    void on_btn_LightSpeedForward_clicked();

    void on_btn_zhengzhuan_1_pressed();

    void on_btn_zhengzhuan_1_released();

    void on_btn_fanzhuan_1_pressed();

    void on_btn_fanzhuan_1_released();

    void on_btn_zhengzhuan_2_pressed();

    void on_btn_zhengzhuan_2_released();

    void on_btn_fanzhuan_2_pressed();

    void on_btn_fanzhuan_2_released();

    void on_btn_zhengzhuan_3_pressed();

    void on_btn_zhengzhuan_3_released();

    void on_btn_fanzhuan_3_pressed();

    void on_btn_fanzhuan_3_released();

    void on_btn_zhengzhuan_4_pressed();

    void on_btn_zhengzhuan_4_released();

    void on_btn_fanzhuan_4_pressed();

    void on_btn_fanzhuan_4_released();

    void on_btn_zhengzhuan1_pressed();

    void on_btn_zhengzhuan1_released();

    void on_btn_fanzhuan1_pressed();

    void on_btn_fanzhuan1_released();

    void on_btn_zhengzhuan2_pressed();

    void on_btn_zhengzhuan2_released();

    void on_btn_fanzhuan2_pressed();

    void on_btn_fanzhuan2_released();

    void on_btn_PianjiaZero1_clicked();

    void on_btn_PianjiaZero2_clicked();

    void on_btn_PianjiaZero3_clicked();

    void on_btn_PianjiaZero4_clicked();

    void on_btn_LightSet0_1_clicked();

    void on_btn_LightSet0_2_clicked();

    void on_btn_PianjiaAbsmove_1_clicked();

    void on_btn_PianjiaAbsmove_2_clicked();

    void on_btn_PianjiaAbsmove_3_clicked();

    void on_btn_PianjiaAbsmove_4_clicked();

    void on_btn_LightAbsmove_1_clicked();

    void on_btn_LightAbsmove_2_clicked();

    void on_btn_ClearPianjiaAlarm_clicked();

    void on_btn_ClearLightAlarm_clicked();

private:
    Ui::MainWindow *ui;
    QTimer* mvGetTimer = nullptr;
    std::vector<std::array<double, 8>> joint_trajectory;

    QSettings *iniRead;
    QSettings *iniReadThreshold;
    ZMotionControl* zm;
    STLFileLoader* mystl;
    DDR6RobotWidget* myDDR6RobotWidget;
    AxisStatus *allAxisStatus;
    AxisStatus *pastallAxisStatus;
    void RobotMove(const Vector3d& position_goal);

    //视觉加六自由度平台
    QTimer* update_detected_Timer = nullptr;
    dahengTwoCams_qt_vs *dahengForm;
    LightSourceController* lightcontroller;
    guangziLaserSensor* lasersensor;
    QTimer *distanceTimer;  // 声明定时器对象
    DMDetected*dmcamera;
    //视觉加六自由度平台部分结束

    QStandardItemModel *m_tableModel;//表格视图绑定的模型

    //数据库
    DatabaseManager* MyDataBase = DatabaseManager::getInstance();

    //初始化线程
    QThread *zmThread;

    // 串口通讯传感器
    SerialSensor *m_forceSensor;
    SerialSensor *m_angleSensor;
    SerialSensor *m_distanceSensor;
    SerialSensor *m_battery;

    //串口传感器独立子线程
    QThread *m_forceThread;
    QThread *m_angleThread;
    QThread *m_distanceThread;
    QStringList m_portNameList;
    //刷新UI的定时器
    QTimer *uiUpdateTimer;
    // 缓存传感器数据
    QMap<QString, float> sensorDataCache;

    float stationValues[4];

    // 定义每个关节的角度
    std::array<double,8> thetas;
    std::vector<Eigen::Vector3d> jointPositions; // 存储每个关节的位置
    //初始化文件
    void iniInit();
    void SerialPortInit();

    // 获取所有可用的串口列表
    QStringList getPortNameList();//

    quint16 calculateCRC(const QByteArray &data);

    //void tableInit();
    QStandardItemModel *model;
    QString text;
    QStandardItem *item;
    void handleDataFetched(ZmotionStatus* packet);

    uint32 statusIOEmergencyStop=-1;

    int32 statusIOInPut[16];
    int32 statusIOInPut_Zmotion[16];//正运动拓展IO
    uint32 statusIOOutPut[16];



    bool m_feetContact[4];       // 记录四个脚座触地状态
    float m_motorSpeed[4];       // 各电机基础速度mm/s
    float m_current[4];          //同步监测窗口的电流信息
    QMutex m_dataMutex;          // 互斥锁
    QByteArray m_serialBuffer;   // 线程安全缓冲区
    QMutex m_bufferMutex;        // 缓冲区互斥锁


    int adjustmentPhase;         // 调整阶段
    bool isLevelEnd = false;     // 调平完成标志
    QMutex m_statusMutex;        // 状态访问锁
    QTimer * AdjustLevelingTimer;//调平定时器

    QTimer * CurrentTimer;//电流定时器
    QTimer * TorqueTimer;//扭矩定时器

    //状态监测窗口
    lrumonitor *monitor;
    AGVMonitor *agvmonitor;
    LevelMonitor *levelmonitor;
    LiftMonitor *liftmonitor;
    CoverPlateMonitor *coverplatemonitor;
    PinMonitor *pinmonitor;
    NewForm * CoverPlate3D;
    // dahengTwoCams_qt_vs *dahengForm;//视觉相机

    //状态监测窗口开启标志位
    bool StartlruMonitor = false;
    bool StartAGVMonitor = false;
    bool StartLevelMonitor = false;
    bool StartLiftMonitor = false;
    bool StartCoverPlateMonitor = false;
    bool StartPinMonitor = false;

    //心跳定时器
    QTimer* HeartTimer;

    //任务调度对象
    TaskMgr *m_TaskMgr;
    //子系统对象
    SCVehicle *m_vehicle;
    LevelingSystem *m_levelingSystem;
    MainLift *m_mainLift;
    CoverPlate *m_coverPlate;
    //新加门半自动流程是否正在等待按钮确认
    bool m_openDoorWaitingConfirm = false;
    bool m_closeDoorWaitingConfirm = false;

    Lift *m_lift;
    Pin *m_pin;


    //自动手动模式切换
    void setupModeButtons();  // 初始化按钮状态

    //自动手动标志位
    bool isAuto = true;

    //标志位是否闪烁
    bool isTransparent = true;

    //出现错误暂停自动模式
    void handleTaskError(eTaskStatus errorStatus);

    //处理TCP传感器数据子线程
    QThread *m_tcpThread;

    //处理锂电池数据子线程
    QThread *m_batteryThread;

    //同步电机状态定时器
    QTimer *motorStatusTimer;

    //任务类型状态字
    QString taskTypeStr;

    //AGV上次错误
    QStringList lastErrors;

    bool IsClearForce = false;
    bool IsClearAngle = false;
    bool IsClearDistance = false;

    //记录子系统变化用于数据库的数据可视化
    eTaskStatus m_TaskStatus;

    //判断是否异常退出
    bool isExitError = false;

    //六维力传感器初始化完成标志
    bool IsSixAxisSensorIni = false;

    //总初始化
    // 总初始化任务队列
    struct InitTask {
        std::function<void()> executeFunc;    // 执行函数
        std::function<bool()> checkFunc;      // 检查函数
        QString description;                  // 描述
        bool executed;                        // 是否已执行
    };

    QList<InitTask> m_initTasks;             // 初始化任务队列
    int m_currentInitTaskIndex;              // 当前执行的任务索引
    bool m_initializing = false;

    //标志位
    bool isConnectController = false;//连接工控机
    bool isInitBus = false;          //初始化总线
    bool isEnableAllAxis = false;    //电机上电使能
    bool isControllerOK = false;

    bool isAGVOpen = false;
    bool isCnntNavigateSvr = false;
    bool isCnntPushSvr = false;
    bool isCnntStatusSvr = false;
    bool isCnntControlSvr = false;
    bool isAGVOK = false;

    bool isShowingCurrent = false;
    bool isLevelingOK = false;

    bool isMainLiftOK = false;

    bool isCoverPlateOK = false;
    bool isESCReady = true;//预留值初始为真

    bool isLiftOK = false;
    bool isSixDofEnable = true;//预留值初始为真

    //基于IO信号的安全保护
    QTimer* SafetyLimitTimer;

    QTimer* SafetyLimitTimer2;

    //数据可视化定时器
    QTimer* DataVisionTimer;

    //防止重复和空白打印
    QString m_lastValidLog;

    QTimer* chartUpdateTimer;//定时刷新可视化图表

    //二维码信息
    double tag_diff_angle;
    double tag_diff_x;
    double tag_diff_y;

    QTimer *m_initTimer;    // 轮询定时器（检测每一步是否完成）
    int m_initStep;         // 当前初始化步骤索引（从0开始）

    //监测手指电流
    QTimer *m_fingerTimer;


    //六维力传感器
    SRISensorWorker* m_sixAxisSensorWorker;
    QThread *m_sixAxisSensorThread;
    QTimer* m_crashCheckTimer;// 碰撞检测定时器
    ForceSensorData SixSensorData;

    // 六维力绘图
    QChart* m_chart;
    QSplineSeries* m_seriesFx;
    QSplineSeries* m_seriesFy;
    QSplineSeries* m_seriesFz;
    QSplineSeries* m_seriesMx;
    QSplineSeries* m_seriesMy;
    QSplineSeries* m_seriesMz;
    QDateTimeAxis* m_axisX;
    QValueAxis* m_axisY;

    int m_dataPointIndex;            // 当前数据点序号
    const int m_maxDataPoints = 30;  // 界面上最多保留的点数

    //PGV信息
    QTimer* PGVTimer;

    //读码器信息
    QTimer* CameraTimer;

    QVector<float> currentVals;

    //片架销子释放标志
    bool IsPinRelease = true;
    bool IsPinShrink = false;

    // UVC 相机
    UVC_Camera* UVC_Device;
    QDialog *cameraPreviewDialog = nullptr;
    QGridLayout *m_cameraGridLayout = nullptr;
    QPushButton *m_btnAddCameraView = nullptr;
    QVector<UVC_Camera*> m_monitorCameras;
    QVector<QCameraViewfinder*> m_monitorViewfinders;
    QSet<int> m_openedCameraIndexes;

    // UVC 相机
    // 子进程+共享内存配置
    QProcess* m_workerProcesses[4];         // 4个相机子进程
    QString m_shmKeys[4];                   // 每个相机的共享内存Key
    SharedMemoryManager* m_shmManagers[4];  // 共享内存管理器

    // 预览窗口
    QDialog* m_previewDialogs[4];
    QLabel* m_previewLabels[4];      // 显示图像的Label
    QTimer* m_updateTimer;           // 定时读取共享内存更新UI

    QPushButton* m_btnSelectLRUType = nullptr;
    bool m_isLRUTypeSelected = false;

    //灯箱销子标志
    bool isLightRelease = true;
    bool isLightClamp = false;

    bool WaitAGV = false;

    //脱离检测定时器
    QTimer* DetachTimer;


    //思博电机
    MotorController * motor = nullptr;

    QThread *m_motorThread = nullptr;//新开一个电机线程

    QTimer* SBtimer;

    QList<quint8> m_motorAddrList = {0x01, 0x02};

    QList<quint8> m_motorAddrList2 = {0x01, 0x02,0x03,0x04};

    int m_currentReadIndex = 0;

    int m_currentReadIndex2 = 0;

    // 当前使用的工装电机组：0=片架，1=灯箱
    int m_fixtureGroup = 0;


signals:
    void UpdateUI(QString str);

    //六自由度和视觉***************开始
    void updateButtonState_servo_enabled_false();

    void updateButtonState_servo_enabled_true();

    void updateButtonState_motion_mode_3();

    void updateButtonState_motion_mode_1();
    void paramsSelected(const LRUInnerParams &params);
    void setcollisionChange(bool newState);
    //六自由度和视觉***************结束

    //六维力传感
    void sixAxisForceDataReceived(SixForeData* data);

    //检测窗口使用
    //发出电机状态信息的信号
    void motorStatusUpdated();
    // 发出六自由度平台信息的信号
    void sixDofPlatformDataPacked(SixDofPlatformData& data);
    //发出电流信息的信号
    void currentInfoUpdated(float current0, float current1, float current2, float current3);

    //发出包含LRU型号的信号，给数字孪生界面自动加载模型并更新位姿
    void SigLRUType(int a);

    //主线程和子线程交互信号
    void Sig_OpenPort_Force(const QString &portName);
    void Sig_ClosePort_Force();
    void Sig_StartAutoSend_Force(int interval);
    void Sig_StopAutoSend_Force();
    void Sig_OpenPort_Angle(const QString &portName);
    void Sig_ClosePort_Angle();
    void Sig_StartAutoSend_Angle(int interval);
    void Sig_StopAutoSend_Angle();
    void Sig_OpenPort_Distance(const QString &portName);
    void Sig_ClosePort_Distance();
    void Sig_StartAutoSend_Distance(int interval);
    void Sig_StopAutoSend_Distance();
    void Sig_OpenPort_Battery(const QString &portName);
    void Sig_ClosePort_Battery();
    void Sig_StartAutoSend_Battery(int interval);
    void Sig_StopAutoSend_Battery();
    //子线程初始化
    void sig_ExecuteEcatInit();
    //六维力传感器
    void sigInitializeSixAxisSensor(const QString& ip, int port);
    void sigStartReadingSixAxisSensor();
    void sigStopReadingSixAxisSensor();
    void sigSetSixAxisSensorZero();


public slots:
    void slotPrintInfo(QString info);
    void slotPrintStatus(eTaskStatus status);
    void slotDisplayVehStatus(VehicleSts veh_sts, int type);
    void slotVehTcpStateChanged(TcpSvrType,QAbstractSocket::SocketState);
};
#endif // MAINWINDOW_H








