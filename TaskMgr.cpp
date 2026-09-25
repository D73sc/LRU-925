#include "TaskMgr.h"
#include <QDebug>


TaskMgr::TaskMgr(QObject* parent) : QObject(parent)
{
    loadCurrentStepFromIni();
    CurrentStepGet = new QTimer(this);
    connect(CurrentStepGet,&QTimer::timeout,this,&TaskMgr::saveCurrentStepToIni);//间隔200ms保存当前步骤到ini文件
    CurrentStepGet->start(200);
    m_errorOccurred = false;
}


void TaskMgr::startTask() {
    m_currentStep = SystemStep::Step_Veh_Forward;//每次开始安装，重置当前步骤为小车前进
    processStep();
}

//eTaskStatus结构体，反应自己是哪个子系统，执行的任务类型，任务完成状态等
void TaskMgr::slotSubSysStateChanged(eTaskStatus status)
{
    // qDebug() << "TaskMgr: Received status from subsystem" << static_cast<int>(status.sys)//子系统
    //          << ", state:" << static_cast<int>(status.state);//子系统任务完成状态

    m_currentSystem = status.sys;
    m_subSystemStatus[m_currentSystem] = status;//m_subSystemStatus键值对数据类型。键是子系统名称，值是TaskStatus结构体

    //状态改变在主界面打印信息
    emit sigStatusInfo(status);

    // 系统运行日志添加进MySQL数据库
    DatabaseManager::getInstance()->logTaskStatus(status);

    if (status.state == eTaskstate::Complete)
    {
        if (!m_errorOccurred)
        {
            if(AllowContinue){
                m_currentStep = static_cast<SystemStep>(static_cast<int>(m_currentStep) + 1);
                processStep();
            }
        }
    }
    else if (status.state == eTaskstate::Error)
    {
        qWarning() << "TaskMgr: Error in subsystem" << static_cast<int>(status.sys) << ":" << status.info;
        m_errorOccurred = true;

        //发出信号给UI处理错误提示
        emit sigError(status);

        // 把错误日志表写入MySQL数据库
        DatabaseManager::getInstance()->logError(SubSystemToString(status.sys),status.info);
    }
}


void TaskMgr::dispatchTask(eSubSystem sys, int taskType)
{
    //任务信息，包括正反向类型，任务链名称，LRU类型
    TaskInfo taskinfo;

    taskinfo.TaskType = taskType;

    // 原：m_subSystemStatus.remove(sys);
    m_subSystemStatus.clear();//只保存当前运行的系统及其状态

    switch (sys)
    {
    case eSubSystem::Vehicle:
        emit sigExeVehTask(taskinfo);
        break;
    case eSubSystem::Levelling:
        emit sigExecLevellingTask(taskinfo);
        break;
    case eSubSystem::MainLift:
        emit sigExecMainLiftTask(taskinfo);
        break;
    case eSubSystem::CoverPlate:
        emit sigExecCoverPlateTask(taskinfo);
        break;
    case eSubSystem::Lift:
        emit sigExecLiftTask(taskinfo);
        break;
    default:
        qWarning() << "TaskMgr: Unknown subsystem!";
        break;
    }
}

//考虑到LRU需要二次顶升，成功安装一个LRU需要往返执行两遍此处的全流程
void TaskMgr::processStep()
{
    if (m_errorOccurred)
    {
        qDebug() << "TaskMgr: Task paused due to error. Waiting for resume.";
        return;
    }

    switch (m_currentStep)
    {
    case SystemStep::Step_Veh_Forward:
        dispatchTask(eSubSystem::Vehicle, 1);
        break;
    case SystemStep::Step_Levelling_Forward:
        dispatchTask(eSubSystem::Levelling, 1);
        break;
    case SystemStep::Step_MainLift_Forward:
        dispatchTask(eSubSystem::MainLift, 1);
        break;
    case SystemStep::Step_CoverPlate_Forward:
        dispatchTask(eSubSystem::CoverPlate, 1);
        break;
    case SystemStep::Step_Lift_Forward:
        dispatchTask(eSubSystem::Lift, 1);
        break;
    case SystemStep::Step_Lift_Backward:
        dispatchTask(eSubSystem::Lift, 2);
        break;
    case SystemStep::Step_CoverPlate_Backward:
        dispatchTask(eSubSystem::CoverPlate, 2);
        break;
    case SystemStep::Step_MainLift_Backward:
        dispatchTask(eSubSystem::MainLift, 2);
        break;
    case SystemStep::Step_Levelling_Backward:
        dispatchTask(eSubSystem::Levelling, 2);
        break;
    case SystemStep::Step_Veh_Backward:
        dispatchTask(eSubSystem::Vehicle, 2);
        break;
    case SystemStep::Step_Done:
        qDebug() << "TaskMgr: All tasks completed.";
        m_currentStep = SystemStep::Step_Veh_Forward;//任务完成重置当前步骤为小车的正向移动，用于下一次的安装
        break;
    default:
        qWarning() << "TaskMgr: Unknown step.";
        break;
    }
}


