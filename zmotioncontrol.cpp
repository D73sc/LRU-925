#include "zmotioncontrol.h";
ZMotionControl* ZMotionControl::instance = nullptr;
std::mutex ZMotionControl::mtx;
ZMotionControl::ZMotionControl() //构造函数
{

    running=false;
    g_handle=NULL;
    //正运动控制类对象被实例化的同时，myZmotionStatus allAxisStatus 被初始化
    myZmotionStatus=new ZmotionStatus;//ZMotionControl 类对象的成员变量
    myZmotionStatus->allAxisStatus=new AxisStatus[AxisNum];//myZmotionStatus的成员变量 allAxisStatus
    for(int i=0;i<AxisNum;i++)
    {
        myZmotionStatus->allAxisStatus[i]=AxisStatus(AxisList[i]);//allAxisStatus[i] 是 AxisStatus类型的数组，每一个元素都是AxisStatus的一个类对象
    }
    myZmotionStatus->allIOStatus = new IOStatus;
}


ZMotionControl::~ZMotionControl() 
{
    // 1. 停止所有活动操作
    stopCallbackThread();

    // 2. 安全停止定时器

    // 3. 释放硬件资源
     Disconnect();
    


    // 4. 释放动态内存
    //delete myZmotionStatus;

    //cleanup();
    // 不需要设置为nullptr，因为对象即将销毁
    //SecurityCheck->stop();

    // 5. 清理单例引用
    //if (instance == this) {
    //    instance = nullptr;
    //}
}

//callback函数对象，它可以 "容纳" void 函数名(ZmotionStatus* 参数名);
//&ZMotionControl::DataFetcher：要在新线程中执行的函数 ZMotionControl 类的成员函数 DataFetcher 的地址
//this：指向当前 ZMotionControl 实例的指针（因为 DataFetcher 是成员函数，需要指定具体哪个对象调用它）
//callback：传递给 DataFetcher 函数的参数

void ZMotionControl::startCallbackThread(std::function<void (ZmotionStatus*)> callback)//第一重回调
{
    if (!running) {
        running = true;
        workerThread = std::thread(&ZMotionControl::DataFetcher, this, callback);//DataFetcher是第二重回调，其中的callback是这里传入的回调函数的副本，它们逻辑相同
    }
}

void ZMotionControl::stopCallbackThread() {
    running = false;

    if (workerThread.joinable()) {
        workerThread.join();
    }
}

bool ZMotionControl::IsAllAxisIdle()
{
    if(!GetConnectStatus())
        return false;
    for(int i=0;i<AxisNum;i++)
    {
        //if(allAxisStatus[i].mtype!=0)
        if(!IsSingleIdle(i))
            return false;
    }
    return true;

}


//读取轴的运动是否结束，0-运动中 -1 停止
bool ZMotionControl::IsSingleIdle(int i)
{
    int Idle = 0;
    ZAux_Direct_GetIfIdle(g_handle,AxisList[i],&Idle);
    if(!Idle) return false;
    return Idle;
}


//void(ZmotionStatus*) 表示callback回调函数：无返回值（void）；接收一个 ZmotionStatus* 类型的指针形参
void ZMotionControl::DataFetcher(std::function<void(ZmotionStatus*)> callback)
{
    bool lastEmergencyPressed = false; //记录上一次急停状态，防止一直重复弹窗

    while(running)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));  // 每隔 50ms 执行一次
        if(!GetConnectStatus())
            continue;
        for(int i=0;i<AxisNum;i++)
        {
            myZmotionStatus->allAxisStatus[i].posi=GetAxisDpos(i);
            myZmotionStatus->allAxisStatus[i].fslimit=GetAxisFSLimit(i);
            myZmotionStatus->allAxisStatus[i].rslimit=GetAxisRSLimit(i);
            myZmotionStatus->allAxisStatus[i].status=GetAxisStatus(i);
            myZmotionStatus->allAxisStatus[i].mtype=GetAxisMType(i);
        }
        GetIOInput(IOInputEmergencyStop,myZmotionStatus->allIOStatus->IOInputEmergencyStop);

        //判断急停按钮是否被按下
        //常闭开关，未按下为1，按下为0
        bool currentEmergencyPressed = (myZmotionStatus->allIOStatus->IOInputEmergencyStop == 0);

        //只在按下的瞬间触发一次
        if(currentEmergencyPressed && !lastEmergencyPressed)
        {
            EmergencyStop();
            emit sig_EmergencyStopPressed();
        }

        lastEmergencyPressed = currentEmergencyPressed;

        //循环读取16进16出IO状态，分别存放在myZmotionStatus->allIOStatus->IOInputValue和myZmotionStatus->allIOStatus->IOInputValue
        GetMultiIOInput(0,15,myZmotionStatus->allIOStatus->IOInputValue);
        GetMultiIOOutput(0,15,myZmotionStatus->allIOStatus->IOOutputValue);

        //汇川拓展IO模块 In口  OUT口对应144-159
        GetMultiIOInput(128,143,myZmotionStatus->allIOStatus->IOInputValue_Zmotion);


        //qDebug()<<myZmotionStatus->allIOStatus->IOInputEmergencyStop;
        //这里相当于给主界面抛出带有IO状态的信号。
        callback(myZmotionStatus);
        //emit dataFetched(allAxisStatus); // 发出数据获取信号
    }
}

