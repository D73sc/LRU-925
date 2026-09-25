#ifndef LIGHTSOURCECONTROLLER_H
#define LIGHTSOURCECONTROLLER_H
#pragma execution_character_set("utf-8")
#include <QObject>
#include <QSerialPort>
#include <QTimer>
#include <QMutex>
#include <QQueue>
#include <QPair>
#include <QDebug>
#include <QDateTime>

class LightSourceController : public QObject
{
    Q_OBJECT
public:
    // 通道定义（对应4个输出通道）
    enum Channel {
        Channel1 = 1,
        Channel2 = 2,
        Channel3 = 3,
        Channel4 = 4
    };

    // 命令结构体
    struct CommandRequest {
        char cmd;           // 命令字
        Channel channel;    // 通道
        int data;           // 数据
        quint32 timestamp;  // 时间戳用于超时检查
        bool isReading;     // 是否为读取命令（需要解析返回数据）
    };

    explicit LightSourceController(QObject *parent = nullptr);
    ~LightSourceController();

    // 打开串口（返回是否成功）
    bool openSerial();
    // 关闭串口
    void closeSerial();
    // 判断串口是否已打开
    bool isSerialOpen() const;

    // 打开指定通道（返回是否成功）
    bool openChannel(Channel channel);
    // 关闭指定通道（返回是否成功）
    bool closeChannel(Channel channel);
    // 设置通道亮度（0-255，返回是否成功）
    bool setBrightness(Channel channel, int brightness);
    // 读取通道亮度（返回亮度值，success标识是否成功）
    int readBrightness(Channel channel);

private:
    QSerialPort *m_serial;           // 串口对象
    QTimer *m_responseTimer;         // 响应超时定时器
    QTimer *m_readTimer;             // 读取数据定时器
    QMutex m_commandMutex;           // 命令队列互斥锁

    // 命令队列和响应缓存
    QQueue<CommandRequest> m_commandQueue;      // 待发送命令队列
    CommandRequest m_currentCommand;            // 当前正在处理的命令
    QByteArray m_responseBuffer;                // 响应数据缓冲区
    bool m_waitingForResponse;                  // 是否正在等待响应

    // 异步读取相关
    QByteArray m_serialBuffer;                  // 串口数据接收缓冲区

    // 发送命令到队列（异步）
    void enqueueCommand(char cmd, Channel channel, int data, bool isReading = false);
    // 处理命令队列
    void processCommandQueue();
    // 发送命令到串口
    void sendCommandToSerial(const CommandRequest &request);
    // 处理串口响应
    void processResponse(const QByteArray &response);
    // 验证响应格式
    bool validateResponse(const QByteArray &response, char expectedCmd, Channel expectedChannel);
    // 解析读取命令的响应
    int parseReadResponse(const QByteArray &response, Channel expectedChannel);
    // 计算异或校验字
    QByteArray calculateChecksum(const QByteArray &baseFrame);

private slots:
    // 串口数据到达
    void onSerialDataReady();
    // 响应超时处理
    void onResponseTimeout();
    // 读取数据超时（用于读取完整数据包）
    void onReadTimeout();

signals:
    // UI更新信号
    void lightsourceUpdateUI(QString str);
    // 亮度更新信号
    void lightBrightness(int light1, int light2);
    // 命令执行结果信号
    void commandExecuted(bool success, int brightness = -1);
    // 串口连接状态信号
    void serialConnectionChanged(bool connected);

    // 内部使用的信号（用于跨线程）
    void commandFinished(bool success, int result = -1);
};

#endif // LIGHTSOURCECONTROLLER_H
