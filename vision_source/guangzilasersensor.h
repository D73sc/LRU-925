#ifndef GUANGZILASERSENSOR_H
#define GUANGZILASERSENSOR_H

#include <QObject>
#include <QSerialPort>
#include <QTimer>
#include <QMutex>
#include <QByteArray>
#include <QDateTime>
#include <QThread>
#pragma execution_character_set("utf-8")

class guangziLaserSensor : public QObject
{
    Q_OBJECT

public:
    explicit guangziLaserSensor(const QString &portName = "COM4", QObject *parent = nullptr);
    ~guangziLaserSensor() override;

    bool openSerialPort();
    void closeSerialPort();

    double getDistance_right();
    double getDistance_left();

    void setSerialConfig(const QString &portName, int baudRate = 9600);

signals:
    void laserUpdateUi(QString str);

private slots:
    void startNextSensorRead();  // 启动下一个传感器的读取（异步）
    void onSerialReadyRead();    // 串口有数据可读时的槽函数
    void onBytesWritten(qint64 bytes); // 指令发送完成的槽函数

private:
    quint16 crc16(const QByteArray &data);
    void sendSensorReadCmd(quint8 station); // 发送单个传感器读取指令
    bool parseSensorResponse(quint8 station, const QByteArray &response, double &distance); // 解析响应数据

    QString m_portName;
    QSerialPort::BaudRate m_baudRate;
    QSerialPort::DataBits m_dataBits;
    QSerialPort::Parity   m_parity;
    QSerialPort::StopBits m_stopBits;
    int m_timeout;

    QSerialPort *m_serialPort;
    QTimer      *m_timer;          // 轮询定时器（触发新一轮读取）
    QTimer      *m_responseTimer;  // 响应超时定时器

    double m_distance1;
    double m_distance2;
    QMutex m_mutex;

    // 异步状态管理
    quint8 m_currentStation;       // 当前正在读取的站号
    bool m_isSending;              // 是否正在发送/等待响应
    QByteArray m_responseBuffer;   // 串口响应缓冲区
};

#endif // GUANGZILASERSENSOR_H
