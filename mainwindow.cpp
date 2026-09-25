#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "lrumonitor.h"
#include <QFileDialog>
#include <QDir>
#include <QCoreApplication>
#include <functional>
#include <QDateTime>
#include <QButtonGroup>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QInputDialog>
#include "motorcontroller.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->btn_FindCamera->setEnabled(false);
    // UVC相机
    // 初始化共享内存Key
    m_shmKeys[0] = "Camera1_ShmKey";
    m_shmKeys[1] = "Camera2_ShmKey";
    m_shmKeys[2] = "Camera3_ShmKey";
    m_shmKeys[3] = "Camera4_ShmKey";

    // 初始化共享内存管理器、子进程
    for (int i = 0; i < 4; ++i) {
        m_shmManagers[i] = new SharedMemoryManager(m_shmKeys[i]);
        m_workerProcesses[i] = nullptr;
        m_previewDialogs[i] = nullptr;
        m_previewLabels[i] = nullptr;
    }

    // 定时更新UI
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &MainWindow::updateCameraPreviews);
    m_updateTimer->setInterval(33);


    //六维力传感器图表初始化
    ChartInit();

    //定时刷新可视化图表
    chartUpdateTimer = new QTimer(this);

    SafetyLimitTimer = new QTimer(this);
    connect(SafetyLimitTimer,&QTimer::timeout,this,&MainWindow::OnSafetyLimitTimerOut);
    SafetyLimitTimer->start(100);

    SafetyLimitTimer2 = new QTimer(this);
    connect(SafetyLimitTimer2,&QTimer::timeout,this,&MainWindow::OnSafetyLimitTimerOut2);
    SafetyLimitTimer2->start(100);

    DataVisionTimer = new QTimer(this);
    connect(DataVisionTimer,&QTimer::timeout,this,&MainWindow::OnDataVisionTimerOut);
    DataVisionTimer->start(500);

    //信息提示表格视图绑定模型
    m_tableModel = new QStandardItemModel(this);
    m_tableModel->setColumnCount(2);
    m_tableModel->setHorizontalHeaderLabels({"时间", "提示信息"});
    ui->tableView_SystemStatus->setModel(m_tableModel);
    ui->tableView_SystemStatus->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    for (int i = 0; i < 4; ++i) {
        m_feetContact[i] = {false};
    }

    iniRead = new QSettings("param.ini", QSettings::IniFormat);

    zm=ZMotionControl::getinstance();

    allAxisStatus=new AxisStatus[ZMotionControl::AxisNum];
    pastallAxisStatus=new AxisStatus[ZMotionControl::AxisNum];
    for(int i=0;i<ZMotionControl::AxisNum;i++)
    {
        allAxisStatus[i]=AxisStatus(i);
        pastallAxisStatus[i]=AxisStatus(i);
    }

    iniInit();
    ThresholdInit();
    initMotorButtons();
    SerialPortInit();

    mvGetTimer = new QTimer();
    mvGetTimer->setInterval(1000);
    connect(mvGetTimer, &QTimer::timeout, this, &MainWindow::onMvGetTimerOut);
    connect(this, &MainWindow::UpdateUI, this, &MainWindow::UpdateUILog);
    connect(zm,&ZMotionControl::ConnectInfo,this,&MainWindow::UpdateUILog);//正运动控制器通讯信息


    // 启动回调线程，并传入主线程的回调函数， 使用 lambda 表达式包装成员函数指针
    //startCallbackThread要求传入一个接收ZmotionStatus*类型的回调函数，即handleDataFetched(status)
    //调用函数的同时创建线程并指定&ZMotionControl::DataFetcher作为执行函数，同时handleDataFetched(status)作为回调传入目的是在DataFetcher函数内调用
    //回调的本质是在函数1的内部执行函数2的逻辑
    zm->startCallbackThread([this](ZmotionStatus* status) {
        handleDataFetched(status);
    });    //回调函数获取数据

    motorStatusTimer = new QTimer(this);

    connect(motorStatusTimer,&QTimer::timeout,this,[=](){
        emit motorStatusUpdated();});

    // 相机
    UVC_Device = new UVC_Camera(this);
    connect(UVC_Device,&UVC_Camera::UVCInfo,this,&MainWindow::UpdateUILog);


    //思博电机
    SBtimer = new QTimer(this);

    m_motorThread = new QThread(this); //创建电机线程

    motor = new MotorController(0x01);

    motor->moveToThread(m_motorThread);// 移到子线程

    connect(m_motorThread, &QThread::finished,motor, &QObject::deleteLater);// 线程结束时自动删除 motor

    connect(motor, &MotorController::motorStatus,this, &MainWindow::UpdateUILog,Qt::QueuedConnection);//电机状态回 UI

    connect(motor,&MotorController::motorFaultChanged,this,&MainWindow::onFixtureMotorFault,Qt::QueuedConnection);

    //启动线程
    m_motorThread->start();
    //motor = new MotorController(0x01,this);
    //connect(motor,&MotorController::motorStatus,this,&MainWindow::UpdateUILog);
    // connect(motor, &MotorController::positionRead, this, [this](double pos){
    //      ui->label_358->setText(QString::number(pos));
    //  });
    // connect(motor,QOverload<quint8, double>::of(&MotorController::positionRead),this,[=](quint8 addr, double pos) {
    //             qDebug() << "电机站号:" << addr << "当前位置:" << pos;
    //         });

    connect(motor,QOverload<quint8, double>::of(&MotorController::positionRead),this,&MainWindow::updateMotorPositionLabel,Qt::QueuedConnection);
    connect(motor,QOverload<quint8, double>::of(&MotorController::positionRead),this,&MainWindow::updateLightMotorPositionLabel,Qt::QueuedConnection);


    // 传感器
    m_forceSensor = new SerialSensor(SensorType::FORCE_SENSOR);
    m_angleSensor = new SerialSensor(SensorType::ANGLE_SENSOR);
    m_distanceSensor = new SerialSensor(SensorType::DISTANCE_SENSOR);
    m_battery = new SerialSensor(SensorType::BATTERY_SENSOR);
    m_sixAxisSensorWorker = new SRISensorWorker();


    //数据独立子线程
    m_forceThread = new QThread(this);
    m_angleThread = new QThread(this);
    m_distanceThread = new QThread(this);
    m_sixAxisSensorThread= new QThread(this);
    m_batteryThread = new QThread(this);


    //移入子线程
    m_forceSensor->moveToThread(m_forceThread);
    m_angleSensor->moveToThread(m_angleThread);
    m_distanceSensor->moveToThread(m_distanceThread);
    m_sixAxisSensorWorker->moveToThread(m_sixAxisSensorThread);
    m_battery->moveToThread(m_batteryThread);


    //开启线程
    m_forceThread->start();
    m_angleThread->start();
    m_distanceThread->start();
    m_sixAxisSensorThread->start();
    m_batteryThread->start();

    //初始化子线程
    zmThread = new QThread(this);

    //在子线程打开串口并开启自动报文定时器
    connect(m_forceSensor, &SerialSensor::newDataReceived, this, &MainWindow::onForceDataReceived, Qt::QueuedConnection);
    connect(m_angleSensor, &SerialSensor::newDataReceived, this, &MainWindow::onAngleDataReceived, Qt::QueuedConnection);
    connect(m_distanceSensor, &SerialSensor::newDataReceived, this, &MainWindow::onDistanceDataReceived, Qt::QueuedConnection);
    connect(m_battery, &SerialSensor::newDataReceived, this, &MainWindow::onBatteryDataReceived, Qt::QueuedConnection);
    // connect(ui->btnOpenForceCOM,&QPushButton::clicked,m_forceSensor,[this](){on_btnOpenForceCOM_click();},Qt::QueuedConnection);
    // connect(ui->btn_ShowForce,&QPushButton::clicked,m_forceSensor,[this](){on_btn_ShowForce_click();},Qt::QueuedConnection);
    // connect(ui->btnOpenAngleCOM,&QPushButton::clicked,m_angleSensor,[this](){on_btnOpenAngleCOM_click();},Qt::QueuedConnection);
    // connect(ui->btn_ShowAngle,&QPushButton::clicked,m_angleSensor,[this](){on_btn_ShowAngle_click();},Qt::QueuedConnection);
    // connect(ui->btnOpenDistanceCOM,&QPushButton::clicked,m_distanceSensor,[this](){on_btnOpenDistanceCOM_click();},Qt::QueuedConnection);
    // connect(ui->btn_ShowDistance,&QPushButton::clicked,m_distanceSensor,[this](){on_btn_ShowDistance_click();},Qt::QueuedConnection);
    // connect(ui->btn_GoZero,&QPushButton::clicked,m_sixAxisSensor,[this](){on_btn_GoZero_click();},Qt::QueuedConnection);
    //主线程和传感器子线程交互
    connect(this, &MainWindow::Sig_OpenPort_Angle,m_angleSensor,&SerialSensor::openPort,Qt::QueuedConnection);
    connect(this, &MainWindow::Sig_ClosePort_Angle,m_angleSensor,&SerialSensor::closePort,Qt::QueuedConnection);
    connect(this, &MainWindow::Sig_StartAutoSend_Angle,m_angleSensor,&SerialSensor::startAutoSend,Qt::QueuedConnection);
    connect(this, &MainWindow::Sig_StopAutoSend_Angle,m_angleSensor,&SerialSensor::stopAutoSend,Qt::QueuedConnection);

    connect(this, &MainWindow::Sig_OpenPort_Distance,m_distanceSensor,&SerialSensor::openPort,Qt::QueuedConnection);
    connect(this, &MainWindow::Sig_ClosePort_Distance,m_distanceSensor,&SerialSensor::closePort,Qt::QueuedConnection);
    connect(this, &MainWindow::Sig_StartAutoSend_Distance,m_distanceSensor,&SerialSensor::startAutoSend,Qt::QueuedConnection);
    connect(this, &MainWindow::Sig_StopAutoSend_Distance,m_distanceSensor,&SerialSensor::stopAutoSend,Qt::QueuedConnection);

    connect(this, &MainWindow::Sig_OpenPort_Force,m_forceSensor,&SerialSensor::openPort,Qt::QueuedConnection);
    connect(this, &MainWindow::Sig_ClosePort_Force,m_forceSensor,&SerialSensor::closePort,Qt::QueuedConnection);
    connect(this, &MainWindow::Sig_StartAutoSend_Force,m_forceSensor,&SerialSensor::startAutoSend,Qt::QueuedConnection);
    connect(this, &MainWindow::Sig_StopAutoSend_Force,m_forceSensor,&SerialSensor::stopAutoSend,Qt::QueuedConnection);

    connect(this, &MainWindow::Sig_OpenPort_Battery,m_battery,&SerialSensor::openPort,Qt::QueuedConnection);
    connect(this, &MainWindow::Sig_ClosePort_Battery,m_battery,&SerialSensor::closePort,Qt::QueuedConnection);
    connect(this, &MainWindow::Sig_StartAutoSend_Battery,m_battery,&SerialSensor::startAutoSend,Qt::QueuedConnection);
    connect(this, &MainWindow::Sig_StopAutoSend_Battery,m_battery,&SerialSensor::stopAutoSend,Qt::QueuedConnection);

    connect(this, &MainWindow::sigInitializeSixAxisSensor, m_sixAxisSensorWorker, &SRISensorWorker::initializeSensor,Qt::QueuedConnection);
    connect(this, &MainWindow::sigStartReadingSixAxisSensor,m_sixAxisSensorWorker, &SRISensorWorker::startReadingData,Qt::QueuedConnection);
    connect(this, &MainWindow::sigStopReadingSixAxisSensor,m_sixAxisSensorWorker, &SRISensorWorker::stopReadingData,Qt::QueuedConnection);
    connect(this, &MainWindow::sigSetSixAxisSensorZero,m_sixAxisSensorWorker, &SRISensorWorker::setAllSensorDataZero,Qt::QueuedConnection);
    connect(m_sixAxisSensorWorker,&SRISensorWorker::initializationResult,this,&MainWindow::onSixAxisSensorInitializationResult,Qt::QueuedConnection);
    connect(m_sixAxisSensorWorker,&SRISensorWorker::sensorDataReady,this,&MainWindow::onSixAxisSensorDataReady,Qt::QueuedConnection);


    //硬件急停信号连接
    connect(ZMotionControl::getinstance(),&ZMotionControl::sig_EmergencyStopPressed,this,&MainWindow::onEmergencyStopPressed,Qt::QueuedConnection);

    initUIUpdateTimer();

    //初始化碰撞检测定时器
    m_crashCheckTimer = new QTimer(this);
    //脱离检测定时器
    DetachTimer = new QTimer(this);
    //电流定时器
    CurrentTimer = new QTimer(this);


    //加载可用的.smap格式文件
    QString mapDirPath = QCoreApplication::applicationDirPath() + "/Map/";
    QDir mapDir(mapDirPath);

    if (mapDir.exists()) {

        QStringList smapFiles = mapDir.entryList(QStringList() << "*.smap", QDir::Files);

        for (const QString& smapFile : smapFiles) {
            QString fullFilePath = mapDir.absoluteFilePath(smapFile);
            ui->comboBox_Map->addItem(fullFilePath);
        }
    }


    //心跳信号机制
    HeartTimer = new QTimer(this);
    connect(HeartTimer, &QTimer::timeout, this, [=]() {

        QTime currentTime = QTime::currentTime();
        float heartbeatTime = currentTime.second() + currentTime.msec() * 0.001f;
        zm->SetHeartBeatInfo(heartbeatTime);

        // float heartBeatInfo = zm->GetHeartBeatInfo();
        // ui->lbl_HeartBeatInfo->setText(QString::number(heartBeatInfo));

    });

    HeartTimer->start(200);


    //任务调度
    //.h文件声明的指针，在构造函数中要初始化，否则空指针会导致进程直接崩溃
    m_TaskMgr =new TaskMgr(this);
    m_vehicle =new SCVehicle(this);
    m_levelingSystem = new LevelingSystem(zm,iniRead,this);
    m_mainLift = new MainLift(zm, this);
    m_coverPlate= new CoverPlate(zm,this);
    m_pin= new Pin(zm,this);

    //视觉+六自由度平台****************************************************************************
    //视觉加六自由度平台***************开始
    //大恒相机控制类对象
    dahengForm = new dahengTwoCams_qt_vs(this);
    connect(dahengForm, &dahengTwoCams_qt_vs::dahengUpdateUI,this, &MainWindow::UpdateUILog);
    //光源控制类对象
    lightcontroller = new LightSourceController(this);
    connect(lightcontroller, &LightSourceController::lightsourceUpdateUI,this, &MainWindow::UpdateUILog);
    connect(lightcontroller, &LightSourceController::lightBrightness,this, &MainWindow::lightbrightnessvaleinit);
    lightcontroller->openSerial();
    connect(ui->light1_value, SIGNAL(valueChanged(int)), this, SLOT(onLight1ValueChanged(int)));
    connect(ui->light2_value, SIGNAL(valueChanged(int)), this, SLOT(onLight2ValueChanged(int)));
    //二级升降控制类对象（传入正运动和大恒图像）
    m_lift = new Lift(zm,dahengForm,lightcontroller,this);
    connect(m_lift->m_SixDof, &UdpCommunication::poseFeedback, this, &MainWindow::update_six_platform, Qt::QueuedConnection);
    connect(m_lift, &Lift::lift_updateGapInfo,
            this, &MainWindow::slot_updateGapInfo,
               Qt::QueuedConnection);
    connect(m_lift,&Lift::LiftUpdateUI,this,&MainWindow::UpdateUILog);
    connect(m_lift->m_vision_detected,&ArucoDetector::arucoUpdateUI,this,&MainWindow::UpdateUILog);

    // 光源自动亮度调节
    connect(m_lift, &Lift::sendBrightnessValueToMainWindow, this, &MainWindow::onLight1ValueChanged);
    connect(m_lift, &Lift::sendBrightnessValueToMainWindow, this, &MainWindow::onLight2ValueChanged);



    connect(ui->x_offset, qOverload<double>(&QDoubleSpinBox::valueChanged),this, &MainWindow::onXChanged);

    connect(ui->y_offset, qOverload<double>(&QDoubleSpinBox::valueChanged),this, &MainWindow::onYChanged);

    connect(ui->rz_offset, qOverload<double>(&QDoubleSpinBox::valueChanged),this, &MainWindow::onRzChanged);


    //碰撞检测标志位
    connect(this, &MainWindow::setcollisionChange,
            m_lift, &Lift::setcollisionState);


    ui->comboBox_LRUdata->addItems(LRUpresetData().keys());
    connect(ui->comboBox_LRUdata, &QComboBox::currentTextChanged, this, &MainWindow::onComboChanged);
    ui->comboBox_LRUdata->hide();
    m_btnSelectLRUType = new QPushButton("选择LRU类型", ui->groupBox_ManualMode);
    ui->gridLayout_43->addWidget(m_btnSelectLRUType, 2, 0, 1, 1);
    connect(m_btnSelectLRUType, &QPushButton::clicked, this, &MainWindow::on_btn_SelectLRUType_clicked);
    connect(this, &MainWindow::paramsSelected,
            m_lift, &Lift::onParamsReceived);
    connect(this, &MainWindow::paramsSelected,
            m_lift->m_vision_detected, &ArucoDetector::onParamsReceived);
    connect(this, &MainWindow::paramsSelected,
            m_lift->m_dahengCamera, &dahengTwoCams_qt_vs::onParamsReceived);


    //一级激光位移传感器
    lasersensor = new guangziLaserSensor("COM11",this);
    // 初始化定时器
    distanceTimer = new QTimer(this);
    // 设置定时周期（单位：毫秒，这里设为200ms，可根据需求调整）
    distanceTimer->setInterval(200);
    // 关联定时器超时信号到更新标签的槽函数
    connect(distanceTimer, &QTimer::timeout, this, &MainWindow::updateDistanceLabels);
    connect(lasersensor, &guangziLaserSensor::laserUpdateUi,this, &MainWindow::UpdateUILog);
    lasersensor->openSerialPort();
    distanceTimer->start();

    connect(m_lift, &Lift::sigAutoLiftFinalSixDofCoordinates,
            this, &MainWindow::onAutoLiftFinalSixDofCoordinates,
            Qt::QueuedConnection);

    //视觉加六自由度平台部分******************************结束
    //******************************************************************************************


    //AGV 信号
    connect(m_vehicle,&SCVehicle::sigPrintInfo,this,&MainWindow::slotPrintInfo);
    connect(m_vehicle,&SCVehicle::sigNaviInfo,this,&MainWindow::slotPrintInfo);
    connect(m_vehicle,&SCVehicle::sigVehSts,this,&MainWindow::slotDisplayVehStatus);// void sigVehSts(VehicleSts)，信号携带VehicleSts类型数据
    connect(m_vehicle,&SCVehicle::sigVehTcpStateChanged,this,&MainWindow::slotVehTcpStateChanged);


    //任务调度信号,信号与槽的连接，要先实例化对象后再连接
    //调度系统向子系统发送执行信号
    connect(m_TaskMgr,&TaskMgr::sigExeVehTask,m_vehicle,&SCVehicle::ExeVehTask);
    connect(m_TaskMgr,&TaskMgr::sigExecLevellingTask,m_levelingSystem,&LevelingSystem::ExeLevellingTask);
    connect(m_TaskMgr,&TaskMgr::sigExecMainLiftTask,m_mainLift,&MainLift::ExeMainLiftTask);
    connect(m_TaskMgr,&TaskMgr::sigExecCoverPlateTask,m_coverPlate,&CoverPlate::ExeCoverPlateTask);
    connect(m_TaskMgr,&TaskMgr::sigExecLiftTask,m_lift,&Lift::ExeLiftTask);
    connect(m_TaskMgr,&TaskMgr::sigExecPinTask,m_pin,&Pin::ExePinTask);

    //子系统向调度系统发送反馈信号
    connect(m_vehicle,&SCVehicle::sigStatusFeedback,m_TaskMgr,&TaskMgr::slotSubSysStateChanged);
    connect(m_levelingSystem,&LevelingSystem::sigStatusFeedback,m_TaskMgr,&TaskMgr::slotSubSysStateChanged);
    connect(m_mainLift,&MainLift::sigStatusFeedback,m_TaskMgr,&TaskMgr::slotSubSysStateChanged);
    connect(m_coverPlate,&CoverPlate::sigStatusFeedback,m_TaskMgr,&TaskMgr::slotSubSysStateChanged);
    connect(m_lift,&Lift::sigStatusFeedback,m_TaskMgr,&TaskMgr::slotSubSysStateChanged);
    connect(m_pin,&Pin::sigStatusFeedback,m_TaskMgr,&TaskMgr::slotSubSysStateChanged);

    //调度系统和主界面的信息交互
    connect(m_TaskMgr,&TaskMgr::sigStatusInfo,this,&MainWindow::slotPrintStatus);
    connect(m_TaskMgr,&TaskMgr::sigError,this,&MainWindow::handleTaskError); //立即暂停当前子系统所有动作


    //主窗口和子系统交互六维力信息
    connect(this, &MainWindow::sixAxisForceDataReceived, m_lift, &Lift::SetForceData);
    // connect(this, &MainWindow::sixDofPlatformDataPacked, m_lift, &Lift::SetPlatformData);

    // 更新标志位
    //调平
    connect(m_levelingSystem, &LevelingSystem::sigSymbolChanged,this, &MainWindow::onLevelSymbolChanged);
    //取盖板
    connect(m_coverPlate, &CoverPlate::sigSymbolChanged, this, &MainWindow::onCoverPlateSymbolChanged);

    //接收“等待确认”信号，新加
    connect(m_coverPlate,&CoverPlate::sigDoorStepConfirmationRequired,this,
        [this](bool takeDoor,
               const QString& currentState,
               const QString& nextState)
        {
            QString info =
                QString("%1已经完成，等待确认。下一步：%2")
                    .arg(currentState)
                    .arg(nextState);

            UpdateUILog(info);

            if (takeDoor) {
                m_openDoorWaitingConfirm = true;
                m_closeDoorWaitingConfirm = false;

                ui->btn_OpenDoor->setText("确认下一步");
                ui->btn_OpenDoor->setToolTip(info);
                ui->btn_OpenDoor->setEnabled(true);

                ui->btn_CloseDoor->setEnabled(false);
            }
            else {
                m_closeDoorWaitingConfirm = true;
                m_openDoorWaitingConfirm = false;

                ui->btn_CloseDoor->setText("确认下一步");
                ui->btn_CloseDoor->setToolTip(info);
                ui->btn_CloseDoor->setEnabled(true);

                ui->btn_OpenDoor->setEnabled(false);
            }
        });

    //子系统状态监测
    monitor = new lrumonitor(this);
    agvmonitor = new AGVMonitor(this);
    levelmonitor = new LevelMonitor(zm,this);
    liftmonitor = new LiftMonitor(zm,this);
    coverplatemonitor = new CoverPlateMonitor(zm,this);
    pinmonitor = new PinMonitor(this);
    CoverPlate3D = new NewForm(this);
    // dahengForm = new dahengTwoCams_qt_vs(this);

    connect(ui->StartMonitor, &QAction::triggered, this, &MainWindow::on_StartMonitor_triggered);
    connect(ui->StartAGVMonitor, &QAction::triggered, this, &MainWindow::on_StartAGVMonitor_triggered);
    connect(ui->StartLevelMonitor, &QAction::triggered, this, &MainWindow::on_StartLevelMonitor_triggered);
    connect(ui->StartLiftMonitor, &QAction::triggered, this, &MainWindow::on_StartLiftMonitor_triggered);
    connect(ui->StartCoverPlateMonitor, &QAction::triggered, this, &MainWindow::on_StartCoverPlateMonitor_triggered);
    connect(ui->StartPinMonitor, &QAction::triggered, this, &MainWindow::on_StartPinMonitor_triggered);
    connect(ui->StartCoverPlate3D, &QAction::triggered, this, &MainWindow::on_StartCoverPlate3D_triggered);
    //AGV
    connect(m_vehicle, &SCVehicle::sigVehSts, agvmonitor, &AGVMonitor::UpdateVehStatus);
    connect(m_distanceSensor, &SerialSensor::newDataReceived, agvmonitor, &AGVMonitor::UpdateDistanceData);
     connect(m_battery, &SerialSensor::newDataReceived, agvmonitor, &AGVMonitor::UpdateBatterydata);
    //调平
    connect(m_levelingSystem, &LevelingSystem::UpdateSystemInfo, this,&MainWindow::UpdateUILog);
    connect(this,&MainWindow::motorStatusUpdated, levelmonitor, &LevelMonitor::onMotorStatusUpdated);
    connect(m_forceSensor, &SerialSensor::newDataReceived, levelmonitor, &LevelMonitor::UpdateForceData);
    connect(m_angleSensor, &SerialSensor::newDataReceived, levelmonitor, &LevelMonitor::UpdateAngleData);
    connect(m_levelingSystem, &LevelingSystem::sigSymbolChanged, levelmonitor, &LevelMonitor::onLevelSymbolChanged);
    connect(this, &MainWindow::currentInfoUpdated, levelmonitor, &LevelMonitor::CurrentUpdate);
    //升降
    connect(m_mainLift, &MainLift::UpdateSystemInfo, this,&MainWindow::UpdateUILog);
    connect(m_lift, &Lift::UpdateSystemInfo, this,&MainWindow::UpdateUILog);
    connect(this, &MainWindow::motorStatusUpdated, liftmonitor, &LiftMonitor::onMotorStatusUpdated);
    connect(this, &MainWindow::sixAxisForceDataReceived, liftmonitor, &LiftMonitor::updateSixAxisSensorInfo);
    connect(this, &MainWindow::sixDofPlatformDataPacked, liftmonitor, &LiftMonitor::updateSixDofPlatformUI);
    // connect(m_lift, &Lift::lift_updateGapInfo, liftmonitor, &LiftMonitor::updateDetectedDisplayUI);
    connect(m_lift, &Lift::isCollision,this,[this](bool Mysymbol,QString Info){
        if (Mysymbol){
            ui->lbl_CrashInfo->setText(Info + "，紧急制动");
            const QString baseStyle = "border-radius: 25px; border: 2px solid black; background-color: red;";
            ui->lbl_isCrash->setStyleSheet(baseStyle);
            liftmonitor->IsCollsion = true;
        }
        else{
            ui->lbl_CrashInfo->setText("当前未检测到碰撞");
            const QString baseStyle = "border-radius: 25px; border: 2px solid black; background-color: green;";
            ui->lbl_isCrash->setStyleSheet(baseStyle);
            liftmonitor->IsCollsion = false;
        }
    });
    connect(m_lift, &Lift::isDetach,this,[this](bool Mysymbol){
        if (Mysymbol){
            ui->lbl_CrashInfo->setText("LRU已成功挂上！");
            const QString baseStyle = "border-radius: 25px; border: 2px solid black; background-color: green;";
            ui->lbl_isCrash->setStyleSheet(baseStyle);
        }
        else{
            ui->lbl_CrashInfo->setText("LRU未挂上，紧急制动！");
            const QString baseStyle = "border-radius: 25px; border: 2px solid black; background-color: green;";
            ui->lbl_isCrash->setStyleSheet(baseStyle);
            liftmonitor->IsCollsion = false;
        }
    });

    //取盖板
    connect(m_coverPlate, &CoverPlate::UpdateSystemInfo, this,&MainWindow::UpdateUILog);
    connect(m_coverPlate, &CoverPlate::sigSymbolChanged, coverplatemonitor, &CoverPlateMonitor::onCoverPlateSymbolChanged);
    connect(this, &MainWindow::motorStatusUpdated, coverplatemonitor , &CoverPlateMonitor::onMotorStatusUpdated);
    connect(m_coverPlate->dmcamera, &DMDetected::statusUpdated, this,&MainWindow::UpdateUILog);
    //初始化按钮状态
    setupModeButtons();

    // ui->textEdit_SystemStatus->setText(QString::number(m_levelingSystem->AngleXThreshold));



    connect(ui->StartCamera,&QAction::triggered,this,&MainWindow::on_StartCamera_trigger);

    //根据LRU型号更新数字孪生
    connect(this,&MainWindow::SigLRUType,monitor,&lrumonitor::UpdateLRU);

    //显示二维码信息
    connect(m_vehicle, &SCVehicle::sigPGVDataUpdated, this, [=](double angle, double x, double y) {
        ui->tag_diff_angle->setText(QString::number(angle, 'f', 2)); // 保留2位小数
        ui->tag_diff_x->setText(QString::number(x, 'f', 2));
        ui->tag_diff_y->setText(QString::number(y, 'f', 2));
        tag_diff_angle = angle;
        tag_diff_x = x;
        tag_diff_y = y;
    });

    //打印状态机状态
    connect(&m_coverPlate->m_takeFloorFsm->getFrontFSM(), &TakeFloorFSMFront::sigStateChanged, this, [&](const QString& state)
            {emit UpdateUI(state);});

    connect(&m_coverPlate->m_takePlateFsm->getFrontFSM(), &TakePlateFSMFront::sigStateChanged, this, [&](const QString& state)
            {emit UpdateUI(state);});

    connect(&m_coverPlate->m_coverPlateFsm->getFrontFSM(), &CoverPlateFSMFront::sigStateChanged, this, [&](const QString& state)
            {emit UpdateUI(state);});

    connect(&m_coverPlate->m_coverFloorFsm->getFrontFSM(), &CoverFloorFSMFront::sigStateChanged, this, [&](const QString& state)
            {emit UpdateUI(state);});

    PGVTimer = new QTimer(this);

    CameraTimer = new QTimer(this);
    connect(CameraTimer, &QTimer::timeout, this, [this](){
        currentVals = m_coverPlate->dmcamera->getValues();
        ui->label_angle->setText(QString::number(currentVals[2]/10,'f',2));
        ui->label_x->setText(QString::number(currentVals[0]/10 - 4,'f',2));
        ui->label_y->setText(QString::number(currentVals[1]/10,'f',2));
        // m_coverPlate->dmcamera->resetValues();
        // ui->label_angle->setText(QString::number(currentVals[2]/10));
        // ui->label_x->setText(QString::number(currentVals[0]/10 - 4 + 6));
        // ui->label_y->setText(QString::number(currentVals[1]/10));
    });

    // connect(CameraTimer, &QTimer::timeout, this, [this](){
    //     currentVals = dmcamera->getValues();
    //     ui->label_angle->setText(QString::number(currentVals[2]/10));
    //     ui->label_x->setText(QString::number(currentVals[0]/10 - 4));
    //     ui->label_y->setText(QString::number(currentVals[1]/10));
    // });

    //子线程初始化工控机
    connect(this, &MainWindow::sig_ExecuteEcatInit, zm, [this](){
        int result = zm->EcatInit(0, 0, 0, 0);
        emit zm->sig_EcatInitFinished(result);
    }, Qt::QueuedConnection);

    connect(zm, &ZMotionControl::sig_EcatInitFinished, this, &MainWindow::on_EcatInitFinished,
            Qt::QueuedConnection);

    //总初始化
    //初始化任务队列
    initializeInitTasks();

    //默认打开数据库
    on_btnDbConnect_clicked();
}


