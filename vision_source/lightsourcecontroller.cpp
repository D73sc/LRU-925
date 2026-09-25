#include "LightSourceController.h"
#include <QByteArray>
#include <QThread>
#include <QEventLoop>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QtEndian>

LightSourceController::LightSourceController(QObject *parent) : QObject(parent)
    , m_serial(nullptr)
    , m_responseTimer(nullptr)
    , m_readTimer(nullptr)
    , m_waitingForResponse(false)
{
    m_serial = new QSerialPort(this);

    // 初始化串口参数（匹配通信协议：9600波特率、8数据位、1停止位、无校验）
    m_serial->setBaudRate(QSerialPort::Baud9600);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    // 初始化定时器
    m_responseTimer = new QTimer(this);
    m_responseTimer->setSingleShot(true);
    m_responseTimer->setInterval(500); // 500ms响应超时

    m_readTimer = new QTimer(this);
    m_readTimer->setSingleShot(true);
    m_readTimer->setInterval(50); // 50ms读取间隔超时

    // 连接信号槽
    connect(m_serial, &QSerialPort::readyRead, this, &LightSourceController::onSerialDataReady);
    connect(m_responseTimer, &QTimer::timeout, this, &LightSourceController::onResponseTimeout);
    connect(m_readTimer, &QTimer::timeout, this, &LightSourceController::onReadTimeout);
    connect(this, &LightSourceController::commandFinished, this, &LightSourceController::commandExecuted);
}

LightSourceController::~LightSourceController()
{
    closeSerial();

    if (m_responseTimer) {
        m_responseTimer->stop();
        delete m_responseTimer;
    }

    if (m_readTimer) {
        m_readTimer->stop();
        delete m_readTimer;
    }

    if (m_serial) {
        delete m_serial;
    }
}

bool LightSourceController::openSerial()
{
    if (m_serial->isOpen()) {
        m_serial->close();
    }

    // 尝试常见串口
    QStringList portNames = {"COM4"};
    bool openSuccess = false;

    for (const QString &portName : portNames) {
        m_serial->setPortName(portName);
        if (m_serial->open(QIODevice::ReadWrite)) {
            openSuccess = true;
            break;
        }
    }

    if (!openSuccess) {
        emit lightsourceUpdateUI("光源控制器连接失败：无法打开任何串口");
        qDebug() << "光源控制器连接失败：无法打开任何串口";
        emit serialConnectionChanged(false);
        return false;
    }

    // 清空缓冲区
    m_serial->clear();
    m_serialBuffer.clear();
    m_commandQueue.clear();
    m_waitingForResponse = false;

    emit lightsourceUpdateUI("光源控制器连接成功");
    qDebug() << "光源控制器连接成功，端口：" << m_serial->portName();
    emit serialConnectionChanged(true);

    // 读取初始亮度
    QTimer::singleShot(100, this, [this]() {
        int lightvalue1 = readBrightness(Channel1);
        int lightvalue2 = readBrightness(Channel2);

        if (lightvalue1 == -1 || lightvalue2 == -1) {
            emit lightsourceUpdateUI("光源控制器连接成功，但通信测试失败");
            qDebug() << "光源控制器连接成功，但通信测试失败";
        } else {
            emit lightBrightness(lightvalue1, lightvalue2);
        }
    });

    return true;
}

void LightSourceController::closeSerial()
{
    if (m_serial && m_serial->isOpen()) {
        m_serial->close();
        emit lightsourceUpdateUI("光源控制器已断开");
        qDebug() << "光源控制器已断开";
        emit serialConnectionChanged(false);
    }
}

bool LightSourceController::isSerialOpen() const
{
    return m_serial && m_serial->isOpen();
}

bool LightSourceController::openChannel(Channel channel)
{
    if (!isSerialOpen()) {
        emit lightsourceUpdateUI("请先连接光源控制器");
        return false;
    }

    enqueueCommand(1, channel, 0x00);
    return true; // 异步执行，返回true表示命令已加入队列
}

bool LightSourceController::closeChannel(Channel channel)
{
    if (!isSerialOpen()) {
        emit lightsourceUpdateUI("请先连接光源控制器");
        return false;
    }

    enqueueCommand(2, channel, 0x00);
    return true; // 异步执行，返回true表示命令已加入队列
}

bool LightSourceController::setBrightness(Channel channel, int brightness)
{
    if (!isSerialOpen()) {
        emit lightsourceUpdateUI("请先连接光源控制器");
        return false;
    }

    // 亮度范围校验（0-255，对应协议数据00-FF）
    if (brightness < 0 || brightness > 255) {
        emit lightsourceUpdateUI(QString("亮度值%1超出范围(0-255)").arg(brightness));
        return false;
    }

    enqueueCommand(3, channel, brightness);
    return true; // 异步执行，返回true表示命令已加入队列
}

