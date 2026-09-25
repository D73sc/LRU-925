#ifndef DATATYPE_H
#define DATATYPE_H
//#include <QObject>
#include <QString>
#include <vector>
#include <QMetaType>
namespace nVehicleDataType
{
    enum class BlockReason:int
    {
        Ultrasonic=0,//超声
        Laser=1,      //激光
        Fallingdown=2,//防跌落传感器
        Collision=3, //碰撞传感器
        Infrared=4,//红外传感器
        Lock=5,   //锁车开关
        DynamicBlock=6,//动态障碍物
        VirtualLaser=7, //虚拟激光
        ThreeDCamera=8,//3D 相机
        DistanceSensor=9, //距离传感器
        DILaser=10      //DI 超声

    };
    //Q_ENUM_NS(BlockReason);
    enum class TaskStatus:int //小车实时的任务状态
    {
        NONE = 0, //
        WAITING = 1,//(目前不可能出现该状态),
        RUNNING = 2,//
        SUSPENDED = 3,//
        COMPLETED =4,//
        FAILED =5,  //
        CANCELED =6, //
        OVERTIME = 7 // 兼容taskListStatus
    };
    //Q_ENUM_NS(TaskStatus);
    enum class TcpSvrType:int
    {
        PushSvr = 0,
        NavigateSvr =1,
        StatusSvr =2,
        ControlSvr =3
    };

    //新增重定位信息
    enum class RelocStatus:int
    {
        RELOC_INIT = 0, // 重定位初始化中
        RELOC_SUCCESS = 1, // 重定位成功
        RELOC_RELOCING = 2, // 正在重定位
        LOADMAP_LOADING = 3 // 地图载入中
    };

    //新增地图载入信息
    enum class LoadmapStatus:int
    {
        FAILED = 0, // 载入地图失败
        SUCCESS = 1, // 载入地图成功
        LOADING = 2 // 正在载入地图
    };

    //传感器类型
    enum SensorType {
        BATTERY_SENSOR,// 锂能源电池
        FORCE_SENSOR,
        ANGLE_SENSOR,
        DISTANCE_SENSOR
    };

    inline QString RelocStatus2Qstr(RelocStatus status)
    {
        switch (status)
        {
        case RelocStatus::RELOC_INIT:
            return "重定位初始化中";
        case RelocStatus::RELOC_SUCCESS:
            return "重定位成功";
        case RelocStatus::RELOC_RELOCING:
            return "正在重定位";
        case RelocStatus::LOADMAP_LOADING:
            return "地图载入中";
        default:
            return "未知状态";
        }
    }

    inline QString LoadmapStatus2Qstr(LoadmapStatus status)
    {
        switch (status)
        {
        case LoadmapStatus::FAILED:
            return "载入地图失败";
        case LoadmapStatus::SUCCESS:
            return "载入地图成功";
        case LoadmapStatus::LOADING:
            return "正在载入地图";
        default:
            return "未知状态";
        }
    }

    struct VehErrors
    {
        int code;
        QString date_time;
        QString desc;
        int times;
    };

    struct TasklistStatus
    {
        int taskid;
        QString taskListName;
        TaskStatus task_status;
    };


    struct PGVInfo//二维码信息
    {
        int device_address;
        int error_code;
        bool id_DMT_detected;
        bool is_absolute_X_pos_valid;
    };//device_address：设备的地址，对特定设备进行唯一标识；error_code：存储错误代码，判断设备当前状态；
      //id_DMT_detected：标志位，指示是否检测到二维码ID；is_absolute_X_pos_valid指示绝对 X 坐标是否有效

