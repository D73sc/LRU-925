#include "CoverPlate.h"
#include <cmath>//新加
CoverPlate::CoverPlate(ZMotionControl* zm, QObject *parent) : QObject(parent), m_zm(zm)
{
    m_TaskStatus.sys =eSubSystem::CoverPlate;

    //读取ini文件
    iniReadThreshold = new QSettings("param.ini", QSettings::IniFormat);
    iniReadPOS = new QSettings("param.ini", QSettings::IniFormat);

    // 设置 CoverPlate 阈值

    //初始化读码器
    dmcamera = new DMDetected(this);
    dmcamera->connectToDevice();

    // 初始化状态机
    m_takeFloorFsm = new TakeFloorFSM(this, zm, dmcamera);
    m_takePlateFsm = new TakePlateFSM(this, zm, dmcamera);
    m_coverPlateFsm = new CoverPlateFSM(this, zm, dmcamera);
    m_coverFloorFsm = new CoverFloorFSM(this, zm, dmcamera);

    // 连接状态机信号
    connect(&m_takeFloorFsm->getFrontFSM(), &TakeFloorFSMFront::sigStateChanged, this, &CoverPlate::onFsmStateChanged);
    connect(&m_takePlateFsm->getFrontFSM(), &TakePlateFSMFront::sigStateChanged, this, &CoverPlate::onFsmStateChanged);
    connect(&m_coverPlateFsm->getFrontFSM(), &CoverPlateFSMFront::sigStateChanged, this, &CoverPlate::onFsmStateChanged);
    connect(&m_coverFloorFsm->getFrontFSM(), &CoverFloorFSMFront::sigStateChanged, this, &CoverPlate::onFsmStateChanged);
    //新加
    connect(&m_takeFloorFsm->getFrontFSM(),&TakeFloorFSMFront::sigStepConfirmationRequired,this,
        [this](const QString& currentState,
               const QString& nextState)
        {
            emit sigDoorStepConfirmationRequired(true,currentState,nextState);
        });
    //新加
    connect(&m_coverFloorFsm->getFrontFSM(),&CoverFloorFSMFront::sigStepConfirmationRequired,this,
        [this](const QString& currentState,
               const QString& nextState)
        {
            emit sigDoorStepConfirmationRequired(false,currentState,nextState);
        });

    // 初始化事件检测定时器(200ms检测一次)
    m_eventCheckTimer = new QTimer(this);
    connect(m_eventCheckTimer, &QTimer::timeout, this, &CoverPlate::checkEvents);

    //电机状态检测判定系统任务状态
    m_statusMonitorTimer = new QTimer(this);
    connect(m_statusMonitorTimer, &QTimer::timeout, this, &CoverPlate::monitorSubsystemStatus);

    // 采集电流数据写入数据库
    m_currentCollectionTimer = new QTimer(this);
    connect(m_currentCollectionTimer, &QTimer::timeout, this, &CoverPlate::collectMotorCurrents);
    m_currentCollectionTimer->start(300);
}


// 选择左右盖板,true代表左侧盖板
void CoverPlate::ChooseCoverPlate(bool symbol)
{
    GlobalMotorPos::setCoverPlateSymbol(symbol);
}

void CoverPlate::ResetChoose()
{
    GlobalMotorPos::resetCoverPlateSymbol();
}

//选择中间盖板
void CoverPlate::ChooseMiddleCoverPlate(bool symbol)
{
    GlobalMotorPos::setCenterSymbol(symbol);
}

// 定时将电机电流写入数据库
void CoverPlate::collectMotorCurrents()
{
    // 系统运行时才采集电流
    if (m_currentState == eTaskstate::Running) {
        for(int i = 5;i < 12 ;i++){
            float current = m_zm->GetMotorCurrent(i);
            QString sensorType = QString("电流-电机%1").arg(i);
            DatabaseManager::getInstance()->logSensorData(sensorType, current);
        }
    }
}