int LightSourceController::readBrightness(Channel channel)
{
    if (!isSerialOpen()) {
        emit lightsourceUpdateUI("请先连接光源控制器");
        return -1;
    }

    // 使用事件循环等待异步读取结果
    QEventLoop loop;
    int result = -1;

    auto connection = connect(this, &LightSourceController::commandExecuted,
                              [&loop, &result, channel](bool success, int brightness) {
                                  if (success) {
                                      result = brightness;
                                  }
                                  loop.quit();
                              });

    // 设置超时
    QTimer::singleShot(1000, &loop, &QEventLoop::quit);

    enqueueCommand(4, channel, 0x00, true);
    loop.exec();

    disconnect(connection);

    if (result == -1) {
        emit lightsourceUpdateUI(QString("读取通道%1亮度失败").arg(channel));
    }

    return result;
}

void LightSourceController::enqueueCommand(char cmd, Channel channel, int data, bool isReading)
{
    QMutexLocker locker(&m_commandMutex);

    CommandRequest request;
    request.cmd = cmd;
    request.channel = channel;
    request.data = data;
    request.timestamp = QDateTime::currentMSecsSinceEpoch();
    request.isReading = isReading;

    m_commandQueue.enqueue(request);

    // 如果没有正在处理的命令，立即开始处理
    if (!m_waitingForResponse) {
        QTimer::singleShot(0, this, &LightSourceController::processCommandQueue);
    }
}

void LightSourceController::processCommandQueue()
{
    QMutexLocker locker(&m_commandMutex);

    if (m_waitingForResponse || m_commandQueue.isEmpty()) {
        return;
    }

    m_currentCommand = m_commandQueue.dequeue();
    sendCommandToSerial(m_currentCommand);

    // 启动响应超时定时器
    m_responseTimer->start();
    m_waitingForResponse = true;
}

void LightSourceController::sendCommandToSerial(const CommandRequest &request)
{
    if (!m_serial || !m_serial->isOpen()) {
        emit lightsourceUpdateUI("串口未打开");
        return;
    }

    // 1. 组装帧数据（特征字+命令字+通道字+数据）
    QByteArray frame;
    frame.append('#'); // 特征字#（协议定义）

    // 命令字（1-4，转换为ASCII字符）
    frame.append(QChar('0' + request.cmd));

    // 通道字（1-4，转换为ASCII字符）
    frame.append(QChar('0' + request.channel));

    // 数据（转换为0XX格式十六进制，如十进制100→"064"）
    QString dataStr = QString("0%1").arg(request.data, 2, 16, QChar('0')).toUpper();
    frame.append(dataStr.toUtf8());

    // 2. 计算并添加异或校验字
    QByteArray checksum = calculateChecksum(frame);
    frame.append(checksum);

    // 3. 发送命令
    m_serial->clear(); // 清空缓冲区
    m_serialBuffer.clear();

    QString cmdStr = QString::fromUtf8(frame);
    // emit lightsourceUpdateUI(QString("发送命令[通道%1]: %2").arg(request.channel).arg(cmdStr));
    // qDebug() << "发送命令:" << cmdStr;

    qint64 written = m_serial->write(frame);
    if (written != frame.size()) {
        emit lightsourceUpdateUI("命令发送失败");
        qDebug() << "命令发送失败，预期:" << frame.size() << "实际:" << written;

        QMutexLocker locker(&m_commandMutex);
        m_waitingForResponse = false;
        m_responseTimer->stop();
        processCommandQueue(); // 继续处理下一个命令
    } else {
        m_serial->flush();
    }
}

void LightSourceController::onSerialDataReady()
{
    if (!m_serial || !m_serial->isOpen() || !m_waitingForResponse) {
        // 清空缓冲区中的无用数据
        m_serial->readAll();
        return;
    }

    // 读取所有可用数据
    QByteArray newData = m_serial->readAll();
    m_serialBuffer.append(newData);

    // qDebug() << "收到数据:" << newData.toHex();

    // 启动读取超时定时器（等待完整数据包）
    m_readTimer->start();

    // 尝试处理响应
    if (m_serialBuffer.size() > 0) {
        // 检查是否有完整响应
        if (m_currentCommand.cmd == 4) {
            // 读取命令：成功响应为8字节，失败为1字节'&'
            if (m_serialBuffer.size() >= 8) {
                processResponse(m_serialBuffer);
                m_readTimer->stop();
            } else if (m_serialBuffer.size() >= 1 && m_serialBuffer[0] == '&') {
                processResponse(m_serialBuffer.left(1));
                m_readTimer->stop();
            }
        } else {
            // 其他命令：成功响应为1字节'#'，失败为1字节'&'
            if (m_serialBuffer.size() >= 1) {
                processResponse(m_serialBuffer.left(1));
                m_readTimer->stop();
            }
        }
    }
}

