#ifndef UDPCOMMUNICATION_H
#define UDPCOMMUNICATION_H
#pragma execution_character_set("utf-8")
#include <QObject>
#include <QUdpSocket>
#include <QThread>
#include <QHostAddress>
#include <QByteArray>
#include <QMutex>
#include <QJsonValue>
#include <QJsonObject>
#include <atomic>

// 接收线程类
class UdpReceiveThread : public QThread
{
    Q_OBJECT
public:
    explicit UdpReceiveThread(const QHostAddress &localIp, qint16 localPort, int receiveTimeoutMs = 500, QObject *parent = nullptr);
    ~UdpReceiveThread() override;

    void stop();

signals:
    void dataReceived(const QByteArray &data);
    void errorOccurred(const QString &errorMsg);
    void receiveTimeout();

protected:
    void run() override;

private:
    QUdpSocket *m_udpSocket = nullptr;
    QHostAddress m_localIp;
    qint16 m_localPort;
    std::atomic_bool m_isRunning { false };
    int m_receiveTimeoutMs;
};

// UDP通信主类（含接收解析逻辑）
class UdpCommunication : public QObject
{
    Q_OBJECT
public:
    explicit UdpCommunication(
        const QString &localIp,
        qint16 localPort,
        const QString &slaveIp,
        quint16 slavePort,
        int receiveTimeoutMs = 500,
        QObject *parent = nullptr
        );
    ~UdpCommunication() override;

    // 核心通信接口

    void stopCommunication();
    bool isRunning() const;
    bool isCommunicationSuccess() const;

    // 数据帧处理函数
    QByteArray createDataFrame();
    QByteArray modifyFrameByte(const QByteArray &frame, int byteIndex, int decimalValue);
    QByteArray modifyFrameBit(const QByteArray &frame, int byteIndex, int bitIndex, bool bitValue);
    QByteArray addChecksum(const QByteArray &frame);

    // 控制函数
    void servoPowerOn();
    void servoPowerOff();
    void posePointMotion(float x, float y, float z, float rx, float ry, float rz, float linearSpeed=2, float angularSpeed=1);
    // 新增控制函数
    void stopMotion();       // 运动停止（第34字节bit2=1）
    void startLeveling();    // 调平（第34字节bit3=1）
    void homing();           // 回零（第31字节bit0=1）新增
    void initialize();       // 初始化（第31字节bit1=1）新增
    void resetSystem();       // 系统复位（第31字节bit2=1且bit3=1）
    // 新增：获取当前最新坐标数据
    QJsonObject getCurrentCoordinates() const;
    // 新增：获取平台运动状态（线程安全）
    bool isPlatformMoving() const;
    //倾角传感器目标位姿
    float target_rx = -0.05;
    float target_ry = 0.46;

signals:
    void receiveData(const QByteArray &rawData); // 原始88字节数据信号（保留）
    void communicationError(const QString &errorMsg);
    void stateChanged(bool isRunning);
    void communicationSuccessChanged(bool isSuccess);

    // 新增：姿态反馈信号（解析后的x/y/z/rx/ry/rz + 到达目标位置反馈 + 报警状态 + 伺服使能）
    void poseFeedback(
        float x, float y, float z, float rx, float ry, float rz,
        float gyroPitch, float gyroRoll, float gyroYaw,
        bool isReachTarget,
        bool emergencyStop,
        bool lightFault,
        bool heavyFault,
        bool servoEnabled,
        bool isMoving  // 新增：是否运动状态
        );

private slots:
    void onThreadDataReceived(const QByteArray &data); // 处理接收数据
    void onThreadErrorOccurred(const QString &errorMsg);
    void onReceiveTimeout();
public slots:
    // 新增：线程安全的发送数据槽函数
    void sendDataAsync(const QByteArray &data);
private:
    QHostAddress m_localIp;
    qint16 m_localPort;
    QHostAddress m_slaveIp;
    quint16 m_slavePort;
    QUdpSocket *m_sendSocket = nullptr;
    UdpReceiveThread *m_receiveThread = nullptr;
    bool m_isRunning = false;
    bool m_isCommSuccess = false;
    mutable QMutex m_commSuccessMutex;
    int m_receiveTimeoutMs;

    // 常量定义（与发送端一致）
    static constexpr float COORD_FACTOR = 100.0f; // 坐标/角度缩放因子（接收时除以该值还原）
    static constexpr int RAW_DATA_LENGTH = 88;    // 接收数据固定长度

    // 内部启动函数
    bool startCommunication();

    // 辅助函数：校验数据帧合法性（发送端用）
    bool isValidDataFrame(const QByteArray &frame);

    // 发送端辅助函数
    int16_t floatToInt16(float value, float factor, int16_t min = -32768, int16_t max = 32767);
    void int16ToTwoBytes(QByteArray &frame, int startIndex, int16_t value);

    bool sendData(const QByteArray &data);

    // 新增：接收端辅助函数：2字节（大端序）→16位有符号整数（int16_t）
    int16_t twoBytesToInt16(const QByteArray &data, int startIndex);
    // 新增：存储最新坐标数据
    QJsonObject m_coordinates;
    // 新增：保护坐标数据访问的互斥锁
    mutable QMutex m_coordinatesMutex;
    // 新增：平台运动状态相关
    bool m_isPlatformMoving = false;
    mutable QMutex m_movingMutex;

};

#endif // UDPCOMMUNICATION_H