MainWindow::~MainWindow() {

    //心跳信号定时器
    HeartTimer->stop();

    //六自由度+视觉
    update_detected_Timer->stop();
    //六自由度加视觉结束

    // 停止并等待线程退出
    m_forceThread->quit();
    m_angleThread->quit();
    m_distanceThread->quit();
    m_tcpThread->quit();
    m_forceThread->wait();
    m_angleThread->wait();
    m_distanceThread->wait();
    m_tcpThread->wait();

    delete m_forceThread;
    delete m_angleThread;
    delete m_distanceThread;
    delete m_tcpThread;

    //删除传感器对象
    delete m_forceSensor;
    delete m_angleSensor;
    delete m_distanceSensor;


    //释放UVC相机资源
    if (UVC_Device) {
        UVC_Device->closeCamera();
    }
    if (cameraPreviewDialog) {
        closeCameraMonitor();
    }
    m_updateTimer->stop();

    for (int i = 0; i < 4; ++i) {
        // 停止子进程
        if (m_workerProcesses[i] && m_workerProcesses[i]->state() == QProcess::Running) {
            m_workerProcesses[i]->terminate();
            m_workerProcesses[i]->waitForFinished(1000);
            delete m_workerProcesses[i];
        }
        // 释放共享内存
        delete m_shmManagers[i];
        // 释放预览窗口
        if (m_previewDialogs[i]) {
            delete m_previewDialogs[i];
        }
    }

    //删除调平类对象
    delete m_levelingSystem;

    // 1. 停止所有定时器（避免触发已销毁的UI对象）
    mvGetTimer->stop();
    //delete mvGetTimer;
    //mvGetTimer = nullptr;


    // 2. 释放资源（按创建的反向顺序）
    // delete optimizer;       // 最上层依赖
    // delete collision_system;
    // delete arm;

    // 3. 释放硬件控制
    if (zm) {
        // 先停止再删除
        zm->stopCallbackThread();
        zm->cleanup();
    }

    // 4. 释放UI相关（Qt会自动处理父子关系）
    //delete myDDR6RobotWidget;  // 假设是new创建的
    //delete mystl;

    // 5. 释放数据模型
    //delete model;
    //delete allAxisStatus;
    //delete pastallAxisStatus;

    // if (m_serialPort->isOpen())
    // {
    //     m_serialPort->close();
    // }
    // delete m_serialPort;
    // 6. 最后删除UI（会自动删除子控件）

    if (SBtimer) {
        SBtimer->stop();
    }
    if (motor && m_motorThread) {
        QMetaObject::invokeMethod(motor, [=]() {
            motor->closeSerialPort();
        }, Qt::BlockingQueuedConnection);
        m_motorThread->quit();
        m_motorThread->wait();
        m_motorThread = nullptr;
        motor = nullptr;
    }

    delete ui;

    // 6. 释放状态数据
    //delete allAxisStatus;
    //delete pastallAxisStatus;

    if (monitor) {
        monitor->deleteLater(); // 安全删除
        monitor = nullptr;
    }
}


void MainWindow::ChartInit()
{
    m_chart = new QChart();
    m_chart->legend()->setVisible(true);
    m_chart->legend()->setAlignment(Qt::AlignBottom);
    m_chart->setTheme(QChart::ChartThemeLight);

    m_seriesFx = new QSplineSeries(); m_seriesFx->setName("FX"); m_seriesFx->setColor(Qt::green);
    m_seriesFy = new QSplineSeries(); m_seriesFy->setName("FY"); m_seriesFy->setColor(Qt::blue);
    m_seriesFz = new QSplineSeries(); m_seriesFz->setName("FZ"); m_seriesFz->setColor(Qt::red);
    m_seriesMx = new QSplineSeries(); m_seriesMx->setName("MX"); m_seriesMx->setColor(Qt::darkYellow);
    m_seriesMy = new QSplineSeries(); m_seriesMy->setName("MY"); m_seriesMy->setColor(Qt::magenta);
    m_seriesMz = new QSplineSeries(); m_seriesMz->setName("MZ"); m_seriesMz->setColor(Qt::cyan);

    m_chart->addSeries(m_seriesFx);
    m_chart->addSeries(m_seriesFy);
    m_chart->addSeries(m_seriesFz);
    m_chart->addSeries(m_seriesMx);
    m_chart->addSeries(m_seriesMy);
    m_chart->addSeries(m_seriesMz);

    m_axisX = new QDateTimeAxis();
    m_axisX->setFormat("HH:mm:ss"); // 只显示时分秒
    m_chart->addAxis(m_axisX, Qt::AlignBottom);

    m_axisY = new QValueAxis();
    m_chart->addAxis(m_axisY, Qt::AlignLeft);

    QList<QSplineSeries*> allSeries = {m_seriesFx, m_seriesFy, m_seriesFz, m_seriesMx, m_seriesMy, m_seriesMz};
    for (auto series : allSeries) {
    series->attachAxis(m_axisX);
    series->attachAxis(m_axisY);
    }

    ui->cvData->setRenderHint(QPainter::Antialiasing);
    ui->cvData->setChart(m_chart);
}


//程序退出或者异常崩溃保存任务调度的当前步骤
void MainWindow::closeEvent(QCloseEvent *event)
{
    m_TaskMgr->saveCurrentStepToIni();
    for(int i = 0;i < 16; i++){zm->SetIOOutput(i,0);}//重置IO状态
    on_btnDisableAxis_clicked();
    event->accept();
}


void MainWindow::OnSafetyLimitTimerOut()
{
    // bool RapidStop_Vertical = false;
    // for(int i = 0;i < 4;i++){
    //     RapidStop_Vertical = RapidStop_Vertical || statusIOInPut_Zmotion[i];
    // }

    // bool RapidStop_Horizontal = statusIOInPut_Zmotion[4] || statusIOInPut_Zmotion[5];

    // bool RapidStop_Lift = statusIOInPut_Zmotion[6] || statusIOInPut_Zmotion[7];

    // if(RapidStop_Vertical){
    //     zm->SingleVMove(5,Cancel);
    //     zm->SingleVMove(6,Cancel);
    // }
    // else if(RapidStop_Horizontal){
    //     zm->SingleVMove(7,Cancel);
    //     zm->SingleVMove(8,Cancel);
    // }
    // else if(RapidStop_Lift){
    //     zm->SingleVMove(9,Cancel);
    //     zm->SingleVMove(10,Cancel);
    // }
}


void MainWindow::OnSafetyLimitTimerOut2()
{
    if (!zm || !zm->GetConnectStatus()) {
        return;
    }
    // IO口定义
    constexpr int IO_FIRST_LIFT_DOWN  = 5;   // 一级下限位
    constexpr int IO_FIRST_LIFT_UP    = 6;   // 一级上限位
    constexpr int IO_SECOND_LIFT_DOWN = 7;   // 二级下限位
    constexpr int IO_SECOND_LIFT_UP   = 8;   // 二级上限位
    // 电机轴号定义
    constexpr int AXIS_FIRST_LIFT  = 4;      // 一级升降电机
    constexpr int AXIS_SECOND_LIFT = 12;     // 二级升降电机（请根据实际修改）
    bool firstDown  = statusIOInPut[IO_FIRST_LIFT_DOWN];
    bool firstUp    = statusIOInPut[IO_FIRST_LIFT_UP];
    bool secondDown = statusIOInPut[IO_SECOND_LIFT_DOWN];
    bool secondUp   = statusIOInPut[IO_SECOND_LIFT_UP];
    // 如果一级上限位触发，并且当前方向是Forward（上升），则停止
    if (firstUp && zm->GetAxisMType(AXIS_FIRST_LIFT) == ForwardVmove) {
        zm->SingleVMove(AXIS_FIRST_LIFT, Cancel);
    }
    // 如果一级下限位触发，并且当前方向是Backward（下降），则停止
    if (firstDown && zm->GetAxisMType(AXIS_FIRST_LIFT) == ReverseVmove) {
        zm->SingleVMove(AXIS_FIRST_LIFT, Cancel);
    }
    // 二级同理
    if (secondUp && zm->GetAxisMType(AXIS_SECOND_LIFT) == ForwardVmove) {
        zm->SingleVMove(AXIS_SECOND_LIFT, Cancel);
    }
    if (secondDown && zm->GetAxisMType(AXIS_SECOND_LIFT) == ReverseVmove) {
        zm->SingleVMove(AXIS_SECOND_LIFT, Cancel);
    }
}


void MainWindow::OnDataVisionTimerOut()
{
    // if(m_TaskStatus.state == eTaskstate::Running){
    //     switch(m_TaskStatus.sys)
    //     {
    //     case eSubSystem::Vehicle :
    //         MyDataBase->generateSensorChart(ui->cvData,"AGV系统",this);

    //     case eSubSystem::Levelling :
    //         MyDataBase->generateSensorChart(ui->cvData,"调平系统",this);

    //     case eSubSystem::MainLift :
    //         MyDataBase->generateSensorChart(ui->cvData,"一级升降系统",this);

    //     case eSubSystem::CoverPlate :
    //         MyDataBase->generateSensorChart(ui->cvData,"取盖板系统",this);

    //     case eSubSystem::Lift :
    //         MyDataBase->generateSensorChart(ui->cvData,"二级升降系统",this);

    //     case eSubSystem::Pin :
    //         MyDataBase->generateSensorChart(ui->cvData,"拔销系统",this);
    //     }

    // }
}


//串口传感器数据显示
void MainWindow::initUIUpdateTimer()
{
    uiUpdateTimer = new QTimer(this);
    connect(uiUpdateTimer, &QTimer::timeout, this, &MainWindow::updateSensorUI);
    uiUpdateTimer->start(150);
}

// 接收数据时缓存
void MainWindow::onForceDataReceived(const QVariantMap &data)
{
    for (auto it = data.begin(); it != data.end(); ++it) {
        sensorDataCache[it.key()] = it.value().toFloat();

        //缓存数据，写入数据库
        QString key = it.key();
        QString sensorType = QString("称重传感器-%1").arg(key.remove("sensor"));
        MyDataBase->logSensorData(sensorType, it.value());
    }
}

void MainWindow::onAngleDataReceived(const QVariantMap &data)
{
    for (auto it = data.begin(); it != data.end(); ++it) {
        sensorDataCache[it.key()] = it.value().toFloat();

        QString key = it.key();
        QString sensorType = QString("倾角传感器-%1").arg(key.remove("Angle"));
        MyDataBase->logSensorData(sensorType, it.value());
    }
}

void MainWindow::onDistanceDataReceived(const QVariantMap &data)
{
    for (auto it = data.begin(); it != data.end(); ++it) {
        sensorDataCache[it.key()] = it.value().toFloat();

        QString key = it.key();
        QString sensorType = QString("激光测距传感器-%1").arg(key.remove("Distance"));
        MyDataBase->logSensorData(sensorType, it.value());
    }
}

void MainWindow::onBatteryDataReceived(const QVariantMap &data)
{
    for (auto it = data.begin(); it != data.end(); ++it) {
        sensorDataCache[it.key()] = it.value().toFloat();
    }
}


//定时刷新 UI
void MainWindow::updateSensorUI()
{
    for (auto it = sensorDataCache.begin(); it != sensorDataCache.end(); ++it) {
            QString key = it.key();
            float value = it.value();

            QLineEdit* lineEdit = findChild<QLineEdit*>(key);
            if (lineEdit) {
                if (key == "remainingmAh") {
                    lineEdit->setText(QString::number(value * 100.0f, 'f', 0) + "%");
                } else {
                    lineEdit->setText(QString::number(value, 'f', 2));
                }
            }

            if(key == "AngleX"){
                m_levelingSystem->angleXforleveling = value;
            }
            else if (key == "AngleY") {
                m_levelingSystem->angleYforleveling = value;
            }
        }

    if(IsClearForce)
    {ui->sensor0->clear();ui->sensor1->clear();ui->sensor2->clear();ui->sensor3->clear();IsClearForce = false;}

    if(IsClearAngle)
    {ui->AngleX->clear();ui->AngleY->clear();IsClearAngle = false;}

    if(IsClearDistance)
    {ui->Distance0->clear();ui->Distance1->clear();ui->Distance2->clear();ui->Distance3->clear();IsClearDistance = false;}

    sensorDataCache.clear(); // 清空缓存
}

//标志位更新
//调平机构
void MainWindow::onLevelSymbolChanged(LevelSymbol symbol)
{
    QString baseStyle = "border-radius: 7px; border: 2px solid black;background-color: white;";

    switch(symbol) {
    case LevelSymbol::FeetToGround0:
        ui->lbl_Symbol0->setStyleSheet(baseStyle.replace("white","green"));
        break;
    case LevelSymbol::FeetToGround1:
        ui->lbl_Symbol1->setStyleSheet(baseStyle.replace("white","green"));
        break;
    case LevelSymbol::FeetToGround2:
        ui->lbl_Symbol2->setStyleSheet(baseStyle.replace("white","green"));
        break;
    case LevelSymbol::FeetToGround3:
        ui->lbl_Symbol3->setStyleSheet(baseStyle.replace("white","green"));
        break;
    case LevelSymbol::Balance:
        ui->lbl_SymbolAll->setStyleSheet(baseStyle.replace("white","green"));
        break;
    case LevelSymbol::Restart:
        ui->lbl_Symbol0->setStyleSheet(baseStyle);
        ui->lbl_Symbol1->setStyleSheet(baseStyle);
        ui->lbl_Symbol2->setStyleSheet(baseStyle);
        ui->lbl_Symbol3->setStyleSheet(baseStyle);
        ui->lbl_SymbolAll->setStyleSheet(baseStyle);
        break;
    }
}

//取盖板机构，新修
void MainWindow::onCoverPlateSymbolChanged(CoverPlateSymbol symbol) {
    QString baseStyle = "border-radius: 7px; border: 2px solid black;background-color: white;";
    resetCoverPlateSymbol();
    switch (symbol) {
    case CoverPlateSymbol::TakeDoorComplete:
        ui->lbl_TakeDoor->setStyleSheet(baseStyle.replace("white","green"));

        m_openDoorWaitingConfirm = false;
        m_closeDoorWaitingConfirm = false;

        ui->btn_OpenDoor->setText("打开封门");
        ui->btn_CloseDoor->setText("关闭封门");

        ui->btn_OpenDoor->setToolTip("");
        ui->btn_CloseDoor->setToolTip("");

        ui->btn_OpenDoor->setEnabled(true);
        ui->btn_CloseDoor->setEnabled(true);
        break;
    case CoverPlateSymbol::TakePlateComplete:
        ui->lbl_TakePlate->setStyleSheet(baseStyle.replace("white","green"));
        break;
    case CoverPlateSymbol::CoverPlateComplete:
        ui->lbl_FixPlate->setStyleSheet(baseStyle.replace("white","green"));
        break;
    case CoverPlateSymbol::CoverDoorComplete:
        ui->lbl_FixDoor->setStyleSheet(baseStyle.replace("white","green"));

        m_openDoorWaitingConfirm = false;
        m_closeDoorWaitingConfirm = false;

        ui->btn_OpenDoor->setText("打开封门");
        ui->btn_CloseDoor->setText("关闭封门");

        ui->btn_OpenDoor->setToolTip("");
        ui->btn_CloseDoor->setToolTip("");

        ui->btn_OpenDoor->setEnabled(true);
        ui->btn_CloseDoor->setEnabled(true);
        break;
    }
}

void MainWindow::resetCoverPlateSymbol() {
    QString baseStyle = "border-radius: 7px; border: 2px solid black; background-color: white;";
    ui->lbl_TakeDoor->setStyleSheet(baseStyle);
    ui->lbl_TakePlate->setStyleSheet(baseStyle);
    ui->lbl_FixPlate->setStyleSheet(baseStyle);
    ui->lbl_FixDoor->setStyleSheet(baseStyle);
}

//各子系统状态监测窗口****************************************************************
void MainWindow::on_StartMonitor_triggered()
{
    if (!monitor) {
        monitor = new lrumonitor(this);
    }

    //视觉数据在更新Ui时候，同步更新
    //获取二级升降电机轴12坐标
    monitor->m_AxisPos = allAxisStatus[12].posi;
    monitor->resize(800, 900);
    monitor->show();
}

void MainWindow::on_StartAGVMonitor_triggered()
{
    if (!agvmonitor) {
        agvmonitor = new AGVMonitor(this);
    }

    agvmonitor->show();

    // 把服务器的连接和传感器的通讯放入总初始化按钮
    // on_btn_CnntPushSvr_clicked();
    // ui->comboBoxPortNameAngle->setCurrentText("COM21");
    // ui->btnOpenDistanceCOM->setText("打开距离串口");
    // on_btnOpenDistanceCOM_clicked();
    // ui->btn_ShowDistance->setText("显示距离信息");
    // on_btn_ShowDistance_clicked();
}

void MainWindow::on_StartLevelMonitor_triggered()
{
    if (!levelmonitor) {
        levelmonitor = new LevelMonitor(zm,this);
    }

    levelmonitor->show();

    // 移入总初始化按钮
    // ui->comboBoxPortNameForce->setCurrentText("COM20");
    // ui->btnOpenForceCOM->setText("打开压力串口");
    // on_btnOpenForceCOM_clicked();
    // ui->btn_ShowForce->setText("显示压力信息");
    // on_btn_ShowForce_clicked();

    // ui->comboBoxPortNameAngle->setCurrentText("COM23");
    // ui->btnOpenAngleCOM->setText("打开角度串口");
    // on_btnOpenAngleCOM_clicked();
    // ui->btn_ShowAngle->setText("显示角度信息");
    // on_btn_ShowAngle_clicked();

    // ui->btn_ShowCurrent->setText("显示电流信息");
    // on_btn_ShowCurrent_clicked();
}

void MainWindow::on_StartLiftMonitor_triggered()
{
    if (!liftmonitor) {
        liftmonitor = new LiftMonitor(zm,this);
    }

    liftmonitor->show();

    //移入总初始化按钮
    // ui->btn_ShowForceInfo->setText("显示力信息");
    // on_btn_ShowForceInfo_clicked();
    // ui->btn_CheckCollision->setText("检测碰撞");
    // on_btn_CheckCollision_clicked();
}

//视觉画面
void MainWindow::on_StartCamera_trigger()
{
    if (!dahengForm) {
        dahengForm = new dahengTwoCams_qt_vs(this);
    }
    dahengForm->show();
}

void MainWindow::on_StartCoverPlateMonitor_triggered()
{
    if (!coverplatemonitor) {
        coverplatemonitor = new CoverPlateMonitor(zm,this);
    }
    coverplatemonitor->show();
}

void MainWindow::on_StartPinMonitor_triggered()
{
    if (!pinmonitor) {
        pinmonitor = new PinMonitor(this);
    }
    pinmonitor->show();
}

void MainWindow::on_StartCoverPlate3D_triggered()
{
    if (!CoverPlate3D) {
        CoverPlate3D = new NewForm(this);
    }
    CoverPlate3D->show();
}

//****************************************************************************


//AGV小车通讯*******************************************************************
//保留使用
void MainWindow::slotPrintInfo(QString info)
{

}

//主窗口系统监测
//显示状态信息
void MainWindow::slotPrintStatus(eTaskStatus status)
{
    if(zm->GetConnectStatus())
    {
        //更新系统用于数据可视化
        m_TaskStatus = status;

        // 定义基础样式
        QString baseStyle = "border-radius: 7px; border: 2px solid black; background-color: white;";

        // 重置错误报警灯
        zm->SetIOOutput(4, 0); // 红灯

        //重置所有任务调度标志位
        QGroupBox *groupBox = this->findChild<QGroupBox*>("groupBox_TaskMgr");
        QList<QWidget*> lblWidgets = groupBox->findChildren<QWidget*>();
        QList<QWidget*> targetWidgets;
        foreach (QWidget *widget, lblWidgets) {
            if (widget->objectName().startsWith("lbl_")) {
                targetWidgets.append(widget);
            }
        }
        foreach (QWidget *widget, targetWidgets) {
            widget->setStyleSheet(baseStyle);
        }

        // // 子系统发生切换时，关闭所有已打开的监测窗口
        // if (status.sys != m_lastSubSystem)
        // {
        //     if (monitor) monitor->close();
        //     if (agvmonitor) agvmonitor->close();
        //     if (levelmonitor) levelmonitor->close();
        //     if (liftmonitor) liftmonitor->close();
        //     if (coverplatemonitor) coverplatemonitor->close();
        //     if (pinmonitor) pinmonitor->close();
        //     // if (dahengForm) dahengForm->close();

        //     // 更新上一次子系统记录
        //     m_lastSubSystem = status.sys;
        // }

        // 根据当前子系统进行界面显示
        switch (status.sys) {
        case eSubSystem::Vehicle:
            HandleTaskTypeAndSystem(ui->lbl_AGV,status.taskinfo.TaskType,baseStyle,ui->lbl_VehTaskForward, ui->lbl_VehTaskBackward);
            HandleSubsystemState(eSubSystem::Vehicle, status.state, baseStyle, ui->lbl_AGV_Ready,ui->lbl_AGV_Running, ui->lbl_AGV_Complete,ui->lbl_AGV_Error);
            // on_StartAGVMonitor_triggered();// 触发AGV监控窗口
            break;

        case eSubSystem::Levelling:
            HandleTaskTypeAndSystem(ui->lbl_Level,status.taskinfo.TaskType,baseStyle,ui->lbl_LevelTaskForward, ui->lbl_LevelTaskBackward);
            HandleSubsystemState(eSubSystem::Levelling, status.state, baseStyle, ui->lbl_Level_Ready,ui->lbl_Level_Running, ui->lbl_Level_Complete,ui->lbl_Level_Error);
            // on_StartLevelMonitor_triggered();// 触发调平监控窗口
            break;

        case eSubSystem::MainLift:
            HandleTaskTypeAndSystem(ui->lbl_MainLift,status.taskinfo.TaskType,baseStyle,ui->lbl_MainLiftTaskForward, ui->lbl_MainLiftTaskBackward);
            HandleSubsystemState(eSubSystem::MainLift, status.state, baseStyle, ui->lbl_MainLift_Ready,ui->lbl_MainLift_Running, ui->lbl_MainLift_Complete,ui->lbl_MainLift_Error);
            // on_StartLiftMonitor_triggered(); // 触发主升降监控窗口
            break;

        case eSubSystem::CoverPlate:
            HandleTaskTypeAndSystem(ui->lbl_CoverPlate,status.taskinfo.TaskType,baseStyle,ui->lbl_CoverPlateTaskForward, ui->lbl_CoverPlateTaskBackward);
            HandleSubsystemState(eSubSystem::CoverPlate, status.state, baseStyle, ui->lbl_CoverPlate_Ready,ui->lbl_CoverPlate_Running, ui->lbl_CoverPlate_Complete,ui->lbl_CoverPlate_Error);
            // on_StartCoverPlateMonitor_triggered();// 触发取装盖板监控窗口
            break;

        case eSubSystem::Lift:
            //进入二级升降系统，如果是自动模式就切换为手动模式,并自动跳转界面
            if(isAuto){
                ui->radioButton_ManualMode->setChecked(true);
                switchToSecondLift();
            }
            HandleTaskTypeAndSystem(ui->lbl_Lift,status.taskinfo.TaskType,baseStyle,ui->lbl_LiftTaskForward, ui->lbl_LiftTaskBackward);
            HandleSubsystemState(eSubSystem::Lift, status.state, baseStyle, ui->lbl_Lift_Ready,ui->lbl_Lift_Running, ui->lbl_Lift_Complete,ui->lbl_Lift_Error);
            // on_StartLiftMonitor_triggered();// 触发升降监控+视觉+总监控窗口
            // on_StartCamera_trigger();
            // on_StartMonitor_triggered();//关闭数字孪生的逻辑再斟酌一下
            break;
        }

        // 更新系统错误信息
        emit UpdateUI(status.info);
    }
}


