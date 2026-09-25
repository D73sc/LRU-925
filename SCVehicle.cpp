#include "SCVehicle.h"
#include <QString>
#include "logmgr.h"
//#include "nlohmann/json.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QFile>

SCVehicle::SCVehicle(QObject *parent) : QObject(parent),m_last_veh_status()
{
    m_robot_push =new SCStatusTcp(this);
    m_robot_navigate = new SCStatusTcp(this);
    m_robot_status = new SCStatusTcp(this);
    m_robot_control = new SCStatusTcp(this);

    m_TaskStatus.sys =eSubSystem::Vehicle;//绑定子系统

    QObject::connect(m_robot_push,&SCStatusTcp::sigPrintInfo,this,&SCVehicle::RecvPushHostStrData);
    QObject::connect(m_robot_push,&SCStatusTcp::sigChangedText,this,&SCVehicle::RecvPushHostVehicleData);
    QObject::connect(m_robot_push,&SCStatusTcp::sigJsonData,this,&SCVehicle::RecvPushHostJsonData);
    QObject::connect(m_robot_push,&SCStatusTcp::sigTcpStateChangedOut,this,&SCVehicle::PushHostSvrStateChanged);
    QObject::connect(m_robot_push,&SCStatusTcp::sigTcpErrorOut,this,&SCVehicle::PushHostSvrTcpError);

    QObject::connect(m_robot_navigate,&SCStatusTcp::sigPrintInfo,this,&SCVehicle::RecvNavigateStrData);
    QObject::connect(m_robot_navigate,&SCStatusTcp::sigChangedText,this,&SCVehicle::RecvNavigateVehicleData);
    QObject::connect(m_robot_navigate,&SCStatusTcp::sigJsonData,this,&SCVehicle::RecvNavigateJsonData);
    QObject::connect(m_robot_navigate,&SCStatusTcp::sigTcpStateChangedOut,this,&SCVehicle::NavigateSvrStateChanged);
    QObject::connect(m_robot_navigate,&SCStatusTcp::sigTcpErrorOut,this,&SCVehicle::NavigateSvrTcpError);

    QObject::connect(m_robot_status,&SCStatusTcp::sigPrintInfo,this,&SCVehicle::RecvStatusStrData);
    QObject::connect(m_robot_status,&SCStatusTcp::sigChangedText,this,&SCVehicle::RecvStatusVehicleData);
    QObject::connect(m_robot_status,&SCStatusTcp::sigJsonData,this,&SCVehicle::RecvStatusJsonData);
    QObject::connect(m_robot_status,&SCStatusTcp::sigTcpStateChangedOut,this,&SCVehicle::StatusSvrStateChanged);
    QObject::connect(m_robot_status,&SCStatusTcp::sigTcpErrorOut,this,&SCVehicle::StatusSvrTcpError);

    QObject::connect(m_robot_control,&SCStatusTcp::sigPrintInfo,this,&SCVehicle::RecvControlStrData);
    QObject::connect(m_robot_control,&SCStatusTcp::sigChangedText,this,&SCVehicle::RecvControlVehicleData);
    QObject::connect(m_robot_control,&SCStatusTcp::sigJsonData,this,&SCVehicle::RecvControlJsonData);
    QObject::connect(m_robot_control,&SCStatusTcp::sigTcpStateChangedOut,this,&SCVehicle::ControlSvrStateChanged);
    QObject::connect(m_robot_control,&SCStatusTcp::sigTcpErrorOut,this,&SCVehicle::ControlSvrTcpError);

    GetHandleInfo = new QTimer(this);
    connect(GetHandleInfo,&QTimer::timeout,this,[=](){
        if(isConnectedStatusHost()){
            queryControlOwner();
        }
    });
    GetHandleInfo->start(200);


//    m_stateMachine->sendControlCommand = [this](const QString& target){
//        this->sendControlCommandToVehicle(target);
//    };
//    m_stateMachine.start();
}
SCVehicle::~SCVehicle()
{
    ClosePushHostConnection();
    CloseNavigateConnection();
    CloseStatusConnection();
    CloseControlConnection();
}


