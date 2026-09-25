#include "UdpCommunication.h"
#include <QDebug>
#include <cstdint>
#include <algorithm>

// ====================== UdpReceiveThread 实现 ======================
UdpReceiveThread::UdpReceiveThread(const QHostAddress &localIp, qint16 localPort, int receiveTimeoutMs, QObject *parent)
    : QThread(parent), m_localIp(localIp), m_localPort(localPort), m_receiveTimeoutMs(receiveTimeoutMs)
{
    setTerminationEnabled(false);
}

UdpReceiveThread::~UdpReceiveThread()
{
    stop();
    wait(100);
    qDebug() << "[接收线程] 已销毁";
}

void UdpReceiveThread::stop()
{
    m_isRunning.store(false, std::memory_order_release);
}

void UdpReceiveThread::run()
{
    m_isRunning.store(true, std::memory_order_release);

    m_udpSocket = new QUdpSocket();
    m_udpSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);

    if (!m_udpSocket->bind(m_localIp, m_localPort, QUdpSocket::ShareAddress)) {
        {
            m_isRunning.store(false, std::memory_order_release);
        }
        emit errorOccurred(QString("上位机IP:%1 端口:%2 绑定失败：%3")
                               .arg(m_localIp.toString())
                               .arg(m_localPort)
                               .arg(m_udpSocket->errorString()));
        delete m_udpSocket;
        m_udpSocket = nullptr;
        return;
    }

    while (m_isRunning.load(std::memory_order_acquire)) {
        bool hasData = m_udpSocket->waitForReadyRead(m_receiveTimeoutMs);

        if (!m_isRunning.load(std::memory_order_acquire))
            break;

        if (!hasData) {
            emit receiveTimeout();
            continue;
        }
        while (m_udpSocket->hasPendingDatagrams() && m_isRunning) {
            QByteArray datagram;
            datagram.resize(m_udpSocket->pendingDatagramSize());
            qint64 bytesRead = m_udpSocket->readDatagram(datagram.data(), datagram.size());

            if (bytesRead == -1) {
                emit errorOccurred(QString("接收数据失败：%1").arg(m_udpSocket->errorString()));
                continue;
            }
            // qDebug() << "[接收线程] 收到数据：" << bytesRead << "字节，内容：" << datagram.toHex(' ');
            if (bytesRead != 88) {
                emit errorOccurred(QString("接收数据长度异常：实际%1字节，期望88字节").arg(bytesRead));
                continue; 
            }
            emit dataReceived(datagram);
        }
    }

    m_udpSocket->abort();
    delete m_udpSocket;
    m_udpSocket = nullptr;
    qDebug() << "[接收线程] 已停止";
}

// ====================== UdpCommunication 实现 ======================
UdpCommunication::UdpCommunication(const QString &localIp, qint16 localPort, const QString &slaveIp, quint16 slavePort, int receiveTimeoutMs, QObject *parent)
    : QObject(parent),
    m_localPort(localPort),
    m_slavePort(slavePort),
    m_receiveTimeoutMs(receiveTimeoutMs)
{
    if (!m_localIp.setAddress(localIp)) {
        qFatal("[UDP通信] 上位机IP:%1 无效！程序退出。", qPrintable(localIp));
    }

    if (!m_slaveIp.setAddress(slaveIp)) {
        qFatal("[UDP通信] 下位机IP:%1 无效！程序退出。", qPrintable(slaveIp));
    }

    m_sendSocket = new QUdpSocket(this);
    m_sendSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);

    m_receiveThread = new UdpReceiveThread(m_localIp, m_localPort, m_receiveTimeoutMs, this);

    // 连接接收线程信号
    connect(m_receiveThread, &UdpReceiveThread::dataReceived,
            this, &UdpCommunication::onThreadDataReceived,
            Qt::QueuedConnection);
    connect(m_receiveThread, &UdpReceiveThread::errorOccurred,
            this, &UdpCommunication::onThreadErrorOccurred,
            Qt::QueuedConnection);
    connect(m_receiveThread, &UdpReceiveThread::receiveTimeout,
            this, &UdpCommunication::onReceiveTimeout,
            Qt::QueuedConnection);

    startCommunication();
}

UdpCommunication::~UdpCommunication()
{
    stopCommunication();
    delete m_sendSocket;
    qDebug() << "[UDP通信] 已销毁";
}