void ZMotionControl::onSecuityChecked()
{
    if(!GetConnectStatus())
        return;
    if(IsPressedEmergencyStopIO())
    {
        EmergencyStop();
    }
}

/**
*@brief Qt总线初始化
*@param[in] 槽位号,本地轴数目,总线轴起始轴号，PDO模式
*@return 0：执行正常、    非0：执行异常
*@ingroup
*@see
*@note
*/
int ZMotionControl::EcatInit(int SlotId,int LocalAxisNum,int BusAxisStartId,int ProFileMode)
{
    char CmdBuff[64]={0},ReceBuff[256];
    float VirtualAxises=0;
    int EcatInitStatus=-1;
    int TimeOut=3000,TimeOutFlag=3000;
    float NodeNum=0;

    // IO板配置参数
    int IO_BLOCK = 0;                  // 纯IO板计数
    const int IO_BLOCK_STAR = 128;     // IO板起始IO地址
    const int IO_PER_BLOCK = 128;      // 每个IO板最大支持IO数

    // 延迟3秒，等待驱动器上电
    QThread::sleep(3);
    // 获取总线通讯周期
    ZAux_Execute(g_handle,"?SERVO_PERIOD", ReceBuff,256 );
    qDebug()<<"总线通讯周期(us)："<<ReceBuff;
    // 初始化还原轴类型
    ZAux_Direct_Rapidstop(g_handle,2);

    ZAux_Execute(g_handle,"Table(0)=SYS_ZFEATURE(0)", ReceBuff,256 );
    ZAux_Direct_GetTable(g_handle, 0, 1, &VirtualAxises);
    if(VirtualAxises>=64)
    {
        VirtualAxises=63;
    }

    for(int i=0;i<VirtualAxises;i++)
    {
        ZAux_Direct_SetAxisAddress(g_handle,i,0);
        ZAux_Direct_SetAxisEnable(g_handle,i,0);
        ZAux_Direct_SetAtype(g_handle,i,0);
        int Idle=0;
        TimeOutFlag =TimeOut;
        while(TimeOutFlag>0)
        {
            ZAux_Direct_GetIfIdle(g_handle,i,&Idle);
            if(Idle)
            {
                break;
            }
            QThread::msleep(10);
            TimeOutFlag = TimeOutFlag-10;
        }
    }
    // 本地轴重新映射
    for(int i=0;i<LocalAxisNum;i++)
    {
        ZAux_Direct_SetAxisAddress(g_handle,i,(-1<<16)+i);
        ZAux_Direct_SetAtype(g_handle,i,1);
    }
    // 停止总线
    sprintf(CmdBuff,"SLOT_STOP(%d)",SlotId);
    ZAux_Execute(g_handle,CmdBuff, ReceBuff,256 );
    QThread::msleep(200);
    // 总线时钟优化
    ZAux_Execute(g_handle,"SYSTEM_ZSET = SYSTEM_ZSET OR 128", ReceBuff,256 );
    // 扫描总线
    sprintf(CmdBuff,"SLOT_SCAN(%d) ?return",SlotId);
    ZAux_Execute(g_handle,CmdBuff, ReceBuff,256 );
    QThread::sleep(3);
    ReceBuff[2]=0;
    if(0 == strcmp("-1",ReceBuff))
    {
        TimeOutFlag = TimeOut;
        while(TimeOutFlag>0)
        {
            sprintf(CmdBuff,"Table(0)=NODE_COUNT(%d)",SlotId);
            ZAux_Execute(g_handle,CmdBuff, ReceBuff,256 );
            ZAux_Direct_GetTable(g_handle, 0, 1, &NodeNum);
            if(NodeNum>0)
            {
                break;
            }
            QThread::msleep(10);
            TimeOutFlag = TimeOutFlag-10;
        }
        qDebug()<<"总线扫描成功，连接设备数："<<NodeNum;
        QString info = QString("总线扫描成功，连接设备数：%1").arg(NodeNum);
        emit ConnectInfo(info);

        // 初始化节点，并根据节点有无轴数区分驱动器和纯IO板，做出不同配置
        if(NodeNum>=0)
        {
            int BusAxis_Num=0; // 总线轴总数
            QList<QPair<int, int>> IOBoardList; // 存储IO板信息，<设备节点号, 起始IO地址>

            for(int i=0; i<NodeNum;i++)
            {
                // 读取节点轴数
                float NodeAxisNum=0;
                sprintf(CmdBuff,"Table(0)=NODE_AXIS_COUNT(%d,%d)",SlotId,i);
                ZAux_Execute(g_handle,CmdBuff, ReceBuff,256 );
                ZAux_Direct_GetTable(g_handle, 0, 1, &NodeAxisNum);

                //判定驱动器和纯IO板
                if(NodeAxisNum > 0)//驱动器
                {
                    for(int j=0;j<NodeAxisNum;j++)
                    {
                        ZAux_Direct_SetAxisAddress(g_handle,BusAxisStartId+BusAxis_Num,BusAxis_Num+1);
                        ZAux_Direct_SetAtype(g_handle,BusAxisStartId+BusAxis_Num,65);
                        sprintf(CmdBuff,"DRIVE_PROFILE(%d) = %d",BusAxisStartId+BusAxis_Num,ProFileMode);
                        ZAux_Execute(g_handle,CmdBuff, ReceBuff,256 );
                        sprintf(CmdBuff,"DISABLE_GROUP(%d)",BusAxisStartId+BusAxis_Num);
                        ZAux_Execute(g_handle,CmdBuff, ReceBuff,256 );
                        BusAxis_Num = BusAxis_Num+1;
                    }
                }
                else if(NodeAxisNum == 0) //纯IO板
                {
                    // 计算当前IO板的起始IO地址
                    int ioStartAddr = IO_BLOCK_STAR + IO_BLOCK * IO_PER_BLOCK;

                    // 配置IO板起始IO地址
                    sprintf(CmdBuff,"NODE_IO(%d,%d) = %d",SlotId, i, ioStartAddr);
                    ZAux_Execute(g_handle,CmdBuff, ReceBuff,256 );

                    // 记录IO板信息
                    IOBoardList.append(qMakePair(i, ioStartAddr));

                    qDebug()<<"识别到纯IO板，设备节点号："<<i<<" 起始IO地址："<<ioStartAddr;
                    QString ioInfo = QString("识别到纯IO板，设备节点号：%1，起始IO地址：%2").arg(i).arg(ioStartAddr);
                    emit ConnectInfo(ioInfo);

                    // IO板计数+1
                    IO_BLOCK++;
                }
            }

            EcatInitStatus = 0;
            qDebug()<<"轴扫描映射完成，连接总线轴数："<<BusAxis_Num;
            QString info1 = QString("轴扫描映射完成，连接总线轴数：%1").arg(BusAxis_Num);
            emit ConnectInfo(info1);

            // IO板汇总信息
            if(!IOBoardList.isEmpty())
            {
                qDebug()<<"共识别到"<<IOBoardList.size()<<"块纯IO板：";
                for(auto &ioBoard : IOBoardList)
                {
                    int node = ioBoard.first;
                    int startAddr = ioBoard.second;
                    int endAddr = startAddr + IO_PER_BLOCK - 1; // IO地址范围
                    qDebug()<<"  - 节点"<<node<<": IO地址范围"<<startAddr<<"-"<<endAddr;
                }
                emit ConnectInfo(QString("共识别到%1块纯IO板").arg(IOBoardList.size()));
            }

            // 开启总线
            QThread::msleep(100);
            sprintf(CmdBuff,"SLOT_START(%d) ?return",SlotId);
            ZAux_Execute(g_handle,CmdBuff, ReceBuff,256 );
            ReceBuff[2]=0;
            if(0 == strcmp("-1",ReceBuff))
            {
                qDebug()<<"总线开启正常";
                QThread::sleep(3);
                int AxisList[1];
                for(int i=BusAxisStartId;i<BusAxisStartId+BusAxis_Num;i++)
                {
                    AxisList[0]=i;
                    ZAux_Direct_Base(g_handle, 1,AxisList);
                    ZAux_Execute(g_handle,"DRIVE_CLEAR(0)", ReceBuff,256 );
                    QThread::msleep(10);
                    sprintf(CmdBuff,"DRIVE_CONTROLWORD(%d) = 128 ",i);
                    ZAux_Execute(g_handle,CmdBuff, ReceBuff,256 );
                    QThread::msleep(10);
                    sprintf(CmdBuff,"DRIVE_CONTROLWORD(%d)=6 ",i);
                    ZAux_Execute(g_handle,CmdBuff, ReceBuff,256 );
                    QThread::msleep(10);
                    sprintf(CmdBuff,"DRIVE_CONTROLWORD(%d)=15 ",i);
                    ZAux_Execute(g_handle,CmdBuff, ReceBuff,256 );
                    QThread::msleep(10);
                }
                QThread::msleep(100);
                ZAux_Execute(g_handle,"DATUM(0)", ReceBuff,256 );
                qDebug()<<"控制器报警清除完成";
                emit ConnectInfo("控制器报警清除完成");
                QThread::msleep(1000);
                ZAux_Execute(g_handle,"WDOG = 1", ReceBuff,256 );
                for(int i=BusAxisStartId;i<BusAxisStartId+BusAxis_Num;i++)
                {
                    ZAux_Direct_SetAxisEnable(g_handle,i,1);
                    QThread::msleep(10);
                }
                qDebug()<<"伺服使能完成";
                emit ConnectInfo("伺服使能完成");
                EcatInitStatus = 1;
            }
            else
            {
                EcatInitStatus=-1;
                qDebug()<<"总线开启异常";
                emit ConnectInfo("总线开启异常");
            }
        }
    }
    return EcatInitStatus;
}
/**
*@brief 等待控制器函数执行完成
*@param[in] 函数执行完成的标志位变量名字符串（该变量在Basic定义）
*@return 0：执行正常、    非0：执行异常
*@ingroup
*@see
*@note
*/
int ZMotionControl::WaitContrFunctComple(const char *cmdbuff, int TimeOut)
{
    int FunctCompleFlag=0;  //函数执行标志 1：执行完成 0：执行未完成
    int Count=0;            //计时器
    int Err=0;              //Zmotion函数返回值
    //等待控制器函数执行完成
    while(0 == FunctCompleFlag)
    {
        Err = ZAux_Direct_GetVariableInt(g_handle,cmdbuff,&FunctCompleFlag);
        QThread::msleep(10);
        Count++;
        if((TimeOut/10) < Count)
        {
            //等待时间大于5s退出循环
            Err=1;
            break;
        }
    }
    //如果FunctCompleFlag 不等于1不是Basic那边函数执行有错误
    if(FunctCompleFlag != 1 )
    {
        Err = FunctCompleFlag;
    }
    return Err;
}
///
/// \brief ZMotionControl::ConnectSimulation
/// 连接仿真器
/// \return 0为成功1为失败
///
int ZMotionControl::ConnectSimulation()
{
    int rint =ZAux_OpenEth((char*)"127.0.0.1", &g_handle);
    qDebug()<<g_handle;
    return rint;
}
///
/// \brief ZMotionControl::Connect
/// \param type
///    ZMC_CONNECTION_COM = 1,
///    ZMC_CONNECTION_ETH = 2,
///    ZMC_CONNECTION_USB = 3,
///    ZMC_CONNECTION_PCI = 4,
///    ZMC_CONNECTION_LOCAL = 5,
/// \param str
/// 连接字符串
/// \return 0为成功1为失败
///
int ZMotionControl::Connect(int type,const char  str[20])
{
    ZMC_CONNECTION_TYPE zmtype;
    switch(type)
    {
    case 1:
        zmtype=ZMC_CONNECTION_COM;
        break;
    case 2:
        zmtype=ZMC_CONNECTION_ETH;
        break;
    case 3:
        zmtype=ZMC_CONNECTION_USB;
        break;
    case 4:
        zmtype=ZMC_CONNECTION_PCI;
        break;
    case 5:
        zmtype=ZMC_CONNECTION_LOCAL;
        break;

    }
    char str1[20];
    strcpy(str1,str);


    int rint = ZMC_Open(zmtype,str1,&g_handle);
    return rint;//0表示连接成功，非0表示连接失败
}