void SCVehicle::ExeVehTask(TaskInfo info)
{
    //执行逻辑，执行过程通过RecvPushHostJsonData，判断任务是否完成
    m_TaskStatus.taskinfo.TaskType = info.TaskType;//动作前先绑定前进还是后退，后面状态改变抛出带有这个任务类型的信号用于UI显示任务状态

    // 记录任务开始时间
    if (!m_isTaskTiming) {
        m_taskStartTime = QDateTime::currentDateTime();
        m_isTaskTiming = true;
    }

    //判断正反向流程，并根据流程执行不同操作
    if(info.TaskType==1)//正向
    {
        sendControlCommandToVehicle(m_TaskStatus.taskinfo.TaskListName);
    }

    else if(info.TaskType==2)//反向
    {
        sendControlCommandToVehicle("task_0");
    }
}



int SCVehicle::connectPushHost(const QString &ip, quint16 port)
{
    return m_robot_push->connectHost(ip,port);
}
void SCVehicle::ClosePushHostConnection()
{
    m_robot_push->releaseTcpSocket();
}


int SCVehicle::connectNavigateHost(const QString &ip, quint16 port)
{
    return m_robot_navigate->connectHost(ip,port);
}
void SCVehicle::CloseNavigateConnection()
{
    m_robot_navigate->releaseTcpSocket();
}


int SCVehicle::connectStatusHost(const QString &ip, quint16 port)
{
    return m_robot_status->connectHost(ip,port);
}
void SCVehicle::CloseStatusConnection()
{
    m_robot_status->releaseTcpSocket();
}


int SCVehicle::connectControlHost(const QString &ip, quint16 port)
{
    return m_robot_control->connectHost(ip,port);
}
void SCVehicle::CloseControlConnection()
{
    m_robot_control->releaseTcpSocket();
}


//sigPrintInfo 信号触发(绑定字符串信息)，触发后向日志文件写入信息
void SCVehicle::RecvPushHostStrData(QString info)
{
    LogMgr::getInstance()->m_logger->info(info.toStdString());
    //emit sigPrintInfo(info);
}
void SCVehicle::RecvPushHostVehicleData(bool ,int ,QByteArray ,QByteArray ,int ,int )
{

}
//TODO:需要完善小车的运动逻辑
//如果通过TCP通讯接收到返回的Json数据，就会发出sigJsonData信号，触发下面的函数进行解析并进行逻辑判断
void SCVehicle::RecvPushHostJsonData(QString jsonData)
{
//    nlohmann::json json_obj = nlohmann::json::parse(jsonData.toStdString());

    VehicleSts veh_sts;//VehicleSts 机器人状态，电量，抱闸，激光等
    if(parseJsonToVehicleSts(jsonData,veh_sts))
    {
        //解析数据后，实时更新目标站点，用于手动的逻辑判断
        m_TaskStatus.taskinfo.TargetStation = veh_sts.target_station;

        //小车实时状态判断
        TaskStatus current_status = veh_sts.task_status;
        //小车从静止开始切换到运动状态的瞬间
        //m_last_veh_status.task_status==TaskStatus::NONE &&  current_status == TaskStatus::RUNNING
        if(current_status == TaskStatus::RUNNING)
        {
            m_TaskStatus.state =eTaskstate::Running;//任务执行中
            emit sigStatusFeedback(m_TaskStatus);
            //可以与主界面进行交互，追溯信息,不过这个信息交互要放到调度中实现
        }
        //小车从运动切换到运动完成状态
        if(m_last_veh_status.task_status==TaskStatus::RUNNING &&  current_status == TaskStatus::COMPLETED)
        {
            m_TaskStatus.state =eTaskstate::Complete;//任务完成
            if (m_isTaskTiming) {
                double elapsed = m_taskStartTime.msecsTo(QDateTime::currentDateTime()) / 1000.0;
                m_TaskStatus.duration = elapsed;
            }
            emit sigStatusFeedback(m_TaskStatus);//如果注释掉，就先不抛出任务完成信号，仅测试执行预存任务链
        }

        if(current_status == TaskStatus::NONE)
        {
            m_TaskStatus.state =eTaskstate::Ready;//任务执行中
            emit sigStatusFeedback(m_TaskStatus);
        }


        //小车被阻挡
        if(veh_sts.blocked)
        {
            // qDebug()<<"vehicle is blocked: "<<static_cast<int>(veh_sts.block_reason);
            // m_TaskStatus.state = eTaskstate::Error;
            // m_TaskStatus.info =BlockReason2Qstr(veh_sts.block_reason);
            // emit sigStatusFeedback(m_TaskStatus);
        }

        if(current_status == TaskStatus::CANCELED)
        {

        }
        //解析二维码和小车运动状态切换小车的状态机


        //发送信号给上层更新显示
        emit sigVehSts(veh_sts,0);

        //存储lats status
        m_last_veh_status = veh_sts;

        //log vehicle status
        //LogMgr::getInstance()->m_logger->info("Vehicle Task Status:{}",static_cast<int>(veh_sts.task_status));
        //emit sigUILog(jsonData);
        //暂时保留这个log一边观察小车反馈数据，后期正式发布后删除
        LogMgr::getInstance()->m_logger->debug(jsonData.toStdString());
    }
}