bool UdpCommunication::sendData(const QByteArray &data)
{
    // 可选：添加线程检查，防止误调用
    if (QThread::currentThread() != this->thread()) {
        qWarning() << "sendData() called from wrong thread! Use sendDataAsync() instead.";
        return false;
    }

    if (data.size() != 40) {
        qDebug()<<QString("发送数据长度异常：实际%1字节，期望40字节").arg(data.size());
        emit communicationError(QString("发送数据长度异常：实际%1字节，期望40字节").arg(data.size()));
        return false;
    }

    if (!isValidDataFrame(data)) {
        qDebug()<<"发送失败：数据帧格式非法（帧头不是'P'或帧尾不是'#'）";
        emit communicationError("发送失败：数据帧格式非法（帧头不是'P'或帧尾不是'#'）");
        return false;
    }

    qint64 bytesSent = m_sendSocket->writeDatagram(data, m_slaveIp, m_slavePort);
    if (bytesSent == -1) {
        qDebug()<<QString("发送数据失败：%1").arg(m_sendSocket->errorString());
        emit communicationError(QString("发送数据失败：%1").arg(m_sendSocket->errorString()));
        return false;
    }

    return true;
}

bool UdpCommunication::startCommunication()
{
    if (m_isRunning) {
        emit communicationError("启动失败：通信已在运行");
        return false;
    }

    if (!m_receiveThread->isRunning()) {
        m_receiveThread->start();
        QThread::msleep(50);
    }

    if (!m_receiveThread->isRunning()) {
        emit communicationError("启动失败：接收线程未启动");
        return false;
    }

    m_isRunning = true;
    emit stateChanged(m_isRunning);
    // qDebug() << "[UDP通信] 启动成功，当前绑定：上位机" << m_localIp.toString() << ":" << m_localPort;
    return true;
}

void UdpCommunication::stopCommunication()
{
    if (!m_isRunning) return;

    if (m_receiveThread->isRunning()) {
        m_receiveThread->stop();
        if (!m_receiveThread->wait(1000)) {
            m_receiveThread->terminate();
            m_receiveThread->wait();
            emit communicationError("停止警告：接收线程强制终止");
        }
    }

    m_sendSocket->abort();

    {
        QMutexLocker locker(&m_commSuccessMutex);
        m_isCommSuccess = false;
        emit communicationSuccessChanged(false);
    }

    m_isRunning = false;
    emit stateChanged(m_isRunning);
    qDebug() << "[UDP通信] 已停止，解绑：上位机" << m_localIp.toString() << ":" << m_localPort;
}

bool UdpCommunication::isRunning() const
{
    return m_isRunning;
}

bool UdpCommunication::isCommunicationSuccess() const
{
    QMutexLocker locker(&m_commSuccessMutex);
    return m_isCommSuccess;
}