// 获取子系统实时状态  电机5~11
void CoverPlate::getSubSystemState()
{
    //检查是否有轴处于错误状态
    bool hasAxisError = false;
    m_errorInfo.clear();
    for (int i = 5; i < 12; ++i) {
        if (m_zm->GetAxisStatus(i) != MyAxisSatus::正常状态) {
            hasAxisError = true;
            // if(!m_errorInfo.isEmpty())
            // {
            //     m_errorInfo += "\n";
            // }
            //无论错误信息是否为空，只要非正常状态就要添加错误信息，只不过非空的时候先添加换行符再继续添加错误信息
            m_errorInfo += QString("轴 %1 错误 : %2").arg(i).arg(AxisStatusQstr(m_zm->GetAxisStatus(i)));
        }
    }

    if (hasAxisError) {
        m_currentState = eTaskstate::Error;
        return;
    }

    //任意一轴运动，视为Running状态
    bool anyAxisMoving = false;
    for (int i = 5; i < 12; ++i) {
        if (m_zm->GetAxisMType(i) != AxisMType::Idle) {
            anyAxisMoving = true;
            break;
        }
    }

    if (anyAxisMoving) {
        m_currentState = eTaskstate::Running;
        return;
    }

    // 全部轴静止，触发标志位为Complete状态，未触发且为正常状态为ready状态
    //IsSingleIdle 获取轴运动状态，0运行中，1运动停止
    bool allAxesIdle = true;
    for (int i = 5; i < 12; ++i) {
        if (m_zm->GetAxisMType(i) != AxisMType::Idle) {
            allAxesIdle = false;
            break;
        }
    }

    if (allAxesIdle) {
        if (m_isAutoOpen || m_isAutoClose) {
            m_currentState = eTaskstate::Complete;
            QTimer::singleShot(400, this, [=]() {
            m_statusMonitorTimer->stop();
            m_isAutoOpen = false;
            m_isAutoClose = false;});
            return;
        }
        else{
            m_currentState = eTaskstate::Ready;
            return;
        }
    }
}

// 处理状态转换
void CoverPlate::handleStateTransition(eTaskstate newState)
{
    bool StatusChange = false;

    if (m_lastState!= newState) StatusChange = true;

    if(newState == eTaskstate::Error && m_lastState == eTaskstate::Error){
        if(m_TaskStatus.info != m_errorInfo) StatusChange = true;
    }

    if(newState != eTaskstate::Error && m_lastState == eTaskstate::Error){
        emit UpdateSystemInfo("报错已解决!");
    }

    if(StatusChange)
    {
        m_lastState = newState;

        //进入新状态，清除错误信息
        m_TaskStatus.info.clear();

        // 计算耗时
        if (newState == eTaskstate::Complete || newState == eTaskstate::Error) {
            double elapsed = m_taskStartTime.msecsTo(QDateTime::currentDateTime()) / 1000.0;
            m_TaskStatus.duration = elapsed;
        }

        // 更新任务状态
        switch (newState) {
        case eTaskstate::Ready:
            m_TaskStatus.state = eTaskstate::Ready;
            break;
        case eTaskstate::Running:
            m_TaskStatus.state = eTaskstate::Running;
            break;
        case eTaskstate::Complete:
            m_TaskStatus.state = eTaskstate::Complete;
            break;
        case eTaskstate::Error:
            m_TaskStatus.state = eTaskstate::Error;
            m_TaskStatus.info = m_errorInfo; // 设置错误信息
            break;
        case nsTaskMgr::eTaskstate::Init:
            break;
        }
        emit sigStatusFeedback(m_TaskStatus);
    }
}

// 状态监测定时器回调
void CoverPlate::monitorSubsystemStatus()
{
    getSubSystemState();
    handleStateTransition(m_currentState);
}

//自动任务
void CoverPlate::ExeCoverPlateTask(TaskInfo info)
{
    m_TaskStatus.taskinfo.TaskType =info.TaskType;
    if(info.TaskType == 1) {
        CoverPlateTask1();
    }
    else if(info.TaskType == 2){
        CoverPlateTask2();
    }
}


//取盖板测试任务
//半自动正向运动_测试用
void CoverPlate::CoverPlateTask1()
{
    m_taskStartTime = QDateTime::currentDateTime();// 记录开始时间
    TakePlate();
}

//半自动反向任务_测试用
void CoverPlate::CoverPlateTask2()
{
    m_taskStartTime = QDateTime::currentDateTime();// 记录开始时间
    FixPlate();
}