void ZMotionControl::Disconnect()
{

    g_handle = NULL;

}
///检测是否连接控制器
///true为成功false为失败
bool ZMotionControl::GetConnectStatus()
{
    if (g_handle != NULL)
    {
        return true;
    }
    return false;
}
///
/// \brief ZMotionControl::SetAxisEnable
/// 使能所有轴
/// \return 错误码
///
int* ZMotionControl::SetAxisEnable()
{
    int* err = new int[AxisNum];
    if(!GetConnectStatus())
        return false;

    for(int i=0;i<AxisNum;i++)
        err[i]=ZAux_Direct_SetAxisEnable(g_handle,AxisList[i],1);
    return err;
}
///
/// \brief ZMotionControl::SetAxisDisable
/// 取消使能所有轴
/// \return 错误码
///
int* ZMotionControl::SetAxisDisable()
{
    int* err = new int[AxisNum];
    if(!GetConnectStatus())
        return false;

    for(int i=0;i<AxisNum;i++)
        err[i]=ZAux_Direct_SetAxisEnable(g_handle,AxisList[i],0);
    return err;

}

void ZMotionControl::SetAxisUnits(int i,float units)
{
    ZAux_Direct_SetUnits(g_handle,AxisList[i],units);
}

void ZMotionControl::SetAxisSpeed(int i, float speed)
{
    ZAux_Direct_SetSpeed(g_handle,AxisList[i],speed);
}