void UdpCommunication::onThreadDataReceived(const QByteArray &data)
{
    // 1. 校验数据长度（必须88字节）
    if (data.size() != RAW_DATA_LENGTH) {
        qDebug() << QString("接收数据长度异常：实际%1字节，期望%2字节")
                        .arg(data.size()).arg(RAW_DATA_LENGTH);
        qDebug() << "接收到的异常数据（十六进制）：" << data.toHex(' ');

        emit communicationError(
            QString("接收数据长度异常：实际%1字节，期望%2字节\n异常数据：%3")
                .arg(data.size())
                .arg(RAW_DATA_LENGTH)
                .arg(QString(data.toHex(' ')))
            );
        return;
    }
    // 2. 校验起始字符（首字节必须为'R'）
    if (data.at(0) != 'R') {
        qDebug() << "格式错误";
        emit communicationError(QString("帧格式错误：起始字符应为'R'，实际为0x%1").arg(
            static_cast<quint8>(data.at(0)), 2, 16, QChar('0')));
        return;
    }

    // 3. 校验停止字符（最后一字节必须为回车'\r'）
    const char stopChar = '\r'; // 回车的ASCII码为0x0D
    if (data.at(RAW_DATA_LENGTH - 1) != stopChar) {
        qDebug() << "停止字符错误";
        emit communicationError(QString("帧格式错误：停止字符应为回车('\\r')，实际为0x%1").arg(
            static_cast<quint8>(data.at(RAW_DATA_LENGTH - 1)), 2, 16, QChar('0')));
        return;
    }

    // 4. 校验和验证（最终正确规则）
    // 4.1 计算参与校验字节的累加和：索引1~84（共84字节，排除首字节0、校验和85~86、帧尾87）
    quint16 calculatedChecksum = 0;
    const int CHECK_START = 1;   // 参与校验起始索引（用户明确：从索引1开始）
    const int CHECK_END = 84;    // 参与校验结束索引（用户明确：到索引84结束）
    for (int i = CHECK_START; i <= CHECK_END; ++i) {
        // 无符号字节累加，直接取16位结果（无需进位、反码、补码）
        calculatedChecksum += static_cast<quint8>(data.at(i));
    }

    // 4.2 提取接收帧中的校验和（85字节=高8位，86字节=低8位，大端序）
    quint8 checksumHigh = static_cast<quint8>(data.at(85));
    quint8 checksumLow = static_cast<quint8>(data.at(86));
    quint16 receivedChecksum = (static_cast<quint16>(checksumHigh) << 8) | checksumLow;

    // 4.3 比较校验和
    if (calculatedChecksum != receivedChecksum) {
        qDebug() << "[通信错误] 校验和不匹配："
                 << "索引1~84累加和=0x" << QString("%1").arg(calculatedChecksum, 4, 16, QChar('0')).toUpper()
                 << "，接收值0x" << QString("%1").arg(receivedChecksum, 4, 16, QChar('0')).toUpper();
        emit communicationError(QString("校验和不匹配：计算值0x%1，接收值0x%2")
                                    .arg(QString("%1").arg(calculatedChecksum, 4, 16, QChar('0')).toUpper())
                                    .arg(QString("%1").arg(receivedChecksum, 4, 16, QChar('0')).toUpper()));
        // return;
    }

    // 校验通过，继续解析姿态数据
    // qDebug() << "[接收解析] 帧格式及校验和验证通过，开始解析姿态和反馈位...";

    // 解析姿态数据（0-based索引52-63字节，对应1-based 53-64）
    int16_t xInt = twoBytesToInt16(data, 53); // x: 52-53字节
    int16_t yInt = twoBytesToInt16(data, 55); // y: 54-55字节
    int16_t zInt = twoBytesToInt16(data, 57); // z: 56-57字节
    int16_t rxInt = twoBytesToInt16(data, 59); // rx: 58-59字节
    int16_t ryInt = twoBytesToInt16(data, 61); // ry: 60-61字节
    int16_t rzInt = twoBytesToInt16(data, 63); // rz: 62-63字节

    // 新增：解析陀螺仪数据（0-based索引41-46字节，每个参数占2字节，顺序：俯仰→测滚→偏航）
    int16_t gyroPitchInt = twoBytesToInt16(data, 41); // 陀螺仪俯仰（pitch）：41-42字节（0-based）
    int16_t gyroRollInt = twoBytesToInt16(data, 43);  // 陀螺仪测滚（roll）：43-44字节（0-based）
    int16_t gyroYawInt = twoBytesToInt16(data, 45);   // 陀螺仪偏航（yaw）：45-46字节（0-based）


    // 还原为浮点数（除以缩放因子）
    float x = static_cast<float>(xInt) / COORD_FACTOR;
    float y = static_cast<float>(yInt) / COORD_FACTOR;
    float z = static_cast<float>(zInt) / 10;
    float rx = static_cast<float>(rxInt) / COORD_FACTOR;
    float ry = static_cast<float>(ryInt) / COORD_FACTOR;
    float rz = static_cast<float>(rzInt) / COORD_FACTOR;
    float gyroPitch = static_cast<float>(gyroPitchInt) / COORD_FACTOR;
    float gyroRoll = static_cast<float>(gyroRollInt) / COORD_FACTOR;
    float gyroYaw = static_cast<float>(gyroYawInt) / COORD_FACTOR;



    // 解析到达目标位置反馈（0-based 81字节的bit0）
    quint8 feedbackByte = static_cast<quint8>(data.at(81));
    bool isReachTarget = (feedbackByte & 0x01) != 0; // bit0=1 → 到达目标
    bool emergencyStop = (feedbackByte & 0x02) != 0;   // bit1：急停报警（1异常）
    bool lightFault = (feedbackByte & 0x04) != 0;      // bit2：轻故障报警（1异常）
    bool heavyFault = (feedbackByte & 0x08) != 0;      // bit3：重故障报警（1异常）
    bool servoEnabled = (feedbackByte & 0x10) != 0;    // bit4：伺服使能（1通电）
    bool isMoving = (feedbackByte & 0x20) != 0;        // bit5 → 是否运动（1=运动中，0=静止）
    // 在 onThreadDataReceived 函数中
    {
        QMutexLocker locker(&m_coordinatesMutex);  // 锁定期间 m_coordinates 可修改
        m_coordinates["x"] = x;
        m_coordinates["y"] = y;
        m_coordinates["z"] = z;
        m_coordinates["rx"] = rx;
        m_coordinates["ry"] = ry;
        m_coordinates["rz"] = rz;
        m_coordinates["gyroPitch"] = gyroPitch;
        m_coordinates["gyroRoll"] = gyroRoll;
        m_coordinates["gyroYaw"] = gyroYaw;

    }  // 作用域结束，自动解锁
    {
        QMutexLocker locker(&m_movingMutex);
        m_isPlatformMoving = isMoving;
    }

    emit poseFeedback(x, y, z, rx, ry, rz,
                      gyroPitch, gyroRoll, gyroYaw, // 新增陀螺仪数据
                      isReachTarget,
                      emergencyStop,
                      lightFault,
                      heavyFault,
                      servoEnabled,
                      isMoving);

    // 更新通讯状态为成功
    {
        QMutexLocker locker(&m_commSuccessMutex);
        if (!m_isCommSuccess) {
            m_isCommSuccess = true;
            emit communicationSuccessChanged(true);
        }
    }

    // 保留原始数据信号发射
    emit receiveData(data);
}