// //半自动正向运动
// void CoverPlate::CoverPlateTask1()
// {
//     m_taskStartTime = QDateTime::currentDateTime();  // 记录开始时间
//     TakeSealDoor();
//     connect(&m_takeFloorFsm->getFrontFSM(), &TakeFloorFSMFront::sigStateChanged, this, [&](const QString& state) {
//         if (state == "TakeFloorCompleted") {
//             stopAllFSMs();
//             m_takePlateFsm->start();
//             istakePlateFsmOpen = true;}});
// }

// //半自动反向任务
// void CoverPlate::CoverPlateTask2()
// {
//     m_taskStartTime = QDateTime::currentDateTime();  // 记录开始时间
//     FixPlate();
//     connect(&m_coverPlateFsm->getFrontFSM(), &TakeFloorFSMFront::sigStateChanged, this, [&](const QString& state) {
//         if (state == "CoverPlateCompleted") {
//             stopAllFSMs();
//             m_coverFloorFsm->start();
//             iscoverFloorFsmOpen = true;}});
// }

// 取封门，新修
void CoverPlate::TakeSealDoor()
{
    stopAllFSMs();
    // 取封门启用逐步确认
    m_takeFloorFsm->getFrontFSM().setStepConfirmEnabled(true);

    m_statusMonitorTimer->start(200);//开启系统任务状态监测
    m_eventCheckTimer->start(200);//开启状态转换条件监测
    m_takeFloorFsm->start();//开启取封门状态机
    istakeFloorFsmOpen = true;
}

// 取盖板
void CoverPlate::TakePlate()
{
    // if (m_takeFloorFsm->getCurrentStateName() != "TakeFloorCompleted")    return;
    // if(!GlobalMotorPos::leftSymbol && !GlobalMotorPos::rightSymbol){
    //     QMessageBox::information(nullptr, "提示", "请先选择取左/右盖板");
    // }
    stopAllFSMs();
    m_eventCheckTimer->start(200);
    m_statusMonitorTimer->start(200);
    m_takePlateFsm->start();//取封门完成后，开启取盖板状态机
    istakePlateFsmOpen = true;
}

// 盖盖板
void CoverPlate::FixPlate()
{
    stopAllFSMs();
    m_eventCheckTimer->start(200);
    m_statusMonitorTimer->start(200);
    m_coverPlateFsm->start();//启动盖盖板状态机
    iscoverPlateFsmOpen = true;
}

// 盖封门，新修
void CoverPlate::FixSealDoor()
{
    // 检查前置条件
    // if (m_coverPlateFsm->getCurrentStateName() != "CoverPlateCompleted") return;
    stopAllFSMs();
    // 盖封门启用逐步确认
    m_coverFloorFsm->getFrontFSM().setStepConfirmEnabled(true);

    m_eventCheckTimer->start(200);
    m_statusMonitorTimer->start(200);
    m_coverFloorFsm->start();//盖盖板完成后，开启盖封门状态机
    iscoverFloorFsmOpen = true;
}
//新加
void CoverPlate::confirmDoorNextStep()
{
    if (istakeFloorFsmOpen) {
        m_takeFloorFsm->getFrontFSM().confirmNextStep();

        emit UpdateSystemInfo(
            "已确认，取封门流程继续执行下一步");
    }
    else if (iscoverFloorFsmOpen) {
        m_coverFloorFsm->getFrontFSM().confirmNextStep();

        emit UpdateSystemInfo(
            "已确认，盖封门流程继续执行下一步");
    }
}

// 停止所有状态机，新修
void CoverPlate::stopAllFSMs()
{
    // 停止所有状态机
    if (m_takeFloorFsm) {
        m_takeFloorFsm->stop();
        m_takeFloorFsm->getFrontFSM().setStepConfirmEnabled(false);
    }
    istakeFloorFsmOpen = false;

    if (m_takePlateFsm) m_takePlateFsm->stop();
    istakePlateFsmOpen = false;

    if (m_coverPlateFsm) m_coverPlateFsm->stop();
    iscoverPlateFsmOpen = false;

    if (m_coverFloorFsm) {
        m_coverFloorFsm->stop();
        m_coverFloorFsm->getFrontFSM().setStepConfirmEnabled(false);
    }
    iscoverFloorFsmOpen = false;

    // // 断开所有状态机的信号连接
    // disconnect(&m_takeFloorFsm->getFrontFSM(), &TakeFloorFSMFront::sigStateChanged, this, nullptr);
    // disconnect(&m_takePlateFsm->getFrontFSM(), &TakePlateFSMFront::sigStateChanged, this, nullptr);
    // disconnect(&m_coverPlateFsm->getFrontFSM(), &CoverPlateFSMFront::sigStateChanged, this, nullptr);
    // disconnect(&m_coverFloorFsm->getFrontFSM(), &CoverFloorFSMFront::sigStateChanged, this, nullptr);
}


