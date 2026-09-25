#include"LevelingSystem.h"

LevelingSystem::LevelingSystem(ZMotionControl* zm,QSettings* iniRead,QObject *parent)
    : QObject(parent), m_zm(zm)
{
    m_TaskStatus.sys =eSubSystem::Levelling;//绑定子系统

    m_balanceMonitorTimer = new QTimer(this);
    m_statusMonitorTimer = new QTimer(this);//执行自动或者手动任务的时候才打开这个定时器，任务完成后关闭，并重置完成标志位
    detectionTimer = new QTimer(this);//脚座触地
    restartTimer = new QTimer(this);//车身恢复触地
    connect(m_statusMonitorTimer, &QTimer::timeout, this, &LevelingSystem::monitorSubsystemStatus);

    // 采集电流数据写入数据库
    m_currentCollectionTimer = new QTimer(this);
    connect(m_currentCollectionTimer, &QTimer::timeout, this, &LevelingSystem::collectMotorCurrents);
    m_currentCollectionTimer->start(300);


    LevelSetting = iniRead;

    // 设置 LevelingSystem 阈值
    AngleXThreshold = iniRead->value("/LevelingSystem/AngleXThreshold").toFloat();
    AngleYThreshold = iniRead->value("/LevelingSystem/AngleYThreshold").toFloat();
    FrontLeftFootCurrentThreshold = iniRead->value("/LevelingSystem/FrontLeftFootCurrentThreshold").toFloat();
    FrontRightFootCurrentThreshold = iniRead->value("/LevelingSystem/FrontRightFootCurrentThreshold").toFloat();
    RearLeftFootCurrentThreshold = iniRead->value("/LevelingSystem/RearLeftFootCurrentThreshold").toFloat();
    RearRightFootCurrentThreshold = iniRead->value("/LevelingSystem/RearRightFootCurrentThreshold").toFloat();

}


// 定时将电机电流写入数据库
void LevelingSystem::collectMotorCurrents()
{
    // 系统运行时才采集电流
    if (m_currentState == eTaskstate::Running) {
        for (int i = 0; i < 4; ++i) {
            float current = m_zm->GetMotorCurrent(i);
            QString sensorType = QString("电流-电机%1").arg(i);
            DatabaseManager::getInstance()->logSensorData(sensorType, current);
        }
    }
}


// 获取子系统实时状态
void LevelingSystem::getSubSystemState()
{
    //检查是否有轴处于错误状态
    bool hasAxisError = false;
    m_errorInfo.clear();//这里如果不清除错误信息，会造成主界面重复打印错误信息
    for (int i = 0; i < 4; ++i) {
        if (m_zm->GetAxisStatus(i) != MyAxisSatus::正常状态) {
            hasAxisError = true;
            // if(!m_errorInfo.isEmpty())
            // {
            //     m_errorInfo += "\n";
            // }
            m_errorInfo += QString("轴 %1 错误 : %2").arg(i).arg(AxisStatusQstr(m_zm->GetAxisStatus(i)));
        }
    }

    if (hasAxisError) {
        m_currentState = eTaskstate::Error;
        return;
    }

    //任意一轴运动，视为Running状态
    bool anyAxisMoving = false;
    for (int i = 0; i < 4; ++i) {
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
    for (int i = 0; i < 4; ++i) {
        if (m_zm->GetAxisMType(i) != AxisMType::Idle) {
            allAxesIdle = false;
            break;
        }
    }

    if (allAxesIdle) {
        // 检查是否已经完成调平
        if (m_isAutoBalance || m_isAutotToGround) {
            m_currentState = eTaskstate::Complete;
            QTimer::singleShot(400, this, [=]() {
            m_statusMonitorTimer->stop();//重置标志位前断开定时器，防止进入下一个系统后，上一个系统还会再次判定一次ready状态
            m_isAutoBalance = false;
            m_isAutotToGround = false;});
            return;
        }
        else
        {
            m_currentState = eTaskstate::Ready;
            return;
        }
    }
}

// 处理状态转换
void LevelingSystem::handleStateTransition(eTaskstate newState)
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

        m_TaskStatus.info.clear();// 状态切换，清除错误信息

        // 计算耗时，任务完成/错误
        if (newState == eTaskstate::Complete || newState == eTaskstate::Error) {
            double elapsed = m_taskStartTime.msecsTo(QDateTime::currentDateTime()) / 1000.0;//计算起始时间和当前时间差值
            m_TaskStatus.duration = elapsed;
        } else {
            m_TaskStatus.duration = 0.0;
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
            m_TaskStatus.info = m_errorInfo; // 进入错误状态，更新错误信息
            break;
        case nsTaskMgr::eTaskstate::Init:
            break;
        }

        emit sigStatusFeedback(m_TaskStatus);
    }
}