QJsonObject UdpCommunication::getCurrentCoordinates() const
{
    QMutexLocker locker(&m_coordinatesMutex);
    return m_coordinates; // 返回数据副本，避免外部直接修改内部状态
}

void UdpCommunication::onThreadErrorOccurred(const QString &errorMsg)
{
    emit communicationError(errorMsg);
    qDebug() << "[UDP通信] 错误：" << errorMsg << " → 通讯失败";

    {
        QMutexLocker locker(&m_commSuccessMutex);
        if (m_isCommSuccess) {
            m_isCommSuccess = false;
            emit communicationSuccessChanged(false);
        }
    }
}

void UdpCommunication::onReceiveTimeout()
{
    qDebug() << "[UDP通信] 接收超时（" << m_receiveTimeoutMs << "ms未收到数据） → 通讯失败";

    {
        QMutexLocker locker(&m_commSuccessMutex);
        if (m_isCommSuccess) {
            m_isCommSuccess = false;
            emit communicationSuccessChanged(false);
        }
    }
}

// 辅助函数：校验发送端数据帧合法性
bool UdpCommunication::isValidDataFrame(const QByteArray &frame)
{
    return (frame.size() == 40) && (frame.at(0) == 'P') && (frame.at(39) == '#');
}

// ====================== 发送端辅助函数实现 ======================
int16_t UdpCommunication::floatToInt16(float value, float factor, int16_t min, int16_t max)
{
    float scaledValue = value * factor;

    // 自定义clamp逻辑，替代std::clamp（兼容C++11/14）
    float clampedValue = scaledValue;
    if (clampedValue < min) {
        clampedValue = static_cast<float>(min);
    } else if (clampedValue > max) {
        clampedValue = static_cast<float>(max);
    }

    int16_t result = static_cast<int16_t>(clampedValue);

    // 保留原有的越界警告逻辑
    if (scaledValue < min || scaledValue > max) {
        qWarning() << "[浮点数转换] 数值超出量程！原始值=" << value
                   << "，缩放后=" << scaledValue
                   << "，截断为" << result;
    }
    return result;
}

void UdpCommunication::int16ToTwoBytes(QByteArray &frame, int startIndex, int16_t value)
{
    if (startIndex < 1 || startIndex + 1 > 36) {
        emit communicationError(QString("[字节填充] 起始索引%1非法（需满足1≤index≤35）").arg(startIndex));
        return;
    }
    uint8_t highByte = (value >> 8) & 0xFF;
    uint8_t lowByte = value & 0xFF;
    frame[startIndex] = static_cast<char>(highByte);
    frame[startIndex + 1] = static_cast<char>(lowByte);
}

