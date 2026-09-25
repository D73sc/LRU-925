#include "MainLift.h"

MainLift::MainLift(ZMotionControl* zm, QObject *parent) : QObject(parent), m_zm(zm)
{
    m_TaskStatus.sys =eSubSystem::MainLift;

    //读取ini文件
    iniReadThreshold = new QSettings("param.ini", QSettings::IniFormat);

    // 设置 MainLift 阈值,可以把LRU和孔洞的距离写入ini,在这里读取

    UpTimer = new QTimer(this);
    DownTimer = new QTimer(this);
    //这个定时器，要确保连接控制器后再启动，因此可以执行任务的时候再启动，每次任务完成后停止定时器并重置任务完成的标志位
    m_statusMonitorTimer = new QTimer(this);
    connect(m_statusMonitorTimer, &QTimer::timeout, this, &MainLift::monitorSubsystemStatus);

    m_currentCollectionTimer = new QTimer(this);
    connect(m_currentCollectionTimer, &QTimer::timeout, this, &MainLift::collectMotorCurrents);
    m_currentCollectionTimer->start(300);

}


// 定时将电机电流写入数据库
void MainLift::collectMotorCurrents()
{
    // 系统运行时才采集电流
    if (m_currentState == eTaskstate::Running) {
        float current = m_zm->GetMotorCurrent(4);
        QString sensorType = QString("电流-电机%1").arg(4);
        DatabaseManager::getInstance()->logSensorData(sensorType, current);
    }
}


// 获取子系统实时状态 电机4
void MainLift::getSubSystemState()
{
    //检查是否处于错误状态
    m_errorInfo.clear();
    if (m_zm->GetAxisStatus(4) != MyAxisSatus::正常状态) {
        // if(!m_errorInfo.isEmpty())
        // {
        //     m_errorInfo += "\n";
        // }
        m_errorInfo += QString("轴 %1 错误 : %2").arg(4).arg(AxisStatusQstr(m_zm->GetAxisStatus(4)));
        m_currentState = eTaskstate::Error;
        return;
    }

    //Running状态
    if (m_zm->GetAxisMType(4) != AxisMType::Idle) {
        m_currentState = eTaskstate::Running;
        return;
    }

    // 全部轴静止，触发标志位为Complete状态，未触发且为正常状态为ready状态
    //IsSingleIdle 获取轴运动状态，0运行中，1运动停止
    if (m_zm->GetAxisMType(4) == AxisMType::Idle) {
        // 检查是否已经完成一级升降
        if (m_isAutotUp || m_isAutoDown) {
            m_currentState = eTaskstate::Complete;
            QTimer::singleShot(400, this, [=]() {
            m_statusMonitorTimer->stop();
            m_isAutotUp = false;
            m_isAutoDown = false;});
            return;
        }
        else{
            m_currentState = eTaskstate::Ready;
            return;
        }
    }
}

// 处理状态转换
void MainLift::handleStateTransition(eTaskstate newState)
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
void MainLift::monitorSubsystemStatus()
{
    getSubSystemState();
    handleStateTransition(m_currentState);
}

//自动、手动任务
void MainLift::ExeMainLiftTask(TaskInfo info)
{
    m_TaskStatus.taskinfo.TaskType =info.TaskType;

    if(info.TaskType==1)//正向,根据LRU类型，确定停止时机
    {
        ExeMainLiftTask1();
    }

    else if(info.TaskType==2)//反向,恢复至初始零位
    {
        ExeMainLiftTask2();
    }
}


//一级升降测试任务
//正向任务_测试用
void MainLift::ExeMainLiftTask1()
{
    m_statusMonitorTimer->start(200);//开启状态监测定时器
    m_taskStartTime = QDateTime::currentDateTime();  // 记录任务开始时间

    //测试验收，上升固定高度75
    m_zm->MoveSingleAbs(4,75);
    UpTimer->start(200);
    connect(UpTimer,&QTimer::timeout,this,[=](){
        if(abs(m_zm->GetAxisDpos(4) - 75) < 0.1){
            m_isAutotUp = true;
            UpTimer->stop();
        }
    });
}

//反向任务_测试用
void MainLift::ExeMainLiftTask2()
{
    m_statusMonitorTimer->start(200);//开启状态监测定时器
    m_taskStartTime = QDateTime::currentDateTime();  // 记录任务开始时间

    m_zm->MoveSingleAbs(4,0);
    DownTimer->start(200);
    connect(DownTimer,&QTimer::timeout,this,[=](){
        if(abs(m_zm->GetAxisDpos(4)) < 0.1){
            m_isAutoDown = true;
            DownTimer->stop();
        }
    });
}

// //正向任务
// void MainLift::ExeMainLiftTask1()
// {
//     m_statusMonitorTimer->start(200);//开启状态监测定时器
//     m_taskStartTime = QDateTime::currentDateTime();  // 记录任务开始时间

//     m_zm->SetAxisSpeed(4,3);
//     m_zm->SetAxisAccel(4,30);
//     m_zm->SetAxisDecel(4,30);
//     m_zm->SingleVMove(4,Forward);
//     UpTimer->start(200);
//     connect(UpTimer,&QTimer::timeout,this,[=](){
//         if(abs(laser->getDistance1()) < 50.0){//激光测距传感器示数达到阈值
//             m_zm->SingleVMove(4,Cancel);
//             m_isAutotUp = true;
//             UpTimer->stop();
//         }
//     });
// }

// //反向任务
// void MainLift::ExeMainLiftTask2()
// {
//     m_statusMonitorTimer->start(200);//开启状态监测定时器
//     m_taskStartTime = QDateTime::currentDateTime();  // 记录任务开始时间

//     m_zm->MoveSingleAbs(4,0);
//     DownTimer->start(200);
//     connect(DownTimer,&QTimer::timeout,this,[=](){
//         if(abs(m_zm->myZmotionStatus->allAxisStatus[4].posi) < 0.1){
//             m_isAutoDown = true;
//             DownTimer->stop();
//         }
//     });
// }

//手动接管逻辑是由自动模式跳转的，此时任务类型已经确定
//无需判断,观察到距离合适,点击恢复自动按钮,直接判定可以恢复自动
void MainLift::IsManualLift()
{
    m_statusMonitorTimer->start(200);

    QTimer* myTimer = new QTimer(this);

    if (m_TaskStatus.taskinfo.TaskType == 1)
    {
        connect(myTimer,&QTimer::timeout,this,[=](){
            if(abs(m_zm->myZmotionStatus->allAxisStatus[4].posi - 500) < 0.1){//后面这个条件换成激光测距传感器数据判断
                m_isManualUp = true;
                myTimer->stop();
                QMessageBox::information(nullptr, "提示", "可恢复自动调度！");
            }
            else{
                QMessageBox::warning(nullptr, "警告", "一级未抬升到位！");
            }
        });
    }
    else if(m_TaskStatus.taskinfo.TaskType == 2)
    {
        connect(myTimer,&QTimer::timeout,this,[=](){
            if(abs(m_zm->myZmotionStatus->allAxisStatus[4].posi) < 0.1){
                m_isManualDown = true;
                myTimer->stop();
                QMessageBox::information(nullptr, "提示", "可恢复自动调度！");
            }
            else{
                QMessageBox::warning(nullptr, "警告", "一级未下降到位！");
            }
        });
    }
}


void MainLift::emergencyStop()
{
    m_zm->SingleVMove(4,Cancel);
}
