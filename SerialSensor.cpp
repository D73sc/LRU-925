#include "SerialSensor.h"
#include <QSerialPortInfo>
#include <QDebug>

SerialSensor::SerialSensor(SensorType type, QObject *parent)
    : QObject(parent), m_type(type), m_currentStation(0)
{


    m_serialPort = new QSerialPort(this);
    m_autoSendTimer = new QTimer(this);

    connect(m_serialPort, &QSerialPort::readyRead, this, &SerialSensor::onReadyRead,Qt::QueuedConnection);
    connect(m_autoSendTimer, &QTimer::timeout, this, &SerialSensor::onAutoSendTimeout,Qt::QueuedConnection);
}

SerialSensor::~SerialSensor()
{
    closePort();
}

bool SerialSensor::openPort(const QString &portName)
{
    if (m_serialPort->isOpen()) {
        return true;
    }

    m_receiveBuffer.clear();
    m_serialPort->setPortName(portName);
    if (!m_serialPort->open(QIODevice::ReadWrite)) {
        emit errorOccurred(QString("无法打开串口 %1: %2").arg(portName, m_serialPort->errorString()));
        return false;
    }

    // 通用串口设置
    m_serialPort->setBaudRate(QSerialPort::Baud9600,QSerialPort::AllDirections);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->clear(QSerialPort::AllDirections);

    return true;
}

void SerialSensor::closePort()
{
    stopAutoSend();
    m_receiveBuffer.clear();
    if (m_serialPort->isOpen()) {
        m_serialPort->clear(QSerialPort::AllDirections);
        m_serialPort->close();
    }
}

bool SerialSensor::isOpen() const
{
    return m_serialPort->isOpen();
}

void SerialSensor::startAutoSend(int interval)
{
    m_autoSendTimer->start(interval);
}

void SerialSensor::stopAutoSend()
{
    m_autoSendTimer->stop();
}


//串口有数据可读时触发，获取数据并进行解析
//串口有数据可读时触发，获取数据并进行解析
void SerialSensor::onReadyRead()
{
    QByteArray newData = m_serialPort->readAll();
    m_receiveBuffer.append(newData);

    // 根据传感器类型确定数据包长度
    int packetLength = 0;
    switch (m_type) {
    case FORCE_SENSOR:
        packetLength = 9;
        break;
    case ANGLE_SENSOR:
        packetLength = 11;
        break;
    case DISTANCE_SENSOR:
        packetLength = 7;
        break;
    case BATTERY_SENSOR:
        packetLength = 136;
        break;
    }

    if (m_type == BATTERY_SENSOR) {
        while (true) {
            int frameStart = m_receiveBuffer.indexOf(static_cast<char>(0x7E));
            if (frameStart < 0) {
                m_receiveBuffer.clear();
                return;
            }
            if (frameStart > 0) {
                m_receiveBuffer.remove(0, frameStart);
            }
            if (m_receiveBuffer.size() < packetLength) {
                return;
            }
            if (m_receiveBuffer.at(packetLength - 1) != static_cast<char>(0x0D)) {
                m_receiveBuffer.remove(0, 1);
                continue;
            }

            QByteArray packet = m_receiveBuffer.left(packetLength);
            m_receiveBuffer.remove(0, packetLength);
            parseData(packet);
        }
    }

    while (m_receiveBuffer.size() >= packetLength) {
        QByteArray packet = m_receiveBuffer.left(packetLength);
        m_receiveBuffer.remove(0, packetLength);
        parseData(packet);
    }
}


//自动发送指令
void SerialSensor::onAutoSendTimeout()
{
    sendCommand();
}