void MainWindow::HandleSubsystemState(eSubSystem system, eTaskstate state, QString& baseStyle, QLabel* readyLbl, QLabel* runningLbl, QLabel* completeLbl,QLabel* errorLbl)
{
    if (state == eTaskstate::Error)
    {
        // 错误状态处理
        errorLbl->setStyleSheet(baseStyle.replace("white", "red"));
        zm->SetIOOutput(4, 1); // 点亮红灯
    }
    else
    {
        switch (state)
        {
        case eTaskstate::Ready:
            readyLbl->setStyleSheet(baseStyle.replace("white", "green"));
            break;

        case eTaskstate::Running:
            runningLbl->setStyleSheet(baseStyle.replace("white", "green"));
            break;

        case eTaskstate::Complete:
            completeLbl->setStyleSheet(baseStyle.replace("white", "green"));
            // if(isAuto){
            //     switch (system)
            //     {
            //     case eSubSystem::Vehicle:
            //         ui->radioButton_ManualMode->setChecked(true);
            //         switchToAGV();
            //         QMessageBox::information(this, "提示", "AGV自动任务完成，手动调节AGV位置后请点击恢复按钮！");
            //         break;

            //     case eSubSystem::Levelling:
            //         ui->radioButton_ManualMode->setChecked(true);
            //         switchToLevelling();
            //         QMessageBox::information(this, "提示", "调平自动任务完成，手动调节脚座高度后请点击恢复按钮！");
            //         break;
            //     }
            // }

            break;
        }
    }
}


void MainWindow::HandleTaskTypeAndSystem(QLabel*SystemLbl,int taskType, QString& baseStyle,QLabel* forwardLbl,QLabel* backwardLbl)
{
    if(isAuto){//自动模式点亮绿色系统标志
        SystemLbl->setStyleSheet(baseStyle.replace("white", "green"));
    }
    else{//手动模式点亮黄色系统标志
        SystemLbl->setStyleSheet(baseStyle.replace("white", "yellow"));
    }

    switch (taskType) {
    case 1:
        forwardLbl->setStyleSheet(baseStyle.replace("white", "green"));
        break;
    case 2:
        backwardLbl->setStyleSheet(baseStyle.replace("white", "green"));
        break;
    }
}


//用于在窗体控件上显示小车的实时状态
void MainWindow::slotDisplayVehStatus(VehicleSts veh_sts, int mytype)
{
    QString Style = "border-radius: 7px; border: 2px solid black; background-color: white;";
    QStringList currentErrors;
    switch(mytype){
    case 0:
        ui->lineEdit_batt_level->setText(
            QString::number(veh_sts.battery_level * 100.0f, 'f', 0) + "%");
        ui->lineEdit_agv_blocked->setText(QString::number(veh_sts.blocked));
        ui->lineEdit_block_reason->setText(BlockReason2Qstr(veh_sts.block_reason));
        ui->lineEdit_taskStatus->setText(TaskStatus2Qstr(veh_sts.task_status));
        ui->lineEdit_emergency->setText(QString::number(veh_sts.emergency));
        ui->lineEdit_soft_emc->setText(QString::number(veh_sts.soft_emc));
        ui->lineEdit_current_station->setText(veh_sts.current_station);
        ui->lineEdit_brake->setText(QString::number(veh_sts.brake));
        ui->lineEdit_task_type->setText(QString::number(veh_sts.task_type));
        ui->lineEdit_target_dist->setText(QString::number(veh_sts.target_dist));
        ui->lineEdit_driver_emc->setText(QString::number(veh_sts.driver_emc));
        ui->lineEdit_electric->setText(QString::number(veh_sts.electric));
        ui->lineEdit_loadmap_status->setText(LoadmapStatus2Qstr(veh_sts.loadmap_status));
        ui->lineEdit_loadmap_status2->setText(LoadmapStatus2Qstr(veh_sts.loadmap_status));
        ui->lineEdit_reloc_status->setText(RelocStatus2Qstr(veh_sts.reloc_status));

        // for (auto var:veh_sts.finished_path){
        //     emit UpdateUI("Finished path:"+var);
        // }
        // for (auto var:veh_sts.unfinished_path){
        //     emit UpdateUI("Finished path:"+var);
        // }

        for (auto error :veh_sts.errors){
            QString errorStr = QString::number(error.code)+":"+error.desc;
            currentErrors.append(errorStr);

            if (!lastErrors.contains(errorStr)){
                // 出现新错误，打印并设置字体为红色，重复错误不打印
                QString logMessage = "<font color='red'>" + errorStr + "</font>";
                emit UpdateUI(logMessage);
            }
        }

        // 更新上次错误信息
        lastErrors = currentErrors;

        //test
        LogMgr::getInstance()->m_logger->info("Parse Json Data");

        for(auto var:veh_sts.pgvs){
            LogMgr::getInstance()->m_logger->info("pgv add[{}],dmt_detected[{}],bsolute_X_pos_valid[{}]", var.device_address,var.id_DMT_detected,var.is_absolute_X_pos_valid);
        }

        //emit UpdateUI()
        LogMgr::getInstance()->m_logger->info("taskid[{}],tasklistname[{}],tasklist_status[{}]",veh_sts.tasklist_status.taskid,veh_sts.tasklist_status.taskListName.toStdString(),TaskStatus2Qstr(veh_sts.tasklist_status.task_status).toStdString());
        break;

    case 1:
        if(veh_sts.locked){
            ui->lbl_handle->setStyleSheet(Style.replace("white","green"));
        }
        else{
            ui->lbl_handle->setStyleSheet(Style);
        }
        break;
    }
}


//小车TCP连接状态监控
void MainWindow::slotVehTcpStateChanged(TcpSvrType svr,QAbstractSocket::SocketState state)//TcpSvrType，TCP服务器:推送服务器、导航服务器、配置服务器
{
    switch (svr) {
    case nVehicleDataType::TcpSvrType::NavigateSvr:
        if(state == QAbstractSocket::ConnectedState || state ==QAbstractSocket::ConnectingState){
            ui->btn_CnntNavigateSvr->setText(tr("断开连接"));
            emit UpdateUI("连接到导航服务器！");
            isCnntNavigateSvr = true;
        }
        else{
            ui->btn_CnntNavigateSvr->setText(tr("连接"));
            emit UpdateUI("断开连接导航服务器！");
            isCnntNavigateSvr = false;
        }
        break;
    case nVehicleDataType::TcpSvrType::PushSvr:
        if(state == QAbstractSocket::ConnectedState || state ==QAbstractSocket::ConnectingState){
            ui->btn_CnntPushSvr->setText(tr("断开连接"));
            emit UpdateUI("连接到推送服务器！");
            isCnntPushSvr = true;
        }
        else{
            ui->btn_CnntPushSvr->setText(tr("连接"));
            emit UpdateUI("断开连接推送服务器！");
            isCnntPushSvr = false;
        }

        break;
    case nVehicleDataType::TcpSvrType::StatusSvr:
        if(state == QAbstractSocket::ConnectedState || state ==QAbstractSocket::ConnectingState){
            ui->btn_CnntStatusSvr->setText(tr("断开连接"));
            emit UpdateUI("连接到状态服务器！");
            isCnntStatusSvr = true;
        }
        else{
            ui->btn_CnntStatusSvr->setText(tr("连接"));
            emit UpdateUI("断开连接状态服务器！");
            isCnntStatusSvr = false;
        }

        break;
    case nVehicleDataType::TcpSvrType::ControlSvr:
        if(state == QAbstractSocket::ConnectedState || state ==QAbstractSocket::ConnectingState){
            ui->btn_CnntControlSvr->setText(tr("断开连接"));
            emit UpdateUI("连接到控制服务器！");
            isCnntControlSvr = true;
        }
        else{
            ui->btn_CnntControlSvr->setText(tr("连接"));
            emit UpdateUI("断开连接控制服务器！");
            isCnntControlSvr = false;
        }
        break;
    default:
        break;
    }
}

void MainWindow::on_btn_CnntPushSvr_clicked()
{
    if(ui->btn_CnntPushSvr->text()=="连接"){
        m_vehicle->connectPushHost(ui->lineEdit_PushSvr->text(),ui->comBox_PushPort->currentText().toInt());
    }
    else{
        m_vehicle->ClosePushHostConnection();
    }
}

void MainWindow::on_btn_CnntNavigateSvr_clicked()
{
    if(ui->btn_CnntNavigateSvr->text() == "连接"){
        m_vehicle->connectNavigateHost(ui->lineEdit_NaviSvr->text(),ui->comBox_NavigatePort->currentText().toInt());
    }
    else{
        m_vehicle->CloseNavigateConnection();
    }
}


void MainWindow::on_btn_CnntStatusSvr_clicked()
{
    if(ui->btn_CnntStatusSvr->text() == "连接"){
        m_vehicle->connectStatusHost(ui->lineEdit_StatusSvr->text(),ui->comBox_StatusPort->currentText().toInt());

        //定时刷新PGV二维码信息
        connect(PGVTimer,&QTimer::timeout,m_vehicle,&SCVehicle::queryPGVData);
        PGVTimer->start(300);
    }
    else{
        PGVTimer->stop();
        m_vehicle->CloseStatusConnection();
    }
}

void MainWindow::on_btn_CnntControlSvr_clicked()
{
    if(ui->btn_CnntControlSvr->text() == "连接"){
        m_vehicle->connectControlHost(ui->lineEdit_ContSvr->text(),ui->comBox_ControlPort->currentText().toInt());
    }
    else{
        m_vehicle->CloseControlConnection();
    }
}


void MainWindow::on_btn_Send_clicked()
{

    if(m_vehicle->isConnectedNavigateHost()){
        //报头数据类型.
        uint16_t sendCommand = ui->spinBox_Type->value();

        //Json 区数据
        QByteArray jsonData = ui->textEdit_Data->toPlainText().toLocal8Bit();

        //序号.
        uint16_t number = ui->spinBox_Num->value();
        uint8_t byte15 = ui->spinBox_Byte15->value();

        if(!m_vehicle->SendData2NavigateHost(sendCommand,jsonData,"",number,byte15)){
            LogMgr::getInstance()->m_logger->error("导航命令发送失败！");
        }
        else{
            LogMgr::getInstance()->m_logger->info("导航命令发送成功！");
        }

    }

    else if(m_vehicle->isConnectedStatusHost())
    {
        //报头数据类型.
        uint16_t sendCommand = ui->spinBox_Type->value();

        //Json 区数据
        QByteArray jsonData = ui->textEdit_Data->toPlainText().toLocal8Bit();

        //序号.
        uint16_t number = ui->spinBox_Num->value();
        uint8_t byte15 = ui->spinBox_Byte15->value();

        if(!m_vehicle->SendData2StatusHost(sendCommand,jsonData,"",number,byte15)){
            LogMgr::getInstance()->m_logger->error("状态命令发送失败！");
            emit UpdateUI("状态命令发送失败！");
        }
        else{
            LogMgr::getInstance()->m_logger->info("配置命令发送成功！");
            emit UpdateUI("配置命令发送成功！");
        }

    }

    else if(m_vehicle->isConnectedControlHost()){
        //报头数据类型.
        uint16_t sendCommand = ui->spinBox_Type->value();

        //Json 区数据
        QByteArray jsonData = ui->textEdit_Data->toPlainText().toLocal8Bit();

        //序号.
        uint16_t number = ui->spinBox_Num->value();
        uint8_t byte15 = ui->spinBox_Byte15->value();

        if(!m_vehicle->SendData2ControlHost(sendCommand,jsonData,"",number,byte15)){
            LogMgr::getInstance()->m_logger->error("控制命令发送失败！");
            emit UpdateUI("控制命令发送失败！");
        }
        else{
            LogMgr::getInstance()->m_logger->info("控制命令发送成功！");
            emit UpdateUI("控制命令发送成功！");
        }
    }
    else{
        emit UpdateUI("未连接到服务器！");
    }
}


void MainWindow::on_btn_CancleTask_clicked()
{
    if(m_vehicle->isConnectedNavigateHost()){
        uint16_t number = 0;
        if(!m_vehicle->SendData2NavigateHost(3003u,"","",number,0u)){
            LogMgr::getInstance()->m_logger->error("Failed to cancel task");
        }
    }
    else{
        emit UpdateUI("Not connected to Navigate server yet!");
    }
}

//上传地图
void MainWindow::on_btn_MapLoad_clicked()
{
    QString mapFilePath = ui->comboBox_Map->currentText();
    m_vehicle->uploadMapToRobot(mapFilePath);
}

//AGV重定位
void MainWindow::on_btn_AGVReLocation_clicked()
{
    m_vehicle->relocateRobot(false,0,0,0,0.5,false);
}


//用于AGV自动导航后的精准定位
//前进/后退,正代表前进，负代表后退
void MainWindow::on_btn_AGV_Translate_clicked()
{
    double distance = ui->doubleSpinBox_AGV_Translate->text().toDouble();
    if(distance > 0){
        m_vehicle->translateRobot(distance, 0.15, 0, 1); // vx=0.5,y方向无速度,采用定位模式
    }
    else{
        m_vehicle->translateRobot(abs(distance), -0.15, 0, 1);
    }

}


//左右横移，正代表向左横移，负代表向右横移
void MainWindow::on_btn_AGV_Transverse_clicked()
{
    double distance = ui->doubleSpinBox_AGV_Transverse->text().toDouble();
    if(distance > 0){
        m_vehicle->translateRobot(distance, 0, 0.15, 1); // vy=0.5,x方向无速度,采用定位模式
    }
    else{
        m_vehicle->translateRobot(abs(distance), 0, -0.15, 1);
    }
}


//旋转,正代表逆时针旋转，负代表顺时针旋转
void MainWindow::on_btn_AGV_Turn_clicked()
{
    double angle_temp = ui->doubleSpinBox_AGV_Turn->text().toDouble();
    double AGV_angle = angle_temp / 180 * 3.1415926;//角度制转换为弧度制
    if(AGV_angle > 0){
        m_vehicle->rotateRobot(AGV_angle, 0.35 , 1);  // vw=0.35 rad/s,近似为20 °/s
    }
    else{
        m_vehicle->rotateRobot(abs(AGV_angle), -0.35, 1);
    }
}


//AGV位置细调节
void MainWindow::on_btn_AGVPositionAdjust_clicked()
{
    //调节角度
    double AGV_angle = tag_diff_angle / 180 * 3.14;
    if(AGV_angle > 0){
        m_vehicle->rotateRobot(AGV_angle, 0.35, 1);
    }
    else{
        m_vehicle->rotateRobot(abs(AGV_angle), -0.35, 1);
    }

    //调节X坐标
    if(tag_diff_x > 0){
        m_vehicle->translateRobot(tag_diff_x, 0.2, 0, 1);
    }
    else{
        m_vehicle->translateRobot(abs(tag_diff_x), -0.2, 0, 1);
    }

    //调节Y坐标
    if(tag_diff_y > 0){
        m_vehicle->translateRobot(tag_diff_y, 0, 0.2, 1);
    }
    else{
        m_vehicle->translateRobot(abs(tag_diff_y), 0, -0.2, 1);
    }
}

//AGV的开机和关机
//IO设置1代表启动，设置0代表关闭
void MainWindow::on_btn_AGVOpen_clicked()
{
    //启动AGV
    if(statusIOOutPut[9] == 0){
        zm->SetIOOutput(9,1);
        emit UpdateUI("AGV小车开机！");
        isAGVOpen = true;
    }
    else{
        emit UpdateUI("AGV小车已经开机！");
        isAGVOpen = true;
    }
}

void MainWindow::on_btn_AGVClose_clicked()
{
    for(int i = 0;i < 16;i++){zm->SetIOOutput(i,0);}
    emit UpdateUI("AGV小车关机！");
    isAGVOpen = false;
}

//AGV的断电急停和复位
void MainWindow::on_btn_AGVRapidStop_clicked()
{
    if(statusIOOutPut[8] == 0){
        //AGV急停
        zm->SetIOOutput(8,1);
        emit UpdateUI("AGV小车急停");
        ui->btn_AGVRapidStop->setStyleSheet("background-color: transparent;border-image: url(:/Reset.svg) 0 stretch;border: 2px solid black;");
    }
    else if(statusIOOutPut[8] == 1){
        //AGV取消急停
        zm->SetIOOutput(8,0);
        emit UpdateUI("AGV小车取消急停");
        ui->btn_AGVRapidStop->setStyleSheet("background-color: transparent;border-image: url(:/RapidStop.svg) 0 stretch;border: 2px solid black;");
    }
}

void MainWindow::waitAGV()
{
    QTimer::singleShot(65000,this,[this](){
        WaitAGV = true;
    });
}

//总控制系统的初始化按钮
void MainWindow::initializeInitTasks()
{
    m_initTasks.clear();

    // 控制器初始化
    m_initTasks.append({
        [this]() { on_btnConnect_clicked(); },
        [this]() { return isConnectController; },
        "连接控制器",
        false
    });

    m_initTasks.append({
        [this]() { on_btnInitBus_clicked(); },
        [this]() { return isInitBus; },
        "初始化总线",
        false
    });

    m_initTasks.append({
        [this]() { on_btnInitAxis_clicked(); },
        []() { return true; },
        "初始化轴参数",
        false
    });

    m_initTasks.append({
        [this]() { on_btnEnableAxis_clicked(); },
        [this]() { return isEnableAllAxis; },
        "使能轴",
        false
    });

    m_initTasks.append({
        []() {},
        []() { return true; },
        "工控机初始化完成",
        false
    });

    //打开数据库
    m_initTasks.append({
        [this]() {on_btnDbConnect_clicked();},
        []() { return true; },
        "开启数据库",
        false
    });

    // AGV初始化
    m_initTasks.append({
        [this]() { on_btn_AGVOpen_clicked(); },
        [this]() { return isAGVOpen; },
        "打开AGV",
        false
    });

    m_initTasks.append({
        [this]() {waitAGV();},
        [this]() { return WaitAGV; },
        "AGV上电初始化",
        false
    });

    m_initTasks.append({
        [this]() { on_btn_Battery2_clicked(); },
        []() { return true; },
        "显示电池2信息",
        false
    });

    m_initTasks.append({
        [this]() { on_btn_CnntStatusSvr_clicked(); },
        [this]() { return isCnntStatusSvr; },
        "连接状态服务器",
        false
    });

    m_initTasks.append({
        [this]() { on_btn_CnntNavigateSvr_clicked(); },
        [this]() { return isCnntNavigateSvr; },
        "连接导航服务器",
        false
    });

    m_initTasks.append({
        [this]() { on_btn_CnntPushSvr_clicked(); },
        [this]() { return isCnntPushSvr; },
        "连接推送服务器",
        false
    });

    m_initTasks.append({
        [this]() { on_btn_CnntControlSvr_clicked(); },
        [this]() { return isCnntControlSvr; },
        "连接控制服务器",
        false
    });

    m_initTasks.append({
        [this]() { on_btn_MapLoad_clicked(); },
        []() { return true; },
        "加载地图",
        false
    });

    m_initTasks.append({
        [this]() {
            ui->comboBoxPortNameDistance->setCurrentText("COM21");
            on_btnOpenDistanceCOM_clicked();
        },
        [this]() { return m_distanceSensor->isOpen(); },
        "初始化距离传感器",
        false
    });

    m_initTasks.append({
        [this]() { on_btn_ShowDistance_clicked(); },
        []() { return true; },
        "显示距离信息",
        false
    });

    m_initTasks.append({
        []() {},
        []() { return true; },
        "AGV系统初始化",
        false
    });

    // 调平系统初始化
    m_initTasks.append({
        [this]() {
            ui->comboBoxPortNameForce->setCurrentText("COM20");
            on_btnOpenForceCOM_clicked();
        },
        [this]() { return m_forceSensor->isOpen(); },
        "初始化称重传感器",
        false
    });

    m_initTasks.append({
        [this]() { on_btn_ShowForce_clicked(); },
        []() { return true; },
        "显示压力信息",
        false
    });

    m_initTasks.append({
        [this]() {
            ui->comboBoxPortNameAngle->setCurrentText("COM26");
            on_btnOpenAngleCOM_clicked();
        },
        [this]() { return m_angleSensor->isOpen(); },
        "初始化倾角传感器",
        false
    });

    m_initTasks.append({
        [this]() { on_btn_ShowAngle_clicked(); },
        []() { return true; },
        "显示角度信息",
        false
    });

    m_initTasks.append({
        [this]() { on_btn_ShowCurrent_clicked(); },
        [this]() { return isShowingCurrent; },
        "显示电流信息",
        false
    });

    m_initTasks.append({
        []() {},
        []() { return true; },
        "调平系统初始化",
        false
    });

    // // 一级升降初始化
    // m_initTasks.append({
    //     [this]() {m_mainLift->laser->openSerialPort(); },
    //     [this]() { return m_mainLift->laser->isConnected; },
    //     "一级升降初始化",
    //     false
    // });

    m_initTasks.append({
        []() {},
        []() { return true; },
        "一级升降系统初始化",
        false
    });

    // 取盖板系统初始化
    m_initTasks.append({
        [this]() { m_coverPlate->dmcamera->connectToDevice(); },
        [this]() { return m_coverPlate->dmcamera->isConnected(); },
        "二维码读码器初始化",
        false
    });

    m_initTasks.append({
        []() {},
        []() { return true; },
        "取盖板系统初始化",
        false
    });

    // 二级升降初始化
    m_initTasks.append({
        [this]() { m_lift->m_lightcontroller->openSerial(); },
        [this]() { return m_lift->m_lightcontroller->isSerialOpen(); },
        "光源控制器初始化",
        false
    });

    m_initTasks.append({
        [this]() { m_lift->m_SixDof->servoPowerOn(); },
        [=]() { return true; },
        "使能六自由度平台",
        false
    });

    m_initTasks.append({
        [this]() {on_btn_ShowForceInfo_clicked(); },
        [this]() { return IsSixAxisSensorIni; },
        "六维力传感器初始化",
        false
    });

    m_initTasks.append({
        [this]() {on_btn_GoZero_clicked(); },
        []() { return true; },
        "六维力传感器调零",
        false
    });

    m_initTasks.append({
        []() {},
        []() { return true; },
        "二级升降系统初始化",
        false
    });
}


void MainWindow::on_btn_InitialAll_clicked()
{
    startInitialization();
}

void MainWindow::startInitialization()
{
    if (m_initializing) {
        emit UpdateUI("正在初始化中，请稍候...");
        return;
    }

    // 重置初始化标志
    isControllerOK = false;
    isAGVOK = false;
    isLevelingOK = false;
    isMainLiftOK = false;
    isCoverPlateOK = false;
    isLiftOK = false;
    IsSixAxisSensorIni = false;
    isConnectController = false;
    isInitBus = false;
    isEnableAllAxis = false;
    isAGVOpen = false;
    isCnntPushSvr = false;
    isCnntNavigateSvr = false;
    isCnntStatusSvr = false;
    isCnntControlSvr = false;
    isShowingCurrent = false;

    // 重置任务执行状态
    for (auto &task : m_initTasks) {
        task.executed = false;
    }

    m_currentInitTaskIndex = 0;

    m_initializing = true;

    // 开始处理第一个任务
    processNextInitTask();
}

void MainWindow::processNextInitTask()
{
    if (m_currentInitTaskIndex >= m_initTasks.size()) {
        m_initializing = false;
        emit UpdateUI("所有系统初始化完成");
        return;
    }

    auto &task = m_initTasks[m_currentInitTaskIndex];

    // 避免重复执行任务
    if (!task.executed) {
        task.executeFunc();
        task.executed = true;
    }

    // 检查任务是否完成
    if (task.checkFunc()) {
        int progress = (m_currentInitTaskIndex + 1) * 100 / m_initTasks.size();
        ui->init_progressBar->setValue(progress);
        QString myString = QString("%1:执行成功!").arg(task.description);
        emit UpdateUI(myString);

        m_currentInitTaskIndex++;
        if(m_currentInitTaskIndex == 2){
            QTimer::singleShot(2000, this, &MainWindow::processNextInitTask);
        }
        else{
            QTimer::singleShot(200, this, &MainWindow::processNextInitTask);
        }
    } else {
        QTimer::singleShot(200, this, &MainWindow::processNextInitTask);
        emit UpdateUI(QString("%1:执行中....").arg(task.description));
    }
}


//前往目标站点
void MainWindow::on_btn_AGVToTarget_clicked()
{
    QString TaskList = m_vehicle->m_TaskStatus.taskinfo.TaskListName;
    m_vehicle->sendControlCommandToVehicle(TaskList);
}


//返回初始站点
void MainWindow::on_btn_AGVToOrigin_clicked()
{
    m_vehicle->sendControlCommandToVehicle("task_0");
}

//暂停导航
void MainWindow::on_btn_AGVPauseNavigate_clicked()
{
    m_vehicle->pauseCurrentNavigation();
}

//继续导航
void MainWindow::on_btn_AGVContinueNavigate_clicked()
{
    m_vehicle->resumeCurrentNavigation();
}

//取消导航
void MainWindow::on_btn_AGVCancelNavigate_clicked()
{
    m_vehicle->cancelCurrentNavigation();
}

//**********************************************************************************

// 解析控件名对应的电机编号
QPair<int, int> MainWindow::parseMotorNumbers(const QString& objName) {
    QRegularExpression regex(R"(\d+(_\d+)?\b)");
    QRegularExpressionMatch match = regex.match(objName);
    if (!match.hasMatch()) {
        return {-1, -1};
    }

    QString numStr = match.captured(0);
    if (numStr.contains('_')) {
        QStringList parts = numStr.split('_');
        return {parts[0].toInt(), parts[1].toInt()};
    } else {
        return {numStr.toInt(), -1};
    }
}


