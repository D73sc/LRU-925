#ifndef ZMOTIONCONTROL_H
#define ZMOTIONCONTROL_H
#pragma execution_character_set("utf-8")
#include "zmotion.h"
#include "zmcaux.h"
#include "QDebug"
#include <QThread>
#include <QObject>
#include <QTimer>
#include <vector>
#include <array>

enum AxisMType : int
{
    Idle = 0,     //没有运动
    Move = 1,     //单轴直线或者插补直线运动
    MoveAbs = 2,  //绝对值单轴直线或者绝对插补直线运动
    MheliCal = 3, //圆心螺旋运动
    MoveCirc =4,  // 圆弧插补
    MoveModify = 5,// 修改运动位置
    MoveSp =6,      // SP速度的单轴直线或者SP速度的插补直线运动
    MoveAbsSp =7,   //SP速度的绝对值单轴直线/SP速度的绝对插补直线运动
    MoveCircSp =8,  //SP速度的圆弧插补
    MheliCalSp =9,  //SP速度的圆心螺旋运动
    ForwardVmove =10,//正向持续运动
    ReverseVmove =11, //负向持续运动
    DatuMing =12,    //回零运动中
    Cam =13,         //凸轮表运动
    FWD_Jog =14,     //映射正向JOG运动
    REV_Jog =15,     //映射负向JOG运动
    Cam_Box =20,     //跟随凸轮表运动
    Connect =21,     //同步运动
    MoveLink =22,     //自动凸轮运动
    ConnPath =23,      //同步运动2，矢量类型的
    MoveSLink =25,     //自动凸轮运动2
    MoveSpiRal =26,     //渐开线圆弧
    Meclpse =27,      //椭圆运动
    Move_Aout =28,    //缓冲IO/缓冲寄存器操作等
    Move_Delay =29, //  缓冲延时
    MspheriCal =31,  //空间圆弧
    Move_PT = 32,   //单位时间内运动距离
    ConnFRame =33,   // 机械手逆解运动
    ConnRefRame = 34,  //机械手正解运动
    EactCycle=65 //ECAT周期位置模式，需支持EtherCAT
};
enum MyAxisSatus : int
{
    正常状态 = 0,
    超过正限位 = 1,
    超过负限位 = 2,
    驱动器错误 = 3,
    轴通讯错误 = 4,
    电源异常 =5,
    外部告警信号 = 6
};
inline QString AxisStatusQstr(MyAxisSatus status)
{
    switch (status)
    {
    case 0:
        return "正常状态";
    case 1:
        return "超过正限位";
    case 2:
        return "超过负限位";
    case 3:
        return "驱动器错误";
    case 4:
        return "轴通讯错误";
    case 5:
        return "电源异常";
    case 6:
        return "外部告警信号";
    default:
        return "未知状态";
    }
}
enum MoveMode : int
{
    /// <summary>
    /// 正向运行
    /// </summary>
    Forward = 1,
    /// <summary>
    /// 反向运动
    /// </summary>
    Backward = -1,
    /// <summary>
    /// 停止或取消运动
    /// </summary>
    Cancel = 2
};
class AxisStatus
{
public:
    AxisStatus();
    AxisStatus(int num_,float posi_=0, float init_=0, float fslimit_=0, float rslimit_=0,float speed_=0,float accel_=0,float decel_=0,float units_=0,MyAxisSatus status_=正常状态,AxisMType mtype_=Idle);
    int num;
    float posi;
    float init;
    float fslimit;
    float rslimit;
    float speed;
    float accel;
    float decel;
    float units;
    MyAxisSatus status;
    AxisMType mtype;

};

    class IOStatus
{
public:

    uint32 IOInputEmergencyStop=0;
    uint32 IOOutputBrake[5];

    //工控机16进16出IO的值
    uint32 IOOutputValue;
    //out:
    //0——启动指示灯
    //1——报警灯
    //2——三色灯黄,代表手动操作
    //3——三色灯绿,代表全自动模式
    //4——三色灯红,代表报警状态
    //8——AGV断电急停
    //9——AGV启动
    //10——一级升降刹车
    //11——复位
    //12——二级升降刹车
    //13——PLC
    int32 IOInputValue;
    //in:
    //0——急停
    //1——模式
    //2——启动
    //3——停止
    //4——复位
    //5——一级下
    //6——一级上
    //7——二级下
    //8——二级上
    int32 IOInputValue_Zmotion;//正运动拓展IO
    //0 和 2 纵移远离电机端限位
    //1 和 3 纵移电机端限位
    //4 横移远离电机端限位
    //5 横移电机端限位
    //6 和 7 托举的限位
    //8 和 9 光纤限位
    //10 接近开关
};

class ZmotionStatus
{
public:

    AxisStatus *allAxisStatus;
    IOStatus* allIOStatus;
};


//正运动控制类
class ZMotionControl:public QObject
{
    Q_OBJECT
private:
    ZMC_HANDLE g_handle;
    QTimer *SecurityCheck;
    ZMotionControl();
    void DataFetcher(std::function<void(ZmotionStatus*)> callback);//回调函数
    static ZMotionControl* instance;
    static std::mutex mtx;
    std::atomic<bool> running;
    std::thread workerThread;
    void onSecuityChecked();

public:
    enum{AxisNum=13};//轴数量
    // int AxisList[AxisNum]={5,6,7,4,3,0,2,1};
    int AxisList[AxisNum]={0,1,2,3,4,6,7,8,9,10,11,12,5};//轴映射
    int IOInputEmergencyStop=0;
    int IOOutputBrake[6]={0,2,4,6,8,10};
    ///
    /// \brief instance单例模式
    /// \return
    ///