//解析json数据
bool SCVehicle::parseJsonToVehicleSts(const QString& jsonString, VehicleSts& vehSts)
{
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
    if (doc.isNull() || !doc.isObject())
    {
        qWarning() << "Failed to parse JSON";
        return false;
    }
    QJsonObject obj = doc.object();//QJsonObject类型数据本质是键值对的容器，其中键为QString类型，值为QJsonValue类型

    vehSts.blocked = obj["blocked"].toBool();
    vehSts.battery_level = obj["battery_level"].toDouble();
    vehSts.block_reason = (BlockReason)obj["block_reason"].toInt();//BlockReason 激光遮挡原因的枚举，枚举也可以理解为键值对
    vehSts.task_status = (TaskStatus)obj["task_status"].toInt();//TaskStatus 任务状态的枚举，正在执行，完成，取消等等
    vehSts.reloc_status = (RelocStatus)obj["reloc_status"].toInt();
    vehSts.loadmap_status = (LoadmapStatus)obj["loadmap_status"].toInt();
    vehSts.is_stop = obj["is_stop"].toBool();
    vehSts.emergency = obj["emergency"].toBool();
    //新增目标站点
    vehSts.target_station = obj["target_id"].toString();

    //解析json字符串数据
    //errors,
    vehSts.errors.clear();
    QJsonArray veh_errors = obj["errors"].toArray();
    QJsonArray veh_warnings = obj["warnings"].toArray();

    //QJsonValue是联合类型，能够存储以下六种不同类型的值：null，bool，double，string，array，object
    for (const QJsonValue& val : veh_errors)//val 是循环中的形参，表示veh_errors数组中当前正在遍历的元素
    {
        QJsonObject errObj = val.toObject();
        VehErrors errors;//VehErrors 结构体；code：错误代码，用来唯一标识车辆出现的错误;date_time：错误发生的日期和时间;desc：错误的详细描述;times：错误发生的次数
        errors.code = errObj["code"].toInt();
        errors.date_time = errObj["dateTime"].toString();
        errors.desc = errObj["desc"].toString();
        errors.times = errObj["times"].toInt();
        vehSts.errors.push_back(errors);//errors是VehErrors类型的容器
    }

    for (const QJsonValue& val : veh_warnings)
    {
        QJsonObject warObj = val.toObject();
        VehErrors warnings;
        warnings.code = warObj["code"].toInt();
        warnings.date_time = warObj["dateTime"].toString();
        warnings.desc = warObj["desc"].toString();
        warnings.times = warObj["times"].toInt();
        vehSts.errors.push_back(warnings);//warnings是VehErrors类型的容器
    }



    vehSts.charging= obj["charging"].toBool();
    vehSts.current_station = obj["current_station"].toString();
    vehSts.current_map = obj["current_map"].toString();
    vehSts.brake = obj["brake"].toBool();
    vehSts.soft_emc = obj["soft_emc"].toBool();
    vehSts.update_reason = obj["update_reason"].toInt();
    vehSts.slowed= obj["slowed"].toBool();
    vehSts.slow_reason = (BlockReason)obj["slow_reason"].toInt();
    vehSts.task_type = obj["task_type"].toInt();
    vehSts.target_dist = obj["target_dist"].toInt();
    vehSts.driver_emc = obj["driver_emc"].toBool();
    vehSts.electric = obj["electric"].toBool();

    //tasklist_status
    QJsonObject tasklist_sts_obj= obj["tasklist_status"].toObject();//键值对嵌套键值对，键"tasklist_status"对应的值是一个键值对容器(QJsonObject)
    vehSts.tasklist_status.taskid =tasklist_sts_obj["taskId"].toInt();//tasklist_status 结构体;taskid 任务编号;taskListName 任务链名称;task_status 任务状态
    vehSts.tasklist_status.taskListName =tasklist_sts_obj["taskListName"].toString();
    vehSts.tasklist_status.task_status = (TaskStatus)tasklist_sts_obj["taskListStatus"].toInt();

    //pgvs
    vehSts.pgvs.clear();
    QJsonArray veh_pgvs = obj["pgvs"].toArray();//键对应的值是一个数组(QJsonArray)，数组中的每个元素都是一个键值对容器(QJsonObject)
    for (const QJsonValue& val : veh_pgvs)//QJsonArray 里面存放的数据类型统称 QJsonValue
    {
        QJsonObject pgvObj = val.toObject();
        PGVInfo pgv;
        pgv.device_address = pgvObj["device_address"].toInt();
        pgv.error_code = pgvObj["error_code"].toInt();
        pgv.id_DMT_detected = pgvObj["id_DMT_detected"].toBool();
        pgv.is_absolute_X_pos_valid = pgvObj["is_absolute_X_pos_valid"].toBool();
        vehSts.pgvs.push_back(pgv);//pgvs是PGVInfo结构体类型的容器
    }

    //普通数组的解析
    //当前导航路径上已经经过的站点, 为站点的数组, 仅当 task_type 为 3 或 4 时该字段有效点。这里会列出所有已经经过的中间点
    vehSts.finished_path.clear();
    QJsonArray finished_path = obj["finished_path"].toArray();
    for (const QJsonValue &val : finished_path)
    {
        vehSts.finished_path.push_back( val.toString());
    }

    //当前导航路径上尚未经过的站点, 为站点的数组, 仅当 task_type 为 3 或 4 时该字段有效。这里会列出所有尚未经过的中间点
    vehSts.unfinished_path.clear();
    QJsonArray unfinished_path = obj["unfinished_path"].toArray();
    for (const QJsonValue &val : unfinished_path)
    {
        vehSts.unfinished_path.push_back( val.toString());
    }

    return true;
}