void ZMotionControl::SetAxisAccel(int i, float accel)
{
    ZAux_Direct_SetAccel(g_handle,AxisList[i],accel);
}

void ZMotionControl::SetAxisDecel(int i, float decel)
{
    ZAux_Direct_SetDecel(g_handle,AxisList[i],decel);

}

void ZMotionControl::SetAxisFSLimit(int i, float fslimit)
{
    ZAux_Direct_SetFsLimit(g_handle,AxisList[i],fslimit);

}

void ZMotionControl::SetAxisRSLimit(int i, float rslimit)
{
    ZAux_Direct_SetRsLimit(g_handle,AxisList[i],rslimit);

}

float ZMotionControl::GetAxisUnits(int i)
{
    float a;
    ZAux_Direct_GetUnits(g_handle,AxisList[i],&a);
    return a;

}

float ZMotionControl::GetAxisSpeed(int i)
{
    float a;
    ZAux_Direct_GetSpeed(g_handle,AxisList[i],&a);
    return a;
}

float ZMotionControl::GetAxisAccel(int i)
{
    float a;
    ZAux_Direct_GetAccel(g_handle,AxisList[i],&a);
    return a;
}

float ZMotionControl::GetAxisDecel(int i)
{
    float a;
    ZAux_Direct_GetDecel(g_handle,AxisList[i],&a);
    return a;
}