// 状态监测定时器回调
void LevelingSystem::monitorSubsystemStatus()
{
    getSubSystemState();
    handleStateTransition(m_currentState);
}

//自动任务  这里的正反向任务可以再封装一层，看着简洁，参考取盖板
void LevelingSystem::ExeLevellingTask(TaskInfo info)
{
    m_TaskStatus.taskinfo.TaskType = info.TaskType;//绑定正反向

    if(info.TaskType==1){//正向
      ExeLevelTask1();
    }

    else if(info.TaskType==2){//反向
      ExeLevelTask2();
    }
}

//正向任务
void LevelingSystem::ExeLevelTask1()
{
    m_balanceMonitorTimer->disconnect();

    m_balanceMonitorTimer->start(200);
    m_statusMonitorTimer->start(200);

    //记录正向任务开始时间
    m_taskStartTime = QDateTime::currentDateTime();

    if(CurrentLevelingState == LevelingState::Idle || CurrentLevelingState == LevelingState::ForContact){
        //内部有定时器，检测到脚座触地，ForLevelOK置为真
        FeetToGround();
        CurrentLevelingState = LevelingState::ForContact;
    }
    else if(CurrentLevelingState == LevelingState::ForBalance){
        QMessageBox::information(nullptr, "提示", "执行调平！");
        BalanceBody();
    }

    connect(m_balanceMonitorTimer, &QTimer::timeout, this, [this]() {
        switch (CurrentLevelingState) {
        case LevelingState::ForContact:
            if (ForLevelOK) {
                CurrentLevelingState = LevelingState::ForBalance;
                BalanceBody();//内部有定时器,调平完成，m_isAutoBalance置为真
            }
            break;

        case LevelingState::ForBalance:
            if (m_isAutoBalance) {
                m_balanceMonitorTimer->stop();
                CurrentLevelingState = LevelingState::Idle;
                // m_isAutoBalance = false;
            }
            break;
        }
    });
}

//反向任务
void LevelingSystem::ExeLevelTask2()
{
    QTimer* myTimer = new QTimer(this);
    myTimer->start(200);
    m_statusMonitorTimer->start(200);

    //记录反向任务开始时间
    m_taskStartTime = QDateTime::currentDateTime();

    BodyToGround();

    connect(myTimer, &QTimer::timeout, this, [=]() {
        if (m_isAutotToGround) {
            myTimer->stop();
        }
    });
}

//手动调平
//手动接管逻辑是由自动模式跳转的，此时任务类型已经确定
void LevelingSystem::IsManualBalance()
{
    m_statusMonitorTimer->start(200);

    //这里要用定时器循环检测if条件
    // 根据倾角传感器判断是否调平
    if (m_TaskStatus.taskinfo.TaskType == 1)
    {
        if (abs(angleXforleveling) <= AngleXThreshold && abs(angleYforleveling) <= AngleYThreshold)
        {
            m_isManualBalance = true;
            QMessageBox::information(nullptr, "提示", "可恢复自动调度！");
        }
        else
        {
            QMessageBox::warning(nullptr, "警告", "车身还未调平！");
        }
    }
    else if(m_TaskStatus.taskinfo.TaskType == 2)
    {
        bool isToGround =true;

        for(int i = 0;i < 4;i++)
        {
            isToGround &= abs(m_zm->myZmotionStatus->allAxisStatus[i].posi) < 0.1;
        }

        if(isToGround)
        {
            m_isManualToGround = true;
            QMessageBox::information(nullptr, "提示", "可恢复自动调度！");
        }
    }
    else
    {
        QMessageBox::warning(nullptr, "警告", "车身还未触地！");
    }
}

