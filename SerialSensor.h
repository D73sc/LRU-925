#ifndef SERIALSENSOR_H
#define SERIALSENSOR_H
#include <iostream>

#include <QObject>
#include <QSerialPort>
#include <QTimer>
#include <QMutex>
#include "DataType.h"

using namespace nVehicleDataType;
class SerialSensor : public QObject
{
    Q_OBJECT
public:

    explicit SerialSensor(SensorType type, QObject *parent = nullptr);
    ~SerialSensor();

    // 串口操作
    bool openPort(const QString &portName);
    void closePort();
    bool isOpen() const;

    // 数据发送与接收
    void startAutoSend(int interval);
    void stopAutoSend();
    void sendCommand();

    // 数据解析
    void parseData(const QByteArray &data);

    // 获取可用串口
    static QStringList getAvailablePorts();

signals:
    void newDataReceived(const QVariantMap &data);
    void errorOccurred(const QString &error);

private slots:
    void onReadyRead();
    void onAutoSendTimeout();

private:
    void parseForceData(const QByteArray &data);
    void parseAngleData(const QByteArray &data);
    void parseDistanceData(const QByteArray &data);
    void parseBatteryData(const QByteArray &data);// 锂能源电池数据解析
    quint16 calculateCRC(const QByteArray &data);
    int32_t hexToDecimal(const QString &hexStr) const;
    float hexToAngle(const QString &hexStr) const;
    int ACSCIItoDecimal(const QString &asciiStr);

    QSerialPort *m_serialPort;
    QTimer *m_autoSendTimer;
    QByteArray m_receiveBuffer;
    SensorType m_type;
    int m_currentStation;
};


#endif // SERIALSENSOR_H