float ZMotionControl::GetAxisFSLimit(int i)
{
    float a;
    ZAux_Direct_GetFsLimit(g_handle,AxisList[i],&a);
    return a;
}

float ZMotionControl::GetAxisRSLimit(int i)
{
    float a;
    ZAux_Direct_GetRsLimit(g_handle,AxisList[i],&a);
    return a;
}


float ZMotionControl::GetMotorCurrent(int i)
{
    int32 a;

    //电流的数据类型是int16,4代表读取类型为int32。读取int16后解析为int32,高16位补充为0，然后再截取低16后，可以确保数据信息完整
    ZAux_BusCmd_SDORead(g_handle, 0, i, 0x6078, 0x00, 4, &a);

    int16_t b = static_cast<int16_t>(a & 0xFFFF); //截取低16位

    float result = (b*80)/(1.414*2048.0);

    return result;
}


float ZMotionControl::GetFingerCurrent(int i)//夹爪电流
{
    int32 a;

    ZAux_BusCmd_SDORead(g_handle, 0, i, 0x6078, 0x00, 4, &a);

    int16_t b = static_cast<int16_t>(a & 0xFFFF); //截取低16位

    float result = (b*80)/(1.414*2048.0);

    return result;
}

//获取电机扭矩
int ZMotionControl::GetMotorTorque(int i)
{
    int32 a;

    ZAux_BusCmd_SDORead(g_handle, 0, i, 0x6077, 0x00, 4, &a);

    int16_t b = static_cast<int16_t>(a & 0xFFFF); //截取低16位

    return b;
}