//bool isUp 反映的是螺杆的直线运动方向
//电机0、3 BackWard 螺杆向下
//电机1、2 ForWard 螺杆向下
MoveMode LevelingSystem::getMotorDirection(int motorId, bool isUp)
{
    if (motorId == 0 || motorId == 3) {
        return isUp ? Forward : Backward;
    } else {
        return isUp ? Backward : Forward;
    }
}

//当前调平逻辑为：先快速运行至电流突变超过5A,判定四脚触地;慢速抬高10mm;最后调平。
//如果手动调节了抬升高度，还要再手动调平一次。
//防虚脚函数
void LevelingSystem::FeetToGround()
{
    float CurrentThresholds[4] = {FrontLeftFootCurrentThreshold,FrontRightFootCurrentThreshold,RearLeftFootCurrentThreshold,RearRightFootCurrentThreshold};

    enum class DetectionState { IDLE, POSITION, CONTACT };
    static DetectionState currentState = DetectionState::IDLE;

    for (int i = 0; i < 4; ++i) {
        singleReached[i] = false;
    }

    //防止上次结果的影响，重置状态
    ForLevelOK = false;

    currentState = DetectionState::POSITION;

    //四台电机同时向下持续运动
    for (int i = 0; i < 4; ++i) {
        m_zm->SingleVMove(i, getMotorDirection(i, false));
    }

    connect(detectionTimer, &QTimer::timeout, this, [=]() {
        switch (currentState) {
        case DetectionState::POSITION: {
            bool allPositionsReached = true;
            LevelSymbol symbol;
            for (int i = 0; i < 4; ++i) {
                //电流突变超过3.5A停止,判定触地
                if ((abs(m_zm->GetMotorCurrent(i)) > CurrentThresholds[i]) && (abs(m_zm->GetAxisDpos(i)) < 12)) {
                    singleReached[i] = true;//脚座触地后，停止等待其他脚座触地
                    switch(i) {
                    case 0: symbol = LevelSymbol::FeetToGround0; break;
                    case 1: symbol = LevelSymbol::FeetToGround1; break;
                    case 2: symbol = LevelSymbol::FeetToGround2; break;
                    case 3: symbol = LevelSymbol::FeetToGround3; break;
                    }
                    emit sigSymbolChanged(symbol);
                    m_zm->SingleVMove(i, Cancel);
                }
            }
            for (int i = 0; i < 4; ++i) {
                allPositionsReached= allPositionsReached && singleReached[i];
            }

            if (allPositionsReached) {
                for (int i = 0; i < 4; ++i) {
                    m_zm->SetAxisSpeed(i,1);
                    m_zm->SetAxisAccel(i,10);
                    m_zm->SetAxisDecel(i,10);
                    if(motorposi[i] != 0){
                        m_zm->MoveSingleAbs(i,motorposi[i]);
                    }
                    else{
                        if(i == 0 || i == 3){
                            motorposi[i] = m_zm->GetAxisDpos(i) - 10;
                        }
                        else{
                            motorposi[i] = m_zm->GetAxisDpos(i) + 10;
                        }
                        m_zm->MoveSingleAbs(i,motorposi[i]);
                    }

                }

                currentState = DetectionState::CONTACT;
            }
            break;
        }
        case DetectionState::CONTACT: {//判定触地后，默认抬高10mm

            bool AllForLevelOK = true;
            bool SingleForLevelOK[4] = {false};
            for (int i = 0; i < 4; ++i) {
                if(m_zm->GetAxisMType(i) == AxisMType::Idle){
                    SingleForLevelOK[i] = true;
                }
                ForLevelOK = AllForLevelOK && SingleForLevelOK[i];
            }

            if (ForLevelOK)
            {
                detectionTimer->stop();
                emit UpdateSystemInfo("脚座抬升完成,开始调平");
            }
            break;
        }
        default: break;
        }
    });
    detectionTimer->start(100);
}