// 连接按钮信号与槽
void MainWindow::initMotorButtons() {
    // 相对移动
    auto disMoveButtons = findChildren<QPushButton*>(QRegularExpression("btnDisMove\\d+(_\\d+)?"));
    for (QPushButton* btn : disMoveButtons) {
        connect(btn, &QPushButton::clicked, this, [this, btn]() {
            onDisMoveClicked(btn->objectName());
        });
    }


    // 绝对移动
    auto absMoveButtons = findChildren<QPushButton*>(QRegularExpression("btnDisAbsMove\\d+(_\\d+)?"));
    for (QPushButton* btn : absMoveButtons) {
        connect(btn, &QPushButton::clicked, this, [this, btn]() {
            onAbsMoveClicked(btn->objectName());
        });
    }


    // 回零
    auto zeroButtons = findChildren<QPushButton*>(QRegularExpression("btnZero\\d+(_\\d+)?"));
    for (QPushButton* btn : zeroButtons) {
        connect(btn, &QPushButton::clicked, this, [this, btn]() {
            onZeroClicked(btn->objectName());
        });
    }


    // 初始
    auto initButtons = findChildren<QPushButton*>(QRegularExpression("btnInit\\d+(_\\d+)?"));
    for (QPushButton* btn : initButtons) {
        connect(btn, &QPushButton::clicked, this, [this, btn]() {
            onInitClicked(btn->objectName());
        });
    }

    //正向持续运动
    auto forwardButtons = findChildren<QPushButton*>(QRegularExpression("btnForward\\d+(_\\d+)?"));
    for (QPushButton* btn : forwardButtons) {
        connect(btn, &QPushButton::pressed, this, [this, btn]() {
            onDirButtonPressed(btn->objectName(), Forward);
        });
        connect(btn, &QPushButton::released, this, [this, btn]() {
            onDirButtonReleased(btn->objectName());
        });
    }

    //反向持续运动
    auto backwardButtons = findChildren<QPushButton*>(QRegularExpression("btnBackward\\d+(_\\d+)?"));
    for (QPushButton* btn : backwardButtons) {
        connect(btn, &QPushButton::pressed, this, [this, btn]() {
            onDirButtonPressed(btn->objectName(), Backward);
        });
        connect(btn, &QPushButton::released, this, [this, btn]() {
            onDirButtonReleased(btn->objectName());
        });
    }
}


// 相对移动
void MainWindow::onDisMoveClicked(const QString& btnName) {
    if (!zm->GetConnectStatus()) {
        emit UpdateUI("未连接控制器");
        return;
    }

    auto [i, j] = parseMotorNumbers(btnName);
    if (i == -1) return;

    QString tempName = btnName;
    QString spinBoxName = tempName.replace("btnDisMove", "spinBox_PosMove");
    QSpinBox* spinBox = findChild<QSpinBox*>(spinBoxName);
    if (!spinBox) {
        qWarning() << "未找到对应的spinBox:" << spinBoxName;
        return;
    }
    int value = spinBox->value();

    // if(i != 11){//除升降电机，其他电机运动前都要进行高度判定
    //     if(zm->GetAxisDpos(11) < -0.2){
    //         QMessageBox::warning(this, "警告", "机构位置过高，请先降低至15再运动！");
    //         return;
    //     }
    // }

    if (j == -1) {
        zm->MoveSingleOpp(i, value);
    } else {
        zm->MoveSingleOpp(i, value);
        zm->MoveSingleOpp(j, value);
    }
}


// 绝对移动
void MainWindow::onAbsMoveClicked(const QString& btnName) {
    if (!zm->GetConnectStatus()) {
        emit UpdateUI("未连接控制器");
        return;
    }

    auto [i, j] = parseMotorNumbers(btnName);
    if (i == -1) return;

    QString tempName = btnName;
    QString spinBoxName = tempName.replace("btnDisAbsMove", "spinBox_PosAbsMove");
    QSpinBox* spinBox = findChild<QSpinBox*>(spinBoxName);
    if (!spinBox) {
        qWarning() << "未找到对应的spinBox:" << spinBoxName;
        return;
    }
    int value = spinBox->value();

    // if(i != 11){//除升降电机，其他电机运动前都要进行高度判定
    //     if(zm->GetAxisDpos(11) < -0.2){
    //         QMessageBox::warning(this, "警告", "机构位置过高，请先降低至15再运动！");
    //         return;
    //     }
    // }

    if (j == -1) {
        zm->MoveSingleAbs(i, value);
    } else {
        zm->MoveSingleAbs(i, value);
        zm->MoveSingleAbs(j, value);
    }
}


// 回零按钮
void MainWindow::onZeroClicked(const QString& btnName) {
    if (!zm->GetConnectStatus()) {
        emit UpdateUI("未连接控制器");
        return;
    }

    auto [i, j] = parseMotorNumbers(btnName);
    if (i == -1) return;

    if(i != 11){//除升降电机，其他电机运动前都要进行高度判定
        if(zm->GetAxisDpos(11) < -0.2){
            QMessageBox::warning(this, "警告", "机构位置过高，请先降低至15再运动！");
            return;
        }
    }

    if (j == -1) {
        zm->MoveZero(i);
    } else {
        zm->MoveZero(i);
        zm->MoveZero(j);
    }
}


// 初始
void MainWindow::onInitClicked(const QString& btnName) {
    if (!zm->GetConnectStatus()) {
        emit UpdateUI("未连接控制器");
        return;
    }

    auto [i, j] = parseMotorNumbers(btnName);
    if (i == -1) return;

    if(i != 11){//除升降电机，其他电机运动前都要进行高度判定
        if(zm->GetAxisDpos(11) < -0.2){
            QMessageBox::warning(this, "警告", "机构位置过高，请先降低至15再运动！");
            return;
        }
    }

    if (j == -1) {
        zm->MoveInit(i);
    } else {
        zm->MoveInit(i);
        zm->MoveInit(j);
    }
}


//点动
void MainWindow::onDirButtonPressed(const QString& btnName, MoveMode dir) {
    if (!zm->GetConnectStatus()) {
        emit UpdateUI("未连接控制器");
        return;
    }

    auto [i, j] = parseMotorNumbers(btnName);
    if (i == -1) return;

    // IO限位定义
    constexpr int IO_FIRST_LIFT_DOWN  = 5;  // 一级下限位
    constexpr int IO_FIRST_LIFT_UP    = 6;  // 一级上限位
    constexpr int IO_SECOND_LIFT_DOWN = 7;  // 二级下限位
    constexpr int IO_SECOND_LIFT_UP   = 8;  // 二级上限位
    // 电机轴号
    constexpr int AXIS_FIRST_LIFT  = 4;
    constexpr int AXIS_SECOND_LIFT = 12; // 实际二级升降轴号请确认
    bool firstDown  = statusIOInPut[IO_FIRST_LIFT_DOWN];
    bool firstUp    = statusIOInPut[IO_FIRST_LIFT_UP];
    bool secondDown = statusIOInPut[IO_SECOND_LIFT_DOWN];
    bool secondUp   = statusIOInPut[IO_SECOND_LIFT_UP];
    // 一级升降限位保护
    if ((i == AXIS_FIRST_LIFT || j == AXIS_FIRST_LIFT)) {
        if (firstUp && dir == Forward) {
            emit UpdateUI("一级升降上限位触发，禁止继续上升");
            return;
        }
        if (firstDown && dir == Backward) {
            emit UpdateUI("一级升降下限位触发，禁止继续下降");
            return;
        }
    }
    // 二级升降限位保护
    if ((i == AXIS_SECOND_LIFT || j == AXIS_SECOND_LIFT)) {
        if (secondUp && dir == Forward) {
            emit UpdateUI("二级升降上限位触发，禁止继续上升");
            return;
        }
        if (secondDown && dir == Backward) {
            emit UpdateUI("二级升降下限位触发，禁止继续下降");
            return;
        }
    }

    if(i != 11){//除升降电机，其他电机运动前都要进行高度判定
        if(zm->GetAxisDpos(7) < -0.2){
            QMessageBox::warning(this, "警告", "机构位置过高，请先降低至15再运动！");
            return;
        }
    }

    if (j == -1) {
        zm->SingleVMove(i, dir);
    } else {
        zm->SingleVMove(i, dir);
        zm->SingleVMove(j, dir);
    }
}


// 停止点动
void MainWindow::onDirButtonReleased(const QString& btnName) {
    if (!zm->GetConnectStatus()) {
        emit UpdateUI("未连接控制器");
        return;
    }

    auto [i, j] = parseMotorNumbers(btnName);
    if (i == -1) return;

    if (j == -1) {
        zm->SingleVMove(i, Cancel);
    } else {
        zm->SingleVMove(i, Cancel);
        zm->SingleVMove(j, Cancel);
    }
}


void MainWindow::iniInit()
{
    ui->txtEditConnectContent->setText(iniRead->value("/IP/ip").toString());
    ui->cBoxConnectType->setCurrentIndex(iniRead->value("/IP/com").toInt()-1);
}

void MainWindow::SerialPortInit()
{
    // 获取可用串口列表
    m_portNameList = SerialSensor::getAvailablePorts();

    // 初始化界面控件
    ui->comboBoxPortNameForce->addItems(m_portNameList);
    ui->comboBoxPortNameAngle->addItems(m_portNameList);
    ui->comboBoxPortNameDistance->addItems(m_portNameList);

    // 片架、灯箱电机串口
    ui->comboBoxPortPJ->addItems(m_portNameList);
    ui->comboBoxPortLight->addItems(m_portNameList);

    QStringList bautRatesList;
    bautRatesList << "1200" << "2400" << "4800" << "9600" << "19200" << "57600" << "115200";
}

//打开压力、倾角、激光测距传感器串口
void MainWindow::on_btn_Battery2_clicked()
{
    if (ui->btn_Battery2->text() == "显示电池二信息") {
        emit Sig_OpenPort_Battery("COM10");
        QTimer::singleShot(500, this, [=]() {
            if (m_battery->isOpen()) {
                ui->btn_Battery2->setText("关闭电池二信息");
                Sig_StartAutoSend_Battery(500);
            }
        });
    } else {
        emit Sig_ClosePort_Battery();
        QTimer::singleShot(500, this, [=]() {
            if (!m_battery->isOpen()) {
                ui->btn_Battery2->setText("显示电池二信息");
                Sig_StopAutoSend_Battery();
                ui->remainingmAh->setText("");
                ui->packTotalVoltage->setText("");
                ui->packTotalCurrent->setText("");
            }
        });
    }
}

void MainWindow::on_btnOpenForceCOM_clicked()
{
    if (ui->btnOpenForceCOM->text() == "打开压力串口") {
        QString portName = ui->comboBoxPortNameForce->currentText();
        emit Sig_OpenPort_Force(portName);
        QTimer::singleShot(50, this, [=]() {
            if (m_forceSensor->isOpen()) {
                ui->btnOpenForceCOM->setText("关闭压力串口");
            }
        });
    } else {
        emit Sig_ClosePort_Force();
        QTimer::singleShot(50, this, [=]() {
            if (!m_forceSensor->isOpen()) {
                ui->btnOpenForceCOM->setText("打开压力串口");
            }
        });
    }
}

void MainWindow::on_btnOpenAngleCOM_clicked()
{
    if (ui->btnOpenAngleCOM->text() == "打开角度串口") {
        QString portName = ui->comboBoxPortNameAngle->currentText();
        emit Sig_OpenPort_Angle(portName);
        QTimer::singleShot(50, this, [=]() {
            if (m_angleSensor->isOpen()) {
                ui->btnOpenAngleCOM->setText("关闭角度串口");
            }
        });
    } else {
        emit Sig_ClosePort_Angle();
        QTimer::singleShot(50, this, [=]() {
            if (!m_angleSensor->isOpen()) {
                ui->btnOpenAngleCOM->setText("打开角度串口");
            }
        });
    }
}

void MainWindow::on_btnOpenDistanceCOM_clicked()
{
    if (ui->btnOpenDistanceCOM->text() == "打开距离串口") {
        QString portName = ui->comboBoxPortNameDistance->currentText();
        emit Sig_OpenPort_Distance(portName);
        QTimer::singleShot(50, this, [=]() {
            if (m_distanceSensor->isOpen()) {
                ui->btnOpenDistanceCOM->setText("关闭距离串口");
            }
        });
    } else {
        emit Sig_ClosePort_Distance();
        QTimer::singleShot(50, this, [=]() {
            if (!m_distanceSensor->isOpen()) {
                ui->btnOpenDistanceCOM->setText("打开距离串口");
            }
        });
    }
}

//显示压力、角度、距离信息
void MainWindow::on_btn_ShowForce_clicked()
{
    if (ui->btn_ShowForce->text() == "压力信息关闭") {
        emit Sig_StopAutoSend_Force();
        ui->btn_ShowForce->setText("显示压力信息");
        IsClearForce = true;
    } else {
        if (ui->btnOpenForceCOM->text() == "关闭压力串口") {
            emit Sig_StartAutoSend_Force(300);
            ui->btn_ShowForce->setText("压力信息关闭");
        } else {
            QMessageBox::warning(this, "警告", "请先打开压力串口！");
        }
    }
}

void MainWindow::on_btn_ShowAngle_clicked()
{
    if (ui->btn_ShowAngle->text() == "角度信息关闭") {
        emit Sig_StopAutoSend_Angle();
        ui->btn_ShowAngle->setText("显示角度信息");
        IsClearAngle = true;
    } else {
        if (ui->btnOpenAngleCOM->text() == "关闭角度串口") {
            emit Sig_StartAutoSend_Angle(300);
            ui->btn_ShowAngle->setText("角度信息关闭");
        } else {
            QMessageBox::warning(this, "警告", "请先打开角度串口！");
        }
    }
}


void MainWindow::on_btn_ShowDistance_clicked()
{
    if (ui->btn_ShowDistance->text() == "距离信息关闭") {
        emit Sig_StopAutoSend_Distance();
        ui->btn_ShowDistance->setText("显示距离信息");
        IsClearDistance = true;
    } else {
        if (ui->btnOpenDistanceCOM->text() == "关闭距离串口") {
            emit Sig_StartAutoSend_Distance(300);
            ui->btn_ShowDistance->setText("距离信息关闭");
        } else {
            QMessageBox::warning(this, "警告", "请先打开距离串口！");
        }
    }
}


void MainWindow::on_btn_ShowCurrent_clicked()
{
    QLineEdit * MotorCurrent[4] = {ui->Current0, ui->Current1, ui->Current2, ui->Current3};

    if(ui->btn_ShowCurrent->text() == "显示电流信息"){
        connect(CurrentTimer, &QTimer::timeout, this, [=]() {

            for (int i = 0; i < 4; i++) {
                m_current[i] = zm->GetMotorCurrent(i);
                MotorCurrent[i]->setText(QString::number(m_current[i], 'f', 2));
            }
            emit currentInfoUpdated(m_current[0], m_current[1], m_current[2], m_current[3]);

            float current9 = zm->GetFingerCurrent(10);
            float current10 = zm->GetFingerCurrent(11);
            ui->lbl_current9->setText(QString::number(current9, 'f', 2));
            ui->lbl_current10->setText(QString::number(current10, 'f', 2));
        });

        // 间隔100ms更新UI
        CurrentTimer->start(100);
        ui->btn_ShowCurrent->setText("电流信息关闭");
        isShowingCurrent = true;
    }

    else{
        CurrentTimer->stop();
        ui->btn_ShowCurrent->setText("显示电流信息");
        for (int i = 0; i < 4; i++) {
            MotorCurrent[i]->setText("");}
        isShowingCurrent = false;
    }
}

// 实现字符串转16进制
bool convertStringToHex(const QString& str, QByteArray& hexData) {
    hexData.clear();
    QString tempStr = str;
    if (tempStr.contains(" ")) {
        tempStr.replace(QString(" "), QString(""));
    }
    if (tempStr.length() % 2 != 0) {
        // qDebug() << "Invalid hex string length.";
        return false;
    }
    for (int i = 0; i < tempStr.length(); i += 2) {
        QString byteStr = tempStr.mid(i, 2);
        bool ok;
        char byte = static_cast<char>(byteStr.toInt(&ok, 16));
        if (!ok) {
            // qDebug() << "Invalid hex character.";
            return false;
        }
        hexData.append(byte);
    }
    return true;
}

void MainWindow::handleDataFetched(ZmotionStatus *packet)
{
    if(!zm->GetConnectStatus())
        return;

    //qDebug()<<QString::number(packet[0].fslimit, 'f', 3);
    for(int i=0;i<ZMotionControl::AxisNum;i++){
        allAxisStatus[i].speed=packet->allAxisStatus[i].speed;
        allAxisStatus[i].accel=packet->allAxisStatus[i].accel;
        allAxisStatus[i].decel=packet->allAxisStatus[i].decel;
        allAxisStatus[i].units=packet->allAxisStatus[i].units;
        allAxisStatus[i].init=packet->allAxisStatus[i].init;
        allAxisStatus[i].posi=packet->allAxisStatus[i].posi;
        allAxisStatus[i].fslimit=packet->allAxisStatus[i].fslimit;
        allAxisStatus[i].rslimit=packet->allAxisStatus[i].rslimit;
        allAxisStatus[i].status=packet->allAxisStatus[i].status;
        allAxisStatus[i].mtype=packet->allAxisStatus[i].mtype;
        //抛出信号，用于同步监测窗口的电机状态和用于电机状态的逻辑判断
        // emit motorStatusUpdated(i, allAxisStatus[i]);
    }
    statusIOEmergencyStop=packet->allIOStatus->IOInputEmergencyStop;

    //在ZmotionControl类中定义存放IO数值的变量，利用封装的获取IO状态的函数给变量赋值，然后在主窗口通过实例化对象调出变量中的IO状态
    // 记录工控机16个输出IO状态
    for (int i = 0; i < 16; i++){
        statusIOOutPut[i] = (packet->allIOStatus->IOOutputValue & (0x1 << i)) >> i;
    }
    // 记录工控机16个输入IO状态
    for (int i = 0; i < 16; i++){
        statusIOInPut[i] = (packet->allIOStatus->IOInputValue & (0x1 << i)) >> i;
    }
    // 记录汇川IO模块16个输入IO状态 128-143
    for (int i = 0; i < 16; i++){
        statusIOInPut_Zmotion[i] = (packet->allIOStatus->IOInputValue_Zmotion & (0x1 << i)) >> i;
    }
}

//调平
// 脚座触地
void MainWindow::on_btn_FeetToGround_clicked(){
   m_levelingSystem->FeetToGround();
   //更新当前任务步骤，用于临时验证手动自动模式切换
   m_TaskMgr->m_currentStep = TaskMgr::SystemStep::Step_Levelling_Forward;
}

//开始调平
void MainWindow::on_btn_BalanceBody_clicked(){
    m_levelingSystem->BalanceBody();
}

// 急停
void MainWindow::on_btn_LevelRapidStop_clicked(){
    m_levelingSystem->emergencyStop();
}

// 车身恢复触地
void MainWindow::on_btn_BodyToGround_clicked(){
    m_levelingSystem->BodyToGround();
}

//车身抬升
void MainWindow::on_btn_SpeedBodyUp_pressed(){
    m_levelingSystem->StartSpeedBodyUp();
}

void MainWindow::on_btn_SpeedBodyUp_released(){
    m_levelingSystem->StopSpeedBodyUp();
}

void MainWindow::on_btn_PosBodyUp_clicked(){
    m_levelingSystem->PosBodyUp();
}

//车身下降
void MainWindow::on_btn_SpeedBodyDown_pressed(){
    m_levelingSystem->StartSpeedBodyDown();
}

void MainWindow::on_btn_SpeedBodyDown_released(){
    m_levelingSystem->StopSpeedBodyDown();
}

void MainWindow::on_btn_PosBodyDown_clicked(){
    m_levelingSystem->PosBodyDown();
}

//一级升降
void MainWindow::on_btn_MainLiftForward_clicked(){
    m_mainLift->ExeMainLiftTask1();
}


void MainWindow::on_btn_MainLiftBackward_clicked(){
    m_mainLift->ExeMainLiftTask2();
}

void MainWindow::on_btn_MainLiftRapidStop_clicked(){
     m_mainLift->emergencyStop();
}

//二级升降
void MainWindow::on_btn_LiftRapidStop_clicked(){
    m_lift->stop_auto_lift();
}

//六维力传感器
void MainWindow::onSixAxisSensorInitializationResult(bool success, const QString& message)
{
    // float data[6];
    // QRegularExpression regex(R"(通道(\d):([+-]?\d*\.?\d+))");
    // QRegularExpressionMatchIterator iter = regex.globalMatch(message);

    // while (iter.hasNext()) {
    //     QRegularExpressionMatch match = iter.next();
    //     int index = match.captured(1).toInt();      // 捕获通道编号
    //     float value = match.captured(2).toFloat();  // 捕获数值

    //     if (index >= 0 && index < 6) {
    //         data[index] = value;
    //     }
    // }

    emit UpdateUI(message);

    if (!success) {
        IsSixAxisSensorIni = false;
    }
    else{
        IsSixAxisSensorIni = true;
    }
}

void MainWindow::onSixAxisSensorDataReady(ForceSensorData* sensorData)
{
    // 更新UI显示
    ui->lbl_FX->setText(QString::number(sensorData->channels[0], 'f', 2));
    ui->lbl_FY->setText(QString::number(sensorData->channels[1], 'f', 2));
    ui->lbl_FZ->setText(QString::number(sensorData->channels[2], 'f', 2));
    ui->lbl_MX->setText(QString::number(sensorData->channels[3], 'f', 2));
    ui->lbl_MY->setText(QString::number(sensorData->channels[4], 'f', 2));
    ui->lbl_MZ->setText(QString::number(sensorData->channels[5], 'f', 2));

    // 发出信号供其他模块使用
    SixForeData* forcedata = new SixForeData();
    forcedata->Fx = sensorData->channels[0];
    forcedata->Fy = sensorData->channels[1];
    forcedata->Fz = sensorData->channels[2];
    forcedata->Mx = sensorData->channels[3];
    forcedata->My = sensorData->channels[4];
    forcedata->Mz = sensorData->channels[5];

    emit sixAxisForceDataReceived(forcedata);

    // 缓存数据，写入数据库
    MyDataBase->logSensorData("六维力传感器-FX", sensorData->channels[0]);
    MyDataBase->logSensorData("六维力传感器-FY", sensorData->channels[1]);
    MyDataBase->logSensorData("六维力传感器-FZ", sensorData->channels[2]);
    MyDataBase->logSensorData("六维力传感器-MX", sensorData->channels[3]);
    MyDataBase->logSensorData("六维力传感器-MY", sensorData->channels[4]);
    MyDataBase->logSensorData("六维力传感器-MZ", sensorData->channels[5]);


    //绘图
    // 获取时间戳
    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    m_seriesFx->append(nowMs, sensorData->channels[0]);
    m_seriesFy->append(nowMs, sensorData->channels[1]);
    m_seriesFz->append(nowMs, sensorData->channels[2]);
    m_seriesMx->append(nowMs, sensorData->channels[3]);
    m_seriesMy->append(nowMs, sensorData->channels[4]);
    m_seriesMz->append(nowMs, sensorData->channels[5]);

    if (m_seriesFx->count() > m_maxDataPoints) {
        m_seriesFx->remove(0);
        m_seriesFy->remove(0);
        m_seriesFz->remove(0);
        m_seriesMx->remove(0);
        m_seriesMy->remove(0);
        m_seriesMz->remove(0);
    }

    if (m_seriesFx->count() >= 2) {
        QDateTime minTime = QDateTime::fromMSecsSinceEpoch(m_seriesFx->points().first().x());
        QDateTime maxTime = QDateTime::fromMSecsSinceEpoch(m_seriesFx->points().last().x());
        m_axisX->setRange(minTime, maxTime);
    }

    double yMin = std::numeric_limits<double>::max();
    double yMax = std::numeric_limits<double>::lowest();

    QList<QSplineSeries*> allSeries = {m_seriesFx, m_seriesFy, m_seriesFz, m_seriesMx, m_seriesMy, m_seriesMz};
    for (auto series : allSeries) {
        for (const QPointF &point : series->points()) {
            yMin = qMin(yMin, point.y());
            yMax = qMax(yMax, point.y());
        }
    }

    if (qFuzzyCompare(yMin, yMax)) {
        yMin -= 1.0;
        yMax += 1.0;
    }

    m_axisY->setRange(yMin, yMax);
}

void MainWindow::on_btn_ShowForceInfo_clicked(){
    if(ui->btn_ShowForceInfo->text()== "显示力信息"){
        emit sigInitializeSixAxisSensor("192.168.192.45", 4008);
        emit sigStartReadingSixAxisSensor();
        ui->btn_ShowForceInfo->setText("停止显示");
    }
    else{
        emit sigStopReadingSixAxisSensor();
        ui->lbl_FX->setText("");
        ui->lbl_FY->setText("");
        ui->lbl_FZ->setText("");
        ui->lbl_MX->setText("");
        ui->lbl_MY->setText("");
        ui->lbl_MZ->setText("");
        ui->btn_ShowForceInfo->setText("显示力信息");
    }
}

//传感器调零
void MainWindow::on_btn_GoZero_clicked(){
    emit sigSetSixAxisSensorZero();
}

//碰撞检测
void MainWindow::on_btn_CheckCollision_clicked(){
    if(ui->btn_CheckCollision->text()=="检测碰撞"){
        connect(m_crashCheckTimer, &QTimer::timeout, this, [=]() {
            m_lift->CheckCollision();});
        m_crashCheckTimer->start(100);
        liftmonitor->UpdateCollsionUI();//同步监测窗口的标志位
        emit setcollisionChange(true);
        ui->btn_CheckCollision->setText("停止检测");
    }
    else{
        m_crashCheckTimer->stop();
        emit setcollisionChange(false);
        ui->btn_CheckCollision->setText("检测碰撞");
    }
}

//检测脱离
void MainWindow::on_btn_CheckDetach_clicked()
{
    if(ui->btn_CheckDetach->text()=="脱离检测"){
        connect(DetachTimer, &QTimer::timeout, this, [=]() {
            m_lift->CheckDetach();});
        DetachTimer->start(200);
        ui->btn_CheckDetach->setText("停止检测");
    }
    else{
        DetachTimer->stop();
        ui->btn_CheckCollision->setText("脱离检测");
    }
}