//设置电机扭矩
void ZMotionControl::SetMotorTorque(int i,int value)
{
    ZAux_BusCmd_SDOWrite(g_handle, 0, i, 0x6077, 0x00, 4, value);
}


float ZMotionControl::GetAxisDpos(int i)
{
    float a;
    ZAux_Direct_GetDpos(g_handle,AxisList[i],&a);
    return a;
}

int ZMotionControl::EmergencyStop()
{
    if (0 == ZAux_Direct_Rapidstop(g_handle, 2))
    {
        return true;
    }

    return false;
}


//获取轴状态，填充错误信息
MyAxisSatus ZMotionControl::GetAxisStatus(int i)
{
    int axisstate=0;
    ZAux_Direct_GetAxisStatus(g_handle, AxisList[i], &axisstate);
    //return static_cast<AxisSatus>(axisstate);
        if (axisstate == 0)
        {
            return 正常状态;
        }
        else if (((axisstate >> 9) & 1) == 1)
        {
            return 超过正限位;
        }
        else if (((axisstate >> 10) & 1) == 1)
        {
            return 超过负限位;
        }
        else if (((axisstate >> 3) & 1) == 1)
        {
            return 驱动器错误;
        }
        else if (((axisstate >> 2) & 1) == 1)
        {
            return 轴通讯错误;
        }
        else if (((axisstate >> 18) & 1) == 1)
        {
            return 电源异常;
        }
        else if (((axisstate >>22) & 1) == 1)
        {
            return 外部告警信号;
        }
        return 正常状态;
}


//
AxisMType ZMotionControl::GetAxisMType(int n)
{
    int mt;
    ZAux_Direct_GetMtype(g_handle,AxisList[n],&mt);
    AxisMType amt=(AxisMType)mt;
    return amt;
}

//指定轴移动到零位
void ZMotionControl::MoveZero(int i)
{
    float poslist[1]{0};
    int axislist[1]{AxisList[i]};

    ZAux_Direct_MoveAbs(g_handle,1,axislist,poslist);
}

//指定轴移动到设定位置
void ZMotionControl::MoveInit(int i)
{
    float poslist[1]{myZmotionStatus->allAxisStatus[i].init};
    int axislist[1]{AxisList[i]};

    ZAux_Direct_MoveAbs(g_handle,1,axislist,poslist);

}

//单轴绝对位置移动
void ZMotionControl::MoveSingleAbs(int i, float position)
{
    int axislist[1]{AxisList[i]};
    float poslist[1]{position};
    ZAux_Direct_MoveAbs(g_handle,1,axislist,poslist);

}

//单轴相对位置移动
void ZMotionControl::MoveSingleOpp(int i,float distance)
{
    int axislist[1]{AxisList[i]};
    float dislist[1]{distance};
    ZAux_Direct_Move(g_handle,1,axislist,dislist);
}


void ZMotionControl::SingleVMove(int i, MoveMode movemod)
{
    if(movemod==Cancel)
    {

        ZAux_Direct_Single_Cancel(g_handle, AxisList[i], 2);
    }
    else
        ZAux_Direct_Single_Vmove(g_handle, AxisList[i], (int)movemod);
}

void ZMotionControl::SetAxisType(int type)
{
    for(int i=0;i<AxisNum;i++)
    {
        ZAux_Direct_SetAtype(g_handle,AxisList[i],type);
    }
}

void ZMotionControl::EnableMergeChange()
{
    for(int i=0;i<AxisNum;i++)
    {
        ZAux_Direct_SetMerge(g_handle,AxisList[i],1);
    }
}

