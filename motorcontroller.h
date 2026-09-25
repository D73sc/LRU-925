#ifndef MOTORCONTROLLER_H
#define MOTORCONTROLLER_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QByteArray>
#include <QTimer>
#include <QQueue>


class MotorController : public QObject
{
    Q_OBJECT
public:
    // 电机运行方向
    enum RunDirection {
        CW = 0,   // 顺时针
        CCW = 1   // 逆时针
    };

    explicit MotorController(quint8 stationAddr = 0x01, QObject *parent = nullptr);
    ~MotorController();

    // // 外部设置串口
    // void setSerialPort(QSerialPort *port);

    // 串口操作
    bool openSerialPort(const QString &portName, qint32 baudRate = 115200);
    void closeSerialPort();
    bool isSerialOpen() const;

    // 基础控制
    void motorEnable();    // 电机使能
    void motorDisable();   // 电机脱机
    void resetDriver();    // 驱动器重启
    void clearAlarm();     // 清除报警
    void clearMotorAlarm(quint8 stationAddr);

    // 运动控制
    void setRunSpeed(qint32 speedRpm);           // 设置运行速度(rpm)
    void runToAbsolutePosition(qint32 pos);      // 运行到绝对位置(脉冲)
    void runHome(RunDirection dir, quint16 speed = 50); // 回原点

    // 电机正反转控制
    void motorRunCWByStation(quint8 stationAddr);  //正转（顺时针）带站号
    void motorRunCCWByStation(quint8 stationAddr);  //反转（逆时针）带站号
    void motorRunCW(qint32 speedRpm);   // 正转（顺时针）
    void motorRunCCW(qint32 speedRpm);  // 反转（逆时针）
    void motorStop();                     // 停止
    void readMotorPosition();  // 读取电机实时位置，发送读取指令
    void setMotorAbsolutePosition(qint32 pos);  // 设置电机当前绝对位置
    // 参数配置
    void setCurrent(quint16 currentMa);          // 设置电机电流(0.01A)
    void setMicrostep(quint32 microstep);        // 设置细分(脉冲/转)
    void setMotorSpeed(qint32 speedRpm);  // 速度设置
    void setMotorAccelerationTime(quint16 timeMs);  // 加速度时间设置
    void setMotorDecelerationTime(quint16 timeMs);  // 减速度时间设置
    void setAccelerationTime(quint8 stationAddr, quint16 timeMs);// 设置加速时间，指定站号
    void setDecelerationTime(quint8 stationAddr, quint16 timeMs);// 设置减速时间，指定站号
    void setPositiveLimit(qint32 pos);  // 正限位位置设置
    void setNegativeLimit(qint32 pos);  // 负限位位置设置

    // 指定站号控制
    void motorEnable(quint8 stationAddr);
    void motorDisable(quint8 stationAddr);
    void motorStop(quint8 stationAddr);

    void runToAbsolutePosition(quint8 stationAddr, qint32 pos);
    void setMotorSpeed(quint8 stationAddr, qint32 speedRpm);
    void readMotorPosition(quint8 stationAddr);
    void readMotorAlarm(quint8 stationAddr);

    void setMotorAbsolutePosition(quint8 stationAddr, qint32 pos);

    bool hasPendingCommand() const;

signals:
    void receiveData(const QByteArray &data);   // 接收数据信号
    void motorStatus(const QString &status); // 状态信号
    // 位置读取完成信号（传给界面显示）
    //void positionRead(double position); // 参数单位：脉冲
    void motorFaultChanged(quint8 stationAddr,bool fault,const QString &reason);

    void positionRead(quint8 stationAddr, double position);

private slots:
    void onSerialReadReady();   // 串口数据可读
    void onSendTimeout();       // 发送超时处理

private:
    quint16 calculateCRC(const QByteArray &data);  // Modbus CRC16校验
    QByteArray buildModbusCmd(quint8 funcCode, quint16 regAddr, quint16 regNum, const QByteArray &data = QByteArray());  // 构造Modbus-RTU指令
    void sendSerialData(const QByteArray &data);  // 发送串口数据
    QByteArray buildModbusCmd(quint8 stationAddr,quint8 funcCode,quint16 regAddr,quint16 regNum,const QByteArray &data = QByteArray());//新加构造Modbus-RTU指令，带站点

    QQueue<QByteArray> m_cmdQueue;
    bool m_queueBusy = false;
    int m_cmdIntervalMs = 80;   // 每条指令间隔
    void sendNextQueuedCommand();

private:
    QSerialPort *m_serialPort;
    quint8 m_stationAddr;       // 驱动器站地址
    QByteArray m_receiveBuffer; // 接收缓存
    QTimer *m_sendTimer;        // 发送超时定时器
};

#endif // MOTORCONTROLLER_H