//取盖板机构,新修
void MainWindow::on_btn_OpenDoor_clicked(){
    //m_coverPlate->TakeSealDoor();
    // 当前已经到位，本次点击表示确认下一步
    if (m_openDoorWaitingConfirm) {
        m_openDoorWaitingConfirm = false;

        ui->btn_OpenDoor->setText("取封门运行中");
        ui->btn_OpenDoor->setEnabled(false);

        m_coverPlate->confirmDoorNextStep();
        return;
    }

    // 每轮首次启动时检查
    if (!m_coverPlate->isDoorInitialPositionReady()) {
        UpdateUILog("封门位置不对，请将封门移动到规定位置");
        return;
    }

    // 第一次点击，启动半自动取封门
    m_openDoorWaitingConfirm = false;
    m_closeDoorWaitingConfirm = false;

    ui->btn_OpenDoor->setText("取封门运行中");
    ui->btn_OpenDoor->setEnabled(false);
    ui->btn_CloseDoor->setEnabled(false);


    m_coverPlate->TakeSealDoor();
}


void MainWindow::on_btn_TakePlate_clicked(){
    m_coverPlate->TakePlate();
}

//新修
void MainWindow::on_btn_CloseDoor_clicked(){
    //m_coverPlate->FixSealDoor();
    // 当前已经到位，本次点击表示确认下一步
    if (m_closeDoorWaitingConfirm) {
        m_closeDoorWaitingConfirm = false;

        ui->btn_CloseDoor->setText("盖封门运行中");
        ui->btn_CloseDoor->setEnabled(false);

        m_coverPlate->confirmDoorNextStep();
        return;
    }

    // 第一次点击，启动半自动盖封门
    m_openDoorWaitingConfirm = false;
    m_closeDoorWaitingConfirm = false;

    ui->btn_CloseDoor->setText("盖封门运行中");
    ui->btn_CloseDoor->setEnabled(false);
    ui->btn_OpenDoor->setEnabled(false);

    m_coverPlate->FixSealDoor();
}


void MainWindow::on_btn_CoverPlate_clicked(){
    m_coverPlate->FixPlate();
}

void MainWindow::on_btn_AutoTakePlate_clicked(){
    m_coverPlate->CoverPlateTask1();
}


void MainWindow::on_btn_AutoCoverPlate_clicked(){
    m_coverPlate->CoverPlateTask2();
}

void MainWindow::on_btn_CoverPlateRapidStop_clicked(){
    m_coverPlate->emergencyStop();
    m_coverPlate->STOPAllFSMs();

    m_openDoorWaitingConfirm = false;
    m_closeDoorWaitingConfirm = false;

    ui->btn_OpenDoor->setText("打开封门");
    ui->btn_CloseDoor->setText("关闭封门");

    ui->btn_OpenDoor->setToolTip("");
    ui->btn_CloseDoor->setToolTip("");

    ui->btn_OpenDoor->setEnabled(true);
    ui->btn_CloseDoor->setEnabled(true);
}


//选择左右盖板
//左侧
void MainWindow::on_radioButton_leftPlate_toggled(bool checked)
{
    m_coverPlate->ChooseCoverPlate(true);
}


//中间
void MainWindow::on_radioButton_middlePlate_toggled(bool checked)
{
    m_coverPlate->ChooseMiddleCoverPlate(true);
}


//右侧
void MainWindow::on_radioButton_rightPlate_toggled(bool checked)
{
    m_coverPlate->ChooseCoverPlate(false);
}

//总控系统急停
void MainWindow::on_btn_AllRapidStop_clicked(){
    m_vehicle->cancelCurrentNavigation();
    m_levelingSystem->emergencyStop();
    m_mainLift->emergencyStop();
    m_coverPlate->emergencyStop();
    m_lift->stop_auto_lift();
    m_pin->emergencyStop();
}

//连接仿真器
void MainWindow::on_btnConnectSimulation_clicked(){
    if(!zm->ConnectSimulation()){
        QString baseStyle = "border-radius: 17px; border: 2px solid black; background-color: red;";
        ui->lbl_isConnect->setStyleSheet(baseStyle.replace("red", "green"));

        emit UpdateUI("仿真器连接成功");
        mvGetTimer->start(500);
        if(allAxisStatus[2].posi==0){
            // ui->robot3D_virtual->mRobotConfig.JVars[3]=-allAxisStatus[2].posi+180;
            // ui->robot3D_virtual->updateGL();
        }


    }
    else{
        emit UpdateUI("仿真器连接失败");
    }
}

//连接控制器
void MainWindow::on_btnConnect_clicked()
{
    std::string str = ui->txtEditConnectContent->toPlainText().toStdString();//QString转换为string
    const char* ch = str.c_str();
    if(!zm->Connect(ui->cBoxConnectType->currentIndex()+1, ch)){
        QString baseStyle = "border-radius: 17px; border: 2px solid black; background-color: red;";
        ui->lbl_isConnect->setStyleSheet(baseStyle.replace("red", "green"));
        emit UpdateUI("控制器连接成功!");

        isConnectController = true;

        //连接成功移入子线程准备初始化
        zm->moveToThread(zmThread);
        zmThread->start();

        motorStatusTimer->start(200);
        CameraTimer->start(200);

        // connect(m_fingerTimer,&QTimer::timeout,this,[this](){
        //     ui->label_finger9->setText(QString::number(zm->GetMotorCurrent(10)));
        //     ui->label_finger10->setText(QString::number(zm->GetMotorCurrent(11)));
        // });
        // m_fingerTimer->start(500);

        mvGetTimer->start(1000);
        iniRead->setValue("/IP/ip",ui->txtEditConnectContent->toPlainText());
        iniRead->setValue("/IP/com",ui->cBoxConnectType->currentIndex()+1);
        // if(allAxisStatus[2].posi==0){
            // ui->robot3D_virtual->mRobotConfig.JVars[3]=-allAxisStatus[2].posi+180;
            // ui->robot3D_virtual->updateGL();
        // }
        // 启动回调线程，并传入主线程的回调函数
        // 使用 lambda 表达式包装成员函数指针
        zm->startCallbackThread([this](ZmotionStatus* status) {
            handleDataFetched(status);
        });    //回调函数获取数据
        // QObject::connect(zm,&ZMotionControl::dataFetched, this, &MainWindow::handleDataFetched);

    }
    else{
        emit UpdateUI("未连接控制器!");
        isConnectController = false;
    }
}

//初始化总线
void MainWindow::on_btnInitBus_clicked()
{
    if(!zm->GetConnectStatus()){
        emit UpdateUI("未连接控制器!");
        return;
    }

    on_btnDisableAxis_clicked();

    emit sig_ExecuteEcatInit();
    emit UpdateUI("总线初始化中....");
}

void MainWindow::on_EcatInitFinished(int result)
{
    if(1 == result){
        emit UpdateUI("总线初始化完成!");

        m_levelingSystem->m_statusMonitorTimer->start(200);//这里打开定时器，确保直接进行手动操作也能更新任务调度UI
        m_mainLift->m_statusMonitorTimer->start(200);
        m_coverPlate->m_statusMonitorTimer->start(200);
        m_lift->m_statusMonitorTimer->start(200);
        // m_pin->m_statusMonitorTimer->start(200);

        isInitBus = true;
        //初始化完成，移回主线程
        zm->moveToThread(QApplication::instance()->thread());
    } else {
        emit UpdateUI("总线初始化失败!");
        isInitBus = false;
    }
}


//初始化轴
void MainWindow::on_btnInitAxis_clicked()
{
    if(!zm->GetConnectStatus()){
        emit UpdateUI("未连接控制器");
        return;
    }

    //初始化UI参数显示
    struct ParamInfo {
        QString key;       // INI文件中的键名
        QString prefix;    // UI控件的前缀
    };
    QVector<ParamInfo> paramInfos = {
        {"Units", "lblAxisUnits"},
        {"Speed", "lblAxisSpeed"},
        {"Accel", "lblAxisAccel"},
        {"Decel", "lblAxisDecel"},
        {"FSLimit", "lblFsLimit"},
        {"RSLimit", "lblBsLimit"},
        {"Init", "lblAxisInit"}
    };
    for (ParamInfo& info : paramInfos) {
        for (int i = 0; i < 13; ++i) {
            QString objName = QString("%1%2").arg(info.prefix).arg(i);
            QLabel* label = findChild<QLabel*>(objName);

            // 如果找到控件，则设置文本
            if (label) {
                QString path = QString("/Axis%1/%2").arg(i).arg(info.key);
                label->setText(iniRead->value(path).toString());
            }
        }
    }

    //初始化电机的实际运动参数
    for(int i = 0; i < 13; i++){
        zm->SetAxisUnits(i,iniRead->value("/Axis"+QString::number(i)+"/Units").toFloat());
        zm->SetAxisSpeed(i,iniRead->value("/Axis"+QString::number(i)+"/Speed").toFloat());
        zm->SetAxisAccel(i,iniRead->value("/Axis"+QString::number(i)+"/Accel").toFloat());
        zm->SetAxisDecel(i,iniRead->value("/Axis"+QString::number(i)+"/Decel").toFloat());
        zm->SetAxisFSLimit(i,iniRead->value("/Axis"+QString::number(i)+"/FSLimit").toFloat());
        zm->SetAxisRSLimit(i,iniRead->value("/Axis"+QString::number(i)+"/RSLimit").toFloat());
        zm->myZmotionStatus->allAxisStatus[i].init=iniRead->value("/Axis"+QString::number(i)+"/Init").toFloat();
    }
    emit UpdateUI("初始化轴参数");
}

//轴参数保存
void MainWindow::on_btnAxisSave_clicked()
{
    int i=ui->cBoxAxisNum->currentIndex();

    //保存至ini文件
    iniRead->setValue("/Axis"+QString::number(i)+"/Speed",ui->cBoxAxisSpeed->currentText());
    iniRead->setValue("/Axis"+QString::number(i)+"/Units",ui->cBoxAxisUnits->currentText());
    iniRead->setValue("/Axis"+QString::number(i)+"/Accel",ui->cBoxAxisAccel->currentText());
    iniRead->setValue("/Axis"+QString::number(i)+"/Decel",ui->cBoxAxisDecel->currentText());
    iniRead->setValue("/Axis"+QString::number(i)+"/FSLimit",ui->cBoxAxisFSLimit->currentText());
    iniRead->setValue("/Axis"+QString::number(i)+"/RSLimit",ui->cBoxAxisRSLimit->currentText());
    iniRead->setValue("/Axis"+QString::number(i)+"/Init",ui->cBoxAxisInit->currentText());

    //实际运动参数、UI界面实时更新
    on_btnInitAxis_clicked();

    emit UpdateUI("保存轴参数");
}

//系统阈值保存
void MainWindow::on_btnLevelSave_clicked()
{
    //保存至ini文件
    iniRead->setValue("/LevelingSystem/FrontLeftFootCurrentThreshold",ui->cBoxCurrent0->currentText());
    iniRead->setValue("/LevelingSystem/FrontRightFootCurrentThreshold",ui->cBoxCurrent1->currentText());
    iniRead->setValue("/LevelingSystem/RearLeftFootCurrentThreshold",ui->cBoxCurrent2->currentText());
    iniRead->setValue("/LevelingSystem/RearRightFootCurrentThreshold",ui->cBoxCurrent3->currentText());
    iniRead->setValue("/LevelingSystem/AngleXThreshold",ui->cBoxAngleX->currentText());
    iniRead->setValue("/LevelingSystem/AngleYThreshold",ui->cBoxAngleY->currentText());
    iniRead->setValue("/LevelingSystem/Anglex",ui->cBoxAngleX_2->currentText());
    iniRead->setValue("/LevelingSystem/Angley",ui->cBoxAngleY_2->currentText());
    m_levelingSystem->AngleXThreshold = iniRead->value("/LevelingSystem/AngleXThreshold").toFloat();
    m_levelingSystem->AngleYThreshold = iniRead->value("/LevelingSystem/AngleYThreshold").toFloat();
    m_levelingSystem->FrontLeftFootCurrentThreshold = iniRead->value("/LevelingSystem/FrontLeftFootCurrentThreshold").toFloat();
    m_levelingSystem->FrontRightFootCurrentThreshold = iniRead->value("/LevelingSystem/FrontRightFootCurrentThreshold").toFloat();
    m_levelingSystem->RearLeftFootCurrentThreshold = iniRead->value("/LevelingSystem/RearLeftFootCurrentThreshold").toFloat();
    m_levelingSystem->RearRightFootCurrentThreshold = iniRead->value("/LevelingSystem/RearRightFootCurrentThreshold").toFloat();
    emit UpdateUI("保存调平系统阈值");
}

void MainWindow::on_btnSixForceSave_clicked()
{
    //保存至ini文件
    iniRead->setValue("/Lift/ForceXThreshold",ui->cBoxForceX->currentText());
    iniRead->setValue("/Lift/ForceYThreshold",ui->cBoxForceY->currentText());
    iniRead->setValue("/Lift/ForceZThreshold",ui->cBoxForceZ->currentText());
    iniRead->setValue("/Lift/TorqueXThreshold",ui->cBoxTorqueX->currentText());
    iniRead->setValue("/Lift/TorqueYThreshold",ui->cBoxTorqueY->currentText());
    iniRead->setValue("/Lift/TorqueZThreshold",ui->cBoxTorqueZ->currentText());
    emit UpdateUI("保存碰撞检测阈值");
}

//阈值初始化，从ini文件读取
//为了防止只有个别参数要修改，从而导致其他参数被0覆盖的情况出现，构造函数中先从ini文件给所有控件赋值
void MainWindow::ThresholdInit()
{
    ui->cBoxAngleX->setCurrentText(iniRead->value("/LevelingSystem/AngleXThreshold").toString());
    ui->cBoxAngleY->setCurrentText(iniRead->value("/LevelingSystem/AngleYThreshold").toString());
    ui->cBoxCurrent0->setCurrentText(iniRead->value("/LevelingSystem/FrontLeftFootCurrentThreshold").toString());
    ui->cBoxCurrent1->setCurrentText(iniRead->value("/LevelingSystem/FrontRightFootCurrentThreshold").toString());
    ui->cBoxCurrent2->setCurrentText(iniRead->value("/LevelingSystem/RearLeftFootCurrentThreshold").toString());
    ui->cBoxCurrent3->setCurrentText(iniRead->value("/LevelingSystem/RearRightFootCurrentThreshold").toString());
    ui->cBoxForceX->setCurrentText(iniRead->value("/Lift/ForceXThreshold").toString());
    ui->cBoxForceY->setCurrentText(iniRead->value("/Lift/TorqueYThreshold").toString());
    ui->cBoxForceZ->setCurrentText(iniRead->value("/Lift/TorqueZThreshold").toString());
    ui->cBoxTorqueX->setCurrentText(iniRead->value("/Lift/TorqueXThreshold").toString());
    ui->cBoxTorqueY->setCurrentText(iniRead->value("/Lift/TorqueYThreshold").toString());
    ui->cBoxTorqueZ->setCurrentText(iniRead->value("/Lift/TorqueZThreshold").toString());
    ui->cBoxAngleX_2->setCurrentText(iniRead->value("/LevelingSystem/Anglex").toString());
    ui->cBoxAngleY_2->setCurrentText(iniRead->value("/LevelingSystem/Angley").toString());
}

void MainWindow::on_cBoxAxisNum_editTextChanged(const QString &arg1)
{
    int i = ui->cBoxAxisNum->currentIndex();

    ui->cBoxAxisSpeed->setCurrentText(iniRead->value("/Axis"+QString::number(i)+"/Speed").toString());
    ui->cBoxAxisUnits->setCurrentText(iniRead->value("/Axis"+QString::number(i)+"/Units").toString());
    ui->cBoxAxisAccel->setCurrentText(iniRead->value("/Axis"+QString::number(i)+"/Accel").toString());
    ui->cBoxAxisDecel->setCurrentText(iniRead->value("/Axis"+QString::number(i)+"/Decel").toString());
    ui->cBoxAxisFSLimit->setCurrentText(iniRead->value("/Axis"+QString::number(i)+"/FSLimit").toString());
    ui->cBoxAxisRSLimit->setCurrentText(iniRead->value("/Axis"+QString::number(i)+"/RSLimit").toString());
    ui->cBoxAxisInit->setCurrentText(iniRead->value("/Axis"+QString::number(i)+"/Init").toString());
}

//使能轴
void MainWindow::on_btnEnableAxis_clicked()
{
    if(!zm->GetConnectStatus()){
        emit UpdateUI("未连接控制器!");
        return;
    }
    int* err=new int[ZMotionControl::AxisNum];
    bool hasFailure = false;



    err=zm->SetAxisEnable();

    for(int i=0;i<ZMotionControl::AxisNum;i++){
        if (err[i] == 1) {
            hasFailure = true;
            emit UpdateUI("失败使能轴" + QString::number(i));
            isEnableAllAxis = false;
        }
    }
    if(!hasFailure){
        emit UpdateUI("使能所有轴!");
        isEnableAllAxis = true;
        QThread::msleep(250);
        //打开一、二级刹车抱闸
        zm->SetIOOutput(10,1);
        zm->SetIOOutput(12,1);
        // 打开托举抱闸
        zm->SetIOOutput(14,1);
    }

}

//断开使能
void MainWindow::on_btnDisableAxis_clicked()
{
    if(!zm->GetConnectStatus()){
        emit UpdateUI("未连接控制器!");
        return;
    }

    //先关闭一、二级的刹车抱闸，再断开使能
    zm->SetIOOutput(10,0);
    zm->SetIOOutput(12,0);
    zm->SetIOOutput(14,0);

    int* err=new int[ZMotionControl::AxisNum];
    bool hasFailure = false;
    zm->SetBrakeDisenable();
    QThread::msleep(250);
    zm->DisenableMergeChange();
    err=zm->SetAxisDisable();
    for(int i=0;i<ZMotionControl::AxisNum;i++){
        if (err[i] == 1) {
            hasFailure = true;
            emit UpdateUI("失败取消使能轴" + QString::number(i));
        }
    }
    if(!hasFailure){
        emit UpdateUI("断开使能所有轴");
    }

}

//断开连接
void MainWindow::on_btnDisconnect_clicked()
{
    if(!zm->GetConnectStatus()){
        emit UpdateUI("未连接控制器！");
        return;
    }
    mvGetTimer->stop();

    zm->Disconnect();
    emit UpdateUI("控制器断开连接!");
    QString baseStyle = "border-radius: 17px; border: 2px solid black; background-color: red;";
    ui->lbl_isConnect->setStyleSheet(baseStyle);
}

void MainWindow::UpdateUILog(QString str)
{
    QString trimmedStr = str.trimmed();
    if (trimmedStr.isEmpty()) {
        return; // 空白信息直接跳过
    }

    if (trimmedStr == m_lastValidLog) {
        return; // 重复信息不打印
    }

    if(str == "无"){
        return;
    }

    QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

    QStandardItem *timeItem = new QStandardItem(timeStr);
    QStandardItem *msgItem = new QStandardItem(trimmedStr); // 用去空白后的字符串打印

    // 居中显示+不可编辑
    timeItem->setTextAlignment(Qt::AlignCenter);
    msgItem->setTextAlignment(Qt::AlignCenter);
    timeItem->setEditable(false);
    msgItem->setEditable(false);

    // 添加到表格
    m_tableModel->appendRow({timeItem, msgItem});

    // 自动滚动到最新行
    int lastRow = m_tableModel->rowCount() - 1;
    ui->tableView_SystemStatus->scrollTo(m_tableModel->index(lastRow, 0));

    // 更新上一次打印的有效日志
    m_lastValidLog = trimmedStr;
}

void MainWindow::onMvGetTimerOut()
{
    if(!zm->GetConnectStatus()){
        return;
    }

    //轴状态显示页面设置
    //新增显示
    QLabel* lbl[13][2] = {
        {ui->lbl_LevelPosi0, ui->lblAxisStatusColor0},
        {ui->lbl_LevelPosi1, ui->lblAxisStatusColor1},
        {ui->lbl_LevelPosi2, ui->lblAxisStatusColor2},
        {ui->lbl_LevelPosi3, ui->lblAxisStatusColor3},
        {ui->lbl_MainLiftPosi4, ui->lblAxisStatusColor4},
        {ui->lbl_CoverPlatePosi5, ui->lblAxisStatusColor5},
        {ui->lbl_CoverPlatePosi6, ui->lblAxisStatusColor6},
        {ui->lbl_CoverPlatePosi7, ui->lblAxisStatusColor7},
        {ui->lbl_CoverPlatePosi8, ui->lblAxisStatusColor8},
        {ui->lbl_CoverPlatePosi9, ui->lblAxisStatusColor9},
        {ui->lbl_CoverPlatePosi10, ui->lblAxisStatusColor10},
        {ui->lbl_CoverPlatePosi11, ui->lblAxisStatusColor11},
        {ui->lbl_LiftPosi12, ui->lblAxisStatusColor12}
    };


    //每当数据发生变化时才修改ui界面
    QString baseStyle = "border-radius: 7px; border: 2px solid black; background-color: white;";

    for(int i=0;i < 13;i++){
        // if(pastallAxisStatus[i].posi!=allAxisStatus[i].posi){
            pastallAxisStatus[i].posi=allAxisStatus[i].posi;
            lbl[i][0]->setText(QString::number(allAxisStatus[i].posi, 'f', 3));
        // }


        if(pastallAxisStatus[i].status!=allAxisStatus[i].status){
            QString statusname;
            pastallAxisStatus[i].status=allAxisStatus[i].status;
            if (allAxisStatus[i].status == 正常状态){
                statusname= "正常状态";
                lbl[i][1]->setStyleSheet(baseStyle);
            }
            else if (allAxisStatus[i].status  == 超过正限位){
                statusname= "超过正限位";
                lbl[i][1]->setStyleSheet(baseStyle.replace("white","red"));//报警变红
            }
            else if (allAxisStatus[i].status == 超过负限位){
                statusname= "超过负限位";
                lbl[i][1]->setStyleSheet(baseStyle.replace("white","red"));
            }
            else if (allAxisStatus[i].status == 驱动器错误){
                statusname= "驱动器错误";
                lbl[i][1]->setStyleSheet(baseStyle.replace("white","red"));
            }
            else if (allAxisStatus[i].status == 轴通讯错误){
                statusname= "轴通讯错误";
                lbl[i][1]->setStyleSheet(baseStyle.replace("white","red"));
            }
            else if (allAxisStatus[i].status == 电源异常){
                statusname= "电源异常";
                lbl[i][1]->setStyleSheet(baseStyle.replace("white","red"));
            }
            else if (allAxisStatus[i].status == 外部告警信号){
                statusname= "外部告警信号";
                lbl[i][1]->setStyleSheet(baseStyle.replace("white","red"));
            }
            else{
                statusname= "正常状态";
                lbl[i][1]->setStyleSheet(baseStyle.replace("white","red"));
            }
        }

        if(pastallAxisStatus[i].mtype!=allAxisStatus[i].mtype){
            if(allAxisStatus[i].mtype==0)
                emit UpdateUI("轴"+QString::number(i)+"停止运动");
            else if(pastallAxisStatus[i].mtype==0)
                emit UpdateUI("轴"+QString::number(i)+"开始运动");
            pastallAxisStatus[i].mtype=allAxisStatus[i].mtype;
        }
    }
}

void MainWindow::on_btnEmergencyStop_clicked()
{
    zm->EmergencyStop();
    UpdateUI("急停！！！");
    qInfo()<<"急停！！！";

}

//六自由度平台**************************开始
void MainWindow::on_btn_PlatformOpen_clicked()
{
    m_lift->m_SixDof->servoPowerOn();
    UpdateUI("六自由度平台已开机");
}

void MainWindow::on_btn_PlatformClose_clicked()
{
    m_lift->m_SixDof->servoPowerOff();
    UpdateUI("六自由度平台已关机");
}

void MainWindow::on_btn_PlatformGo_clicked()
{
    float x = ui->SpinBox_PlatformMoveX->text().toFloat();
    float y = ui->SpinBox_PlatformMoveY->text().toFloat();
    float z = ui->SpinBox_PlatformMoveZ->text().toFloat();
    float rx = ui->SpinBox_PlatformMoveRx->text().toFloat();
    float ry = ui->SpinBox_PlatformMoveRy->text().toFloat();
    float rz = ui->SpinBox_PlatformMoveRz->text().toFloat();

    if (x < 90.0f && x > -90.0f &&
        y < 90.0f && y > -90.0f &&
        z < 90.0f && z > -90.0f &&
        rx < 10.0f && rx > -10.0f &&
        ry < 10.0f && ry > -10.0f &&
        rz < 10.0f && rz > -10.0f)
    {
        // qDebug() <<"开始运动";
        UpdateUI("六自由度平台开始定点运动");
        m_lift->m_SixDof->posePointMotion(x, y, z, rx, ry, rz,2,1);
    }
    else
    {
        UpdateUI("输入坐标不在许可范围内");
    }
}


void MainWindow::on_btn_PlatformStop_clicked()
{
    m_lift->m_SixDof->stopMotion();
    UpdateUI("六自由度平台已暂停运动");
}


void MainWindow::on_btn_PlatformGoZero_clicked()
{
    UpdateUI("六自由度平台开始回零");
    m_lift->m_SixDof->homing();
}


void MainWindow::on_btn_PlatformReset_clicked()
{
    m_lift->m_SixDof->resetSystem();
    UpdateUI("六自由度平台已复位");
}


void MainWindow::on_btn_PlatformInitial_clicked()
{
    UpdateUI("六自由度平台开始初始化");
    m_lift->m_SixDof->initialize();
}