void ZMotionControl::DisenableMergeChange()
{
    for(int i=0;i<AxisNum;i++)
    {
        ZAux_Direct_SetMerge(g_handle,AxisList[i],0);
    }
}


void ZMotionControl::MoveAbsTrajectoryThetas(std::vector<std::array<double, 8>> thetas)
{

    float poslist[8];
    int axislist[8] = {
        AxisList[0], AxisList[1], AxisList[2], AxisList[3],
        AxisList[4], AxisList[5], AxisList[6], AxisList[7]
    };

        for (const auto& pos : thetas) {
            for (int i = 0; i < 8; ++i) {

                if(i<3)
                    poslist[i] = static_cast<float>(pos[i]);
                else
                    poslist[i] = static_cast<float>(pos[i]*180/3.1415926);
                if (i == 1)
                    poslist[i] -= 200;
                if(i==2)
                    poslist[i] += 400;
                // if (i == 7)
                // {
                //     if(poslist[i]<=0)
                //         poslist[i] += 180;
                //     else
                //         poslist[i] -= 180;

                // }


            }
            ZAux_Direct_MoveAbs(g_handle, 8, axislist, poslist);
        }


}


//16进16出IO信号的获取和设置
void ZMotionControl::GetIOInput(int IONum, uint32 &value)
{
    ZAux_Direct_GetIn(g_handle,IONum,&value);
}

void ZMotionControl::GetIOOutput(int IONum,  uint32 &value)
{
    ZAux_Direct_GetOp(g_handle,IONum,&value);

}

void ZMotionControl::GetMultiIOInput(int StartNum, int EndNum, int32 &IOvalue)
{
    ZAux_Direct_GetInMulti(g_handle, StartNum , EndNum, &IOvalue);
}

void ZMotionControl::GetMultiIOOutput(int StartNum, int EndNum, uint32 &IOvalue)
{
    ZAux_Direct_GetOutMulti(g_handle, StartNum , EndNum, &IOvalue);
}

void ZMotionControl::SetIOOutput(int IONum, const uint32 value)
{
    ZAux_Direct_SetOp(g_handle,IONum,value);
}

///
/// \brief JudgeEmergencyStopIO 判断急停开关是否按下
/// \return 常闭开关，未按下为1，按下为0
///
bool ZMotionControl::IsPressedEmergencyStopIO()
{
    // uint32 emergencyStopValue=2;
    //GetIOInput(IOInputEmergencyStop,emergencyStopValue);

    if(myZmotionStatus->allIOStatus->IOInputEmergencyStop==1)
        return false;
    else
        return true;
}

void ZMotionControl::SetBrakeEnable()
{
    for(int i=0;i< (sizeof(IOOutputBrake) / sizeof(IOOutputBrake[0]));i++)
        SetIOOutput(IOOutputBrake[i],1);
}

void ZMotionControl::SetBrakeDisenable()
{
    for(int i=0;i<(sizeof(IOOutputBrake) / sizeof(IOOutputBrake[0]));i++)
        SetIOOutput(IOOutputBrake[i],0);
}

AxisStatus::AxisStatus()
{

}

AxisStatus::AxisStatus(int num_,float posi_, float init_, float fslimit_, float rslimit_,float speed_,float accel_,float decel_,float units_,MyAxisSatus status_,AxisMType mtype_)
{
    num=num_;
    posi=posi_;
    init=init_;
    status=status_;
    mtype=mtype_;
    fslimit=fslimit_;
    rslimit=rslimit_;
    speed=speed_;
    accel=accel_;
    decel=decel_;
    units=units_;
}


//写入和读取心跳信息
void ZMotionControl::SetHeartBeatInfo(float value)
{
    ZAux_Direct_SetVrf(g_handle, 0, 1, &value);
}


float ZMotionControl::GetHeartBeatInfo()
{
    float value;
    ZAux_Direct_GetVrf(g_handle, 0, 1, &value);
    return value;
}


//控制单轴使能
int ZMotionControl::SetSingleAxisEnable(int axisIndex, bool enable)
{
    if(!GetConnectStatus()) return -1; // 控制器未连接

    int value = enable ? 1 : 0; // 1=使能，0=断能
    return ZAux_Direct_SetAxisEnable(g_handle, AxisList[axisIndex], value);
}