//sigPrintInfo信号触发，触发后抛出sigNaviInfo信号，被主窗口接收，触发slotPrintInfo函数，打印信息
void SCVehicle::RecvNavigateStrData(QString info)
{
    LogMgr::getInstance()->m_logger->info(info.toStdString());
    emit sigNaviInfo(info);
}
void SCVehicle::RecvNavigateVehicleData(bool ,int ,QByteArray ,QByteArray ,int ,int )
{

}
void SCVehicle::RecvNavigateJsonData(QString jsonData)
{

}


void SCVehicle::RecvStatusStrData(QString info)
{
    LogMgr::getInstance()->m_logger->info(info.toStdString());
}
void SCVehicle::RecvStatusVehicleData(bool ,int ,QByteArray ,QByteArray ,int ,int )
{

}
void SCVehicle::RecvStatusJsonData(QString jsonData)
{
    LogMgr::getInstance()->m_logger->info(jsonData.toStdString());

    VehicleSts handleInfo;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8());
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "Failed to parse control JSON data";
        return;
    }

    QJsonObject obj = doc.object();

    // PGV响应数据
    if (obj.contains("pgvs")) {
        QJsonArray pgvsArray = obj["pgvs"].toArray();
        if (!pgvsArray.isEmpty()) {
            // 取第一个PGV设备的数据(可根据device_address筛选特定设备)
            QJsonObject pgvObj = pgvsArray[0].toObject();

            // 提取原始数据
            double tagDiffAngleRaw = pgvObj["tag_diff_angle"].toDouble();
            double tagDiffX = pgvObj["tag_diff_x"].toDouble();
            double tagDiffY = pgvObj["tag_diff_y"].toDouble();

            // 角度转换
            double tagDiffAngle = tagDiffAngleRaw / 10.0;

            // 发送信号更新UI
            emit sigPGVDataUpdated(tagDiffAngle, tagDiffX, tagDiffY);
        }
    }

    handleInfo.locked = obj["locked"].toBool();//为真代表手柄激活
    handleInfo.nickName = obj["nick_name"].toString(); //手柄名字
    emit sigVehSts(handleInfo,1);
}