//倾角传感器调平
void LevelingSystem::BalanceBody()
{
    Anglex = LevelSetting->value("/LevelingSystem/Anglex").toFloat();
    Angley = LevelSetting->value("/LevelingSystem/Angley").toFloat();
    m_adjustLevelingTimer = new QTimer(this);
    m_adjustLevelingTimer->start(200);

    if(!ForLevelOK){
        emit UpdateSystemInfo("脚座抬升未完成，无法调平！");
        return;
    }

    m_isAutoBalance = false;//防止上次结果的影响，重置状态
    adjustmentPhase = 0;//重置调整阶段

    connect(m_adjustLevelingTimer, &QTimer::timeout, this, [this]() {
        auto shouldStop = [&](float current) { return qAbs(current) <= 0.1; };

        auto stopAllMotors = [&]() {
            for(int id : currentMotors) { m_zm->SingleVMove(id, Cancel); }
            currentMotors.clear();
        };

        auto startNewMotors = [&](const QVector<int>& motors, bool isUp) {
            stopAllMotors();
            currentMotors = motors;
            for(int id : motors) {
                m_zm->SetAxisSpeed(id, 0.5);
                m_zm->SingleVMove(id, getMotorDirection(id, isUp));
            }
        };

        float angleX_deg = angleXforleveling;
        float angleY_deg = angleYforleveling;

        switch(adjustmentPhase) {
        case 0: {
            bool frontTilt = (angleX_deg-Anglex) > 0.05;
            bool rearTilt  = (angleX_deg-Anglex)< -0.05;
            bool leftTilt  = (angleY_deg-Angley)> 0.05;
            bool rightTilt = (angleY_deg-Angley) < -0.05;

            // bool frontTilt = (angleX_deg) > 0.05;
            // bool rearTilt  = (angleX_deg)< -0.05;
            // bool leftTilt  = (angleY_deg)> 0.05;
            // bool rightTilt = (angleY_deg) < -0.05;

            if(frontTilt) {
                startNewMotors({0,1}, false);
                adjustmentPhase = 1;
                break;
            }
            else if(rearTilt) {
                startNewMotors({2,3}, false);
                adjustmentPhase = 1;
                break;
            }
            else if(leftTilt) {
                startNewMotors({0,2}, false);
                adjustmentPhase = 2;
                break;
            }
            else if(rightTilt) {
                startNewMotors({1,3}, false);
                adjustmentPhase = 2;
                break;
            }
            else{
                m_isAutoBalance = true;
                emit UpdateSystemInfo("调平完成！");
                m_adjustLevelingTimer->stop();
                //抛出调平信号
                emit sigSymbolChanged(LevelSymbol::Balance);
            }
            break;
        }
        case 1: {
            if(shouldStop(angleX_deg-Anglex)){
            // if(shouldStop(angleX_deg)) {
                stopAllMotors();
                emit UpdateSystemInfo("X轴调平完成！");
                if(qAbs(angleY_deg) > 0.1) {
                    emit UpdateSystemInfo("开始Y轴调平！");
                    if(angleY_deg < 0) startNewMotors({1,3}, false);
                    else startNewMotors({0,2}, false);
                    adjustmentPhase = 2;
                } else {
                    emit UpdateSystemInfo("调平完成！");
                    m_isAutoBalance = true;
                    //抛出调平信号
                    emit sigSymbolChanged(LevelSymbol::Balance);
                    m_adjustLevelingTimer->stop();
                    adjustmentPhase = 0;
                }
            }
            break;
        }
        case 2: {
            if(shouldStop(angleY_deg-Angley)) {
            // if(shouldStop(angleY_deg)) {
                stopAllMotors();
                m_isAutoBalance = true;
                emit UpdateSystemInfo("调平完成！");
                //抛出调平信号
                emit sigSymbolChanged(LevelSymbol::Balance);
                m_adjustLevelingTimer->stop();
                adjustmentPhase = 0;
            }
            break;
        }
        }
    });
}

void LevelingSystem::BodyToGround()
{
    m_isAutotToGround= false;//每次判断前为了防止上一次判断结果的影响，手动置为假

    for (int i = 0; i < 4; ++i) {
        m_feetContact[i] = true;
    }

    for(int i=0; i<4; ++i){
        m_zm->SetAxisSpeed(i, 1.5);
        if(i==0 || i==3)
        {
            m_zm->MoveSingleAbs(i,15);
        }
        else
        {
            m_zm->MoveSingleAbs(i,-15);
        }
    }

    connect(restartTimer, &QTimer::timeout, this, [=]() {
        bool notContact = true;
        for(int i=0; i<4; ++i){
            if(i==0 || i==3)
            {
                if(abs(m_zm->myZmotionStatus->allAxisStatus[i].posi - 15) < 0.1)
                    m_feetContact[i] = false;
            }
            else
            {
                if(abs(m_zm->myZmotionStatus->allAxisStatus[i].posi + 15) < 0.1)
                    m_feetContact[i] = false;
            }
            notContact &= !m_feetContact[i];
        }
        if(notContact)
        {
            m_isAutoBalance = false;
            m_isAutotToGround = true;
            emit sigSymbolChanged(LevelSymbol::Restart);//标志位复原信号
            emit UpdateSystemInfo("脚座已收回！");
            restartTimer->stop();
        }
    });
    restartTimer->start(100);
}