// ====================== 接收端辅助函数实现 ======================
// 2字节（大端序）→16位有符号整数（int16_t）
int16_t UdpCommunication::twoBytesToInt16(const QByteArray &data, int startIndex)
{
    // 校验起始索引合法性（确保startIndex和startIndex+1在0-87范围内）
    if (startIndex < 0 || startIndex + 1 >= RAW_DATA_LENGTH) {
        qWarning() << "[字节解析] 起始索引" << startIndex << "非法（超出88字节范围）";
        return 0;
    }

    // 提取高低字节（大端序：高位在前，低位在后）
    uint8_t highByte = static_cast<uint8_t>(data.at(startIndex));
    uint8_t lowByte = static_cast<uint8_t>(data.at(startIndex + 1));

    // 组合为int16_t（有符号）
    int16_t result = (static_cast<int16_t>(highByte) << 8) | lowByte;

    // qDebug() << "[字节解析] 索引" << startIndex << "-" << startIndex+1
             // << "：高字节=0x" << QString("%1").arg(highByte, 2, 16, QChar('0')).toUpper()
             // << "，低字节=0x" << QString("%1").arg(lowByte, 2, 16, QChar('0')).toUpper()
             // << "，解析为int16_t=" << result;

    return result;
}

// ====================== 数据帧处理函数实现 ======================
QByteArray UdpCommunication::createDataFrame()
{
    QByteArray frame(40, 0x00);
    frame[0] = 'P';
    frame[39] = '#';
    return frame;
}

QByteArray UdpCommunication::modifyFrameByte(const QByteArray &frame, int byteIndex, int decimalValue)
{
    if (!isValidDataFrame(frame)) {
        emit communicationError("字节修改失败：输入数据帧非法（不是40字节/帧头帧尾错误）");
        return frame;
    }

    if (byteIndex < 1 || byteIndex > 36) {
        emit communicationError(QString("字节修改失败：字节索引%1非法（允许范围1-36）").arg(byteIndex));
        return frame;
    }

    if (decimalValue < 0 || decimalValue > 255) {
        emit communicationError(QString("字节修改失败：十进制数值%1非法（允许范围0-255）").arg(decimalValue));
        return frame;
    }

    QByteArray modifiedFrame = frame;
    quint8 hexValue = static_cast<quint8>(decimalValue);
    modifiedFrame[byteIndex] = static_cast<char>(hexValue);

    return modifiedFrame;
}

QByteArray UdpCommunication::modifyFrameBit(const QByteArray &frame, int byteIndex, int bitIndex, bool bitValue)
{
    if (!isValidDataFrame(frame)) {
        emit communicationError("位修改失败：输入数据帧非法（不是40字节/帧头帧尾错误）");
        return frame;
    }

    if (byteIndex < 1 || byteIndex > 36) {
        emit communicationError(QString("位修改失败：字节索引%1非法（允许范围1-36）").arg(byteIndex));
        return frame;
    }

    if (bitIndex < 0 || bitIndex > 7) {
        emit communicationError(QString("位修改失败：位索引%1非法（允许范围0-7）").arg(bitIndex));
        return frame;
    }

    QByteArray modifiedFrame = frame;
    quint8 originalByte = static_cast<quint8>(modifiedFrame.at(byteIndex));
    if (bitValue) {
        originalByte |= (1 << bitIndex);
    } else {
        originalByte &= ~(1 << bitIndex);
    }
    modifiedFrame[byteIndex] = static_cast<char>(originalByte);


    return modifiedFrame;
}

QByteArray UdpCommunication::addChecksum(const QByteArray &frame)
{
    if (!isValidDataFrame(frame)) {
        emit communicationError("添加校验位失败：输入数据帧非法（不是40字节/帧头帧尾错误）");
        return frame;
    }

    uint16_t checksum = 0;
    // 校验和计算逻辑不变（累加1-36字节的所有数据）
    for (int i = 1; i <= 36; ++i) {
        checksum += static_cast<uint8_t>(frame.at(i));
    }

    QByteArray frameWithChecksum = frame;
    // 16位校验和 = 高8位 + 低8位
    frameWithChecksum[37] = static_cast<char>((checksum >> 8) & 0xFF);  // 高字节（H字节）
    frameWithChecksum[38] = static_cast<char>(checksum & 0xFF);         // 低字节（L字节）

    return frameWithChecksum;
}

// ====================== 控制函数实现 ======================
void UdpCommunication::servoPowerOn()
{
    QByteArray powerOnFrame = createDataFrame();
    powerOnFrame = modifyFrameBit(powerOnFrame, 31, 4, true);
    powerOnFrame = addChecksum(powerOnFrame);
    sendDataAsync(powerOnFrame);
}