//修改：恢复自动调度****
void TaskMgr::resumeFromError()
{
    if (m_errorOccurred)
    {
        qDebug() << "TaskMgr: Resuming from error at step:" << static_cast<int>(m_currentStep);

        if (checkSubsystemErrorsResolved()) {
            m_errorOccurred = false;
            processStep(); // 继续当前步骤
        } else {
            QString Info2 = QString("报错未解决!当前错误:%1").arg(m_subSystemStatus[m_currentSystem].info);
            QMessageBox::information(nullptr, "提示", Info2);
        }
    }
}

//新增：判断报错是否解决****
bool TaskMgr::checkSubsystemErrorsResolved()
{
    for (const auto &sys : m_subSystemStatus.keys()) {
        if (m_subSystemStatus[sys].state == eTaskstate::Error) {
            return false;
        }
    }
    return true;
}



//自动调度根据任务的当前步骤分配任务并抛出相应的任务执行信号
//这里分配任务前必须指定当前任务步骤才能正常模拟自动调度流程
//否则每次启动，默认步骤为AGV正向移动，无论执行什么动作，下一个动作必然是调平
void TaskMgr::EmitsigExeVehTask1()
{
    m_currentStep = SystemStep::Step_Veh_Forward;
    dispatchTask(eSubSystem::Vehicle, 1);
}
void TaskMgr::EmitsigExeVehTask2()
{
    m_currentStep = SystemStep::Step_Veh_Backward;
    dispatchTask(eSubSystem::Vehicle, 2);
}


void TaskMgr::EmitsigExeLevelingTask1()
{
    m_currentStep = SystemStep::Step_Levelling_Forward;
    dispatchTask(eSubSystem::Levelling, 1);
}
void TaskMgr::EmitsigExeLevelingTask2()
{
    m_currentStep = SystemStep::Step_Levelling_Backward;
    dispatchTask(eSubSystem::Levelling, 2);
}


void TaskMgr::EmitsigExeMainLiftTask1()
{
    m_currentStep = SystemStep::Step_MainLift_Forward;
    dispatchTask(eSubSystem::MainLift, 1);
}
void TaskMgr::EmitsigExeMainLiftTask2()
{
    m_currentStep = SystemStep::Step_MainLift_Backward;
    dispatchTask(eSubSystem::MainLift, 2);
}


void TaskMgr::EmitsigExeCoverPlateTask1()
{
    m_currentStep = SystemStep::Step_CoverPlate_Forward;
    dispatchTask(eSubSystem::CoverPlate, 1);
}
void TaskMgr::EmitsigExeCoverPlateTask2()
{
    m_currentStep = SystemStep::Step_CoverPlate_Backward;
    dispatchTask(eSubSystem::CoverPlate, 2);
}


void TaskMgr::EmitsigExeLiftTask1()
{
    m_currentStep = SystemStep::Step_Lift_Forward;
    dispatchTask(eSubSystem::Lift, 1);
}
void TaskMgr::EmitsigExeLiftTask2()
{
    m_currentStep = SystemStep::Step_Lift_Backward;
    dispatchTask(eSubSystem::Lift, 2);
}


void TaskMgr::EmitsigExePinTask()
{
    // dispatchTask(eSubSystem::Pin, 1);
}




//保存当前步骤到ini文件
void TaskMgr::saveCurrentStepToIni()
{
    QSettings settings("param.ini",QSettings::IniFormat);
    settings.setValue("TaskMgr/currentstep",static_cast<int>(m_currentStep));
}

//从ini文件读取当前步骤
void TaskMgr::loadCurrentStepFromIni()
{
    QSettings settings("SystemThreshold.ini",QSettings::IniFormat);
    int stepValue = settings.value("TaskMgr/currentstep").toInt();
    m_currentStep = static_cast<SystemStep>(stepValue);
}

//自动化流程：实例化TaskMgr *m_TaskMgr,同时实例化各个子系统对象。
//首先调用startTask()函数，其中的 processStep()函数获取初始子系统为AGV小车系统，调用dispatchTask()函数，抛出AGV小车前进的信号。
//然后在主窗口触发小车对象的执行函数ExeVehTask(TaskInfo info)，m_TaskStatus在这里获取前进后退信息。
//然后通过RecvPushHostJsonData(QString jsonData)检测推送的实时数据中的任务状态,m_TaskStatus在这里获取任务状态信息。
//任务完成时，抛出反馈信号sigStatusFeedback(m_TaskStatus)，触发调度对象的slotSubSysStateChanged函数(此时条件满足)，把当前子系统及其状态对应放入容器 QMap<eSubSystem, TaskStatus> m_subSystemStatus中，然后执行下一个子系统任务。