// //防虚脚函数
// void LevelingSystem::FeetToGround()
// {
//     float CurrentThresholds[4] = {FrontLeftFootCurrentThreshold,FrontRightFootCurrentThreshold,RearLeftFootCurrentThreshold,RearRightFootCurrentThreshold};

//     enum class DetectionState { IDLE, POSITION, CONTACT };
//     static DetectionState currentState = DetectionState::IDLE;

//     for (int i = 0; i < 4; ++i) {
//         singleReached[i] = false;
//     }

//     //防止上次结果的影响，重置状态
//     ForLevelOK = false;

//     currentState = DetectionState::POSITION;

//     //四台电机同时向下持续运动
//     for (int i = 0; i < 4; ++i) {
//          m_zm->SingleVMove(i, getMotorDirection(i, false));
//     }

//     connect(detectionTimer, &QTimer::timeout, this, [=]() {
//         switch (currentState) {
//         case DetectionState::POSITION: {
//             bool allPositionsReached = true;
//             LevelSymbol symbol;
//             for (int i = 0; i < 4; ++i) {
//                 //电流突变超过3.5A停止,判定触地
//                 if ((abs(m_zm->GetMotorCurrent(i)) > CurrentThresholds[i]) && (abs(m_zm->GetAxisDpos(i)) < 12)) {
//                     singleReached[i] = true;//脚座触地后，停止等待其他脚座触地
//                     switch(i) {
//                     case 0: symbol = LevelSymbol::FeetToGround0; break;
//                     case 1: symbol = LevelSymbol::FeetToGround1; break;
//                     case 2: symbol = LevelSymbol::FeetToGround2; break;
//                     case 3: symbol = LevelSymbol::FeetToGround3; break;
//                     }
//                     emit sigSymbolChanged(symbol);
//                     m_zm->SingleVMove(i, Cancel);
//                 }
//             }
//             for (int i = 0; i < 4; ++i) {
//                 allPositionsReached= allPositionsReached && singleReached[i];
//             }

//             if (allPositionsReached) {
//                 for (int i = 0; i < 4; ++i) {
//                     m_zm->SetAxisSpeed(i,1);
//                     m_zm->SetAxisAccel(i,10);
//                     m_zm->SetAxisDecel(i,10);
//                     if(motorposi[i] != 0){
//                        m_zm->MoveSingleAbs(i,motorposi[i]);
//                     }
//                     else{
//                         if(i == 0 || i == 3){
//                             motorposi[i] = m_zm->GetAxisDpos(i) - 10;
//                         }
//                         else{
//                             motorposi[i] = m_zm->GetAxisDpos(i) + 10;
//                         }
//                         m_zm->MoveSingleAbs(i,motorposi[i]);
//                     }

//                 }

//                 currentState = DetectionState::CONTACT;
//             }
//             break;
//         }
//         case DetectionState::CONTACT: {//判定触地后，默认抬高10mm

//             bool AllForLevelOK = true;
//             bool SingleForLevelOK[4] = {false};
//             for (int i = 0; i < 4; ++i) {
//                 if(m_zm->GetAxisMType(i) == AxisMType::Idle){
//                     SingleForLevelOK[i] = true;
//                 }
//                 ForLevelOK = AllForLevelOK && SingleForLevelOK[i];
//             }

//             if (ForLevelOK)
//             {
//                 detectionTimer->stop();
//                 // QMessageBox::information(nullptr, "提示", "脚座抬升完成,请开始调平");
//             }
//             break;
//         }
//         default: break;
//         }
//     });
//     detectionTimer->start(100);
// }