void MainWindow::update_six_platform(float x, float y, float z, float rx, float ry, float rz,float gyroPitch, float gyroRoll, float gyroYaw,
                                     bool isReachTarget,
                                     bool emergencyStop,
                                     bool lightFault,
                                     bool heavyFault,
                                     bool servoEnabled,
                                     bool isMoving)
{
    // 原有坐标和到达目标显示逻辑（保持不变）
    ui->lbl_PlatformInfoX->setText(QString("%1").arg(x, 0, 'f', 2));
    ui->lbl_PlatformInfoY->setText(QString("%1").arg(y, 0, 'f', 2));
    ui->lbl_PlatformInfoZ->setText(QString("%1").arg(z, 0, 'f', 2));
    ui->lbl_PlatformInfoRx->setText(QString("%1").arg(rx, 0, 'f', 2));
    ui->lbl_PlatformInfoRy->setText(QString("%1").arg(ry, 0, 'f', 2));
    ui->lbl_PlatformInfoRz->setText(QString("%1").arg(rz, 0, 'f', 2));
    // ui->label_gyroPitch->setText(QString("%1").arg(gyroPitch, 0, 'f', 2));
    // ui->label_gyroRoll->setText(QString("%1").arg(gyroRoll, 0, 'f', 2));
    // ui->label_gyroYaw->setText(QString("%1").arg(gyroYaw, 0, 'f', 2));
    ui->lbl_PlatformIsTarget->setText(isReachTarget ? "是" : "否");
    ui->lbl_PlatformIsTarget->setStyleSheet(isReachTarget ?
                                                "color: green; font-weight: bold;" :
                                                "color: red; font-weight: bold;");

    // 新增：急停报警显示（label_emergencyStop）
    ui->lbl_PlatformWarning->setText(emergencyStop ? "异常" : "正常");
    ui->lbl_PlatformWarning->setStyleSheet(emergencyStop ?
                                               "color: red; font-weight: bold;" :
                                               "color: green; font-weight: bold;");

    // 新增：轻故障报警显示（label_lightFault）
    ui->lbl_PlatformWarning->setText(lightFault ? "异常" : "正常");
    ui->lbl_PlatformWarning->setStyleSheet(lightFault ?
                                               "color: orange; font-weight: bold;" :
                                               "color: green; font-weight: bold;");

    // 新增：重故障报警显示（label_heavyFault）
    ui->lbl_PlatformWarning->setText(heavyFault ? "异常" : "正常");
    ui->lbl_PlatformWarning->setStyleSheet(heavyFault ?
                                               "color: red; font-weight: bold;" :
                                               "color: green; font-weight: bold;");

    // 新增：伺服使能反馈显示（label_servoEnabled）
    ui->lbl_PlatformEnable->setText(servoEnabled ? "已通电" : "未通电");
    ui->lbl_PlatformEnable->setStyleSheet(servoEnabled ?
                                              "color: green; font-weight: bold;" :
                                              "color: gray; font-weight: bold;");
    // 新增：是否运动状态显示
    ui->lbl_PlatformStatus->setText(isMoving ? "运动中" : "静止");
    ui->lbl_PlatformStatus->setStyleSheet(isMoving ?
                                              "color: blue; font-weight: bold;" :
                                              "color: green; font-weight: bold;");
    ui->groypitch->setText(QString("%1").arg(gyroPitch, 0, 'f', 2));
    ui->groyroll->setText(QString("%1").arg(gyroRoll, 0, 'f', 2));

    // 发出信号，用于碰撞检测信息实时更新
    // emit sixDofPlatformDataReceived(x, y, z, rx, ry, rz);

    // 打包数据
    SixDofPlatformData data;
    data.x = x;
    data.y = y;
    data.z = z;
    data.rx = rx;
    data.ry = ry;
    data.rz = rz;
    data.motion_mode = isMoving;
    data.servo_enabled = servoEnabled;
    data.target_reached = isReachTarget;
    data.alarmText = lightFault;

    // 发出打包后的信号
    emit sixDofPlatformDataPacked(data);

    //禁用按钮，防止冲突
    // if(servo_enabled == false)
    // {
    //     ui->btn_PlatformGo->setEnabled(false);
    //     ui->btn_PlatformInitial->setEnabled(false);
    //     ui->btn_PlatformGoZero->setEnabled(false);
    //     ui->btn_PlatformClose->setEnabled(false);
    // }
    // if(servo_enabled == true)
    // {
    //     if(motion_mode != 1)
    //     {
    //         ui->btn_PlatformGo->setEnabled(false);
    //         ui->btn_PlatformInitial->setEnabled(false);
    //         ui->btn_PlatformGoZero->setEnabled(false);
    //         ui->btn_PlatformClose->setEnabled(false);
    //         ui->btn_PlatformOpen->setEnabled(false);
    //     }
    //     if(motion_mode == 1)
    //     {
    //         ui->btn_PlatformGo->setEnabled(true);
    //         ui->btn_PlatformInitial->setEnabled(true);
    //         ui->btn_PlatformGoZero->setEnabled(true);
    //         ui->btn_PlatformClose->setEnabled(true);
    //         ui->btn_PlatformOpen->setEnabled(false);
    //     }
    // }
}
//六自由度平台**************************结束




//视觉模块******************************开始

void MainWindow::on_btn_secondary_lift_clicked()
{
    if (!m_isLRUTypeSelected) {
        emit UpdateUI("未选择LRU类型");
        return;
    }

    ui->btn_secondary_lift->setEnabled(false);
    QThread* startThread = new QThread;
    QObject::connect(startThread, &QThread::started, [this,startThread]() {
        m_lift->autolift = true;
        m_lift->auto_lift();
        startThread->quit();
    });
    // 连接 finished 信号到 deleteLater 槽，确保线程执行完毕后释放资源
    QObject::connect(startThread, &QThread::finished, startThread, &QObject::deleteLater);
    QObject::connect(startThread, &QThread::finished, this,  [this](){
        ui->btn_secondary_lift->setEnabled(true); // 线程结束后恢复按钮可用
    });

    // 启动线程
    startThread->start();
}


void MainWindow::on_btn_stop_secondry_clicked()
{
    m_lift->stop_auto_lift();
}


void MainWindow::on_btn_CheckStatus_clicked()
{
    ui->btn_CheckStatus->setEnabled(false);
    QThread* startThread = new QThread;
    QObject::connect(startThread, &QThread::started, [this,startThread]() {
        m_lift->vision_detected();
        startThread->quit();
    });
    // 连接 finished 信号到 deleteLater 槽，确保线程执行完毕后释放资源
    QObject::connect(startThread, &QThread::finished, startThread, &QObject::deleteLater);
    QObject::connect(startThread, &QThread::finished, this,  [this](){
        ui->btn_CheckStatus->setEnabled(true); // 线程结束后恢复按钮可用
    });

    // 启动线程
    startThread->start();
}


void MainWindow::on_btn_StatusModifyLatter_clicked()
{
    ui->btn_StatusModifyLatter->setEnabled(false);
    QThread* startThread = new QThread;
    QObject::connect(startThread, &QThread::started, [this,startThread]() {
        m_lift->StatusModifyLatte();
        startThread->quit();
    });
    // 连接 finished 信号到 deleteLater 槽，确保线程执行完毕后释放资源
    QObject::connect(startThread, &QThread::finished, startThread, &QObject::deleteLater);
    QObject::connect(startThread, &QThread::finished, this,  [this](){
        ui->btn_StatusModifyLatter->setEnabled(true); // 线程结束后恢复按钮可用
    });

    // 启动线程
    startThread->start();
}

void MainWindow::slot_updateGapInfo(nsTaskMgr::DetectedDisplayData data)
{
    // 在主线程中更新UI（线程安全）
    QString angleText = QString::number(data.angle, 'f', 2);
    QString gapwidth_x = QString::number(data.rho1, 'f', 2);
    QString gapwidth_y = QString::number(data.rho2, 'f', 2);
    ui->lbl_GapAngle->setText(angleText);
    ui->lbl_GapWidth_x->setText(gapwidth_x);
    ui->lbl_GapWidth_y->setText(gapwidth_y);

    //同时更新数字孪生的角度和缝隙数据
    monitor->m_GapWidth = gapwidth_x.toFloat();
    monitor->m_GapWidth1 = gapwidth_y.toFloat();
    monitor->m_GapAngle = angleText.toFloat();
}

void MainWindow::onLight1ValueChanged(int value)
{
    m_lift->m_lightcontroller->setBrightness(LightSourceController::Channel1, value);
}

void MainWindow::onLight2ValueChanged(int value)
{
    m_lift->m_lightcontroller->setBrightness(LightSourceController::Channel2, value);
}
void MainWindow::lightbrightnessvaleinit(int value1, int value2)
{
    ui->light1_value->setValue(value1);
    ui->light2_value->setValue(value2);
}

void MainWindow::onComboChanged(const QString &text)
{
    if (LRUpresetData().contains(text)) {
        emit paramsSelected(LRUpresetData().value(text));
    }
}

void MainWindow::on_btn_SelectLRUType_clicked()
{
    QStringList lruItems;
    for (int i = 0; i < ui->comboBox_LRUdata->count(); ++i) {
        lruItems << ui->comboBox_LRUdata->itemText(i);
    }

    if (lruItems.isEmpty()) {
        emit UpdateUI("未配置LRU类型");
        return;
    }

    QInputDialog dialog(this);
    dialog.setWindowTitle("选择LRU类型");
    dialog.setLabelText("请选择LRU类型：");
    dialog.setComboBoxItems(lruItems);
    dialog.setComboBoxEditable(false);
    dialog.setTextValue(ui->comboBox_LRUdata->currentText());
    dialog.setMinimumSize(560, 220);
    dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString selectedLRU = dialog.textValue();
    if (selectedLRU.isEmpty()) {
        return;
    }

    ui->comboBox_LRUdata->setCurrentText(selectedLRU);
    onComboChanged(selectedLRU);
    m_isLRUTypeSelected = true;
    emit UpdateUI(QString("已选择LRU类型：%1").arg(selectedLRU));
}



void MainWindow::on_distance_open_clicked()
{
    // 打开串口后启动定时器
    if (lasersensor->openSerialPort()) {  // 假设openSerialPort返回bool表示是否成功打开
        distanceTimer->start();
        UpdateUI("打开了");
    } else {
        distanceTimer->stop();
        // 可选：提示串口打开失败
        ui->distance1->setText("串口失败");
        UpdateUI("打开失败");
        // ui->distance2->setText("串口失败");
    }
}
void MainWindow::updateDistanceLabels()
{
    // 读取距离值（假设getDistance1/2返回double类型）
    double dist1 = lasersensor->getDistance_right();
    double dist2 = lasersensor->getDistance_left();

    // 格式化显示（保留2位小数，单位可自定义）无效为0
    // ui->right_laser->setText(QString::number(dist1, 'f', 1));
    // ui->left_laser->setText(QString::number(dist2, 'f', 1));

    ui->right_laser->setText("右" + QString::number(dist1, 'f', 1));
    ui->left_laser->setText("左" + QString::number(dist2, 'f', 1));
}

void MainWindow::onXChanged(double val)
{
    m_lift->m_vision_detected->x_offset = static_cast<float>(val);
}
void MainWindow::onYChanged(double val)
{
    m_lift->m_vision_detected->y_offset = static_cast<float>(val);
}
void MainWindow::onRzChanged(double val)
{
    m_lift->m_vision_detected->rz_offset = static_cast<float>(val);
}
//视觉模块******************************结束



void MainWindow::on_spinBox_Distance_valueChanged(double arg1)
{
   m_levelingSystem->disforleveling = arg1;
}




//任务调度槽函数******************************************************************
void MainWindow::on_btn_ExeVehTaskForward_clicked(){
    m_TaskMgr->EmitsigExeVehTask1();
}

void MainWindow::on_btn_ExeVehTaskBackward_clicked(){
     m_TaskMgr->EmitsigExeVehTask2();
}

void MainWindow::on_btn_ExeLevelTaskForward_clicked(){
    m_TaskMgr->EmitsigExeLevelingTask1();
}

void MainWindow::on_btn_ExeLevelTaskBackward_clicked(){
    m_TaskMgr->EmitsigExeLevelingTask2();
}

void MainWindow::on_btn_ExeMainLiftTaskForward_clicked(){
    m_TaskMgr->EmitsigExeMainLiftTask1();
}

void MainWindow::on_btn_ExeMainLiftTaskBackward_clicked(){
    m_TaskMgr->EmitsigExeMainLiftTask2();
}

void MainWindow::on_btn_ExeCoverPlateTaskForward_clicked(){
    m_TaskMgr->EmitsigExeCoverPlateTask1();
}

void MainWindow::on_btn_ExeCoverPlateTaskBackward_clicked(){
    m_TaskMgr->EmitsigExeCoverPlateTask2();
}

void MainWindow::on_btn_ExeLiftTaskForward_clicked(){
    m_TaskMgr->EmitsigExeLiftTask1();
}

void MainWindow::on_btn_ExeLiftTaskBackward_clicked(){
    m_TaskMgr->EmitsigExeLiftTask2();
}

void MainWindow::on_btn_ExePinTaskForward_clicked(){
    m_TaskMgr->EmitsigExePinTask();
}

void MainWindow::on_btn_StartTask_clicked(){
    // if(m_TaskMgr->m_currentStep != TaskMgr::SystemStep::Step_Veh_Forward){
    //     QMessageBox::warning(this, "警告", "请点击继续按钮！");
    //     isExitError = true;
    //     return;
    // }
    m_TaskMgr->startTask();
}

void MainWindow::on_btn_StopTask_clicked(){
   on_btn_AllRapidStop_clicked();
}

void MainWindow::on_btn_PauseTask_clicked(){
    on_btn_AllRapidStop_clicked();
}
//********************************************************************************
void MainWindow::setupModeButtons()
{
    // 自动和手动模式互斥
    QButtonGroup* modeGroup = new QButtonGroup(this);
    modeGroup->addButton(ui->radioButton_AutoMode);
    modeGroup->addButton(ui->radioButton_ManualMode);
    modeGroup->setExclusive(true);

    // //初始禁用手动模式
    // for(int i = 1;i < 6;++i)
    // {
    //    ui->tabWidget->tabBar()->setTabEnabled(i, false);
    // }



    connect(ui->radioButton_AutoMode, &QRadioButton::toggled, this, &MainWindow::onAutoModeToggled);
    connect(ui->radioButton_ManualMode, &QRadioButton::toggled, this, &MainWindow::onManualModeToggled);

    // 默认选择手动模式
    ui->radioButton_ManualMode->setChecked(true);

    connect(ui->btn_AGVManual, &QPushButton::clicked, this, &MainWindow::switchToAGV);
    connect(ui->btn_LevelManual, &QPushButton::clicked, this, &MainWindow::switchToLevelling);
    connect(ui->btn_MainLiftManual, &QPushButton::clicked, this, &MainWindow::switchToFirstLift);
    connect(ui->btn_CoverPlateManual, &QPushButton::clicked, this, &MainWindow::switchToCoverPlate);
    connect(ui->btn_LiftManual, &QPushButton::clicked, this, &MainWindow::switchToSecondLift);
    connect(ui->btn_PinManual, &QPushButton::clicked, this, &MainWindow::switchToPin);
}

void MainWindow::onAutoModeToggled(bool checked)
{

    QString baseStyle = "border-radius: 7px; border: 2px solid black; background-color: white;";
    ui->lbl_Manual->setStyleSheet(baseStyle);
    ui->lbl_Auto->setStyleSheet(baseStyle.replace("white","green"));

    bool autoEnabled = checked;

    // 自动模式下的按钮启用/禁用
    QGroupBox* GroupBox1 = findChild<QGroupBox*>("groupBox_AutoMode");
    if (GroupBox1) {
        for (auto button : GroupBox1->findChildren<QPushButton*>()) {
            button->setEnabled(autoEnabled);
        }
        for (auto label : GroupBox1->findChildren<QLabel*>()) {
            label->setEnabled(autoEnabled);
        }
        for (auto box : GroupBox1->findChildren<QComboBox*>()) {
            box->setEnabled(autoEnabled);
        }
    }

    // // 手动模式下的按钮启用/禁用
    // QGroupBox* GroupBox2 = findChild<QGroupBox*>("groupBox_ManualMode");
    // if (GroupBox2) {
    //     for (auto button : GroupBox2->findChildren<QPushButton*>()) {
    //         button->setEnabled(!autoEnabled);
    //     }
    // }

    if (autoEnabled)
    {
        // // 禁用手动Tab页
        // for (int i = 1; i < 6; ++i) {
        //     ui->tabWidget->tabBar()->setTabEnabled(i, !autoEnabled);
        // }
        zm->SetIOOutput(2, 0);
        zm->SetIOOutput(3, 1);// 自动模式,点亮绿灯
        zm->SetIOOutput(4, 0);

        //自动模式标志位改变，用于UI更新手动标志
        isAuto = true;

        //自动模式允许系统切换
        m_TaskMgr->AllowContinue = true;
    }
}

void MainWindow::onManualModeToggled(bool checked)
{
    QString baseStyle = "border-radius: 7px; border: 2px solid black; background-color: white;";
    ui->lbl_Auto->setStyleSheet(baseStyle);
    ui->lbl_Manual->setStyleSheet(baseStyle.replace("white","green"));

    bool manualEnabled = checked;

    QGroupBox* GroupBox1 = findChild<QGroupBox*>("groupBox_AutoMode");
    if (GroupBox1) {
        for (auto button : GroupBox1->findChildren<QPushButton*>()) {
            button->setEnabled(!manualEnabled);
        }
        for (auto box : GroupBox1->findChildren<QComboBox*>()) {
            box->setEnabled(!manualEnabled);
        }
        for (auto radio : GroupBox1->findChildren<QRadioButton*>()) {
            if (radio != ui->radioButton_AutoMode) {
                radio->setEnabled(!manualEnabled);
            }
        }
    }


    QGroupBox* GroupBox2 = findChild<QGroupBox*>("groupBox_ManualMode");
    if (GroupBox2) {
        for (auto button : GroupBox2->findChildren<QPushButton*>()) {
            button->setEnabled(manualEnabled);
        }
    }

    if (manualEnabled)
    {
        // 切换到手动模式后立即停止当前的自动任务
        m_vehicle->cancelCurrentNavigation();
        m_levelingSystem->emergencyStop();
        m_mainLift->emergencyStop();
        m_coverPlate->emergencyStop();
        //目前调试，手动也需要使能动作，后面发布解开注释
        // m_coverPlate->DisableAllFSMs();
        m_lift->stop_auto_lift();
        m_pin->emergencyStop();

        //手动模式禁止系统切换
        m_TaskMgr->AllowContinue = false;

        // 启用手动Tab页
        for (int i = 1; i < 6; ++i) {
            ui->tabWidget->tabBar()->setTabEnabled(i, manualEnabled);
        }

        zm->SetIOOutput(2, 1); // 手动模式,点亮黄灯
        zm->SetIOOutput(3, 0);
        zm->SetIOOutput(4, 0);

        //手动模式标志位改变，用于UI更新手动标志
        isAuto = false;
    }
}

void MainWindow::switchToAGV()
{
    ui->tabWidget->setCurrentIndex(1);  //AGV
}

void MainWindow::switchToLevelling()
{
    ui->tabWidget->setCurrentIndex(2);  //调平
}

void MainWindow::switchToFirstLift()
{
    ui->tabWidget->setCurrentIndex(3);  //一级升降
}

void MainWindow::switchToCoverPlate()
{
    ui->tabWidget->setCurrentIndex(3);  //盖板
}

void MainWindow::switchToSecondLift()
{
    ui->tabWidget->setCurrentIndex(4);  //二级升降+六自由度+视觉
}

void MainWindow::switchToPin()
{
    ui->tabWidget->setCurrentIndex(5);  //拔销
}

void MainWindow::switchToTaskMgr()
{
    ui->tabWidget->setCurrentIndex(0);  //任务调度
}

void MainWindow::handleTaskError(eTaskStatus errorStatus)
{
    // 获取发生错误的子系统
    eSubSystem errorSubsystem = errorStatus.sys;

    // 根据不同的子系统执行停止操作
    switch (errorSubsystem) {
    case eSubSystem::Vehicle:
        //AGV内部有报错停止和修复后恢复运动的逻辑
        break;
    case eSubSystem::Levelling:
        m_levelingSystem->emergencyStop();  // 停止调平系统
        break;
    case eSubSystem::MainLift:
        m_mainLift->emergencyStop();  // 停止一级升降
        break;
    case eSubSystem::CoverPlate:
        m_coverPlate->emergencyStop();  // 停止盖板系统
        break;
    case eSubSystem::Lift:
       m_lift->stop_auto_lift();  // 停止二级升降
        break;
    default:
        // qDebug() << "未知子系统发生错误";
        break;
    }
}

//此时人为判定任务完成，函数更新标志位(2s后复原的逻辑异步执行，执行前函数已经结束)，触发页面跳转
//各子系统切换自动模式
void MainWindow::on_btn_AGVToAuto_clicked()
{
    // m_vehicle->IsManualNavigate(ui->lineEdit_current_station->text());
    // if(m_vehicle->m_manualNavigate) {
    //     switchToTaskMgr();
    //     on_btn_ContinueTask_clicked();
    // }
    switchToTaskMgr();
    ui->radioButton_AutoMode->setChecked(true);
    //如果是AGV前往目标站点过程中停止并恢复，当前步骤为AGV(0)，调用此处，继续前往目标站点
    //如果是AGV已经到达目标站点，当前步骤已经为调平(1)跳转至AGV手操界面对AGV进行微调，恢复自动直接调用此处，从调平开始执行
    on_btn_ContinueTask_clicked();
}

void MainWindow::on_btn_LevelToAuto_clicked()
{
    // m_levelingSystem->IsManualBalance();
    // if(m_levelingSystem->m_isManualBalance || m_levelingSystem->m_isManualToGround){
    //    switchToTaskMgr();
    //    on_btn_ContinueTask_clicked();
    // }

    // switchToTaskMgr();
    // ui->radioButton_AutoMode->setChecked(true);
    on_btn_ContinueTask_clicked();
}

void MainWindow::on_btn_MainLiftToAuto_clicked()
{
    switchToTaskMgr();
    ui->radioButton_AutoMode->setChecked(true);
    on_btn_ContinueTask_clicked();
}

void MainWindow::on_btn_CoverPlateToAuto_clicked()
{
    //由于点击这里的按钮的时候不是完成状态，因此选择自动后还要再次通过继续按钮调用TaskMgr的process函数从而继续自动流程
    //完成状态只在自动模式下抛出
    switchToTaskMgr();
    ui->radioButton_AutoMode->setChecked(true);
    on_btn_ContinueTask_clicked();
}

void MainWindow::on_btn_LiftToAuto_clicked()
{
    //由于二级升降是手动，所以点击这里的按钮的时候代表完成，但是由于手动了两个动作，不能利用标志位更新任务完成状态
    //直接指定当前步骤即可
    switchToTaskMgr();
    ui->radioButton_AutoMode->setChecked(true);
    m_TaskMgr->m_currentStep = TaskMgr::SystemStep::Step_CoverPlate_Backward;
    on_btn_ContinueTask_clicked();
}

//满足恢复自动条件，点击恢复按钮
void MainWindow::on_btn_ContinueTask_clicked()
{
    //崩溃后，不能从头开始运动，如果点击自动安装，提示点击这里的按钮
    if(isExitError){
        m_TaskMgr->processStep();
        isExitError = false;
        return;
    }

    //报错恢复逻辑
    if(m_TaskMgr->m_errorOccurred)
    {
        m_TaskMgr->resumeFromError();
        QTimer* timer = new QTimer(this);
        connect(timer,&QTimer::timeout,this,[=](){
            if(!m_TaskMgr->m_errorOccurred){//恢复自动化模式的绿灯
                zm->SetIOOutput(2,0);
                zm->SetIOOutput(3,1);
                zm->SetIOOutput(4,0);
            }
        });
        return;
    }

    m_TaskMgr->processStep();//从当前步骤继续执行任务

    // //手动导航恢复逻辑
    // else if(m_vehicle->m_manualNavigate)
    // {
    //     m_vehicle->m_TaskStatus.state = eTaskstate::Complete;
    //     emit m_vehicle->sigStatusFeedback(m_vehicle->m_TaskStatus);
    //     zm->SetIOOutput(2,0);
    //     zm->SetIOOutput(3,1);//恢复自动化模式的绿灯
    //     zm->SetIOOutput(4,0);
    // }

    // //手动调平恢复逻辑
    // else if(m_levelingSystem->m_isManualBalance || m_levelingSystem->m_isManualToGround)
    // {
    //     m_levelingSystem->m_allowEmit = true;
    //     zm->SetIOOutput(2,0);
    //     zm->SetIOOutput(3,1);
    //     zm->SetIOOutput(4,0);
    //     m_levelingSystem->m_isManualBalance = false;
    //     m_levelingSystem->m_isManualToGround = false;
    // }
}


//根据LRU的类型，判定预存的任务链
void MainWindow::on_comboBox_LRU_currentTextChanged(const QString &LRU_Name)
{
    int currentIndex = ui->comboBox_LRU->currentIndex();
    emit SigLRUType(currentIndex - 1);//更新数字孪生
    // 更新 AGV 中的 taskinfo
    m_vehicle->m_TaskStatus.taskinfo.LRU = LRU_Name;
}

//由于任务链名字的拼接规则，必须确保索引1的目标站点对应执行task_0的目标站点LM0,依此类推
void MainWindow::on_comboBox_Station_currentTextChanged(const QString &arg1)
{
    // 获取当前索引
    int currentIndex = ui->comboBox_Station->currentIndex();

    // 拼接任务链名称
    QString TaskListName = QString("task_%1").arg(currentIndex - 1);

    // 更新 AGV 中的 taskinfo
    m_vehicle->m_TaskStatus.taskinfo.TaskListName = TaskListName;
}
//**************************************************************************************
void MainWindow::on_btn_Motor78Forward_clicked()
{
    zm->MoveSingleAbs(4,300);
    zm->MoveSingleAbs(5,300);
}

void MainWindow::on_btn_Motor78Backward_clicked()
{
    zm->MoveSingleAbs(4,0);
    zm->MoveSingleAbs(5,0);
}


//MySQL数据库
void MainWindow::on_btnDbConnect_clicked()
{
    QString host = ui->leDbHost->text();
    QString database = ui->leDbName->text();
    QString user = ui->leDbUser->text();
    QString password = ui->leDbPassword->text();
    if(MyDataBase->init(host,database,user,password)){
        ui->lbl_database->setStyleSheet("background-color: green;");
    };
}


// 断开数据库
void MainWindow::on_btnDbDisconnect_clicked()
{
    MyDataBase->close();
    ui->lbl_database->setStyleSheet("");
    ui->tvDbData->setModel(nullptr); //断开数据库连接的同时，解绑model和QChartView
}


// 刷新MySQL数据并显示到UI
void MainWindow::refreshTableData(const QString& filterCondition) {
    QString tableName = ui->cbxDbTables->currentText();
    if (tableName.isEmpty()) {
        QMessageBox::warning(this, "提示", "请选择数据表");
        return;
    }

    // 获取模型，此处模型绑定数据库以及数据库中的具体数据表
    QSqlTableModel* model = MyDataBase->getTableModel(tableName, filterCondition, this);
    if (!model) {
        QMessageBox::critical(this, "错误", "获取表格数据失败");
        return;
    }

    // 设置表格属性
    ui->tvDbData->setModel(model);//此处绑定表格视图和模型
    ui->tvDbData->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked);
    ui->tvDbData->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tvDbData->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}


// 显示数据
void MainWindow::on_btnDbShow_clicked() {
    refreshTableData();
}


// 查询数据
void MainWindow::on_btnDbQuery_clicked() {
    QString userCondition = ui->leQueryCondition->text().trimmed();
    if (userCondition.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先输入查询条件");
        return;
    }
    refreshTableData(userCondition);
}