void LightSourceController::onReadTimeout()
{
    if (m_waitingForResponse && !m_serialBuffer.isEmpty()) {
        // 读取超时，尝试处理当前缓冲区中的数据
        processResponse(m_serialBuffer);
    }
}

void LightSourceController::processResponse(const QByteArray &response)
{
    if (!m_waitingForResponse) {
        return;
    }

    m_responseTimer->stop();
    m_readTimer->stop();

    QString respStr = QString::fromUtf8(response);
    // emit lightsourceUpdateUI(QString("收到响应: %1").arg(respStr));
    // qDebug() << "处理响应:" << respStr;

    bool success = false;
    int result = -1;

    if (response.isEmpty()) {
        emit lightsourceUpdateUI("响应超时");
    } else if (response[0] == '#') {
        // 成功响应
        if (m_currentCommand.cmd == 4) {
            // 读取命令，需要解析完整响应
            result = parseReadResponse(response, m_currentCommand.channel);
            success = (result != -1);
        } else {
            // 其他命令，只需要确认成功
            success = true;
            emit lightsourceUpdateUI(QString("光源%1操作成功").arg(m_currentCommand.channel));
        }
    } else if (response[0] == '&') {
        // 失败响应
        emit lightsourceUpdateUI(QString("光源%1操作失败").arg(m_currentCommand.channel));
        // qDebug() << "操作失败响应:" << respStr;
    } else {
        // 无效响应
        emit lightsourceUpdateUI("无效响应格式");
        // qDebug() << "无效响应:" << respStr;
    }

    // 发送命令完成信号
    emit commandFinished(success, result);

    // 重置状态，处理下一个命令
    {
        QMutexLocker locker(&m_commandMutex);
        m_waitingForResponse = false;
        m_serialBuffer.clear();
    }

    // 处理队列中的下一个命令
    QTimer::singleShot(50, this, &LightSourceController::processCommandQueue);
}

void LightSourceController::onResponseTimeout()
{
    if (m_waitingForResponse) {
        emit lightsourceUpdateUI("响应超时");
        qDebug() << "响应超时，当前命令: cmd=" << m_currentCommand.cmd
                 << "channel=" << m_currentCommand.channel;

        // 发送超时信号
        emit commandFinished(false, -1);

        // 重置状态
        {
            QMutexLocker locker(&m_commandMutex);
            m_waitingForResponse = false;
            m_serialBuffer.clear();
        }

        // 处理下一个命令
        QTimer::singleShot(50, this, &LightSourceController::processCommandQueue);
    }
}

bool LightSourceController::validateResponse(const QByteArray &response, char expectedCmd, Channel expectedChannel)
{
    if (response.size() < 1) {
        return false;
    }

    if (expectedCmd == 4) {
        // 读取命令的响应应该是8字节
        if (response.size() < 8 || response[0] != '#') {
            return false;
        }

        // 验证命令字（应为'4'表示读取成功）
        if (response[1] != '4') {
            return false;
        }

        // 验证通道字
        int responseChannel = response[2] - '0';
        if (responseChannel != expectedChannel) {
            return false;
        }

        return true;
    } else {
        // 其他命令的响应是1字节'#'
        return (response.size() >= 1 && response[0] == '#');
    }
}

int LightSourceController::parseReadResponse(const QByteArray &response, Channel expectedChannel)
{
    if (!validateResponse(response, 4, expectedChannel)) {
        return -1;
    }

    // 提取亮度数据（第3-5字节）
    QByteArray dataBytes = response.mid(3, 3);

    // 转换为十进制
    bool ok;
    int brightness = dataBytes.toInt(&ok, 16);

    if (!ok) {
        qDebug() << "亮度数据转换失败，原始数据:" << dataBytes;
        return -1;
    }
    return brightness;
}

QByteArray LightSourceController::calculateChecksum(const QByteArray &baseFrame)
{
    // 计算异或和（除校验字外所有字节）
    unsigned char xorSum = 0;
    for (char c : baseFrame) {
        xorSum ^= static_cast<unsigned char>(c);
    }

    // 转换为ASCII（高半字节在前，低半字节在后，如0x13→"13"）
    QString checksumStr = QString("%1").arg(xorSum, 2, 16, QChar('0')).toUpper();
    return checksumStr.toUtf8();
}