// //倾角传感器调平
// void LevelingSystem::BalanceBody()
// {
//     m_adjustLevelingTimer = new QTimer(this);
//     m_adjustLevelingTimer->start(200);

//     if(!ForLevelOK){
//         QMessageBox::warning(nullptr, "警告", "脚座抬升未完成，无法调平！");
//         return;
//     }

//     m_isAutoBalance = false;//防止上次结果的影响，重置状态
//     adjustmentPhase = 0;//重置调整阶段

//     connect(m_adjustLevelingTimer, &QTimer::timeout, this, [this]() {
//         auto shouldStop = [&](float current) { return qAbs(current) <= 0.1; };

//         auto stopAllMotors = [&]() {
//             for(int id : currentMotors) { m_zm->SingleVMove(id, Cancel); }
//             currentMotors.clear();
//         };

//         auto startNewMotors = [&](const QVector<int>& motors, bool isUp) {
//             stopAllMotors();
//             currentMotors = motors;
//             for(int id : motors) {
//                 m_zm->SetAxisSpeed(id, 0.5);
//                 m_zm->SingleVMove(id, getMotorDirection(id, isUp));
//             }
//         };

//         float angleX_deg = angleXforleveling;
//         float angleY_deg = angleYforleveling;

//         switch(adjustmentPhase) {
//         case 0: {
//             bool frontTilt = angleX_deg > 0.1;
//             bool rearTilt  = angleX_deg < -0.1;
//             bool leftTilt  = angleY_deg > 0.1;
//             bool rightTilt = angleY_deg < -0.1;

//             if(frontTilt) {
//                 startNewMotors({0,1}, false);
//                 adjustmentPhase = 1;
//                 break;
//             }
//             else if(rearTilt) {
//                 startNewMotors({2,3}, false);
//                 adjustmentPhase = 1;
//                 break;
//             }
//             else if(leftTilt) {
//                 startNewMotors({0,2}, false);
//                 adjustmentPhase = 2;
//                 break;
//             }
//             else if(rightTilt) {
//                 startNewMotors({1,3}, false);
//                 adjustmentPhase = 2;
//                 break;
//             }
//             else{
//                 m_isAutoBalance = true;
//                 // QMessageBox::information(nullptr, "提示", "已经调平！");
//                 m_adjustLevelingTimer->stop();
//                 //抛出调平信号
//                 emit sigSymbolChanged(LevelSymbol::Balance);
//             }
//             break;
//         }
//         case 1: {
//             if(shouldStop(angleX_deg)) {
//                 stopAllMotors();
//                 if(qAbs(angleY_deg) > 0.1) {
//                     if(angleY_deg < 0) startNewMotors({1,3}, false);
//                     else startNewMotors({0,2}, false);
//                     // QMessageBox::information(nullptr, "提示", "X轴调平完成！");
//                     adjustmentPhase = 2;
//                 } else {
//                     // QMessageBox::information(nullptr, "提示", "已经调平完成！");
//                     m_isAutoBalance = true;
//                     //抛出调平信号
//                     emit sigSymbolChanged(LevelSymbol::Balance);
//                     m_adjustLevelingTimer->stop();
//                     adjustmentPhase = 0;
//                 }
//             }
//             break;
//         }
//         case 2: {
//             if(shouldStop(angleY_deg)) {
//                 stopAllMotors();
//                 m_isAutoBalance = true;
//                 // QMessageBox::information(nullptr, "提示", "调平完成，车身升降就绪！");
//                 //抛出调平信号
//                 emit sigSymbolChanged(LevelSymbol::Balance);
//                 m_adjustLevelingTimer->stop();
//                 adjustmentPhase = 0;
//             }
//             break;
//         }
//         }
//     });
// }



// void LevelingSystem::BodyToGround()
// {
//     m_isAutotToGround= false;//每次判断前为了防止上一次判断结果的影响，手动置为假

//     for (int i = 0; i < 4; ++i) {
//         m_feetContact[i] = true;
//     }

//     for(int i=0; i<4; ++i){
//         m_zm->SetAxisSpeed(i, 1.5);
//         if(i==0 || i==3)
//         {
//             m_zm->MoveSingleAbs(i,15);
//         }
//         else
//         {
//             m_zm->MoveSingleAbs(i,-15);
//         }
//     }