// 禁用所有状态机动作
void CoverPlate::DisableAllFSMs()
{
    m_takeFloorFsm->getFrontFSM().m_enableEntryActions = false;
    m_takePlateFsm->getFrontFSM().m_enableEntryActions = false;
    m_coverFloorFsm->getFrontFSM().m_enableEntryActions = false;
    m_coverPlateFsm->getFrontFSM().m_enableEntryActions = false;
}

// 状态机处理状态变化
void CoverPlate::onFsmStateChanged(const QString& stateName)
{
    if (stateName == "TakePlateCompleted"){
        emit sigSymbolChanged(CoverPlateSymbol::TakePlateComplete);
        m_eventCheckTimer->stop();
        m_isAutoOpen = true;
    }
    else if (stateName == "CoverPlateCompleted"){
        emit sigSymbolChanged(CoverPlateSymbol::CoverPlateComplete);
        m_eventCheckTimer->stop();
        m_isAutoClose = true;
    }
    //else if (stateName == "CoverFloorCompleted") emit sigSymbolChanged(CoverPlateSymbol::CoverDoorComplete);
    //else if (stateName == "TakeFloorCompleted") emit sigSymbolChanged(CoverPlateSymbol::TakeDoorComplete);
    //新修
    else if (stateName == "CoverFloorCompleted") {
        m_eventCheckTimer->stop();
        m_statusMonitorTimer->stop();

        iscoverFloorFsmOpen = false;
        m_coverFloorFsm->getFrontFSM()
            .setStepConfirmEnabled(false);

        emit sigSymbolChanged(
            CoverPlateSymbol::CoverDoorComplete);
        emit UpdateSystemInfo(
            "半自动盖封门流程完成");
    }
    else if (stateName == "TakeFloorCompleted") {
        m_eventCheckTimer->stop();
        m_statusMonitorTimer->stop();

        istakeFloorFsmOpen = false;
        m_takeFloorFsm->getFrontFSM()
            .setStepConfirmEnabled(false);

        emit sigSymbolChanged(
            CoverPlateSymbol::TakeDoorComplete);
        emit UpdateSystemInfo(
            "半自动取封门流程完成");
    }
}