    //创建并返回ZMotionControl类的唯一实例
    static ZMotionControl* getinstance() {
        if (instance == nullptr) {
            std::lock_guard<std::mutex> lock(mtx);
            if (instance == nullptr) {
                instance = new ZMotionControl();
            }
        }
        return instance;
    }
    // 禁用拷贝构造和赋值操作符，确保实例唯一
    ZMotionControl(const ZMotionControl&) = delete;
    ZMotionControl& operator=(const ZMotionControl&) = delete;
    ~ZMotionControl();
    // 清理单例实例
    static void cleanup() {
        delete instance;
    }

    // 启动回调线程
    void startCallbackThread(std::function<void(ZmotionStatus*)> callback);

    // 停止回调线程
    void stopCallbackThread();


    ZmotionStatus *myZmotionStatus;//正运动状态指针
    //AxisStatus *allAxisStatus;
    bool IsAllAxisIdle();
    bool IsSingleIdle(int i);
    int EcatInit(int SlotId,int LocalAxisNum,int BusAxisStartId,int ProFileMode);
    int WaitContrFunctComple(const char *cmdbuff, int TimeOut);
    int ConnectSimulation();
    int Connect(int type,const char str[20]);
    void Disconnect();
    bool GetConnectStatus();
    int* SetAxisEnable();
    int* SetAxisDisable();
    void SetAxisUnits(int i,float units);
    void SetAxisSpeed(int i,float speed);
    void SetAxisAccel(int i,float accel);
    void SetAxisDecel(int i,float decel);
    void SetAxisFSLimit(int i,float fslimit);
    void SetAxisRSLimit(int i,float rslimit);
    float GetAxisUnits(int i);
    float GetAxisSpeed(int i);
    float GetAxisAccel(int i);
    float GetAxisDecel(int i);
    float GetAxisFSLimit(int i);
    float GetAxisRSLimit(int i);
    float GetAxisDpos(int i);
    float GetMotorCurrent(int i);//电机电流
    float GetFingerCurrent(int i);//夹爪电流
    void SetMotorTorque(int i, int value);//设置电机扭矩
    int GetMotorTorque(int i);//获取电机扭矩
    int EmergencyStop();
    MyAxisSatus GetAxisStatus(int i);
    void MoveZero(int i);
    void MoveInit(int i);
    void MoveSingleAbs(int i,float position);
    void MoveSingleOpp(int i,float distance);
    void SingleVMove(int i,MoveMode movemod);
    void SetAxisType(int type);
    void EnableMergeChange();//打开连续插补
    void DisenableMergeChange();//打开连续插补

    AxisMType GetAxisMType(int n);

    void MoveAbsTrajectoryThetas(std::vector<std::array<double, 8>> thetas);
    /// \brief 获取指定 I/O 编号的输入值
    /// \param IONum I/O 编号
    /// \param value 输入值（返回参数）
    void GetIOInput(int IONum, uint32& value);
    /// \brief 设置指定 I/O 编号的输出值
    /// \param IONum I/O 编号
    /// \param value 输出值
    // void SetIOOutput(int IONum,const uint32 value);
    /// \brief 获取指定 I/O 编号的输出值
    /// \param IONum I/O 编号
    /// \param value 输出值
    void GetIOOutput(int IONum, uint32 &value);
    ///
    /// \brief JudgeEmergencyStopIO 判断急停开关是否按下
    /// \return 常闭开关，未按下为1，按下为0
    ///
    bool IsPressedEmergencyStopIO();
    ///
    /// \brief SetBrakeEnable打开刹车IO，让刹车上电
    ///
    void SetBrakeEnable();
    ///
    /// \brief SetBrakeDisenable关闭刹车IO，刹车下电
    ///
    void SetBrakeDisenable();

    //设置心跳信号
    void SetHeartBeatInfo(float heartbeatTime);

    float GetHeartBeatInfo();

    //输入IO是从外部设备向运动控制器传输信号，属于 “接收端”。核心作用是采集外部环境或设备的状态信息，为控制器的决策提供依据。
    //输出IO是从运动控制器向外部设备发送信号，属于 “发送端”。核心作用：执行控制器的决策，控制外部设备的动作或状态。

    //读取16个输入IO和16个输出IO
    void GetMultiIOInput(int StartNum, int EndNum, int32& IOvalue);

    void GetMultiIOOutput(int StartNum, int EndNum, uint32& IOvalue);

    //设置输出IO，控制AGV的启动和停止
    void SetIOOutput(int IONum,const uint32 value);


    int SetSingleAxisEnable(int axisIndex, bool enable);

signals:
    void dataFetched(AxisStatus* status); // 定义信号
    void ConnectInfo(QString info);
    void sig_EcatInitFinished(int result);

    //急停按钮被按下信号
    void sig_EmergencyStopPressed();
};


#endif // ZMOTIONCONTROL_H
