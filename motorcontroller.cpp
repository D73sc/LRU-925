#include "motorcontroller.h"
#include <QDebug>
#include <QSerialPort>
#include <QTimer>


MotorController::MotorController(quint8 stationAddr, QObject *parent)
    : QObject(parent),m_stationAddr(stationAddr)
{

    m_serialPort = new QSerialPort(this);
    m_sendTimer = new QTimer(this);
    m_sendTimer->setSingleShot(true);
    m_sendTimer->setInterval(100);

    // 串口信号绑定
    connect(m_serialPort, &QSerialPort::readyRead, this, &MotorController::onSerialReadReady);
    connect(m_sendTimer, &QTimer::timeout, this, &MotorController::onSendTimeout);
}

MotorController::~MotorController()
{
    closeSerialPort();
}


// 打开串口
bool MotorController::openSerialPort(const QString &portName, qint32 baudRate)
{
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
    }

    m_serialPort->setPortName(portName);
    m_serialPort->setBaudRate(baudRate);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serialPort->open(QIODevice::ReadWrite)) {
        emit motorStatus(QString("串口打开失败：%1").arg(m_serialPort->errorString()));
        return false;
    }
    emit motorStatus("串口已打开");
    return true;
}

// 关闭串口
void MotorController::closeSerialPort()
{
    // 停止发送超时定时器
    if (m_sendTimer->isActive()) {
        m_sendTimer->stop();
    }
    // 清空待发送队列
    m_cmdQueue.clear();
    // 重置队列发送状态
    m_queueBusy = false;

    if (m_serialPort->isOpen()) {
        m_serialPort->close();
        emit motorStatus("串口已关闭");
    }
}

bool MotorController::isSerialOpen() const
{
    return m_serialPort->isOpen();
}