void SCVehicle::RecvControlStrData(QString info)
{
    LogMgr::getInstance()->m_logger->info(info.toStdString());
}
void SCVehicle::RecvControlVehicleData(bool ,int ,QByteArray ,QByteArray ,int ,int )
{

}
void SCVehicle::RecvControlJsonData(QString jsonData)
{

}

bool SCVehicle::SendData2PushHost(uint16_t sendCommand,
                         const QByteArray &jsonData,
                         const QByteArray &sendData,
                         uint16_t &number, uint8_t byte15)
{
    return m_robot_push->writeTcpData(sendCommand,jsonData,sendData,number,byte15);
}

//number:通信序列号,作为通信的唯一标识，用来匹配请求和响应；sendCommand：报文类型，即API编号；
//jsonData：格式化的Json数据；sendData：与Json数据共同组成完整数据区，与Json数据以及头部数据组合后作为请求发出
bool SCVehicle::SendData2NavigateHost(uint16_t sendCommand,
                           const QByteArray &jsonData,
                           const QByteArray &sendData,
                           uint16_t &number, uint8_t byte15)
{
    return m_robot_navigate->writeTcpData(sendCommand,jsonData,sendData,number,byte15);
}


bool SCVehicle::SendData2StatusHost(uint16_t sendCommand,
                                      const QByteArray &jsonData,
                                      const QByteArray &sendData,
                                      uint16_t &number, uint8_t byte15)
{
    return m_robot_status->writeTcpData(sendCommand,jsonData,sendData,number,byte15);
}


bool SCVehicle::SendData2ControlHost(uint16_t sendCommand,
                                       const QByteArray &jsonData,
                                       const QByteArray &sendData,
                                       uint16_t &number, uint8_t byte15)
{
    return m_robot_control->writeTcpData(sendCommand,jsonData,sendData,number,byte15);
}



//封装一个执行预存任务链的函数，这里形参修改为:任务链名字，然后下面的函数内部调用封装的函数并传入形参
//根据主界面选择的LRU类型，框内的名字赋值给TaskInfo中的LRU，然后根据框的序号，拼接task_序号赋值给TaskInfo中的TaskListName
void SCVehicle::sendControlCommandToVehicle(const QString &TaskListName)
{
    if (!TaskListName.isEmpty()) {

        QJsonObject jsonObj;
        jsonObj["name"] = TaskListName;

        QJsonDocument jsonDoc(jsonObj);
        QByteArray jsonData = jsonDoc.toJson(QJsonDocument::Compact);

        uint16_t number = 0x01;

        // 发送请求
        if (SendData2NavigateHost(0x0C22, jsonData, "", number, 0)) {
            qDebug() << "尝试执行预存任务链: " << TaskListName;
        } else {
            qWarning() << "指令发送失败:" << m_robot_navigate->lastError();
        }
    }
}


void SCVehicle::PushHostSvrStateChanged(QAbstractSocket::SocketState state)
{
    emit sigVehTcpStateChanged(TcpSvrType::PushSvr,state);
}
void SCVehicle::PushHostSvrTcpError(QAbstractSocket::SocketError error)
{

}

void SCVehicle::NavigateSvrStateChanged(QAbstractSocket::SocketState state)
{
    emit sigVehTcpStateChanged(TcpSvrType::NavigateSvr,state);
}
void SCVehicle::NavigateSvrTcpError(QAbstractSocket::SocketError error)
{

}

void SCVehicle::ControlSvrStateChanged(QAbstractSocket::SocketState state)
{
    emit sigVehTcpStateChanged(TcpSvrType::ControlSvr,state);
}
void SCVehicle::ControlSvrTcpError(QAbstractSocket::SocketError error)
{

}

void SCVehicle::StatusSvrStateChanged(QAbstractSocket::SocketState state)
{
    emit sigVehTcpStateChanged(TcpSvrType::StatusSvr,state);
}
void SCVehicle::StatusSvrTcpError(QAbstractSocket::SocketError error)
{

}

void SCVehicle::stateChanged(QAbstractSocket::SocketState state)
{

}
void SCVehicle::receiveTcpError(QAbstractSocket::SocketError error)
{

}

