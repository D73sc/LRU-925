#include "guangziLaserSensor.h"
#include <QDebug>

guangziLaserSensor::guangziLaserSensor(const QString &portName, QObject *parent)
    : QObject(parent)
{
    // 串口默认配置
    m_portName  = portName;  // 优先使用传入的端口号
    m_baudRate  = QSerialPort::Baud115200;
    m_dataBits  = QSerialPort::Data8;
    m_parity    = QSerialPort::NoParity;
    m_stopBits  = QSerialPort::OneStop;
    m_timeout   = 1000;

    // 初始化距离值
    m_distance1 = 0.0;
    m_distance2 = 0.0;

    // 串口初始化
    m_serialPort = new QSerialPort(this);
    m_serialPort->setPortName(m_portName);
    m_serialPort->setBaudRate(m_baudRate);
    m_serialPort->setDataBits(m_dataBits);
    m_serialPort->setParity(m_parity);
    m_serialPort->setStopBits(m_stopBits);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);
    m_serialPort->setReadBufferSize(1024);

    // 异步状态初始化
    m_currentStation = 0;
    m_isSending = false;
    m_responseBuffer.clear();

    // 定时器：触发新一轮传感器轮询（间隔100ms）
    m_timer = new QTimer(this);
    m_timer->setInterval(100);

    // 响应超时定时器：防止串口无响应时卡死
    m_responseTimer = new QTimer(this);
    m_responseTimer->setSingleShot(true);
    m_responseTimer->setInterval(m_timeout);

    // 信号槽连接（事件驱动核心）
    connect(m_timer, &QTimer::timeout, this, &guangziLaserSensor::startNextSensorRead);
    connect(m_serialPort, &QSerialPort::readyRead, this, &guangziLaserSensor::onSerialReadyRead);
    connect(m_serialPort, &QSerialPort::bytesWritten, this, &guangziLaserSensor::onBytesWritten);
    connect(m_responseTimer, &QTimer::timeout, this, [this]() {
        emit laserUpdateUi(QString("[站号%1] 响应超时，未收到数据").arg(m_currentStation));
        m_isSending = false;
        m_currentStation = 0;
        m_responseBuffer.clear();
    });
}

guangziLaserSensor::~guangziLaserSensor()
{
    closeSerialPort();
}

bool guangziLaserSensor::openSerialPort()
{
    bool ok = true;

    if (!m_serialPort->isOpen()) {
        if (m_serialPort->open(QIODevice::ReadWrite)) {
            m_serialPort->clear();
            emit laserUpdateUi(QString("一级激光测距串口已打开：%1 @ %2")
                                   .arg(m_serialPort->portName())
                                   .arg(m_baudRate));
        } else {
            ok = false;
            emit laserUpdateUi(QString("无法打开一级激光测距串口：%1")
                                   .arg(m_serialPort->errorString()));
        }
    } else {
        emit laserUpdateUi("一级激光测距串口已打开");
    }

    if (ok)
        m_timer->start();
    else
        m_timer->stop();

    return ok;
}

void guangziLaserSensor::closeSerialPort()
{
    m_timer->stop();
    m_responseTimer->stop();

    if (m_serialPort && m_serialPort->isOpen()) {
        m_serialPort->close();
        emit laserUpdateUi("[提示] 串口已关闭");
    }

    // 重置异步状态
    m_isSending = false;
    m_currentStation = 0;
    m_responseBuffer.clear();
}

double guangziLaserSensor::getDistance_right()
{
    QMutexLocker locker(&m_mutex);
    return m_distance1;
}

double guangziLaserSensor::getDistance_left()
{
    QMutexLocker locker(&m_mutex);
    return m_distance2;
}

void guangziLaserSensor::setSerialConfig(const QString &portName, int baudRate)
{
    Q_UNUSED(portName);
    m_baudRate = static_cast<QSerialPort::BaudRate>(baudRate);

    if (m_serialPort) {
        m_serialPort->setBaudRate(m_baudRate);
    }
}

// 启动下一个传感器的读取（异步轮询：1→2→1→2...）
void guangziLaserSensor::startNextSensorRead()
{
    if (!m_serialPort->isOpen() || m_isSending) {
        return; // 串口未打开/正在发送，跳过本次轮询
    }

    // 切换轮询站号（1→2→1）
    m_currentStation = (m_currentStation == 0 || m_currentStation == 2) ? 1 : 2;
    // 发送当前站号的读取指令
    sendSensorReadCmd(m_currentStation);
}