//     connect(restartTimer, &QTimer::timeout, this, [=]() {
//         bool notContact = true;
//         for(int i=0; i<4; ++i){
//             if(i==0 || i==3)
//             {
//                 if(m_zm->myZmotionStatus->allAxisStatus[i].posi>=0)
//                     m_feetContact[i] = false;
//             }
//             else
//             {
//                 if(m_zm->myZmotionStatus->allAxisStatus[i].posi<=0)
//                     m_feetContact[i] = false;
//             }
//             notContact &= !m_feetContact[i];
//         }
//         if(notContact)
//             {
//                 m_isAutoBalance = false;
//                 m_isAutotToGround = true;
//                  emit sigSymbolChanged(LevelSymbol::Restart);//标志位复原信号
//                 restartTimer->stop();
//             }
//     });
//     restartTimer->start(100);
// }


void LevelingSystem::StartSpeedBodyUp()
{
    QMutexLocker locker(&m_statusMutex);
    QTimer *timer = new QTimer(this);

    // if(!m_isAutoBalance){
    //     QMessageBox::warning(nullptr, "警告", "请先完成调平！");
    //     return;
    // }

    for(int i=0; i<4; ++i) {
        m_zm->SetAxisSpeed(i, 1.5);
        m_zm->SingleVMove(i, getMotorDirection(i, false));
    }

    connect(timer, &QTimer::timeout, this, [=]() {
    for(int i=0; i<4; ++i) {
        if(i==0 || i==3){
            if(m_zm->myZmotionStatus->allAxisStatus[i].posi<=-80){
                m_zm->SingleVMove(i,Cancel);
                QMessageBox::warning(nullptr, "警告", "位置已达上限！");
                timer->stop();
                return;
            }
        }
        else{
            if(m_zm->myZmotionStatus->allAxisStatus[i].posi>=80){
                m_zm->SingleVMove(i,Cancel);
                QMessageBox::warning(nullptr, "警告", "位置已达上限！");
                timer->stop();
                return;
            }
        }
    }});

    timer->start(200);
}


void LevelingSystem::StopSpeedBodyUp()
{
    emergencyStop();
}

void LevelingSystem::StartSpeedBodyDown()
{
    QMutexLocker locker(&m_statusMutex);
    QTimer *timer = new QTimer(this);

    // if(!m_isAutoBalance){
    //     QMessageBox::warning(nullptr, "警告", "请先完成调平！");
    //     return;
    // }
    for(int i=0; i<4; ++i) {
        m_zm->SetAxisSpeed(i, 1.5);
        m_zm->SingleVMove(i, getMotorDirection(i, true));
    }

    //持续检测是否超过位置限制
    connect(timer, &QTimer::timeout, this, [=]() {
    for(int i=0; i<4; ++i) {
        if(i==0 || i==3){
            if(m_zm->myZmotionStatus->allAxisStatus[i].posi>=0){
                m_zm->SingleVMove(i,Cancel);
                QMessageBox::warning(nullptr, "警告", "位置已达下限！");
                timer->stop();
                return;
            }
        }
        else{
            if(m_zm->myZmotionStatus->allAxisStatus[i].posi<=0){
                m_zm->SingleVMove(i,Cancel);
                QMessageBox::warning(nullptr, "警告", "位置已达下限！");
                timer->stop();
                return;
            }
        }
    } });

    timer->start(200);
}


void LevelingSystem::StopSpeedBodyDown()
{
    emergencyStop();
}

void LevelingSystem::PosBodyUp()
{
    QMutexLocker locker(&m_statusMutex);
    QTimer *timer = new QTimer(this);

    // if(!m_isAutoBalance){
    //     QMessageBox::warning(nullptr, "警告", "请先完成调平！");
    //     return;
    // }
    for(int i=0; i<4; ++i) {
        m_zm->SetAxisSpeed(i, 1.5);
    }
    float dis = disforleveling;
    m_zm->MoveSingleOpp(0,-dis);
    m_zm->MoveSingleOpp(1,dis);
    m_zm->MoveSingleOpp(2,dis);
    m_zm->MoveSingleOpp(3,-dis);

    connect(timer, &QTimer::timeout, this, [=]() {
        for(int i=0; i<4; ++i) {
            if(i==0 || i==3){
                if(m_zm->myZmotionStatus->allAxisStatus[i].posi<=-80){
                    m_zm->SingleVMove(i,Cancel);
                    QMessageBox::warning(nullptr, "警告", "位置已达上限！");
                    timer->stop();
                    return;
                }
            }
            else{
                if(m_zm->myZmotionStatus->allAxisStatus[i].posi>=80){
                    m_zm->SingleVMove(i,Cancel);
                    QMessageBox::warning(nullptr, "警告", "位置已达上限！");
                    timer->stop();
                    return;
                }
            }
        }});

    timer->start(200);
}