bool SCVehicle::isConnectedPushHost()
{
    return (m_robot_push->tcpSocket()&& m_robot_push->tcpSocket()->state()==QAbstractSocket::ConnectedState);
}
bool SCVehicle::isConnectedNavigateHost()
{
     return (m_robot_navigate->tcpSocket()&& m_robot_navigate->tcpSocket()->state()==QAbstractSocket::ConnectedState);
}
bool SCVehicle::isConnectedStatusHost()
{
    return (m_robot_status->tcpSocket()&& m_robot_status->tcpSocket()->state()==QAbstractSocket::ConnectedState);
}
bool SCVehicle::isConnectedControlHost()
{
    return (m_robot_control->tcpSocket()&& m_robot_control->tcpSocket()->state()==QAbstractSocket::ConnectedState);
}



// 查询控制权所有者
bool SCVehicle::queryControlOwner()
{
    if (isConnectedStatusHost()) {
        uint16_t number = 0x01;
        if (SendData2StatusHost(0x0424, "", "", number, 0)) {
            // qDebug() << "已发送控制权所有者查询请求";
            return true;
        } else {
            qWarning() << "控制权所有者查询请求发送失败:" << m_robot_status->lastError();
            return false;
        }
    } else {
        QMessageBox::information(nullptr, "警告", "未连接控制服务器！");
        return false;
    }
}


//上传并加载地图
bool SCVehicle::uploadMapToRobot(const QString& mapFilePath)
{
    // 读取地图文件
    QFile mapFile(mapFilePath);
    if (!mapFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "打开地图文件失败:" << mapFilePath;
        return false;
    }
    QByteArray mapData = mapFile.readAll();
    mapFile.close();


    //这个函数内部调用了writeTcpData函数，会构建请求信息字符串，并通过 sigPrintInfo 信号发送出去
    //在 SCVehicle 类的构造函数中，sigPrintInfo 信号与 RecvConfigureStrData(往配置服务器发送的请求) 槽函数连接
    if(isConnectedControlHost()){//先确保连接到控制服务器
        uint16_t number = 0x01;
        if (SendData2ControlHost(0x07E9, mapData, "", number, 0)) {
            qDebug() << "尝试加载地图！";
            return true;
        } else {
            qWarning() << "指令发送失败:" << m_robot_control->lastError();
            return false;
        }
    }
    else{
        QMessageBox::information(nullptr, "警告", "未连接控制服务器！");
    }
}

//AGV重定位
bool SCVehicle::relocateRobot(bool isAuto, double x, double y, double angle, double length, bool home)
{
    if(isConnectedControlHost()){
        QJsonObject jsonObj;

        jsonObj["isAuto"] = isAuto;
        jsonObj["x"] = x;
        jsonObj["y"] = y;
        jsonObj["angle"] = angle;
        jsonObj["length"] = length;
        jsonObj["home"] = home;

        QJsonDocument jsonDoc(jsonObj);
        QByteArray jsonData = jsonDoc.toJson(QJsonDocument::Compact);
        uint16_t number = 0x01;

        if (SendData2ControlHost(0x072D, jsonData, "", number, 0)) {
            qDebug() << "尝试重定位！";
            return true;
        } else {
            qWarning() << "指令发送失败:" << m_robot_control->lastError();
            return false;
        }
    }
    else{
        QMessageBox::information(nullptr, "警告", "未连接控制服务器！");
    }
}

bool SCVehicle::queryPGVData()
{
    if (isConnectedStatusHost()) {
        uint16_t number = 0x01;
        if (SendData2StatusHost(0x03F9, "", "", number, 0)) {
            // qDebug() << "已发送PGV二维码数据查询请求";
            return true;
        } else {
            qWarning() << "PGV查询请求发送失败:" << m_robot_status->lastError();
            return false;
        }
    } else {
        QMessageBox::information(nullptr, "警告", "未连接状态服务器！");
        return false;
    }
}


// 暂停当前导航
bool SCVehicle::pauseCurrentNavigation()
{
    if(isConnectedNavigateHost()){
        uint16_t number = 0x01;
        if (SendData2NavigateHost(0x0BB9, "", "", number, 0)) { // 3001
            qDebug() << "尝试暂停当前导航！";
            return true;
        } else {
            qWarning() << "指令发送失败:" << m_robot_control->lastError();
            return false;
        }
    }
    else{
        QMessageBox::information(nullptr, "警告", "未连接导航服务器！");
    }
}