// 检查是否触发状态机事件
void CoverPlate::checkEvents()
{
    //机构恢复事件
    bool isRecover1 = (qAbs(m_zm->GetAxisDpos(5) - 81) < 0.1) && (qAbs(m_zm->GetAxisDpos(6) - 47) < 0.1);
    bool isRecover2 = (qAbs(m_zm->GetAxisDpos(7)) < 0.1) && (qAbs(m_zm->GetAxisDpos(8)) < 0.1);
    if (isRecover1 && isRecover2) {
        //if (iscoverPlateFsmOpen) m_coverPlateFsm->process_event(MyRecoverDone());//***
        if (iscoverFloorFsmOpen) m_coverFloorFsm->process_event(MyRecoverDone());//****
    }

    // 处理LiftingDone事件
    if (qAbs(m_zm->GetAxisDpos(11) - 14) < 0.1) {
        if (istakeFloorFsmOpen) m_takeFloorFsm->process_event(LiftingDone());//*
        if (iscoverFloorFsmOpen) m_coverFloorFsm->process_event(LiftingDone());//****
    }
    if (qAbs(m_zm->GetAxisDpos(11)) < 0.1) {
        if (istakePlateFsmOpen) m_takePlateFsm->process_event(LiftingDone());//**
        if (iscoverPlateFsmOpen) m_coverPlateFsm->process_event(LiftingDone());//***
    }
    if (qAbs(m_zm->GetAxisDpos(11) - 18) < 0.1) {
        if (iscoverFloorFsmOpen) m_coverFloorFsm->process_event(LiftingDone());//****
    }

    // 处理LoweringDone事件
    if (qAbs(m_zm->GetAxisDpos(11) - 39) < 0.1) {
        if (istakeFloorFsmOpen) m_takeFloorFsm->process_event(LoweringDone());//*
        if (iscoverFloorFsmOpen) m_coverFloorFsm->process_event(LoweringDone());//****
    }
    if (qAbs(m_zm->GetAxisDpos(11) - 55) < 0.1) {//-15
        if (istakePlateFsmOpen) m_takePlateFsm->process_event(LoweringDone());//**
        if (iscoverPlateFsmOpen) m_coverPlateFsm->process_event(LoweringDone());//***
    }
    if (qAbs(m_zm->GetAxisDpos(11) - 23) < 0.1) {
        if (istakePlateFsmOpen) m_takePlateFsm->process_event(LoweringDone());//**
    }
    if (qAbs(m_zm->GetAxisDpos(11) - 58) < 0.1) {
        if (istakePlateFsmOpen) m_takePlateFsm->process_event(LoweringDone_55());//**
        if (iscoverPlateFsmOpen) m_coverPlateFsm->process_event(LoweringDone_55());//***
    }

    // 处理正向横移、摘取横移事件
    bool isTransverse1134 = (qAbs(m_zm->GetAxisDpos(7) - 1120) < 0.1) &&(qAbs(m_zm->GetAxisDpos(8) - 1120) < 0.1);
    if (isTransverse1134) {//1120
        if (istakeFloorFsmOpen) m_takeFloorFsm->process_event(ForwardTransverseMovementDone());//*
        if (istakePlateFsmOpen) m_takePlateFsm->process_event(ForwardTransverseMovementDone());//**
        if (iscoverFloorFsmOpen) {
            m_coverFloorFsm->process_event(ForwardTransverseMovementDone());//****
            m_coverFloorFsm->process_event(AntiHangingDone());//****
        }
    }

    // 处理悬挂横移事件
    if ((qAbs(m_zm->GetAxisDpos(7) - 1145) < 0.1) && (qAbs(m_zm->GetAxisDpos(8) - 1145) < 0.1)) {
        if (istakeFloorFsmOpen) m_takeFloorFsm->process_event(HangingDone()); //*
        if (iscoverFloorFsmOpen) m_coverFloorFsm->process_event(HangingDone());//****
    }

    // 处理反向横移事件
    if (qAbs(m_zm->GetAxisDpos(7)) < 0.1 && qAbs(m_zm->GetAxisDpos(8)) < 0.1) {
        if (iscoverFloorFsmOpen) m_coverFloorFsm->process_event(ReverseTransverseMovementDone());//****
    }

    if (qAbs(m_zm->GetAxisDpos(7) - 100) < 0.1 && qAbs(m_zm->GetAxisDpos(8) - 100) < 0.1) {
        if (istakePlateFsmOpen) m_takePlateFsm->process_event(ReverseTransverseMovementTo100());//**
    }

    //手指电流相关判断
    if (istakePlateFsmOpen)
    {
        if(abs(m_zm->GetFingerCurrent(10)) > 14){
            m_zm->SingleVMove(9,Cancel);
            FingleCurrent9  = true;
        }

        if(abs(m_zm->GetFingerCurrent(11)) > 14){
            m_zm->SingleVMove(10,Cancel);
            FingleCurrent10  = true;
        }

        if(FingleCurrent9 && FingleCurrent10){
            FingleCurrent9  = false;
            FingleCurrent10  = false;
            m_takePlateFsm->process_event(FingerClampingDurDone());
        }

        motor9Pos  = GlobalMotorPos::getFingerPos(0);
        motor10Pos = GlobalMotorPos::getFingerPos(1);
        if(abs(m_zm->GetAxisDpos(9) - motor9Pos + 12) < 0.1 && abs(m_zm->GetAxisDpos(10) - motor10Pos + 12) < 0.1){
            m_takePlateFsm->process_event(FingerClampingOppDone());
        }
    }

    if ((qAbs(m_zm->GetAxisDpos(9) - 11.4) < 0.1) && (qAbs(m_zm->GetAxisDpos(10) - 11.4) < 0.1)) {
        if (iscoverFloorFsmOpen) m_coverFloorFsm->process_event(FingerClampingDone());//****
    }

    // 处理手指夹紧事件
    bool isFingerNear1 = (qAbs(m_zm->GetAxisDpos(9) + 22) < 0.1) && (qAbs(m_zm->GetAxisDpos(10) + 22) < 0.1);//**
    bool isFingerNear2 = (qAbs(m_zm->GetAxisDpos(9) + 50) < 0.1) && (qAbs(m_zm->GetAxisDpos(10) + 50) < 0.1);//**
    if (isFingerNear1) {
        if (istakePlateFsmOpen) m_takePlateFsm->process_event(FingerNear1Done());//**
        if (iscoverPlateFsmOpen) m_coverPlateFsm->process_event(FingerNear1Done());//***
    }
    if (isFingerNear2) {
        if (istakePlateFsmOpen) m_takePlateFsm->process_event(FingerNear2Done());//**
    }

    // 处理手指松开事件
    if ((qAbs(m_zm->GetAxisDpos(9) - 10) < 0.1) && (qAbs(m_zm->GetAxisDpos(10) - 10) < 0.1)) {
        if (istakeFloorFsmOpen)m_takeFloorFsm->process_event(FingerReleasingDone());//*
        if (iscoverFloorFsmOpen)m_coverFloorFsm->process_event(FingerReleasingDone());//****
    }

    if ((qAbs(m_zm->GetAxisDpos(9) + 20) < 0.1) && (qAbs(m_zm->GetAxisDpos(10) + 20) < 0.1)) {
        if (iscoverPlateFsmOpen) m_coverPlateFsm->process_event(FingerReleasingDone());//***
    }

    //处理纵移事件
    if ((qAbs(m_zm->GetAxisDpos(5) - 50 ) < 0.2) && (qAbs(m_zm->GetAxisDpos(6) - 50) < 0.2)) {
        if (istakePlateFsmOpen) m_takePlateFsm->process_event(LongitudinalMoveto50Done()); //**
    }


    float LastDMVal0 = GlobalMotorPos::getDmValues(0);
    float LastDMVal1 = GlobalMotorPos::getDmValues(1);
    float LastDMVal2 = GlobalMotorPos::getDmValues(2);
    QVector<float>CurrentDMVal = dmcamera->getValues();
    bool DMisOK = (CurrentDMVal[0] != LastDMVal0 || CurrentDMVal[1] != LastDMVal1 || CurrentDMVal[2] != LastDMVal2);
    if (DMisOK) {
        if (istakePlateFsmOpen) m_takePlateFsm->process_event(ReverseLongitudinalMovementDone()); //**
    }

    // if ((qAbs(m_zm->GetAxisDpos(5) - 65) < 0.1) && (qAbs(m_zm->GetAxisDpos(6) - 65) < 0.1)) {
    //     if (istakePlateFsmOpen) m_takePlateFsm->process_event(ForwardLongitudinalMovementDone()); //**
    // }

    if ((qAbs(m_zm->GetAxisDpos(5)) < 0.1) && (qAbs(m_zm->GetAxisDpos(6)) < 0.1)) {
        if (istakePlateFsmOpen) m_takePlateFsm->process_event(LongitudinalMovementDone()); //**
    }

    float  motor5BorrowPos = GlobalMotorPos::getBorrowPos(0);
    float  motor6BorrowPos = GlobalMotorPos::getBorrowPos(1);

    if ((qAbs(m_zm->GetAxisDpos(5) - motor5BorrowPos + 324 ) < 0.2) && (qAbs(m_zm->GetAxisDpos(6) - motor6BorrowPos + 324) < 0.2)) {
        if (istakePlateFsmOpen) m_takePlateFsm->process_event(ReverseLongitudinalForBorrowDone()); //**
    }

    if ((qAbs(m_zm->GetAxisDpos(5) - motor5BorrowPos - 324) < 0.1) && (qAbs(m_zm->GetAxisDpos(6) - motor6BorrowPos - 324) < 0.1)) {
        if (istakePlateFsmOpen) m_takePlateFsm->process_event(ForwardLongitudinalForBorrowDone()); //**
    }

    if ((qAbs(m_zm->GetAxisDpos(5) - motor5BorrowPos - 320) < 0.1) && (qAbs(m_zm->GetAxisDpos(6) - motor6BorrowPos - 320) < 0.1)) {
        if (istakePlateFsmOpen) m_takePlateFsm->process_event(ForwardLongitudinalForCenterBorrowDone()); //**
    }

    // 处理读码器调整相关事件
    currentVals = dmcamera->getValues();
    bool OK = currentVals[0] != 0 || currentVals[1] != 0 || currentVals[2] != 0;
    if (currentVals.size() >= 3 && OK) {
        // 角度调整
        if (istakePlateFsmOpen && abs(currentVals[2] / 10 - 90) <= 0.3) {
            m_takePlateFsm->process_event(AngleAdjustDone());//**
        }
        // X调整
        if (istakePlateFsmOpen && (abs(currentVals[1] / 10) <= 1)) {
            m_takePlateFsm->process_event(XAdjustDone());//**
            m_takePlateFsm->process_event(XAdjustAdvanceDone());//**
        }
        // Y调整
        if (istakePlateFsmOpen && (abs(currentVals[0] / 10 - 4) <= 1)) {
            m_takePlateFsm->process_event(YAdjustDone());//**
            m_takePlateFsm->process_event(YAdjustAdvanceDone());//**
        }
    }

    if (iscoverPlateFsmOpen){
        // motor5Pos = GlobalMotorPos::getCoverPlatePos(0);
        // motor7Pos = GlobalMotorPos::getCoverPlatePos(2);
        // motor6Pos = GlobalMotorPos::getCoverPlatePos(1);
        // motor8Pos = GlobalMotorPos::getCoverPlatePos(3);
        motor5Pos =iniReadPOS->value("/Motorpos/Pos5").toString().toFloat();
        motor6Pos =iniReadPOS->value("/Motorpos/Pos6").toString().toFloat();
        motor7Pos =iniReadPOS->value("/Motorpos/Pos7").toString().toFloat();
        motor8Pos =iniReadPOS->value("/Motorpos/Pos8").toString().toFloat();

        if (qAbs(m_zm->GetAxisDpos(7) - motor7Pos) < 1 && qAbs(m_zm->GetAxisDpos(8) - motor8Pos) < 1) {
            m_coverPlateFsm->process_event(MyPlateRecoverDone1());//***
        }

        if (qAbs(m_zm->GetAxisDpos(5) - motor5Pos) < 1 && qAbs(m_zm->GetAxisDpos(6) - motor6Pos) < 1) {
            m_coverPlateFsm->process_event(MyPlateRecoverDone2());//***
        }
    }
}