// 添加数据
void MainWindow::on_btnDbAdd_clicked() {
    QString tableName = ui->cbxDbTables->currentText();
    if (tableName.isEmpty()) {
        QMessageBox::warning(this, "提示", "请选择数据表");
        return;
    }

    QSqlTableModel* model = qobject_cast<QSqlTableModel*>(ui->tvDbData->model());
    if (!model) {
        QMessageBox::critical(this, "错误", "请先刷新表格数据");
        return;
    }

    if (MyDataBase->addRowToModel(model, tableName)) {
        ui->tvDbData->setCurrentIndex(model->index(0, 0)); // 选中新行
        QMessageBox::information(this, "提示", "添加数据后，请点击保存数据按钮");
    } else {
        QMessageBox::critical(this, "错误", "添加行失败");
    }
}


// 保存数据
void MainWindow::on_btnDbSave_clicked() {
    QSqlTableModel* model = qobject_cast<QSqlTableModel*>(ui->tvDbData->model());
    if (!model) {
        QMessageBox::critical(this, "错误", "表格模型无效");
        return;
    }

    if (MyDataBase->submitModelChanges(model)) {
        QMessageBox::information(this, "成功", "数据已保存");
    } else {
        QMessageBox::critical(this, "失败", "保存失败：" + model->lastError().text());
    }
}


// 删除数据
void MainWindow::on_btnDbDelete_clicked() {
    QString tableName = ui->cbxDbTables->currentText();
    if (tableName.isEmpty()) {
        QMessageBox::warning(this, "提示", "请选择数据表");
        return;
    }

    QSqlTableModel* model = qobject_cast<QSqlTableModel*>(ui->tvDbData->model());
    if (!model) {
        QMessageBox::critical(this, "错误", "请先刷新表格数据");
        return;
    }

    int id = getSelectedRowId();
    if (id == -1) return;

    if (QMessageBox::question(this, "确认删除", QString("确定要删除ID为%1的记录吗？").arg(id),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    if (MyDataBase->deleteRecordById(tableName, id)) {
        model->select(); // 刷新模型
        QMessageBox::information(this, "成功", "记录已删除");
    } else {
        QMessageBox::critical(this, "失败", "删除记录失败");
    }
}


// 获取当前选中行的ID,这里是利用model查询数据库表头中的ID所在列索引，主界面UI可以不用显示ID
int MainWindow::getSelectedRowId() {
    QSqlTableModel* model = qobject_cast<QSqlTableModel*>(ui->tvDbData->model());
    if (!model) {
        QMessageBox::critical(this, "错误", "表格模型无效");
        return -1;
    }

    //selectedRows() 获取的是选中行的模型索引,这个索引指向model中的一整行数据不是仅指向 UI 显示的行
    QModelIndexList selection = ui->tvDbData->selectionModel()->selectedRows();
    if (selection.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先选中一行数据");
        return -1;
    }

    // 获取ID列的索引
    int idCol = model->fieldIndex("ID");
    if (idCol == -1) {
        QMessageBox::critical(this, "错误", "未找到ID字段");
        return -1;
    }

    // 获取选中行的ID值
    QModelIndex index = selection.first().siblingAtColumn(idCol);
    bool ok;
    int id = index.data().toInt(&ok);
    if (!ok) {
        QMessageBox::critical(this, "错误", "获取ID失败，数据格式不正确");
        return -1;
    }
    return id;
}


void MainWindow::on_btnGenerateChart_clicked() {
    chartUpdateTimer->disconnect();

    if(!MyDataBase->getDatabase().isOpen()){
        on_btnDbConnect_clicked();
    }

    if(ui->cbxShowType->currentText() == "系统数据统计"){
        connect(chartUpdateTimer, &QTimer::timeout, this, [=]() {//0.3s重绘折线图
            MyDataBase->generateSensorChart(ui->cvData,ui->cbxSensorType->currentText(),this);});
    }
    else{
        connect(chartUpdateTimer, &QTimer::timeout, this, [=]() {//0.3s重绘柱状图
            MyDataBase->generateAverageDurationChart(ui->cvData,this);});
    }

    chartUpdateTimer->start(300);
}




void MainWindow::on_btn_Motor78Forward_2_clicked()
{
    uint32 a;
    zm->GetIOInput(128,a);
    QString b = QString("IO端口128的输入值：%1").arg(a, 0, 10);
    QMessageBox::critical(this, "提示", b);
}


void MainWindow::on_btn_Motor78Forward_3_clicked()
{
    zm->SetIOOutput(144,0);
}


void MainWindow::on_btn_Motor78Forward_4_clicked()
{
    if(ui->btn_Motor78Forward_4->text() == "显示电池数据"){
        emit Sig_OpenPort_Battery("COM33");
        QTimer::singleShot(100, this, [=]() {
            if(m_battery->isOpen()){
                ui->btn_Motor78Forward_4->setText("关闭电池数据");
                emit Sig_StartAutoSend_Battery(3000);
            }
        });
    }
    else{
        ui->btn_Motor78Forward_4->setText("显示电池数据");
        emit Sig_StopAutoSend_Battery();
        emit Sig_ClosePort_Battery();
    }

}



//取盖板调试
void MainWindow::on_btn_Motor78Forward_10_pressed()
{
    zm->SingleVMove(4,Forward);
    zm->SingleVMove(5,Forward);
}


void MainWindow::on_btn_Motor78Forward_10_released()
{
    zm->SingleVMove(4,Cancel);
    zm->SingleVMove(5,Cancel);
}


void MainWindow::on_btn_Motor78Forward_9_pressed()
{
    zm->SingleVMove(4,Backward);
    zm->SingleVMove(5,Backward);
}


void MainWindow::on_btn_Motor78Forward_9_released()
{
    zm->SingleVMove(4,Cancel);
    zm->SingleVMove(5,Cancel);
}


void MainWindow::on_btn_Motor78Forward_5_pressed()
{
    zm->SingleVMove(0,Forward);
    zm->SingleVMove(1,Forward);
}


void MainWindow::on_btn_Motor78Forward_5_released()
{
    zm->SingleVMove(0,Cancel);
    zm->SingleVMove(1,Cancel);
}


void MainWindow::on_btn_Motor78Forward_6_pressed()
{
    zm->SingleVMove(0,Backward);
    zm->SingleVMove(1,Backward);
}


void MainWindow::on_btn_Motor78Forward_6_released()
{
    zm->SingleVMove(0,Cancel);
    zm->SingleVMove(1,Cancel);
}


void MainWindow::on_btn_Motor78Forward_7_pressed()
{
    zm->SingleVMove(2,Forward);
    zm->SingleVMove(3,Forward);
}


void MainWindow::on_btn_Motor78Forward_7_released()
{
    zm->SingleVMove(2,Cancel);
    zm->SingleVMove(3,Cancel);
}


void MainWindow::on_btn_Motor78Forward_8_pressed()
{
    zm->SingleVMove(2,Backward);
    zm->SingleVMove(3,Backward);
}


void MainWindow::on_btn_Motor78Forward_8_released()
{
    zm->SingleVMove(2,Cancel);
    zm->SingleVMove(3,Cancel);
}


void MainWindow::on_btn_Motor78Forward_11_pressed()
{
    zm->SingleVMove(0,Backward);
}


void MainWindow::on_btn_Motor78Forward_11_released()
{
    zm->SingleVMove(0,Cancel);
}


void MainWindow::on_btn_Motor78Forward_12_pressed()
{
    zm->SingleVMove(0,Forward);
}


void MainWindow::on_btn_Motor78Forward_12_released()
{
    zm->SingleVMove(0,Cancel);
}


void MainWindow::on_btn_Motor78Forward_13_pressed()
{
    zm->SingleVMove(2,Forward);
}


void MainWindow::on_btn_Motor78Forward_13_released()
{
    zm->SingleVMove(2,Cancel);
}


void MainWindow::on_btn_Motor78Forward_14_pressed()
{
    zm->SingleVMove(2,Backward);
}


void MainWindow::on_btn_Motor78Forward_14_released()
{
    zm->SingleVMove(2,Cancel);
}


void MainWindow::on_btn_Motor78Forward_15_pressed()
{
    zm->SingleVMove(6,Forward);
}


void MainWindow::on_btn_Motor78Forward_15_released()
{
    zm->SingleVMove(6,Cancel);
}


void MainWindow::on_btn_Motor78Forward_16_pressed()
{
    zm->SingleVMove(6,Backward);
}


void MainWindow::on_btn_Motor78Forward_16_released()
{
    zm->SingleVMove(6,Cancel);
}


void MainWindow::on_btn_Motor78Forward_17_clicked()
{
    for (int i = 0; i < 16; i++){
        emit UpdateUI(QString::number(statusIOInPut_Zmotion[i]));
    }
}




void MainWindow::on_btn_PlatformLeveling_clicked()
{
    UpdateUI("六自由度平台正在调平！");
    m_lift->m_SixDof->startLeveling();
}


void MainWindow::on_btnTest_clicked()
{
    for(int i = 0;i < 60;i++){
      MyDataBase->logSensorData("称重传感器-0", i);
    }
}


void MainWindow::on_btn_auto_descent_clicked()
{
    if (!m_isLRUTypeSelected) {
        emit UpdateUI("未选择LRU类型");
        return;
    }

    QThread* startThread = new QThread;
    QObject::connect(startThread, &QThread::started, [this,startThread]() {
        m_lift->autodescent = true;
        m_lift->auto_descent();
        startThread->quit();
    });
    // 连接 finished 信号到 deleteLater 槽，确保线程执行完毕后释放资源
    QObject::connect(startThread, &QThread::finished, startThread, &QObject::deleteLater);

    // 启动线程
    startThread->start();
}

void MainWindow::on_btn_IOForward_pressed()
{
    zm->SetIOOutput(6,1);
}


void MainWindow::on_btn_IOForward_released()
{
    zm->SetIOOutput(6,0);
}


void MainWindow::on_btn_IOBackward_pressed()
{
    zm->SetIOOutput(7,1);
}


void MainWindow::on_btn_IOBackward_released()
{
    zm->SetIOOutput(7,0);
}

// void MainWindow::on_btn_IOForward_clicked()
// {
//     if(true){
//         zm->SetIOOutput(6,1);
//         QTimer::singleShot(4800, this, [this](){
//             zm->SetIOOutput(6,0);
//             IsPinRelease = true;
//             IsPinShrink = false;
//         });
//     }
// }


// void MainWindow::on_btn_IOBackward_clicked()
// {
//     if(true){
//         zm->SetIOOutput(7,1);
//         QTimer::singleShot(5500, this, [this](){
//             zm->SetIOOutput(7,0);
//             IsPinRelease = false;
//             IsPinShrink = true;
//         });
//     }
// }


void MainWindow::on_btn_FindCamera_clicked()
{
    int cameraIndexes[] = {0, 1, 2, 3};
    for (int i = 0; i < 4; ++i) {
        if (m_workerProcesses[i]) {
            m_workerProcesses[i]->terminate();
            m_workerProcesses[i]->waitForFinished(1000);
            delete m_workerProcesses[i];
        }

        // 启动新子进程
        m_workerProcesses[i] = new QProcess(this);
        // 子进程程序路径
        QString workerPath = QCoreApplication::applicationDirPath() + "/camera_worker";
        // 传递参数，相机索引、共享内存Key
        QStringList args;
        args << QString::number(cameraIndexes[i]) << m_shmKeys[i];

        // 捕获标准输出，Info、Debug
        connect(m_workerProcesses[i], &QProcess::readyReadStandardOutput, this, [=]() {
            QByteArray rawData = m_workerProcesses[i]->readAllStandardOutput();
            QString log = QString::fromLocal8Bit(rawData).trimmed(); // 处理中文编码
            if (!log.isEmpty()) {
                QString uiMsg = QString("【相机%1】%2").arg(i).arg(log);
                emit UpdateUI(uiMsg);
            }
        });

        // 捕获错误，Warning、Critical
        connect(m_workerProcesses[i], &QProcess::readyReadStandardError, this, [=]() {
            QByteArray rawData = m_workerProcesses[i]->readAllStandardError();
            QString errLog = QString::fromLocal8Bit(rawData).trimmed();
            if (!errLog.isEmpty()) {
                QString uiMsg = QString("【相机%1错误】%2").arg(i).arg(errLog);
                emit UpdateUI(uiMsg);
            }
        });

        // 监听子进程退出
        connect(m_workerProcesses[i], QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [=](int exitCode, QProcess::ExitStatus exitStatus) {
                    QString status = (exitStatus == QProcess::NormalExit) ? "正常退出" : "异常崩溃";
                    QString uiMsg = QString("【相机%1】子进程%2，退出码：%3").arg(i).arg(status).arg(exitCode);
                    emit UpdateUI(uiMsg);
                });

        m_workerProcesses[i]->start(workerPath, args);

        // 子进程启动状态
        if (!m_workerProcesses[i]->waitForStarted(1000)) {
            QString info = QString("相机%1子进程启动失败:%2").arg(i).arg(m_workerProcesses[i]->errorString());
            emit UpdateUI(info);
            delete m_workerProcesses[i];
            m_workerProcesses[i] = nullptr;
        } else {
            QString info2  = QString("相机%1子进程启动成功").arg(i);
            emit UpdateUI(info2);
        }
    }
}

void MainWindow::on_btn_CameraMonitor_clicked()
{
    if (ui->btn_CameraMonitor->text() == "视觉监测") {
        cameraPreviewDialog = new QDialog(this);
        cameraPreviewDialog->setWindowTitle("UVC相机监测");
        cameraPreviewDialog->resize(960, 720);
        cameraPreviewDialog->setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowMaximizeButtonHint);

        m_btnAddCameraView = new QPushButton("添加相机画面", cameraPreviewDialog);
        m_cameraGridLayout = new QGridLayout();
        m_cameraGridLayout->setContentsMargins(0, 0, 0, 0);
        m_cameraGridLayout->setSpacing(4);

        QVBoxLayout* layout = new QVBoxLayout(cameraPreviewDialog);
        layout->addWidget(m_btnAddCameraView);
        layout->addLayout(m_cameraGridLayout);
        layout->setContentsMargins(4, 4, 4, 4);
        cameraPreviewDialog->setLayout(layout);

        connect(m_btnAddCameraView, &QPushButton::clicked, this, &MainWindow::addCameraMonitorView);
        connect(cameraPreviewDialog, &QDialog::finished, this, [this]() {
            closeCameraMonitor();
        });

        addCameraMonitorView();
        if (m_monitorCameras.isEmpty()) {
            closeCameraMonitor();
            return;
        }

        cameraPreviewDialog->show();
        ui->btn_CameraMonitor->setText("停止监测");
    } else {
        closeCameraMonitor();
    }
}

bool MainWindow::selectMonitorCamera(int &cameraIndex, QString &cameraName)
{
    QList<QCameraInfo> cameras = UVC_Device->enumUvcCameras();
    if (cameras.isEmpty()) {
        emit UpdateUI("未检索到UVC相机");
        return false;
    }

    QStringList cameraItems;
    QList<int> cameraIndexes;
    for (int i = 0; i < cameras.size(); ++i) {
        QString name = cameras[i].description().isEmpty()
        ? cameras[i].deviceName()
        : cameras[i].description();
        QString deviceName = cameras[i].deviceName();
        if (name.contains("Daheng", Qt::CaseInsensitive)
            || name.contains("大恒", Qt::CaseInsensitive)
            || deviceName.contains("Daheng", Qt::CaseInsensitive)
            || deviceName.contains("大恒", Qt::CaseInsensitive)) {
            emit UpdateUI(QString("已隐藏不适用于视觉监测的工业相机：%1").arg(name));
            continue;
        }
        if (m_openedCameraIndexes.contains(i)) {
            continue;
        }
        cameraItems << QString("%1：%2").arg(i).arg(name);
        cameraIndexes << i;
    }

    if (cameraItems.isEmpty()) {
        emit UpdateUI("没有可继续添加的UVC相机");
        return false;
    }

    QInputDialog dialog(this);
    dialog.setWindowTitle("选择监控画面");
    dialog.setLabelText("请选择要打开的摄像头：");
    dialog.setComboBoxItems(cameraItems);
    dialog.setComboBoxEditable(false);
    dialog.setTextValue(cameraItems.first());
    dialog.setMinimumSize(560, 220);
    dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    QString selectedCamera = dialog.textValue();
    int selectedIndex = cameraItems.indexOf(selectedCamera);
    if (selectedIndex < 0 || selectedIndex >= cameraIndexes.size()) {
        emit UpdateUI("选择的摄像头无效");
        return false;
    }

    cameraIndex = cameraIndexes[selectedIndex];
    cameraName = selectedCamera;
    return true;
}

void MainWindow::addCameraMonitorView()
{
    if (m_monitorCameras.size() >= 4) {
        emit UpdateUI("最多支持4个相机画面");
        if (m_btnAddCameraView) {
            m_btnAddCameraView->setEnabled(false);
        }
        return;
    }

    int cameraIndex = -1;
    QString cameraName;
    if (!selectMonitorCamera(cameraIndex, cameraName)) {
        return;
    }

    UVC_Camera* camera = new UVC_Camera(this);
    connect(camera, &UVC_Camera::UVCInfo, this, &MainWindow::UpdateUILog);
    if (!camera->openCamera(cameraIndex)) {
        delete camera;
        return;
    }

    QCameraViewfinder* viewfinder = new QCameraViewfinder(cameraPreviewDialog);
    viewfinder->setMinimumSize(320, 240);
    if (!camera->startPreview(viewfinder)) {
        camera->closeCamera();
        delete viewfinder;
        delete camera;
        return;
    }

    m_monitorCameras.append(camera);
    m_monitorViewfinders.append(viewfinder);
    m_openedCameraIndexes.insert(cameraIndex);
    refreshCameraMonitorLayout();

    if (m_btnAddCameraView) {
        m_btnAddCameraView->setEnabled(m_monitorCameras.size() < 4);
    }
    emit UpdateUI(QString("已添加视觉监测画面：%1").arg(cameraName));
}

void MainWindow::refreshCameraMonitorLayout()
{
    if (!m_cameraGridLayout) {
        return;
    }

    while (QLayoutItem* item = m_cameraGridLayout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->setParent(cameraPreviewDialog);
        }
        delete item;
    }

    for (int i = 0; i < m_monitorViewfinders.size(); ++i) {
        int row = 0;
        int col = 0;
        int rowSpan = 1;
        int colSpan = 1;

        if (m_monitorViewfinders.size() == 1) {
            rowSpan = 2;
            colSpan = 2;
        } else if (m_monitorViewfinders.size() == 2) {
            col = i;
            rowSpan = 2;
        } else {
            row = i / 2;
            col = i % 2;
        }

        m_cameraGridLayout->addWidget(m_monitorViewfinders[i], row, col, rowSpan, colSpan);
    }
}

void MainWindow::closeCameraMonitor()
{
    for (UVC_Camera* camera : m_monitorCameras) {
        if (camera) {
            camera->closeCamera();
            delete camera;
        }
    }
    m_monitorCameras.clear();
    m_monitorViewfinders.clear();
    m_openedCameraIndexes.clear();
    m_cameraGridLayout = nullptr;
    m_btnAddCameraView = nullptr;

    if (cameraPreviewDialog) {
        disconnect(cameraPreviewDialog, nullptr, this, nullptr);
        cameraPreviewDialog->close();
        delete cameraPreviewDialog;
        cameraPreviewDialog = nullptr;
    }

    ui->btn_CameraMonitor->setText("视觉监测");
}