void SerialSensor::sendCommand()
{
    if (!m_serialPort->isOpen()) return;

    QByteArray sendBuf;
    switch (m_type) {
    case FORCE_SENSOR: {
        QVector<quint8> stationAddresses = {0x01, 0x02, 0x03, 0x04};
        quint8 address = stationAddresses[m_currentStation];
        sendBuf.append(address);
        sendBuf.append(static_cast<char>(0x04));
        sendBuf.append(static_cast<char>(0x00));
        sendBuf.append(static_cast<char>(0x06));
        sendBuf.append(static_cast<char>(0x00));
        sendBuf.append(static_cast<char>(0x02));
        quint16 crc = calculateCRC(sendBuf);
        sendBuf.append(static_cast<char>(crc & 0xFF));
        sendBuf.append(static_cast<char>((crc >> 8) & 0xFF));
        m_currentStation = (m_currentStation + 1) % stationAddresses.size();
        break;
    }
    case ANGLE_SENSOR: {
        sendBuf.append(static_cast<char>(0x50));
        sendBuf.append(static_cast<char>(0x03));
        sendBuf.append(static_cast<char>(0x00));
        sendBuf.append(static_cast<char>(0x3D));
        sendBuf.append(static_cast<char>(0x00));
        sendBuf.append(static_cast<char>(0x03));
        quint16 crc = calculateCRC(sendBuf);
        sendBuf.append(static_cast<char>(crc & 0xFF));
        sendBuf.append(static_cast<char>((crc >> 8) & 0xFF));
        break;
    }
    case DISTANCE_SENSOR: {
        QVector<quint8> stationAddresses = {0x01, 0x02, 0x03, 0x04};
        quint8 address = stationAddresses[m_currentStation];
        sendBuf.append(address);
        sendBuf.append(static_cast<char>(0x03));
        sendBuf.append(static_cast<char>(0x02));
        sendBuf.append(static_cast<char>(0x01));
        sendBuf.append(static_cast<char>(0x00));
        sendBuf.append(static_cast<char>(0x01));
        quint16 crc = calculateCRC(sendBuf);
        sendBuf.append(static_cast<char>(crc & 0xFF));
        sendBuf.append(static_cast<char>((crc >> 8) & 0xFF));
        m_currentStation = (m_currentStation + 1) % stationAddresses.size();
        break;
    }
    case BATTERY_SENSOR: {
        sendBuf = QByteArray::fromHex("7e3230303134363432453030323031464433350d");// 向锂电池发送命令获取模拟量数值
        break;
    }
  }

    m_serialPort->write(sendBuf);
}


//数据解析
void SerialSensor::parseData(const QByteArray &data)
{
    switch (m_type) {
    case FORCE_SENSOR:
        parseForceData(data);
        break;
    case ANGLE_SENSOR:
        parseAngleData(data);
        break;
    case DISTANCE_SENSOR:
        parseDistanceData(data);
        break;
    case BATTERY_SENSOR:
        parseBatteryData(data);
        break;
    }
}

void SerialSensor::parseForceData(const QByteArray &data)
{
    QString strReceiveData = data.toHex().toUpper();

    if (strReceiveData.length() == 18) {
        bool ok;
        quint8 address = strReceiveData.mid(0, 2).toUInt(&ok, 16);
        int stationIndex = address - 1;

        if (ok && address >= 1 && address <= 4) {
            QString dataHex = strReceiveData.mid(6, 8);
            float value = hexToDecimal(dataHex);

            QVariantMap result;
            switch (stationIndex) {
            case 0: value += 440; break;
            case 1: value -= 375; break;
            case 2: value += 500; break;
            case 3: value += 3300; break;
            }

            result[QString("sensor%1").arg(stationIndex)] = value;
            emit newDataReceived(result);
        }
    }
}

void SerialSensor::parseAngleData(const QByteArray &data)
{
    QString strReceiveData = data.toHex().toUpper();

    if (strReceiveData.length() == 22) {
        QString angleX = strReceiveData.mid(6, 4);
        QString angleY = strReceiveData.mid(10, 4);

        QVariantMap result;
        result["AngleX"] = hexToAngle(angleX);
        result["AngleY"] = hexToAngle(angleY);
        emit newDataReceived(result);
    }
}