    struct VehicleSts//先暂时定义这么多状态数据，后面有需要可以再增加
    {
        VehicleSts() {}
        bool blocked;
        BlockReason block_reason;
        float battery_level;     //机器人电池电量, 范围 [0, 1]
        TaskStatus task_status;
        bool is_stop;             //机器人底盘是否静止（行走电机）
        bool emergency;            //true 表示急停按钮处于激活状态(按下), false 表示急停按钮处于非激活状态(拔起)
        bool charging;              //电池是否正在充电
        std::vector<VehErrors> errors; 
        QString current_station;   //离机器人最近站点的 id
        QString current_map;       // 当前地图名
        bool brake;                //是否抱闸
        bool soft_emc;             //是否处于软急停状态
        int update_reason; //
        TasklistStatus tasklist_status; //TODO:需要确认是数据还是单个，如果是数组则需要用vector装载list,20250503,单个Jason对象
        bool slowed; //机器人是否减速
        BlockReason slow_reason;   //减速的原因
        int task_type;         //导航类型, 0 = 没有导航, 1 = 自由导航到任意点, 2 = 自由导航到站点, 3 = 路径导航到站点, 7 = 平动转动, 100 = 其他
        int target_dist; //剩余路径曲线的总长，m
        std::vector<QString> finished_path; //
        std::vector<QString> unfinished_path; //
        std::vector<PGVInfo> pgvs; //二维码检测
        bool driver_emc;    //true 表示电机驱动器发生急停, false 表示电机驱动器发生未急停
        bool electric; //true 表示继电器开启, false 表示继电器关闭
        //新增
        RelocStatus reloc_status; // 重定位状态
        LoadmapStatus loadmap_status; // 地图载入状态
        QString target_station;//机器人目标站点的id
        bool locked = false;//手柄是否激活
        QString nickName;//控制权所有者名字

    };
    inline QString BlockReason2Qstr(BlockReason param)
    {

        QString str = "超声";

        switch (param)
        {
        case BlockReason::Ultrasonic:
            str = "超声";
            break;
        case BlockReason::Laser:
            str = "激光";
            break;
        case BlockReason::Fallingdown:
            str = "防跌落传感器";
            break;
        case BlockReason::Collision:
            str = "碰撞传感器";
            break;
        case BlockReason::Infrared:
            str = "红外传感器";
            break;
        case BlockReason::Lock:
            str = "锁车开关";
            break;
        case BlockReason::DynamicBlock:
            str = "动态障碍物";
            break;
        case BlockReason::VirtualLaser:
            str = "虚拟激光点";
            break;
        case BlockReason::ThreeDCamera:
            str = "3D 相机";
            break;
        case BlockReason::DistanceSensor:
            str = "距离传感器";
            break;
        case BlockReason::DILaser:
            str = "DI 超声";
            break;
        default:
            break;
        }

        return str;
    }
    inline QString TaskStatus2Qstr(TaskStatus param)
    {
        QString str = "NONE";

        switch (param) {
        case TaskStatus::NONE:
            str = "NONE";
            break;
        case TaskStatus::WAITING:
            str = "WAITING";
            break;
        case TaskStatus::RUNNING:
            str = "RUNNING";
            break;
        case TaskStatus::SUSPENDED:
            str = "SUSPENDED";
            break;
        case TaskStatus::COMPLETED:
            str = "COMPLETED";
            break;
        case TaskStatus::FAILED:
            str = "FAILED";
            break;
        case TaskStatus::CANCELED:
            str = "CANCELED";
            break;
        default:
            break;
        }

        return str;
    }
}//nVehicleDataType

namespace nsTaskMgr
{
struct TaskInfo
{
    TaskInfo() {}
    int TaskType = 0;//1表示正向任务，2表示反向任务
    // int LRU;//只针对Vehicle子系统
    QString LRU = "无";
    QString TaskListName = "无";
    QString TargetStation = "无";
};
//用于反馈子系统任务完成状态
enum class eTaskstate:int
{
    Init,
    Ready,
    Running,
    Complete,
    Error
};

enum class eSubSystem:int
{
    Vehicle,
    Levelling,
    MainLift,
    CoverPlate,
    Lift
};

// 任务状态枚举转字符串
inline QString TaskStateToString(eTaskstate state)
{
    switch (state)
    {
    case eTaskstate::Init:     return "初始化";
    case eTaskstate::Ready:    return "准备中";
    case eTaskstate::Running:  return "运行中";
    case eTaskstate::Complete: return "已完成";
    case eTaskstate::Error:    return "错误";
    }
}

// 子系统枚举转字符串
inline QString SubSystemToString(eSubSystem system)
{
    switch (system)
    {
    case eSubSystem::Vehicle:     return "AGV子系统";
    case eSubSystem::Levelling:   return "调平子系统";
    case eSubSystem::MainLift:    return "一级升降子系统";
    case eSubSystem::CoverPlate:  return "盖板子系统";
    case eSubSystem::Lift:        return "二级升降子系统";
    }
}


// 任务类型转字符串
inline QString TaskTypeToString(int TaskType)
{
    switch (TaskType){
    case 0:
        return "暂无任务";
        break;
    case 1:
        return "正向任务";
        break;
    case 2:
        return "反向任务";
        break;
    }
}
//子系统作完任务后反馈的状态，这里结构体内的变量是需要手动赋值的，并不是小车实时数据
struct eTaskStatus
{
    eTaskStatus() {}
    eSubSystem sys;//表示自己是哪个子系统
    // int TaskType; //执行的任务类型
    TaskInfo taskinfo;//任务信息，包括正反向、LRU和任务链名称
    eTaskstate state; // 任务完成状态
    double duration = 0.0f;    //执行任务耗时
    QString info = "无";    //额外的详细信息,如果有
};


//调平标志位更新
enum class LevelSymbol {
    FeetToGround0,
    FeetToGround1,
    FeetToGround2,
    FeetToGround3,
    Balance,
    Restart
};

//取盖板标志位更新
enum class CoverPlateSymbol {
    TakeDoorComplete,//取封门
    TakePlateComplete,//取盖板
    CoverPlateComplete,//盖盖板
    CoverDoorComplete//盖封门
};

// 六自由度平台信息结构体
struct SixDofPlatformData {
    float x;
    float y;
    float z;
    float rx;
    float ry;
    float rz;
    int motion_mode;
    bool servo_enabled;
    bool target_reached;
    QString alarmText;
    bool hasClientConnected;
};

// 视觉信息结构体
struct DetectedDisplayData {
    float angle;
    float rho1;
    float rho2;
};
// 六维力传感器信息
struct SixForeData {
    float Fx;
    float Fy;
    float Fz;
    float Mx;
    float My;
    float Mz;
};

}
Q_DECLARE_METATYPE(nsTaskMgr::DetectedDisplayData)
#endif // DATATYPE_H
