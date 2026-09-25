#include "TCPSensor.h"
#include <QDebug>

SixAxisSensor::SixAxisSensor(QObject *parent) : QObject(parent)
{
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::readyRead, this, &SixAxisSensor::onReadyRead);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &SixAxisSensor::onErrorOccurred);
}

bool SixAxisSensor::initialize(const QString &ip, quint16 port)
{
    m_socket->connectToHost(ip, port);
    return m_socket->waitForConnected(1000);
}

void SixAxisSensor::startContinuousMeasurement()
{
    if(m_socket->state() == QTcpSocket::ConnectedState){
        m_socket->write("AT+GSD\r\n");
    }
}

void SixAxisSensor::sensorZeroCalibration()
{
    if(m_socket->state() == QTcpSocket::ConnectedState){
        m_socket->write("AT+ADJZF=1;1;1;1;1;1\r\n");
    }
}

void SixAxisSensor::closeConnection()
{
    m_socket->disconnectFromHost();
}

void SixAxisSensor::onReadyRead()
{
    m_buffer.append(m_socket->readAll());

    // 处理完整数据包
    while(m_buffer.size() >= 4) {
        // 检查帧头
        if(static_cast<quint8>(m_buffer[0]) == 0xAA && static_cast<quint8>(m_buffer[1]) == 0x55) {
            // 获取包长度（小端）
            quint16 pkgLength = static_cast<quint8>(m_buffer[2]) | (static_cast<quint8>(m_buffer[3]) << 8);

            if(m_buffer.size() >= pkgLength) {
                QByteArray packet = m_buffer.left(pkgLength);
                m_buffer.remove(0, pkgLength);

                QVector<float> forces = parseSensorData(packet);
                if(!forces.isEmpty()) {
                    emit dataReceived(forces);
                }
            } else {
                break; // 等待更多数据
            }
        } else {
            m_buffer.remove(0, 1); // 丢弃无效数据
        }
    }
}

QVector<float> SixAxisSensor::parseSensorData(const QByteArray &data)
{
    QVector<float> forces;
    if(data.size() < 27) return forces; // 最小数据包长度检查

    // 跳过帧头（AA55）和包长度（2字节）
    const char *p = data.constData() + 6;

    // 解析6个通道数据（每个通道4字节）
    for(int i = 0; i < 6; ++i) {
        if(p + 4 > data.constData() + data.size()) break;

        // 转换为小端字节序
        quint32 raw = static_cast<quint8>(p[3]) << 24 |
                      static_cast<quint8>(p[2]) << 16 |
                      static_cast<quint8>(p[1]) << 8 |
                      static_cast<quint8>(p[0]);

        float value;
        memcpy(&value, &raw, sizeof(float));
        forces.append(value);

        p += 4; // 移动到下一个通道
    }

    return forces;
}

void SixAxisSensor::onErrorOccurred(QAbstractSocket::SocketError error)
{
    emit errorOccurred(m_socket->errorString());
}
