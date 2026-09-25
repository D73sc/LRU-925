#ifndef DMDETECTED_H
#define DMDETECTED_H

#include <QObject>
#include <QTcpSocket>
#include <QMutex>
#include <QVector>
#include <QByteArray>
#include <QString>
#include <QDebug>
#include <QStringList>
#include <cmath>
#include <QTimer>  // 新增：用于超时和重连定时器

/**
 * @class DMDetected
 * @brief 用于通过TCP协议与相机通信、接收并解析数据的类
 * 功能：连接相机TCP服务器（192.168.192.55:3000），接收并解析数据
 */
class DMDetected : public QObject
{
    Q_OBJECT

public:
    explicit DMDetected(QObject *parent = nullptr);
    ~DMDetected();

    void connectToDevice();       // 连接相机
    void disconnectFromDevice();  // 断开连接
    bool isConnected();           // 获取连接状态
    QByteArray getCurrentData();  // 获取原始数据
    QVector<float> getValues();   // 获取解析后的数值
    QString getDmCode();          // 获取解析后的DM码
    bool getParseStatus();        // 获取解析状态
    void resetValues();

signals:
    void statusUpdated(const QString &status);  // 状态更新信号
    void errorOccurred(const QString &error);    // 错误信号

private slots:
    void onConnected();           // 连接成功回调
    void onDisconnected();        // 断开连接回调
    void onErrorOccurred(QAbstractSocket::SocketError socketError);  // 错误回调
    void onReadyRead();           // 接收数据回调
    void onConnectTimeout();      // 新增：连接超时回调

private:
    QTcpSocket *m_socket;         // TCP套接字
    QByteArray m_rawData;         // 原始数据缓冲区
    QVector<float> m_values;      // 解析后的数值
    QString m_dmCode;             // 解析后的DM码
    bool m_connectedFlag;         // 连接状态标志
    bool m_parseStatus;           // 解析状态标志
    QMutex m_dataMutex;           // 数据访问互斥锁
    QMutex m_flagMutex;           // 状态标志互斥锁
    QTimer *m_connectTimer;       // 新增：连接超时定时器
};

#endif // DMDETECTED_H