// 手动任务处理
void CoverPlate::IsManualCoverPlate()
{
    m_statusMonitorTimer->start(200);

    connect(&m_takeFloorFsm->getFrontFSM(), &TakeFloorFSMFront::sigStateChanged, this, [&](const QString& state) {
        if (state == "TakeFloorCompleted") {
            m_isManualOpen = true;
            QMessageBox::information(nullptr, "提示", "手动取盖板完成，可恢复自动调度");
        } else {
            QMessageBox::information(nullptr, "提示", "取盖板未完成");
        }});

    connect(&m_coverPlateFsm->getFrontFSM(), &TakeFloorFSMFront::sigStateChanged, this, [&](const QString& state) {
        if (state == "CoverPlateCompleted") {
            m_isManualClose = true;
            QMessageBox::information(nullptr, "提示", "手动盖盖板完成，可恢复自动调度");
        } else {
            QMessageBox::information(nullptr, "提示", "盖盖板未完成");
        }});
}

void CoverPlate::emergencyStop()
{
    for(int i = 5;i < 12;i++)
    {
        m_zm->SingleVMove(i,Cancel);
    }
}


void CoverPlate::STOPAllFSMs()
{
    stopAllFSMs();
}

//新加
bool CoverPlate::isDoorInitialPositionReady()
{
    if (!m_zm || !m_zm->GetConnectStatus())
        return false;

    // 纵移5、6；横移7、8；手指9、10；托举11
    const int axes[] = {5, 6, 7, 8, 9, 10, 11};
    const float targets[] = {81, 47, 0, 0, 12, 12, 18};
    const float tolerance = 0.1f;

    for (int i = 0; i < 7; ++i) {
        if (!m_zm->IsSingleIdle(axes[i]))
            return false;

        const float pos = m_zm->GetAxisDpos(axes[i]);
        if (!std::isfinite(pos) ||
            !(qAbs(pos - targets[i]) < tolerance)) {
            return false;
        }
    }

    return true;
}