void MainWindow::updateCameraPreviews()
{
    // 从共享内存读取图像
    for (int i = 0; i < 4; ++i) {
        if (!m_previewLabels[i] || !m_shmManagers[i]) continue;

        QImage frame = m_shmManagers[i]->readImage();
        if (!frame.isNull()) {
            // 更新UI
            m_previewLabels[i]->setPixmap(QPixmap::fromImage(frame).scaled(
                640, 480, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
}

// PMC 电机命令执行结果
void MainWindow::checkResult(PUSIResult res, const QString& tip)
{
    if(res == RET_SUCCESS){
        emit UpdateUI(tip + "成功！");
    }else{
        emit UpdateUI(tip + "错误码：" + QString::number(res));
    }
}


void MainWindow::on_btn_PMCConnect_clicked()
{
    if(ui->btn_LightConnect->text() == "连接灯箱电机"){
        // 设置驱动器类型
        SetPlatformType(PMC006B4);
        // 打开串口
        PUSIResult res = OpenPort(32,9600);
        checkResult(res, "PMC电机连接");
        ui->btn_LightConnect->setText("断开灯箱电机");
    }
    else{
        ClosePort();
        ui->btn_LightConnect->setText("连接灯箱电机");
    }
}



//释放销子
void MainWindow::on_btn_PMCSpeedForward_pressed()
{
    SetDirection(10, TRUE);
    SetVelocity(10, 3000);
    SetStartVelocity(10,3000);
    SetStopVelocity(10,3000);
    PUSIResult res = SetVelocityMode(10, TRUE);
    checkResult(res, "左电机正转");

    SetDirection(20, TRUE);
    SetVelocity(20, 3000);
    SetStartVelocity(20,3000);
    SetStopVelocity(20,3000);
    PUSIResult res1 = SetVelocityMode(20, TRUE);
    checkResult(res1, "右电机正转");
}

void MainWindow::on_btn_PMCSpeedForward_released()
{
    AbortStep(10);
    AbortStep(20);
}


//收缩销子
void MainWindow::on_btn_PMCSpeedBackward_pressed()
{
    SetDirection(10, FALSE);
    SetVelocity(10, 3000);
    SetStartVelocity(10,3000);
    SetStopVelocity(10,3000);
    PUSIResult res = SetVelocityMode(10, TRUE);
    checkResult(res, "左电机反转");

    SetDirection(20, FALSE);
    SetVelocity(20, 3000);
    SetStartVelocity(20,3000);
    SetStopVelocity(20,3000);
    PUSIResult res1 = SetVelocityMode(20, TRUE);
    checkResult(res1, "右电机反转");
}

void MainWindow::on_btn_PMCSpeedBackward_released()
{
    AbortStep(10);
    AbortStep(20);
}

void MainWindow::on_btn_PMCGetPos_clicked()
{
    int curPos = 0;
    GetMotorPosition(10, &curPos);
    emit UpdateUI(QString("左侧电机位置：%1").arg(curPos));

    int curPos2 = 0;
    GetMotorPosition(20, &curPos2);
    emit UpdateUI(QString("右侧电机位置：%1").arg(curPos2));
}


void MainWindow::on_btn_PMCSetPos_clicked()
{
    PUSIResult res = SetMotorPosition(10, 0);
    PUSIResult res1 = SetMotorPosition(20, 0);
    checkResult(res, "左电机标零");
    checkResult(res1, "右电机标零");
}


//收缩销子即电机步进到指定位置
void MainWindow::on_btn_LightClamp_clicked()
{
    // if(!isLightClamp){
        SetVelocityMode(10, FALSE);
        SetVelocity(10,3000);
        SetStartVelocity(10,3000);
        SetStopVelocity(10,3000);
        SetDirection(10, FALSE);
        StartStep(10, 70000);

        SetVelocityMode(20, FALSE);
        SetVelocity(20,3000);
        SetStartVelocity(20,3000);
        SetStopVelocity(20,3000);
        SetDirection(20, FALSE);
        StartStep(20, 70000);
        emit UpdateUI("正在收缩销子!");
        isLightClamp = true;
        isLightRelease = false;
    // }
}


//释放销子即电机反向回零
void MainWindow::on_btn_LightRelease_clicked()
{
    // if(!isLightRelease){
        SetVelocityMode(10, FALSE);
        SetVelocity(10,3000);
        SetStartVelocity(10,3000);
        SetStopVelocity(10,3000);
        SetDirection(10, TRUE);
        StartStep(10, 70000);

        SetVelocityMode(20, FALSE);
        SetVelocity(20,3000);
        SetStartVelocity(20,3000);
        SetStopVelocity(20,3000);
        SetDirection(20, TRUE);
        StartStep(20, 70000);
        emit UpdateUI("正在释放销子!");
        isLightRelease = true;
        isLightClamp = false;
    // }
}


void MainWindow::on_btn_LightGuDing_clicked()
{

}


void MainWindow::on_btn_LightSongKai_clicked()
{

}


void MainWindow::on_btn_LightGuDing_pressed()
{
    zm->SetIOOutput(11,1);
}


void MainWindow::on_btn_LightGuDing_released()
{
    zm->SetIOOutput(11,0);
}


void MainWindow::on_btn_LightSongKai_pressed()
{
    zm->SetIOOutput(13,1);
}


void MainWindow::on_btn_LightSongKai_released()
{
    zm->SetIOOutput(13,0);
}


void MainWindow::on_btn_PMCSpeedBackward_2_clicked()
{
    float a = zm->GetAxisSpeed(12);
    emit UpdateUI(QString::number(a));
}



void MainWindow::on_radioButton_middlePlate_clicked()
{

}


void MainWindow::on_radioButton_middlePlate_clicked(bool checked)
{

}

void MainWindow::onAutoLiftFinalSixDofCoordinates(float x,float y)
{
    // 注意：这里运行在主线程，可以安全更新 UI
    ui->final_x->setText(QString::number(x,  'f', 2));
    ui->final_y->setText(QString::number(y,  'f', 2));
}

void MainWindow::onEmergencyStopPressed()
{
    QMessageBox::critical(
        this,
        "急停提示",
        "急停按钮被按下,设备停止运行,请关闭设备电源然后松开急停，最后重启设备");
}




void MainWindow::on_btnEnableMainLiftAxis_clicked()
{
    if(!zm->GetConnectStatus()){
        emit UpdateUI("未连接控制器!");
        return;
    }
    // 先使能电机
    int result = zm->SetSingleAxisEnable(4, true);
    if(result != 0) {
        emit UpdateUI(QString("一级升降电机4使能失败，错误码：%1").arg(result));
        return;
    }
    //emit UpdateUI("已使能一级升降电机4");
    //延时等待驱动器稳定
    QThread::msleep(300);
    //松开抱闸（上电释放刹车）
    zm->SetIOOutput(10, 1);
    emit UpdateUI("已使能一级升降电机4");
}



void MainWindow::on_btnDisableMainLiftAxis_clicked()
{
    if(!zm->GetConnectStatus()){
        emit UpdateUI("未连接控制器!");
        return;
    }
    zm->SingleVMove(4, Cancel);   // 停止
    zm->SetIOOutput(10, 0);       // 下电抱死
    QThread::msleep(300);
    // 调用单轴断使能函数
    int err = zm->SetSingleAxisEnable(4, false);
    if(err == 0)
        emit UpdateUI("一级升降电机4已成功断开使能");
    else
        emit UpdateUI(QString("断开一级升降电机4使能失败，错误码: %1").arg(err));
}


void MainWindow::on_btn_CheckDetach_2_clicked()
{
    emit UpdateUI(iniRead->fileName());
}

//UI显示工装电机位置
void MainWindow::updateMotorPositionLabel(quint8 stationAddr, double position)
{
    if (ui->btn_PianjiaConnect->text() == "连接片架电机") {
        return;
    }

    switch (stationAddr) {
    case 0x01:
        ui->label_69->setText(QString::number(position, 'f', 3));
        break;
    case 0x02:
        ui->label_78->setText(QString::number(position, 'f', 3));
        break;
    case 0x03:
        ui->label_81->setText(QString::number(position, 'f', 3));
        break;
    case 0x04:
        ui->label_82->setText(QString::number(position, 'f', 3));
        break;
    default:
        break;
    }
}


void MainWindow::updateLightMotorPositionLabel(quint8 stationAddr,double position)
{
    // 灯箱电机未连接时不更新
    if (ui->btn_LightConnect->text() == "连接灯箱电机") {
        return;
    }

    switch (stationAddr) {
    case 0x01:
        ui->label_91->setText(QString::number(position, 'f', 3));
        break;

    case 0x02:
        ui->label_92->setText(QString::number(position, 'f', 3));
        break;

    default:
        break;
    }
}


void MainWindow::onFixtureMotorFault(quint8 stationAddr,bool fault,const QString &reason)
{
    // 片架电机未连接，不更新片架状态灯
    if (m_fixtureGroup == 0 &&
        ui->btn_PianjiaConnect->text() == "连接片架电机") {
        return;
    }

    // 灯箱电机未连接，不更新灯箱状态灯
    if (m_fixtureGroup == 1 &&
        ui->btn_LightConnect->text() == "连接灯箱电机") {
        return;
    }

    QLabel *label = nullptr;

    if (m_fixtureGroup == 0) {
        switch (stationAddr) {
        case 0x01: label = ui->lblAxisStatusColorPj1; break;
        case 0x02: label = ui->lblAxisStatusColorPj2; break;
        case 0x03: label = ui->lblAxisStatusColorPj3; break;
        case 0x04: label = ui->lblAxisStatusColorPj4; break;
        default: break;
        }
    } else {
        switch (stationAddr) {
        case 0x01: label = ui->lblAxisStatusColorLight1; break;
        case 0x02: label = ui->lblAxisStatusColorLight2; break;
        default: break;
        }
    }

    if (!label)
        return;

    label->setStyleSheet(QString(
                             "border-radius:7px;"
                             "border:2px solid black;"
                             "background-color:%1;")
                             .arg(fault ? "red" : "white"));

    label->setToolTip(reason);
}


void MainWindow::on_btn_PianjiaConnect_clicked()
{
    if (ui->btn_PianjiaConnect->text() == "连接片架电机") {

        // 从片架串口下拉框读取当前串口号
        const QString portName =ui->comboBoxPortPJ->currentText().trimmed();

        if (portName.isEmpty()) {
            emit UpdateUI("请选择片架电机串口！");
            return;
        }

        bool opened = false;

        // 在电机线程里打开串口
        QMetaObject::invokeMethod(motor, [this, portName, &opened]() {
            opened = motor->openSerialPort(portName, 115200);
        }, Qt::BlockingQueuedConnection);

        if (opened) {

            m_fixtureGroup = 0;
            m_currentReadIndex2 = 0;

            disconnect(SBtimer, nullptr, this, nullptr);

            connect(SBtimer, &QTimer::timeout, this, [this]() {
                if (m_motorAddrList2.isEmpty())
                    return;

                QMetaObject::invokeMethod(motor, [this]() {
                    if (!motor->isSerialOpen() ||
                        motor->hasPendingCommand()) {
                        return;
                    }

                    quint8 stationAddr = m_motorAddrList2[m_currentReadIndex2];

                    motor->readMotorPosition(stationAddr);
                    motor->readMotorAlarm(stationAddr);

                    m_currentReadIndex2++;
                    if (m_currentReadIndex2 >= m_motorAddrList2.size())
                        m_currentReadIndex2 = 0;
                }, Qt::QueuedConnection);
            });

            SBtimer->start(100);

            ui->btn_PianjiaConnect->setText("断开片架电机");
        }
    }
    else {

        SBtimer->stop();

        QMetaObject::invokeMethod(motor, [=]() {
            motor->closeSerialPort();
        }, Qt::BlockingQueuedConnection);

        ui->btn_PianjiaConnect->setText("连接片架电机");
    }
}




void MainWindow::on_btn_EnablePianjia_clicked()
{
    if(ui->btn_EnablePianjia->text() == "使能片架电机"){
        double speed = 60;
        double acc = 900;
        double dec = 900;
        double AcctimeMs_double = (speed / acc) * 1000.0;
        double DectimeMs_double = (speed / dec) * 1000.0;
        quint16 AcctimeMs = (quint16)AcctimeMs_double;
        quint16 DectimeMs = (quint16)DectimeMs_double;

        QMetaObject::invokeMethod(motor, [=]() {
            motor->motorEnable(0x01);
            motor->motorEnable(0x02);
            motor->motorEnable(0x03);
            motor->motorEnable(0x04);

            motor->setMotorSpeed(0x01,speed);
            motor->setMotorSpeed(0x02,speed);
            motor->setMotorSpeed(0x03,speed);
            motor->setMotorSpeed(0x04,speed);

            motor->setAccelerationTime(0x01,AcctimeMs);
            motor->setAccelerationTime(0x02,AcctimeMs);
            motor->setAccelerationTime(0x03,AcctimeMs);
            motor->setAccelerationTime(0x04,AcctimeMs);

            motor->setDecelerationTime(0x01,DectimeMs);
            motor->setDecelerationTime(0x02,DectimeMs);
            motor->setDecelerationTime(0x03,DectimeMs);
            motor->setDecelerationTime(0x04,DectimeMs);
        }, Qt::QueuedConnection);

        ui->btn_EnablePianjia->setText("断开使能片架电机");
    }
    else
    {
        QMetaObject::invokeMethod(motor, [=]() {
            motor->motorDisable(0x01);
            motor->motorDisable(0x02);
            motor->motorDisable(0x03);
            motor->motorDisable(0x04);
        }, Qt::QueuedConnection);

        ui->btn_EnablePianjia->setText("使能片架电机");
    }
}


void MainWindow::on_btn_IOForward_clicked()
{
    double val = 0;
    int pos = val * 4000;
    // 暂停位置轮询，避免查询命令插入运动命令之间
    SBtimer->stop();

    QMetaObject::invokeMethod(motor, [=]() {
        motor->runToAbsolutePosition(0x01,pos);
        motor->runToAbsolutePosition(0x02,pos);
        motor->runToAbsolutePosition(0x03,pos);
        motor->runToAbsolutePosition(0x04,pos);
    }, Qt::QueuedConnection);
    emit UpdateUI("正在释放片架销子!");

    // 等运动命令发送完成后恢复位置轮询
    QTimer::singleShot(1000, this, [this]() {
        if (motor->isSerialOpen()) {
            SBtimer->start(100);
        }
    });
}


void MainWindow::on_btn_IOBackward_clicked()
{
    double val = 20;
    int pos = val * 4000;

    SBtimer->stop();

    QMetaObject::invokeMethod(motor, [=]() {
        motor->runToAbsolutePosition(0x01,pos);
        motor->runToAbsolutePosition(0x02,pos);
        motor->runToAbsolutePosition(0x03,pos);
        motor->runToAbsolutePosition(0x04,pos);
    }, Qt::QueuedConnection);
    emit UpdateUI("正在收缩片架销子!");

    QTimer::singleShot(1000, this, [this]() {
        if (motor->isSerialOpen()) {
            SBtimer->start(100);
        }
    });
}


void MainWindow::on_btn_LightConnect_clicked()
{
    if (ui->btn_LightConnect->text() == "连接灯箱电机") {

        const QString portName = ui->comboBoxPortLight->currentText().trimmed();

        if (portName.isEmpty()) {
            emit UpdateUI("请选择灯箱电机串口！");
            return;
        }

        bool opened = false;

        QMetaObject::invokeMethod(motor, [this, portName, &opened]() {
            opened = motor->openSerialPort(portName, 115200);
        }, Qt::BlockingQueuedConnection);

        if (opened) {

            m_fixtureGroup = 1;
            m_currentReadIndex = 0;

            disconnect(SBtimer, nullptr, this, nullptr);

            connect(SBtimer, &QTimer::timeout, this, [this]() {
                if (m_motorAddrList.isEmpty())
                    return;

                QMetaObject::invokeMethod(motor, [this]() {
                    if (!motor->isSerialOpen() ||
                        motor->hasPendingCommand()) {
                        return;
                    }

                    quint8 stationAddr = m_motorAddrList[m_currentReadIndex];

                    motor->readMotorPosition(stationAddr);
                    motor->readMotorAlarm(stationAddr);

                    m_currentReadIndex++;
                    if (m_currentReadIndex >= m_motorAddrList.size())
                        m_currentReadIndex = 0;
                }, Qt::QueuedConnection);
            });

            SBtimer->start(100);

            ui->btn_LightConnect->setText("断开灯箱电机");
        }
    }
    else {

        SBtimer->stop();

        QMetaObject::invokeMethod(motor, [=]() {
            motor->closeSerialPort();
        }, Qt::BlockingQueuedConnection);

        ui->btn_LightConnect->setText("连接灯箱电机");
    }
}


void MainWindow::on_btn_EnableLight_clicked()
{
    if(ui->btn_EnableLight->text() == "使能灯箱电机"){
        double speed = 60;
        double acc = 900;
        double dec = 900;
        double AcctimeMs_double = (speed / acc) * 1000.0;
        double DectimeMs_double = (speed / dec) * 1000.0;
        quint16 AcctimeMs = (quint16)AcctimeMs_double;
        quint16 DectimeMs = (quint16)DectimeMs_double;

        QMetaObject::invokeMethod(motor, [=]() {
            motor->motorEnable(0x01);
            motor->motorEnable(0x02);

            motor->setMotorSpeed(0x01,speed);
            motor->setMotorSpeed(0x02,speed);

            motor->setAccelerationTime(0x01,AcctimeMs);
            motor->setAccelerationTime(0x02,AcctimeMs);

            motor->setDecelerationTime(0x01,DectimeMs);
            motor->setDecelerationTime(0x02,DectimeMs);
        }, Qt::QueuedConnection);

        ui->btn_EnableLight->setText("断开使能灯箱电机");
    }
    else
    {
        QMetaObject::invokeMethod(motor, [=]() {
            motor->motorDisable(0x01);
            motor->motorDisable(0x02);
        }, Qt::QueuedConnection);

        ui->btn_EnableLight->setText("使能灯箱电机");
    }
}


void MainWindow::on_btn_LightSpeedBackward_clicked()
{
    double val = 20;
    int pos = val * 4000 ;
    QMetaObject::invokeMethod(motor, [=]() {
        motor->runToAbsolutePosition(0x01,pos);
        motor->runToAbsolutePosition(0x02,pos);
    }, Qt::QueuedConnection);
    emit UpdateUI("正在收缩灯箱销子!");
}


void MainWindow::on_btn_LightSpeedForward_clicked()
{
    double val = 0;
    int pos = val * 4000 ;
    QMetaObject::invokeMethod(motor, [=]() {
        motor->runToAbsolutePosition(0x01,pos);
        motor->runToAbsolutePosition(0x02,pos);
    }, Qt::QueuedConnection);
    emit UpdateUI("正在释放灯箱销子!");
}

void MainWindow::on_btn_zhengzhuan_1_pressed()
{
    if (ui->btn_PianjiaConnect->text() == "连接片架电机") {
        emit UpdateUI("请先连接片架电机！");
        return;
    }

    QMetaObject::invokeMethod(motor, [this]() {
        if (!motor->isSerialOpen()) {
            return;
        }

        motor->motorRunCWByStation(0x01);
    }, Qt::QueuedConnection);
}


void MainWindow::on_btn_zhengzhuan_1_released()
{
    QMetaObject::invokeMethod(motor, [this]() {
        if (!motor->isSerialOpen()) {
            return;
        }

        motor->motorStop(0x01);
    }, Qt::QueuedConnection);
}


void MainWindow::on_btn_fanzhuan_1_pressed()
{
    if (ui->btn_PianjiaConnect->text() == "连接片架电机") {
        emit UpdateUI("请先连接片架电机！");
        return;
    }

    QMetaObject::invokeMethod(motor, [this]() {
        if (!motor->isSerialOpen()) {
            return;
        }

        motor->motorRunCCWByStation(0x01);
    }, Qt::QueuedConnection);
}


void MainWindow::on_btn_fanzhuan_1_released()
{
    QMetaObject::invokeMethod(motor, [this]() {
        if (!motor->isSerialOpen()) {
            return;
        }

        motor->motorStop(0x01);
    }, Qt::QueuedConnection);
}


void MainWindow::on_btn_zhengzhuan_2_pressed()
{
    if (ui->btn_PianjiaConnect->text() == "连接片架电机") {
        emit UpdateUI("请先连接片架电机！");
        return;
    }

    QMetaObject::invokeMethod(motor, [this]() {
        if (!motor->isSerialOpen()) {
            return;
        }

        motor->motorRunCWByStation(0x02);
    }, Qt::QueuedConnection);
}


void MainWindow::on_btn_zhengzhuan_2_released()
{
    QMetaObject::invokeMethod(motor, [this]() {
        if (!motor->isSerialOpen()) {
            return;
        }

        motor->motorStop(0x02);
    }, Qt::QueuedConnection);
}


void MainWindow::on_btn_fanzhuan_2_pressed()
{
    if (ui->btn_PianjiaConnect->text() == "连接片架电机") {
        emit UpdateUI("请先连接片架电机！");
        return;
    }

    QMetaObject::invokeMethod(motor, [this]() {
        if (!motor->isSerialOpen()) {
            return;
        }

        motor->motorRunCCWByStation(0x02);
    }, Qt::QueuedConnection);
}


void MainWindow::on_btn_fanzhuan_2_released()
{
    QMetaObject::invokeMethod(motor, [this]() {
        if (!motor->isSerialOpen()) {
            return;
        }

        motor->motorStop(0x02);
    }, Qt::QueuedConnection);
}


void MainWindow::on_btn_zhengzhuan_3_pressed()
{
    if (ui->btn_PianjiaConnect->text() == "连接片架电机") {
        emit UpdateUI("请先连接片架电机！");
        return;
    }

    QMetaObject::invokeMethod(motor, [this]() {
        if (!motor->isSerialOpen()) {
            return;
        }

        motor->motorRunCWByStation(0x03);
    }, Qt::QueuedConnection);
}


void MainWindow::on_btn_zhengzhuan_3_released()
{
    QMetaObject::invokeMethod(motor, [this]() {
        if (!motor->isSerialOpen()) {
            return;
        }

        motor->motorStop(0x03);
    }, Qt::QueuedConnection);
}


void MainWindow::on_btn_fanzhuan_3_pressed()
{
    if (ui->btn_PianjiaConnect->text() == "连接片架电机") {
        emit UpdateUI("请先连接片架电机！");
        return;
    }

    QMetaObject::invokeMethod(motor, [this]() {
        if (!motor->isSerialOpen()) {
            return;
        }

        motor->motorRunCCWByStation(0x03);
    }, Qt::QueuedConnection);
}


void MainWindow::on_btn_fanzhuan_3_released()
{
    QMetaObject::invokeMethod(motor, [this]() {
        if (!motor->isSerialOpen()) {
            return;
        }

        motor->motorStop(0x03);
    }, Qt::QueuedConnection);
}


void MainWindow::on_btn_zhengzhuan_4_pressed()
{
    if (ui->btn_PianjiaConnect->text() == "连接片架电机") {
        emit UpdateUI("请先连接片架电机！");
        return;
    }

    QMetaObject::invokeMethod(motor, [this]() {
        if (!motor->isSerialOpen()) {
            return;
        }

        motor->motorRunCWByStation(0x04);
    }, Qt::QueuedConnection);
}


void MainWindow::on_btn_zhengzhuan_4_released()
{
    QMetaObject::invokeMethod(motor, [this]() {
        if (!motor->isSerialOpen()) {
            return;
        }

        motor->motorStop(0x04);
    }, Qt::QueuedConnection);
}


void MainWindow::on_btn_fanzhuan_4_pressed()
{
    if (ui->btn_PianjiaConnect->text() == "连接片架电机") {
        emit UpdateUI("请先连接片架电机！");
        return;
    }

    QMetaObject::invokeMethod(motor, [this]() {
        if (!motor->isSerialOpen()) {
            return;
        }

        motor->motorRunCCWByStation(0x04);
    }, Qt::QueuedConnection);
}


void MainWindow::on_btn_fanzhuan_4_released()
{
    QMetaObject::invokeMethod(motor, [this]() {
        if (!motor->isSerialOpen()) {
            return;
        }

        motor->motorStop(0x04);
    }, Qt::QueuedConnection);
}


void MainWindow::on_btn_zhengzhuan1_pressed()
{
    if (ui->btn_LightConnect->text() == "连接灯箱电机") {
        emit UpdateUI("请先连接灯箱电机！");
        return;
    }

    QMetaObject::invokeMethod(motor, [this]() {
        if (!motor->isSerialOpen()) {
            return;
        }

        motor->motorRunCWByStation(0x01);
    }, Qt::QueuedConnection);
}


void MainWindow::on_btn_zhengzhuan1_released()
{
    QMetaObject::invokeMethod(motor, [this]() {
        if (!motor->isSerialOpen()) {
            return;
        }

        motor->motorStop(0x01);
    }, Qt::QueuedConnection);
}


void MainWindow::on_btn_fanzhuan1_pressed()
{
    if (ui->btn_LightConnect->text() == "连接灯箱电机") {
        emit UpdateUI("请先连接灯箱电机！");
        return;
    }

    QMetaObject::invokeMethod(motor, [this]() {
        if (!motor->isSerialOpen()) {
            return;
        }

        motor->motorRunCCWByStation(0x01);
    }, Qt::QueuedConnection);
}


void MainWindow::on_btn_fanzhuan1_released()
{
    QMetaObject::invokeMethod(motor, [this]() {
        if (!motor->isSerialOpen()) {
            return;
        }

        motor->motorStop(0x01);
    }, Qt::QueuedConnection);
}


void MainWindow::on_btn_zhengzhuan2_pressed()
{
    if (ui->btn_LightConnect->text() == "连接灯箱电机") {
        emit UpdateUI("请先连接灯箱电机！");
        return;
    }

    QMetaObject::invokeMethod(motor, [this]() {
        if (!motor->isSerialOpen()) {
            return;
        }

        motor->motorRunCWByStation(0x02);
    }, Qt::QueuedConnection);
}


void MainWindow::on_btn_zhengzhuan2_released()
{
    QMetaObject::invokeMethod(motor, [this]() {
        if (!motor->isSerialOpen()) {
            return;
        }

        motor->motorStop(0x02);
    }, Qt::QueuedConnection);
}


void MainWindow::on_btn_fanzhuan2_pressed()
{
    if (ui->btn_LightConnect->text() == "连接灯箱电机") {
        emit UpdateUI("请先连接灯箱电机！");
        return;
    }

    QMetaObject::invokeMethod(motor, [this]() {
        if (!motor->isSerialOpen()) {
            return;
        }

        motor->motorRunCCWByStation(0x02);
    }, Qt::QueuedConnection);
}


void MainWindow::on_btn_fanzhuan2_released()
{
    QMetaObject::invokeMethod(motor, [this]() {
        if (!motor->isSerialOpen()) {
            return;
        }

        motor->motorStop(0x02);
    }, Qt::QueuedConnection);
}

void MainWindow::on_btn_PianjiaZero1_clicked()
{
    if (ui->btn_PianjiaConnect->text() == "连接片架电机") {
        emit UpdateUI("请先连接片架电机！");
        return;
    }

    QMetaObject::invokeMethod(motor, [this]() {
        motor->setMotorAbsolutePosition(0x01, 0);
    }, Qt::QueuedConnection);

    emit UpdateUI("1号片架电机标零!");
}


void MainWindow::on_btn_PianjiaZero2_clicked()
{
    if (ui->btn_PianjiaConnect->text() == "连接片架电机") {
        emit UpdateUI("请先连接片架电机！");
        return;
    }

    QMetaObject::invokeMethod(motor, [this]() {
        motor->setMotorAbsolutePosition(0x02, 0);
    }, Qt::QueuedConnection);

    emit UpdateUI("2号片架电机标零!");
}


void MainWindow::on_btn_PianjiaZero3_clicked()
{
    if (ui->btn_PianjiaConnect->text() == "连接片架电机") {
        emit UpdateUI("请先连接片架电机！");
        return;
    }

    QMetaObject::invokeMethod(motor, [this]() {
        motor->setMotorAbsolutePosition(0x03, 0);
    }, Qt::QueuedConnection);

    emit UpdateUI("3号片架电机标零!");
}


void MainWindow::on_btn_PianjiaZero4_clicked()
{
    if (ui->btn_PianjiaConnect->text() == "连接片架电机") {
        emit UpdateUI("请先连接片架电机！");
        return;
    }

    QMetaObject::invokeMethod(motor, [this]() {
        motor->setMotorAbsolutePosition(0x04, 0);
    }, Qt::QueuedConnection);

    emit UpdateUI("4号片架电机标零!");
}


void MainWindow::on_btn_LightSet0_1_clicked()
{
    if (ui->btn_LightConnect->text() == "连接灯箱电机") {
        emit UpdateUI("请先连接灯箱电机！");
        return;
    }

    QMetaObject::invokeMethod(motor, [this]() {
        motor->setMotorAbsolutePosition(0x01, 0);
    }, Qt::QueuedConnection);

    emit UpdateUI("1号灯箱电机标零!");
}


void MainWindow::on_btn_LightSet0_2_clicked()
{
    if (ui->btn_LightConnect->text() == "连接灯箱电机") {
        emit UpdateUI("请先连接灯箱电机！");
        return;
    }

    QMetaObject::invokeMethod(motor, [this]() {
        motor->setMotorAbsolutePosition(0x02, 0);
    }, Qt::QueuedConnection);

    emit UpdateUI("2号灯箱电机标零!");
}


void MainWindow::on_btn_PianjiaAbsmove_1_clicked()
{
    if (ui->btn_PianjiaConnect->text() == "连接片架电机") {
        emit UpdateUI("请先连接片架电机！");
        return;
    }

    double positionMm =ui->doubleSpinBox->value();

    qint32 pos =static_cast<qint32>(positionMm * 4000.0);

    QMetaObject::invokeMethod(motor, [this, pos]() {
        motor->runToAbsolutePosition(0x01, pos);
    }, Qt::QueuedConnection);

    emit UpdateUI(QString("1号片架电机运行到：%1 mm").arg(positionMm));
}


void MainWindow::on_btn_PianjiaAbsmove_2_clicked()
{
    if (ui->btn_PianjiaConnect->text() == "连接片架电机") {
        emit UpdateUI("请先连接片架电机！");
        return;
    }

    double positionMm =ui->doubleSpinBox_2->value();

    qint32 pos =static_cast<qint32>(positionMm * 4000.0);

    QMetaObject::invokeMethod(motor, [this, pos]() {
        motor->runToAbsolutePosition(0x02, pos);
    }, Qt::QueuedConnection);

    emit UpdateUI(QString("2号片架电机运行到：%1 mm").arg(positionMm));
}


void MainWindow::on_btn_PianjiaAbsmove_3_clicked()
{
    if (ui->btn_PianjiaConnect->text() == "连接片架电机") {
        emit UpdateUI("请先连接片架电机！");
        return;
    }

    double positionMm =ui->doubleSpinBox_3->value();

    qint32 pos =static_cast<qint32>(positionMm * 4000.0);

    QMetaObject::invokeMethod(motor, [this, pos]() {
        motor->runToAbsolutePosition(0x03, pos);
    }, Qt::QueuedConnection);

    emit UpdateUI(QString("3号片架电机运行到：%1 mm").arg(positionMm));
}


void MainWindow::on_btn_PianjiaAbsmove_4_clicked()
{
    if (ui->btn_PianjiaConnect->text() == "连接片架电机") {
        emit UpdateUI("请先连接片架电机！");
        return;
    }

    double positionMm =ui->doubleSpinBox_4->value();

    qint32 pos =static_cast<qint32>(positionMm * 4000.0);

    QMetaObject::invokeMethod(motor, [this, pos]() {
        motor->runToAbsolutePosition(0x04, pos);
    }, Qt::QueuedConnection);

    emit UpdateUI(QString("4号片架电机运行到：%1 mm").arg(positionMm));
}


void MainWindow::on_btn_LightAbsmove_1_clicked()
{
    if (ui->btn_LightConnect->text() == "连接灯箱电机") {
        emit UpdateUI("请先连接灯箱电机！");
        return;
    }

    double positionMm =ui->doubleSpinBoxLight_1->value();

    qint32 pos =static_cast<qint32>(positionMm * 4000.0);

    QMetaObject::invokeMethod(motor, [this, pos]() {
        motor->runToAbsolutePosition(0x01, pos);
    }, Qt::QueuedConnection);

    emit UpdateUI(QString("1号灯箱电机运行到：%1 mm").arg(positionMm));
}


void MainWindow::on_btn_LightAbsmove_2_clicked()
{
    if (ui->btn_LightConnect->text() == "连接灯箱电机") {
        emit UpdateUI("请先连接灯箱电机！");
        return;
    }

    double positionMm =ui->doubleSpinBoxLight_2->value();

    qint32 pos =static_cast<qint32>(positionMm * 4000.0);

    QMetaObject::invokeMethod(motor, [this, pos]() {
        motor->runToAbsolutePosition(0x02, pos);
    }, Qt::QueuedConnection);

    emit UpdateUI(QString("2号灯箱电机运行到：%1 mm").arg(positionMm));
}


void MainWindow::on_btn_ClearPianjiaAlarm_clicked()
{
    if (ui->btn_PianjiaConnect->text() == "连接片架电机") {
        emit UpdateUI("请先连接片架电机！");
        return;
    }

    for (quint8 addr : {quint8(0x01),
                        quint8(0x02),
                        quint8(0x03),
                        quint8(0x04)}) {
        QMetaObject::invokeMethod(
            motor,
            [this, addr]() {
                if (motor->isSerialOpen())
                    motor->clearMotorAlarm(addr);
            },
            Qt::QueuedConnection);
    }

    emit UpdateUI("片架电机报警清除命令已发送");
}


void MainWindow::on_btn_ClearLightAlarm_clicked()
{
    if (ui->btn_LightConnect->text() == "连接灯箱电机") {
        emit UpdateUI("请先连接灯箱电机！");
        return;
    }

    for (quint8 addr : {quint8(0x01),
                        quint8(0x02)}) {
        QMetaObject::invokeMethod(
            motor,
            [this, addr]() {
                if (motor->isSerialOpen())
                    motor->clearMotorAlarm(addr);
            },
            Qt::QueuedConnection);
    }

    emit UpdateUI("灯箱电机报警清除命令已发送");
}