// 继续当前导航
bool SCVehicle::resumeCurrentNavigation()
{
    if(isConnectedNavigateHost()){
        uint16_t number = 0x01;
        if (SendData2NavigateHost(0x0BBA, "", "", number, 0)) { //3002
            qDebug() << "尝试继续当前导航！";
            return true;
        } else {
            qWarning() << "指令发送失败:" << m_robot_control->lastError();
            return false;
        }
    }
    else{
        QMessageBox::information(nullptr, "警告", "未连接导航服务器！");
    }
}

// 取消当前导航
bool SCVehicle::cancelCurrentNavigation()
{
    if(isConnectedNavigateHost()){
        uint16_t number = 0x01;
        if (SendData2NavigateHost(0x0BBB, "", "", number, 0)) { //3003
            qDebug() << "尝试取消当前导航！";
            return true;
        } else {
            qWarning() << "指令发送失败:" << m_robot_control->lastError();
            return false;
        }
    }
}

//手动导航
//平动
bool SCVehicle::translateRobot(double dist, double vx, double vy, int mode)
{
    if (!isConnectedNavigateHost()) {
        QMessageBox::information(nullptr, "警告", "未连接导航服务器！");
        return false;
    }

    QJsonObject jsonObj;
    jsonObj["dist"] = dist;
    jsonObj["vx"] = vx;
    jsonObj["vy"] = vy;
    jsonObj["mode"] = mode;

    QJsonDocument jsonDoc(jsonObj);
    QByteArray jsonData = jsonDoc.toJson(QJsonDocument::Compact);
    uint16_t number = 0x01;

    return SendData2NavigateHost(0x0BEF, jsonData, "", number, 0);
}

// 转动
bool SCVehicle::rotateRobot(double angle, double vw, int mode)
{
    if (!isConnectedNavigateHost()) {
        QMessageBox::information(nullptr, "警告", "未连接导航服务器！");
        return false;
    }

    QJsonObject jsonObj;
    jsonObj["angle"] = angle;
    jsonObj["vw"] = vw;
    jsonObj["mode"] = mode;

    QJsonDocument jsonDoc(jsonObj);
    QByteArray jsonData = jsonDoc.toJson(QJsonDocument::Compact);
    uint16_t number = 0x01;

    return SendData2NavigateHost(0x0BF0, jsonData, "", number, 0);
}

//手动模式
void SCVehicle::IsManualNavigate(const QString& station)
{
    if (station == m_TaskStatus.taskinfo.TargetStation) {
        QMessageBox::information(nullptr, "提示", "可恢复自动调度！");
        m_manualNavigate = true;
    } else {
        QMessageBox::information(nullptr, "提示", "还未行驶至目标站点");
    }
}

/*
使用Qt原生Json解析
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDebug>

bool parseJsonToPerson(const QString& jsonString, Person& person) {
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "Failed to parse JSON";
        return false;
    }

    QJsonObject obj = doc.object();

    person.name = obj["name"].toString();
    person.age = obj["age"].toInt();

    QJsonObject addressObj = obj["address"].toObject();
    person.address.street = addressObj["street"].toString();
    person.address.city = addressObj["city"].toString();
    person.address.zip = addressObj["zip"].toString();

    QJsonArray phoneArray = obj["phone_numbers"].toArray();
    for (const QJsonValue& val : phoneArray) {
        QJsonObject phoneObj = val.toObject();
        PhoneNumber phone;
        phone.type = phoneObj["type"].toString();
        phone.number = phoneObj["number"].toString();
        person.phoneNumbers.append(phone);
    }

    return true;
}


*/


/*
使用nlohmann
#include <iostream>
#include <nlohmann/json.hpp>

int main() {
    // JSON 字符串
    std::string json_str = R"({
        "name": "Alice",
        "age": 25,
        "is_student": false,
        "skills": ["C++", "Python", "JavaScript"]
    })";

    // 解析 JSON 字符串为 JSON 对象
    nlohmann::json json_obj = nlohmann::json::parse(json_str);

    // 访问 JSON 数据
    std::string name = json_obj["name"];
    int age = json_obj["age"];
    bool is_student = json_obj["is_student"];
    std::vector<std::string> skills = json_obj["skills"];

    // 输出结果
    std::cout << "Name: " << name << "\n";
    std::cout << "Age: " << age << "\n";
    std::cout << "Is student: " << (is_student ? "Yes" : "No") << "\n";
    std::cout << "Skills: ";
    for (const auto& skill : skills) {
        std::cout << skill << " ";
    }
    std::cout << std::endl;

    return 0;
}



*/