// 发送单个传感器读取指令（异步发送，无阻塞）
void guangziLaserSensor::sendSensorReadCmd(quint8 station)
{
    try {
        // 构造Modbus指令帧
        QByteArray cmd;
        cmd.append(static_cast<char>(station));  // 站号
        cmd.append(static_cast<char>(0x03));     // 功能码
        cmd.append(static_cast<char>(0x00));
        cmd.append(static_cast<char>(0x3B));
        cmd.append(static_cast<char>(0x00));
        cmd.append(static_cast<char>(0x02));

        // 附加CRC校验
        quint16 crc = crc16(cmd);
        cmd.append(static_cast<char>(crc & 0xFF));
        cmd.append(static_cast<char>((crc >> 8) & 0xFF));

        // 标记为发送中，清空响应缓冲区
        m_isSending = true;
        m_responseBuffer.clear();

        // 异步发送指令（无阻塞）
        m_serialPort->clear(QSerialPort::Input);
        qint64 writeBytes = m_serialPort->write(cmd);
        if (writeBytes <= 0) {
            emit laserUpdateUi(QString("[站号%1] 指令发送失败").arg(station));
            m_isSending = false;
            m_currentStation = 0;
        } else {
            // qDebug() << QString("[站号%1] 发送指令（十六进制）：%2")
            //                 .arg(station)
            //                 .arg(QString(cmd.toHex(' ').toUpper()));
            // 启动响应超时定时器
            m_responseTimer->start();
        }
    } catch (...) {
        emit laserUpdateUi(QString("[站号%1] 指令构造异常").arg(station));
        m_isSending = false;
        m_currentStation = 0;
    }
}

// 串口有数据可读时触发（事件驱动核心）
void guangziLaserSensor::onSerialReadyRead()
{
    if (!m_isSending || m_currentStation == 0) {
        m_serialPort->readAll(); // 非预期数据，直接清空
        return;
    }

    // 异步读取所有可用数据
    QByteArray newData = m_serialPort->readAll();
    m_responseBuffer += newData;

    // qDebug() << QString("[站号%1] 收到数据（%2字节，累计%3字节）：%4")
    //                 .arg(m_currentStation)
    //                 .arg(newData.size())
    //                 .arg(m_responseBuffer.size())
    //                 .arg(QString(newData.toHex(' ').toUpper()));

    // 响应数据长度足够（Modbus响应至少9字节），开始解析
    if (m_responseBuffer.size() >= 9) {
        m_responseTimer->stop(); // 停止超时定时器

        double distance = 100.0;
        if (parseSensorResponse(m_currentStation, m_responseBuffer, distance)) {
            // 更新对应站号的距离值
            QMutexLocker locker(&m_mutex);
            if (m_currentStation == 1) {
                m_distance1 = distance;
                // qDebug()<<"COM3 站号1:"<<distance;
            } else if (m_currentStation == 2) {
                m_distance2 = distance;
                // qDebug()<<"COM3 站号2:"<<distance;
            }
        }

        // 重置状态，允许下一次发送
        m_isSending = false;
        m_responseBuffer.clear();
    }
}

// 指令发送完成的回调（可选：用于确认发送成功）
void guangziLaserSensor::onBytesWritten(qint64 bytes)
{
    // qDebug() << QString("[站号%1] 指令发送完成，共发送%2字节").arg(m_currentStation).arg(bytes);
}

// 解析传感器响应数据
bool guangziLaserSensor::parseSensorResponse(quint8 station, const QByteArray &response, double &distance)
{
    // 校验站号
    if (static_cast<quint8>(response[0]) != station) {
        emit laserUpdateUi(QString("[站号%1] 站号不匹配，收到0x%2")
                               .arg(station)
                               .arg(static_cast<quint8>(response[0]), 2, 16, QLatin1Char('0')));
        return false;
    }

    // 解析距离数据
    QByteArray highBytes     = response.mid(5, 2);
    QByteArray lowBytes      = response.mid(3, 2);
    QByteArray distanceBytes = highBytes + lowBytes;

    qint32 distanceValue = 0;
    if (distanceBytes.size() == 4) {
        distanceValue = static_cast<qint32>(
            (static_cast<quint8>(distanceBytes[0]) << 24) |
            (static_cast<quint8>(distanceBytes[1]) << 16) |
            (static_cast<quint8>(distanceBytes[2]) << 8)  |
            static_cast<quint8>(distanceBytes[3])
            );
    } else {
        emit laserUpdateUi(QString("[站号%1] 距离数据格式不正确").arg(station));
        return false;
    }

    // 计算实际距离（mm）
    distance = distanceValue * 0.001;
    // emit laserUpdateUi(QString("[站号%1] 距离：%2 mm")
    //                        .arg(station)
    //                        .arg(distance, 0, 'f', 3));
    return true;
}

// CRC16校验算法（保持不变）
quint16 guangziLaserSensor::crc16(const QByteArray &data)
{
    quint16 crc = 0xFFFF;
    for (char byte : data) {
        crc ^= static_cast<quint8>(byte);
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}