// CRC16校验（思博485标准算法）
quint16 MotorController::calculateCRC(const QByteArray &data)
{
    quint16 crc = 0xFFFF;
    for (int i = 0; i < data.size(); i++) {
        crc ^= static_cast<quint8>(data.at(i));
        for (int j = 0; j < 8; j++) {
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

// 构造Modbus-RTU指令
QByteArray MotorController::buildModbusCmd(quint8 funcCode, quint16 regAddr, quint16 regNum, const QByteArray &data)
{
    // QByteArray cmd;
    // cmd.append(m_stationAddr);       // 站地址
    // cmd.append(funcCode);           // 功能码
    // cmd.append(static_cast<char>((regAddr >> 8) & 0xFF)); // 寄存器地址高8位
    // cmd.append(static_cast<char>(regAddr & 0xFF));        // 寄存器地址低8位

    // if (funcCode == 0x03 || funcCode == 0x04) { // 读指令
    //     cmd.append(static_cast<char>((regNum >> 8) & 0xFF));
    //     cmd.append(static_cast<char>(regNum & 0xFF));
    // } else if (funcCode == 0x06) { // 写单个寄存器
    //     cmd.append(data);
    // } else if (funcCode == 0x10) { // 写多个寄存器
    //     cmd.append(static_cast<char>((regNum >> 8) & 0xFF));
    //     cmd.append(static_cast<char>(regNum & 0xFF));
    //     cmd.append(static_cast<char>(data.size()));
    //     cmd.append(data);
    // }

    // // 添加CRC校验
    // quint16 crc = calculateCRC(cmd);
    // cmd.append(static_cast<char>(crc & 0xFF));
    // cmd.append(static_cast<char>((crc >> 8) & 0xFF));
    // return cmd;
    return buildModbusCmd(m_stationAddr, funcCode, regAddr, regNum, data);
}

// 构造Modbus-RTU指令（带站号）
QByteArray MotorController::buildModbusCmd(quint8 stationAddr,quint8 funcCode,quint16 regAddr,quint16 regNum,const QByteArray &data)
{
    QByteArray cmd;

    cmd.append(static_cast<char>(stationAddr));
    cmd.append(static_cast<char>(funcCode));

    cmd.append(static_cast<char>((regAddr >> 8) & 0xFF));
    cmd.append(static_cast<char>(regAddr & 0xFF));

    if (funcCode == 0x03 || funcCode == 0x04) {
        cmd.append(static_cast<char>((regNum >> 8) & 0xFF));
        cmd.append(static_cast<char>(regNum & 0xFF));
    } else if (funcCode == 0x06) {
        cmd.append(data);
    } else if (funcCode == 0x10) {
        cmd.append(static_cast<char>((regNum >> 8) & 0xFF));
        cmd.append(static_cast<char>(regNum & 0xFF));
        cmd.append(static_cast<char>(data.size()));
        cmd.append(data);
    }

    quint16 crc = calculateCRC(cmd);
    cmd.append(static_cast<char>(crc & 0xFF));
    cmd.append(static_cast<char>((crc >> 8) & 0xFF));

    return cmd;
}


// 发送串口数据
void MotorController::sendSerialData(const QByteArray &data)
{
    if (!m_serialPort->isOpen()) {
        emit motorStatus("串口未打开");
        return;
    }

    // 先入队，不立即连续发送
    m_cmdQueue.enqueue(data);
    // 如果当前没有正在发送队列，就启动发送
    if (!m_queueBusy) {
        sendNextQueuedCommand();
    }
    // m_serialPort->write(data);
    // m_sendTimer->start();
}


// void MotorController::sendNextQueuedCommand()
// {
//     if (!m_serialPort->isOpen()) {
//         m_queueBusy = false;
//         m_cmdQueue.clear();
//         return;
//     }

//     if (m_cmdQueue.isEmpty()) {
//         m_queueBusy = false;
//         return;
//     }

//     m_queueBusy = true;

//     QByteArray cmd = m_cmdQueue.dequeue();

//     m_serialPort->write(cmd);
//     m_serialPort->flush();

//     m_sendTimer->start();

//     // 固定间隔后发送下一条
//     QTimer::singleShot(m_cmdIntervalMs, this, [this]() {
//         sendNextQueuedCommand();
//     });
// }

bool MotorController::hasPendingCommand() const
{
    return m_queueBusy || !m_cmdQueue.isEmpty();
}


void MotorController::sendNextQueuedCommand()
{
    if (!m_serialPort->isOpen()) {
        m_queueBusy = false;
        m_cmdQueue.clear();
        return;
    }

    // 正在等待上一条命令响应
    if (m_queueBusy) {
        return;
    }

    if (m_cmdQueue.isEmpty()) {
        return;
    }

    m_queueBusy = true;

    QByteArray cmd = m_cmdQueue.dequeue();

    //emit motorStatus(QString("TX 长度=%1，数据=%2").arg(cmd.size()).arg(QString::fromLatin1(cmd.toHex(' '))));

    qint64 result = m_serialPort->write(cmd);

    if (result < 0) {
        emit motorStatus(
            QString("串口发送失败：%1")
                .arg(m_serialPort->errorString()));

        m_queueBusy = false;
        sendNextQueuedCommand();
        return;
    }

    m_serialPort->flush();

    // 最多等待当前命令响应200ms
    m_sendTimer->start();
}


// 电机使能
void MotorController::motorEnable()
{
    QByteArray data;
    data.append(static_cast<char>(0x00));
    data.append(static_cast<char>(0x00));
    QByteArray cmd = buildModbusCmd(0x06, 0x00D4, 1, data);
    sendSerialData(cmd);
    emit motorStatus("电机已使能");
}

// 电机使能（带站号）
void MotorController::motorEnable(quint8 stationAddr)
{
    QByteArray data;
    data.append(static_cast<char>(0x00));
    data.append(static_cast<char>(0x00));

    QByteArray cmd = buildModbusCmd(stationAddr, 0x06, 0x00D4, 1, data);
    sendSerialData(cmd);

    emit motorStatus(QString("电机 %1 已使能").arg(stationAddr));
}


// 电机脱机
void MotorController::motorDisable()
{
    QByteArray data;
    data.append(static_cast<char>(0x00));
    data.append(static_cast<char>(0x01));
    QByteArray cmd = buildModbusCmd(0x06, 0x00D4, 1, data);
    sendSerialData(cmd);
    emit motorStatus("电机已脱机");
}

// 电机脱机（带站号）
void MotorController::motorDisable(quint8 stationAddr)
{
    QByteArray data;
    data.append(static_cast<char>(0x00));
    data.append(static_cast<char>(0x01));

    QByteArray cmd = buildModbusCmd(stationAddr, 0x06, 0x00D4, 1, data);
    sendSerialData(cmd);

    emit motorStatus(QString("电机 %1 已脱机").arg(stationAddr));
}


// 驱动器重启
void MotorController::resetDriver()
{
    QByteArray data;
    data.append(static_cast<char>(0x01));
    data.append(static_cast<char>(0x00));
    QByteArray cmd = buildModbusCmd(0x06, 0x00D4, 1, data);
    sendSerialData(cmd);
    emit motorStatus("驱动器重启中");
}

// 清除报警
void MotorController::clearAlarm()
{
    QByteArray data;
    data.append(static_cast<char>(0x00));
    data.append(static_cast<char>(0x00));
    QByteArray cmd = buildModbusCmd(0x06, 0x00A4, 1, data);
    sendSerialData(cmd);
    emit motorStatus("报警已清除");
}

void MotorController::clearMotorAlarm(quint8 stationAddr)
{
    QByteArray data;
    data.append(static_cast<char>(0x00));
    data.append(static_cast<char>(0x00));

    QByteArray cmd = buildModbusCmd(stationAddr, 0x06, 0x00A4, 1, data);

    sendSerialData(cmd);

    emit motorStatus(
        QString("%1号电机清除报警命令已发送")
            .arg(stationAddr));
}

// 设置运行速度(rpm)
void MotorController::setRunSpeed(qint32 speedRpm)
{
    qint32 speed = speedRpm * 100; // 转换为0.01rpm
    QByteArray data;
    data.append(static_cast<char>((speed >> 24) & 0xFF));
    data.append(static_cast<char>((speed >> 16) & 0xFF));
    data.append(static_cast<char>((speed >> 8) & 0xFF));
    data.append(static_cast<char>(speed & 0xFF));
    QByteArray cmd = buildModbusCmd(0x10, 0x00D9, 2, data);
    sendSerialData(cmd);
    emit motorStatus(QString("速度已设置为：%1 rpm").arg(speedRpm));
}

//绝对位置运动
void MotorController::runToAbsolutePosition(qint32 pos)
{
    QByteArray data;

    // ------------------ 核心修正：字节序 ------------------
    // 思博32位数据写入顺序：[低字高8位][低字低8位][高字高8位][高字低8位]
    // 1. 先拼低16位 (Word 0)
    data.append(static_cast<char>((pos >> 8) & 0xFF));  // 低字高8位
    data.append(static_cast<char>(pos & 0xFF));         // 低字低8位
    // 2. 再拼高16位 (Word 1)
    data.append(static_cast<char>((pos >> 24) & 0xFF)); // 高字高8位
    data.append(static_cast<char>((pos >> 16) & 0xFF));// 高字低8位
    // -------------------------------------------------------

    QByteArray cmd = buildModbusCmd(0x10, 0x00D0, 2, data);
    sendSerialData(cmd);
    emit motorStatus(QString("运行到绝对位置：%1 脉冲").arg(pos));
}

//绝对位置运动（带站号）
void MotorController::runToAbsolutePosition(quint8 stationAddr, qint32 pos)
{
    QByteArray data;

    // 思博32位数据顺序：[低字高8位][低字低8位][高字高8位][高字低8位]
    data.append(static_cast<char>((pos >> 8) & 0xFF));
    data.append(static_cast<char>(pos & 0xFF));
    data.append(static_cast<char>((pos >> 24) & 0xFF));
    data.append(static_cast<char>((pos >> 16) & 0xFF));

    QByteArray cmd = buildModbusCmd(stationAddr, 0x10, 0x00D0, 2, data);
    sendSerialData(cmd);

    emit motorStatus(QString("电机 %1 运行到绝对位置：%2 脉冲").arg(stationAddr).arg(pos));
}


// 回原点
void MotorController::runHome(RunDirection dir, quint16 speed)
{
    quint16 cmdData = (dir << 15) | (speed << 6) | 0x01;
    QByteArray data;
    data.append(static_cast<char>((cmdData >> 8) & 0xFF));
    data.append(static_cast<char>(cmdData & 0xFF));
    QByteArray cmd = buildModbusCmd(0x06, 0x00C9, 1, data);
    sendSerialData(cmd);
    emit motorStatus("回原点执行中");
}

// 设置电机电流(0.01A)
void MotorController::setCurrent(quint16 currentMa)
{
    QByteArray data;
    data.append(static_cast<char>((currentMa >> 8) & 0xFF));
    data.append(static_cast<char>(currentMa & 0xFF));
    QByteArray cmd = buildModbusCmd(0x06, 0x000D, 1, data);
    sendSerialData(cmd);
    emit motorStatus(QString("电流已设置为：%1 mA").arg(currentMa));
}

// 设置细分
void MotorController::setMicrostep(quint32 microstep)
{
    QByteArray data;
    data.append(static_cast<char>((microstep >> 24) & 0xFF));
    data.append(static_cast<char>((microstep >> 16) & 0xFF));
    data.append(static_cast<char>((microstep >> 8) & 0xFF));
    data.append(static_cast<char>(microstep & 0xFF));
    QByteArray cmd = buildModbusCmd(0x10, 0x0024, 2, data);
    sendSerialData(cmd);
    emit motorStatus(QString("细分已设置为：%1 脉冲/转").arg(microstep));
}

// // 串口数据接收
// void MotorController::onSerialReadReady()
// {
//     QByteArray data = m_serialPort->readAll();
//     m_receiveBuffer.append(data);
//     emit receiveData(m_receiveBuffer);
//     m_sendTimer->stop();

//     // --- 修正后的：解析位置返回数据 ---
//     // Modbus-RTU返回帧格式：站号(1) + 功能码(1) + 字节数(1) + 数据(4) + CRC(2) = 9字节
//     if (m_receiveBuffer.size() >= 9) {
//         // 判断是否是读位置的返回（功能码0x03，字节数0x04）
//         if (static_cast<quint8>(m_receiveBuffer[1]) == 0x03 &&
//             static_cast<quint8>(m_receiveBuffer[2]) == 0x04) {

//             // ------------------ 核心修正：字节序解析 ------------------
//             // 思博32位数据返回顺序：[低字高8位][低字低8位][高字高8位][高字低8位]
//             quint8 byte0 = static_cast<quint8>(m_receiveBuffer[3]); // 低字高8位
//             quint8 byte1 = static_cast<quint8>(m_receiveBuffer[4]); // 低字低8位
//             quint8 byte2 = static_cast<quint8>(m_receiveBuffer[5]); // 高字高8位
//             quint8 byte3 = static_cast<quint8>(m_receiveBuffer[6]); // 高字低8位

//             // 1. 先分别拼出低16位和高16位
//             quint16 lowWord  = (byte0 << 8) | byte1;  // 低16位 (Word 0)
//             quint16 highWord = (byte2 << 8) | byte3;  // 高16位 (Word 1)

//             // 2. 拼出最终32位位置：高16位左移16位，加上低16位
//             qint32 position = (static_cast<qint32>(highWord) << 16) | lowWord;
//             // -----------------------------------------------------------

//             // 发送信号给界面（这里的单位转换你根据实际情况保留）
//             //emit positionRead(double(position*100) / 4096.0 / 105.0);
//             emit positionRead(double(position) / 4000.0 );
//             //emit motorStatus(QString("当前位置：%1 脉冲").arg(position));
//         }

//         m_receiveBuffer.clear(); // 解析完清空缓存
//     }
// }

// // 串口数据接收（带站号）
// void MotorController::onSerialReadReady()
// {
//     QByteArray data = m_serialPort->readAll();
//     m_receiveBuffer.append(data);
//     emit receiveData(m_receiveBuffer);
//     m_sendTimer->stop();

//     // 读位置返回：站号(1) + 功能码(1) + 字节数(1) + 数据(4) + CRC(2) = 9字节
//     if (m_receiveBuffer.size() >= 9) {

//         quint8 stationAddr = static_cast<quint8>(m_receiveBuffer[0]);
//         quint8 funcCode    = static_cast<quint8>(m_receiveBuffer[1]);
//         quint8 byteCount   = static_cast<quint8>(m_receiveBuffer[2]);

//         if (funcCode == 0x03 && byteCount == 0x04) {

//             quint8 byte0 = static_cast<quint8>(m_receiveBuffer[3]);
//             quint8 byte1 = static_cast<quint8>(m_receiveBuffer[4]);
//             quint8 byte2 = static_cast<quint8>(m_receiveBuffer[5]);
//             quint8 byte3 = static_cast<quint8>(m_receiveBuffer[6]);

//             quint16 lowWord  = (byte0 << 8) | byte1;
//             quint16 highWord = (byte2 << 8) | byte3;

//             qint32 position = (static_cast<qint32>(highWord) << 16) | lowWord;

//             //double realPos = double(position * 100) / 4000.0 / 105.0;
//             double realPos = double(position) / 4000.0 ;

//             emit positionRead(realPos);                 // 保留原来的信号
//             emit positionRead(stationAddr, realPos);    // 新增带站号的信号
//         }

//         m_receiveBuffer.clear();
//     }
// }


// 串口数据接收（带站号）
void MotorController::onSerialReadReady()
{
    QByteArray data = m_serialPort->readAll();

    //emit motorStatus(QString("RX 长度=%1，数据=%2").arg(data.size()).arg(QString::fromLatin1(data.toHex(' '))));

    m_receiveBuffer.append(data);

    emit receiveData(m_receiveBuffer);

    while (m_receiveBuffer.size() >= 2) {

        quint8 funcCode = static_cast<quint8>(m_receiveBuffer[1]);

        int frameLength = 0;

        // 写单寄存器、写多个寄存器响应
        if (funcCode == 0x06 || funcCode == 0x10) {
            frameLength = 8;
        }
        // 位置响应
        else if (funcCode == 0x03) {
            if (m_receiveBuffer.size() < 3) {
                break;
            }

            quint8 byteCount = static_cast<quint8>(m_receiveBuffer[2]);

            // 0x04：位置，两个寄存器
            // 0x02：报警状态，一个寄存器
            if (byteCount != 0x04 && byteCount != 0x02) {
                m_receiveBuffer.remove(0, 1);
                continue;
            }

            frameLength = 3 + byteCount + 2;
        }
        else {
            m_receiveBuffer.remove(0, 1);
            continue;
        }

        // 当前帧还没接收完整
        if (m_receiveBuffer.size() < frameLength) {
            break;
        }

        QByteArray frame = m_receiveBuffer.left(frameLength);

        quint16 receivedCRC =
            static_cast<quint8>(frame[frameLength - 2]) |
            (static_cast<quint16>(
                 static_cast<quint8>(
                     frame[frameLength - 1])) << 8);

        quint16 calculatedCRC =calculateCRC(frame.left(frameLength - 2));

        if (receivedCRC != calculatedCRC) {
            emit motorStatus("思博电机返回数据CRC校验失败");

            m_receiveBuffer.remove(0, 1);
            continue;
        }

        // CRC正确，只删除当前帧
        m_receiveBuffer.remove(0, frameLength);

        // 解析读取响应
        if (funcCode == 0x03) {

            quint8 stationAddr =
                static_cast<quint8>(frame[0]);

            quint8 byteCount =
                static_cast<quint8>(frame[2]);

            // 报警状态响应：0x00A3
            if (byteCount == 0x02) {

                quint16 alarmCode =
                    (static_cast<quint8>(frame[3]) << 8) |
                    static_cast<quint8>(frame[4]);

                // 当前报警信息在低4位
                alarmCode &= 0x000F;

                QString reason;

                switch (alarmCode) {
                case 0:
                    reason = "正常";
                    break;
                case 1:
                    reason = "电机相位过流";
                    break;
                case 2:
                    reason = "供电电压过高";
                    break;
                case 3:
                    reason = "供电电压过低";
                    break;
                case 4:
                    reason = "电机A相开路";
                    break;
                case 5:
                    reason = "电机B相开路";
                    break;
                case 6:
                    reason = "其他报警或位置超差";
                    break;
                case 7:
                    reason = "内部24V电压偏移";
                    break;
                case 8:
                    reason = "AI电压错误";
                    break;
                case 9:
                    reason = "BI电压错误";
                    break;
                case 10:
                    reason = "编码器错误";
                    break;
                default:
                    reason =
                        QString("未知报警代码：%1")
                            .arg(alarmCode);
                    break;
                }

                emit motorFaultChanged(
                    stationAddr,
                    alarmCode != 0,
                    reason);
            }

            // 位置响应：0x0004~0x0005
            else if (byteCount == 0x04) {

                quint8 byte0 =
                    static_cast<quint8>(frame[3]);
                quint8 byte1 =
                    static_cast<quint8>(frame[4]);
                quint8 byte2 =
                    static_cast<quint8>(frame[5]);
                quint8 byte3 =
                    static_cast<quint8>(frame[6]);

                quint16 lowWord = (byte0 << 8) | byte1;

                quint16 highWord = (byte2 << 8) | byte3;

                quint32 rawPosition = (static_cast<quint32>(highWord) << 16) | lowWord;

                qint32 position = static_cast<qint32>(rawPosition);

                double realPos = static_cast<double>(position) / 4000.0;

                emit positionRead(stationAddr,realPos);
            }
        }

        // 0x06、0x10写响应不需要解析业务数据。
        // 收到CRC正确的响应后，当前命令结束。
        m_sendTimer->stop();
        m_queueBusy = false;

        // 发送队列中的下一条命令
        sendNextQueuedCommand();
    }
}


// 发送超时
void MotorController::onSendTimeout()
{
    emit motorStatus("指令发送超时，无响应");

    //emit motorStatus(QString("指令发送超时，无响应；待发送队列数量=%1").arg(m_cmdQueue.size()));

    m_queueBusy = false;
    sendNextQueuedCommand();
}



// 电机正转（顺时针）
void MotorController::motorRunCW(qint32 speedRpm)
{
    // 1. 设置目标速度（使用手册推荐的 0x00D8~0x00D9 寄存器，32位）
    qint32 speedVal = speedRpm * 100; // 转换为手册单位：0.01rpm
    QByteArray speedData;
    speedData.append(static_cast<char>((speedVal >> 8) & 0xFF));  // 低字高8位
    speedData.append(static_cast<char>(speedVal & 0xFF));         // 低字低8位
    speedData.append(static_cast<char>((speedVal >> 24) & 0xFF)); // 高字高8位
    speedData.append(static_cast<char>((speedVal >> 16) & 0xFF));// 高字低8位
    QByteArray speedCmd = buildModbusCmd(0x10, 0x00D8, 2, speedData);
    sendSerialData(speedCmd);

    QTimer::singleShot(500,this,[=](){
        // 2. 启动正转（使用手册 0x00C8 寄存器：运行和停止）
        // 手册示例：写 0x0001 启动正转运行
        QByteArray runData;
        runData.append(static_cast<char>(0x00)); // 寄存器值高位
        runData.append(static_cast<char>(0x01)); // 寄存器值低位：0x0001 表示正转启动
        QByteArray runCmd = buildModbusCmd(0x06, 0x00C8, 1, runData);
        sendSerialData(runCmd);

        emit motorStatus(QString("电机正转中，速度：%1 rpm").arg(speedRpm));
    });
}

// 电机正转（顺时针）带站号
void MotorController::motorRunCWByStation(quint8 stationAddr)
{
    QByteArray runData;
    runData.append(static_cast<char>(0x00));
    runData.append(static_cast<char>(0x01)); // 0x0001：正转启动

    QByteArray runCmd =
        buildModbusCmd(stationAddr, 0x06, 0x00C8, 1, runData);

    sendSerialData(runCmd);

    emit motorStatus(
        QString("%1号电机开始正转").arg(stationAddr));
}

// 电机反转（逆时针）
void MotorController::motorRunCCW(qint32 speedRpm)
{
    // 1. 设置目标速度（负值表示反转方向，或通过方向位控制，这里用速度正负+方向位双重保险）
    qint32 speedVal = -speedRpm * 100; // 转换为负值
    QByteArray speedData;
    speedData.append(static_cast<char>((speedVal >> 8) & 0xFF));  // 低字高8位
    speedData.append(static_cast<char>(speedVal & 0xFF));         // 低字低8位
    speedData.append(static_cast<char>((speedVal >> 24) & 0xFF)); // 高字高8位
    speedData.append(static_cast<char>((speedVal >> 16) & 0xFF));// 高字低8位
    // speedData.append(static_cast<char>((speedVal >> 24) & 0xFF));
    // speedData.append(static_cast<char>((speedVal >> 16) & 0xFF));
    // speedData.append(static_cast<char>((speedVal >> 8) & 0xFF));
    // speedData.append(static_cast<char>(speedVal & 0xFF));
    QByteArray speedCmd = buildModbusCmd(0x10, 0x00D8, 2, speedData);
    sendSerialData(speedCmd);

    QTimer::singleShot(500,this,[=](){
        // 2. 启动反转（使用 0x00C8 寄存器）
        // 写 0x0101 启动反转运行（bit15 为方向位，1 表示反转）
        QByteArray runData;
        runData.append(0x01); // 寄存器值高位：bit15=1 表示反转
        runData.append(0x01); // 寄存器值低位：0x01 表示启动
        QByteArray runCmd = buildModbusCmd(0x06, 0x00C8, 1, runData);
        sendSerialData(runCmd);
        emit motorStatus(QString("电机反转中，速度：%1 rpm").arg(speedRpm));
    });
}

//电机反转（逆时针）带站号
void MotorController::motorRunCCWByStation(quint8 stationAddr)
{
    QByteArray runData;
    runData.append(static_cast<char>(0x01));
    runData.append(static_cast<char>(0x01)); // 沿用原代码：0x0101反转启动

    QByteArray runCmd =
        buildModbusCmd(stationAddr, 0x06, 0x00C8, 1, runData);

    sendSerialData(runCmd);

    emit motorStatus(
        QString("%1号电机开始反转").arg(stationAddr));
}

// 电机停止
void MotorController::motorStop()
{
    QByteArray stopData;
    stopData.append(static_cast<char>(0x00));
    stopData.append(static_cast<char>(0x00));
    QByteArray stopCmd = buildModbusCmd(0x06, 0x00C8, 1, stopData);
    sendSerialData(stopCmd);

    emit motorStatus("电机已停止");
}

// 电机停止（带站号）
void MotorController::motorStop(quint8 stationAddr)
{
    QByteArray stopData;
    stopData.append(static_cast<char>(0x00));
    stopData.append(static_cast<char>(0x00));

    QByteArray stopCmd = buildModbusCmd(stationAddr, 0x06, 0x00C8, 1, stopData);
    sendSerialData(stopCmd);

    emit motorStatus(QString("电机 %1 已停止").arg(stationAddr));
}

// 读取电机实时位置
void MotorController::readMotorPosition()
{
    // 构造读指令：功能码0x03，起始地址0x0004，读取2个寄存器(1个DWORD=4字节)
    QByteArray cmd = buildModbusCmd(0x03, 0x0004, 2);
    sendSerialData(cmd);
}

// 读取电机实时位置（带站号）
void MotorController::readMotorPosition(quint8 stationAddr)
{
    QByteArray cmd = buildModbusCmd(stationAddr, 0x03, 0x0004, 2);
    sendSerialData(cmd);
}

// 读取当前报警状态
void MotorController::readMotorAlarm(quint8 stationAddr)
{
    // 手册：0x00A3，读取当前报警状态
    QByteArray cmd = buildModbusCmd(stationAddr, 0x03, 0x00A3, 1);

    sendSerialData(cmd);
}

// 强制设置电机当前绝对位置
// 参数 pos：要设置的目标位置（脉冲数）
void MotorController::setMotorAbsolutePosition(qint32 pos)
{
    QByteArray data;
    // 1. 先拼低16位（Word 0）：高字节在前
    data.append(static_cast<char>((pos >> 8) & 0xFF));  // 低16位的高8位
    data.append(static_cast<char>(pos & 0xFF));         // 低16位的低8位
    // 2. 再拼高16位（Word 1）：高字节在前
    data.append(static_cast<char>((pos >> 24) & 0xFF)); // 高16位的高8位
    data.append(static_cast<char>((pos >> 16) & 0xFF));// 高16位的低8位

    // 构造Modbus写指令：
    // 功能码 0x10（写多个寄存器）
    // 起始地址 0x00D2（思博手册中“设置当前位置”的寄存器地址）
    // 寄存器数量 2（1个32位数据占2个寄存器）
    QByteArray cmd = buildModbusCmd(0x10, 0x00D2, 2, data);
    sendSerialData(cmd);

    emit motorStatus(QString("电机当前位置已设置为：%1 脉冲").arg(pos));
}


// 强制设置指定站号电机的当前绝对位置
// stationAddr：电机站号
// pos：要设置的位置，单位脉冲
void MotorController::setMotorAbsolutePosition(quint8 stationAddr, qint32 pos)
{
    QByteArray data;

    quint32 rawPos = static_cast<quint32>(pos);

    // 1. 先拼低16位 Word0，高字节在前
    data.append(static_cast<char>((rawPos >> 8) & 0xFF));
    data.append(static_cast<char>(rawPos & 0xFF));

    // 2. 再拼高16位 Word1，高字节在前
    data.append(static_cast<char>((rawPos >> 24) & 0xFF));
    data.append(static_cast<char>((rawPos >> 16) & 0xFF));

    // 功能码 0x10：写多个寄存器
    // 起始地址 0x00D2：设置当前位置寄存器
    // 寄存器数量 2：32位数据占2个寄存器
    QByteArray cmd = buildModbusCmd(stationAddr, 0x10, 0x00D2, 2, data);

    sendSerialData(cmd);

    emit motorStatus(QString("电机[%1]当前位置已设置为：%2 脉冲")
                         .arg(stationAddr)
                         .arg(pos));
}


// 设置电机运行速度
void MotorController::setMotorSpeed(qint32 speedRpm)
{
    // 单位转换：rpm -> 0.01rpm
    qint32 speedVal = speedRpm * 100;

    QByteArray data;
    data.append(static_cast<char>((speedVal >> 8) & 0xFF));  // 低字高8位
    data.append(static_cast<char>(speedVal & 0xFF));         // 低字低8位
    data.append(static_cast<char>((speedVal >> 24) & 0xFF)); // 高字高8位
    data.append(static_cast<char>((speedVal >> 16) & 0xFF));// 高字低8位

    // 寄存器地址 0x00D8~0x00D9
    QByteArray cmd = buildModbusCmd(0x10, 0x00D8, 2, data);
    sendSerialData(cmd);

    emit motorStatus(QString("速度已设置为：%1 rpm").arg(speedRpm));
}

// 设置电机运行速度（带站号）
void MotorController::setMotorSpeed(quint8 stationAddr, qint32 speedRpm)
{
    qint32 speedVal = speedRpm * 100;

    QByteArray data;
    data.append(static_cast<char>((speedVal >> 8) & 0xFF));
    data.append(static_cast<char>(speedVal & 0xFF));
    data.append(static_cast<char>((speedVal >> 24) & 0xFF));
    data.append(static_cast<char>((speedVal >> 16) & 0xFF));

    QByteArray cmd = buildModbusCmd(stationAddr, 0x10, 0x00D8, 2, data);
    sendSerialData(cmd);

    emit motorStatus(QString("电机 %1 速度设置为：%2 rpm")
                         .arg(stationAddr)
                         .arg(speedRpm));
}


// 设置电机加速时间
void MotorController::setMotorAccelerationTime(quint16 timeMs)
{
    QByteArray data;
    // 16位寄存器，高字节在前
    data.append(static_cast<char>((timeMs >> 8) & 0xFF));
    data.append(static_cast<char>(timeMs & 0xFF));

    // 手册标准地址0x0098，功能码0x06写单个寄存器
    QByteArray cmd = buildModbusCmd(0x06, 0x0098, 1, data);
    sendSerialData(cmd);

    emit motorStatus(QString("加速时间已设置为：%1 ms").arg(timeMs));
}


// 设置指定站号电机加速时间
void MotorController::setAccelerationTime(quint8 stationAddr, quint16 timeMs)
{
    QByteArray data;

    // 16位寄存器，高字节在前
    data.append(static_cast<char>((timeMs >> 8) & 0xFF));
    data.append(static_cast<char>(timeMs & 0xFF));

    // 手册标准地址 0x0098，功能码 0x06 写单个寄存器
    QByteArray cmd = buildModbusCmd(stationAddr, 0x06, 0x0098, 1, data);
    sendSerialData(cmd);

    emit motorStatus(QString("电机 %1 加速时间已设置为：%2 ms")
                         .arg(stationAddr)
                         .arg(timeMs));
}


// 设置电机减速时间
void MotorController::setMotorDecelerationTime(quint16 timeMs)
{
    QByteArray data;
    // 16位寄存器，高字节在前
    data.append(static_cast<char>((timeMs >> 8) & 0xFF));
    data.append(static_cast<char>(timeMs & 0xFF));

    // 手册标准地址0x0099，功能码0x06写单个寄存器
    QByteArray cmd = buildModbusCmd(0x06, 0x0099, 1, data);
    sendSerialData(cmd);

    emit motorStatus(QString("减速时间已设置为：%1 ms").arg(timeMs));
}


// 设置指定站号电机减速时间
void MotorController::setDecelerationTime(quint8 stationAddr, quint16 timeMs)
{
    QByteArray data;

    // 16位寄存器，高字节在前
    data.append(static_cast<char>((timeMs >> 8) & 0xFF));
    data.append(static_cast<char>(timeMs & 0xFF));

    // 手册标准地址 0x0099，功能码 0x06 写单个寄存器
    QByteArray cmd = buildModbusCmd(stationAddr, 0x06, 0x0099, 1, data);
    sendSerialData(cmd);

    emit motorStatus(QString("电机 %1 减速时间已设置为：%2 ms")
                         .arg(stationAddr)
                         .arg(timeMs));
}


// 正限位设置
void MotorController::setPositiveLimit(qint32 pos)
{
    QByteArray data;
    // 低16位在前
    data.append(static_cast<char>((pos >> 8) & 0xFF));
    data.append(static_cast<char>(pos & 0xFF));
    // 高16位在后
    data.append(static_cast<char>((pos >> 24) & 0xFF));
    data.append(static_cast<char>((pos >> 16) & 0xFF));

    QByteArray cmd = buildModbusCmd(0x10, 0x0070, 2, data);
    sendSerialData(cmd);
}

// 负限位设置
void MotorController::setNegativeLimit(qint32 pos)
{
    QByteArray data;
    // 低16位在前
    data.append(static_cast<char>((pos >> 8) & 0xFF));
    data.append(static_cast<char>(pos & 0xFF));
    // 高16位在后
    data.append(static_cast<char>((pos >> 24) & 0xFF));
    data.append(static_cast<char>((pos >> 16) & 0xFF));

    QByteArray cmd = buildModbusCmd(0x10, 0x006E, 2, data);
    sendSerialData(cmd);
}