void UdpCommunication::servoPowerOff()
{
    QByteArray powerOffFrame = createDataFrame();
    powerOffFrame = modifyFrameBit(powerOffFrame, 31, 5, true);
    powerOffFrame = addChecksum(powerOffFrame);
    sendDataAsync(powerOffFrame);

}

void UdpCommunication::posePointMotion(float x, float y, float z, float rx, float ry, float rz, float linearSpeed, float angularSpeed)
{
    QByteArray poseFrame = createDataFrame();
    int16ToTwoBytes(poseFrame, 1,  floatToInt16(x, COORD_FACTOR));
    int16ToTwoBytes(poseFrame, 3,  floatToInt16(y, COORD_FACTOR));
    int16ToTwoBytes(poseFrame, 5,  floatToInt16(z, COORD_FACTOR));
    int16ToTwoBytes(poseFrame, 7,  floatToInt16(linearSpeed, 10.0));
    int16ToTwoBytes(poseFrame, 9,  floatToInt16(angularSpeed, 100.0));
    int16ToTwoBytes(poseFrame, 11, floatToInt16(ry, COORD_FACTOR));
    int16ToTwoBytes(poseFrame, 13, floatToInt16(rx, COORD_FACTOR));
    int16ToTwoBytes(poseFrame, 15, floatToInt16(rz, COORD_FACTOR));

    poseFrame[36] = 0xFF;

    poseFrame = addChecksum(poseFrame);
    sendDataAsync(poseFrame);
}

// 运动停止：第34字节的第2位置为1
void UdpCommunication::stopMotion() {
    QByteArray stopFrame = createDataFrame();
    // 修改第34字节（0-based索引34）的第2位（bit2）为1
    stopFrame = modifyFrameBit(stopFrame, 33, 2, true);
    stopFrame = addChecksum(stopFrame);
    sendDataAsync(stopFrame);
    // sendOk = sendData(stopFrame);
    // qDebug() << "[运动控制] 运动停止帧1发送" << (sendOk ? "成功" : "失败") << "！";


}

// 调平：第34字节的第3位置为1
void UdpCommunication::startLeveling() {
    QJsonObject m=getCurrentCoordinates();
    float current_rx = static_cast<float>(m["gyroPitch"].toDouble());
    float current_ry = static_cast<float>(m["gyroRoll"].toDouble());

    float x = static_cast<float>(m["x"].toDouble());
    float y = static_cast<float>(m["y"].toDouble());
    float z = static_cast<float>(m["z"].toDouble());
    float rx = static_cast<float>(m["rx"].toDouble());
    float ry = static_cast<float>(m["ry"].toDouble());
    float rz = static_cast<float>(m["rz"].toDouble());
    posePointMotion(x,y,z,rx+target_rx - current_rx,ry-current_ry + target_ry,rz,2,1);
}

void UdpCommunication::homing()
{
    QByteArray homingFrame = createDataFrame();
    // 修改第31字节的第0位为1（回零命令）
    homingFrame = modifyFrameBit(homingFrame, 31, 0, true);
    homingFrame = addChecksum(homingFrame);
    sendDataAsync(homingFrame);
}

void UdpCommunication::initialize()
{
    QByteArray initFrame = createDataFrame();
    // 修改第31字节的第1位为1（初始化命令）
    initFrame = modifyFrameBit(initFrame, 31, 1, true);
    initFrame = addChecksum(initFrame);
    sendDataAsync(initFrame);
}

// 系统复位：第31字节的第2位和第3位同时置为1
void UdpCommunication::resetSystem() {
    QByteArray resetFrame = createDataFrame();
    // 先将第31字节的第2位置为1
    resetFrame = modifyFrameBit(resetFrame, 31, 2, true);
    // 再将第31字节的第3位置为1
    resetFrame = modifyFrameBit(resetFrame, 31, 3, true);
    // 添加校验和并发送
    resetFrame = addChecksum(resetFrame);
    sendDataAsync(resetFrame);

}
// 新增：获取平台运动状态的公共函数
bool UdpCommunication::isPlatformMoving() const
{
    QMutexLocker locker(&m_movingMutex);
    return m_isPlatformMoving;
}

void UdpCommunication::sendDataAsync(const QByteArray &data)
{
    QMetaObject::invokeMethod(this, [this, data]() {
        sendData(data);
    }, Qt::QueuedConnection);
}