void SerialSensor::parseDistanceData(const QByteArray &data)
{
    QString strReceiveData = data.toHex().toUpper();

    if (strReceiveData.length() == 14) {
        bool ok;
        quint8 address = strReceiveData.mid(0, 2).toUInt(&ok, 16);
        int stationIndex = address - 1;

        if (ok && address >= 1 && address <= 4) {
            QString dataHex = strReceiveData.mid(6, 4);

            if (dataHex.length() != 4) return;

            bool highOk, lowOk;
            quint8 highByte = dataHex.left(2).toUShort(&highOk, 16);
            quint8 lowByte = dataHex.right(2).toUShort(&lowOk, 16);

            if (highOk && lowOk) {
                quint32 value = (highByte << 8) | lowByte;

                QVariantMap result;
                result[QString("Distance%1").arg(stationIndex)] = static_cast<float>(value);
                emit newDataReceived(result);
            }
        }
    }
}


// 电池传感器数据解析
void SerialSensor::parseBatteryData(const QByteArray &data)
{
    if (data.isEmpty() || data.at(0) != 0x7E || data.at(data.size() - 1) != 0x0D) {
        return;
    }

    QVariantMap result;

    // 总电流 93-96
    QByteArray currentData = data.mid(93, 4);
    QString currentStr = QString(currentData);
    int packCurrent = ACSCIItoDecimal(currentStr);
    result["packTotalCurrent"] = packCurrent / 1000;

    // 总电压 97-100
    QByteArray voltageData = data.mid(97, 4);
    int packVoltage = ACSCIItoDecimal(QString(voltageData));
    result["packTotalVoltage"] = packVoltage / 1000;

    // 剩余mAh 101-104
    QByteArray remainingData = data.mid(101, 4);
    int remainingmAh = ACSCIItoDecimal(QString(remainingData));//通过QString自动转换为ASCII码
    result["remainingmAh"] = float(remainingmAh) / float(36000);

    emit newDataReceived(result);
}

quint16 SerialSensor::calculateCRC(const QByteArray &data)
{
    quint16 crc = 0xFFFF;
    for (int i = 0; i < data.size(); ++i) {
        crc ^= static_cast<quint8>(data.at(i));
        for (int j = 0; j < 8; ++j) {
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

int32_t SerialSensor::hexToDecimal(const QString &hexStr) const
{
    QString rearrangeHex = hexStr.mid(4, 2) + hexStr.mid(6, 2) + hexStr.mid(0, 2) + hexStr.mid(2, 2);
    QByteArray bytes = QByteArray::fromHex(rearrangeHex.toLatin1());

    if (bytes.size() != 4) {
        return 0;
    }

    uint32_t raw = 0;
    for (int i = 0; i < 4; ++i) {
        raw |= static_cast<uint8_t>(bytes[i]) << (24 - i * 8);
    }

    bool isNegative = (raw & 0x80000000);
    if (isNegative) {
        raw = (~raw) + 1;
    }

    int32_t decimal = static_cast<int32_t>(raw);
    return isNegative ? -decimal : decimal;

}

float SerialSensor::hexToAngle(const QString &hexStr) const
{
    if (hexStr.length() != 4) {
        return 0.0f;
    }

    bool okHigh, okLow;
    ushort high = hexStr.mid(0, 2).toUShort(&okHigh, 16);
    ushort low = hexStr.mid(2, 2).toUShort(&okLow, 16);

    if (!okHigh || !okLow) {
        return 0.0f;
    }

    uint16_t uValue = (high << 8) | low;
    int16_t sValue = static_cast<int16_t>(uValue);
    return static_cast<float>(sValue) / 32768.0f * 180.0f;

}

QStringList SerialSensor::getAvailablePorts()
{
    QStringList ports;
    foreach(const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        ports << info.portName();
    }
    return ports;
}


int SerialSensor::ACSCIItoDecimal(const QString &asciiStr)
{
    if (asciiStr.length() != 4) {
        emit errorOccurred("ASCII字符串长度不正确");
        return 0;
    }

    // 转换ASCII码为字符
    QString charStr;
    for (QChar c : asciiStr) {
        charStr += c;
    }

    // 16进制转换为10进制
    bool ok;
    int value = charStr.toInt(&ok, 16);
    if (!ok) {
        emit errorOccurred("ASCII转换为10进制失败: " + charStr);
    }
    return ok ? value : 0;
}