void LevelingSystem::PosBodyDown()
{
    QMutexLocker locker(&m_statusMutex);
    QTimer *timer = new QTimer(this);

    // if(!m_isAutoBalance){
    //     QMessageBox::warning(nullptr, "警告", "请先完成调平！");
    //     return;
    // }
    for(int i=0; i<4; ++i) {
        m_zm->SetAxisSpeed(i, 1.5);
    }
    float dis = disforleveling;
    m_zm->MoveSingleOpp(0,dis);
    m_zm->MoveSingleOpp(1,-dis);
    m_zm->MoveSingleOpp(2,-dis);
    m_zm->MoveSingleOpp(3,dis);

    connect(timer, &QTimer::timeout, this, [=]() {
        for(int i=0; i<4; ++i) {
            if(i==0 || i==3){
                if(m_zm->myZmotionStatus->allAxisStatus[i].posi<=0){
                    m_zm->SingleVMove(i,Cancel);
                    QMessageBox::warning(nullptr, "警告", "位置已达下限！");
                    timer->stop();
                    return;
                }
            }
            else{
                if(m_zm->myZmotionStatus->allAxisStatus[i].posi>=0){
                    m_zm->SingleVMove(i,Cancel);
                    QMessageBox::warning(nullptr, "警告", "位置已达下限！");
                    timer->stop();
                    return;
                }
            }
        } });

    timer->start(200);
}

//急停
void LevelingSystem::emergencyStop()
{
    detectionTimer->stop();

    for(int i = 0;i < 4;i++)
    {
        m_zm->SingleVMove(i,Cancel);
    }
}


// // 车身恢复触地
// void MainWindow::on_btn_BodyToGround_clicked()
// {
//     QTimer *timer = new QTimer(this);

//     // QLineEdit * sensor[] ={ui->sensor0,ui->sensor1,ui->sensor2,ui->sensor3};

//     QLabel * labels[] ={ui->lbl_Symbol0,ui->lbl_Symbol1,ui->lbl_Symbol2,ui->lbl_Symbol3};

//     for(int i=0; i<4; ++i){
//         zm->SetAxisSpeed(i, 3.0);
//         if(i==0 || i==3){
//             zm->MoveSingleAbs(i,30);
//         }
//         else{
//             zm->MoveSingleAbs(i,-30);
//         }
//     }

//     connect(timer, &QTimer::timeout, this, [=]() {

//         bool allContact = true;

//         for(int i=0; i<4; ++i){

//             //小车触地的过程，小于阈值恢复初始状态
//             if(i==0 || i==3)
//             {
//                 if(allAxisStatus[i].posi>=0)
//                 {
//                     m_feetContact[i] =false ;
//                     labels[i]->setStyleSheet("background-color: green;");//未触地标志位设为绿
//                 }

//             }
//             else
//             {
//                 if(allAxisStatus[i].posi<=0)
//                 {
//                     m_feetContact[i] =false ;
//                     labels[i]->setStyleSheet("background-color: green;");//未触地标志位设为绿
//                 }

//             }

//             allContact &= !m_feetContact[i];
//         }


//         if(allContact)
//         {
//             timer->stop();
//         }

//     });


//     // connect(timer, &QTimer::timeout, this, [=]() {
//     //     for(int i=0; i<4; ++i){

//     //         float force = sensor[i]->text().toFloat();

//     //         //小车触地的过程，小于阈值恢复初始状态
//     //         if(abs(force) <= 200)
//     //         {
//     //             m_feetContact[i] =false ;
//     //             labels[i]->setStyleSheet("background-color: green;");//未触地标志位设为绿
//     //             timer->stop();
//     //         }
//     //     }
//     // });

//     timer->start(200);
// }
