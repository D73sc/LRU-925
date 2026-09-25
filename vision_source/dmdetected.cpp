#include "dmdetected.h"

DMDetected::DMDetected(QObject *parent) : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_values(3, 0.0f)
    , m_dmCode("")
    , m_connectedFlag(false)
    , m_parseStatus(false)
    , m_connectTimer(new QTimer(this))    // 初始化超时定时器
{
    connect(m_socket, &QTcpSocket::connected,
            this, &DMDetected::onConnected);          // 连接成功
    connect(m_socket, &QTcpSocket::disconnected,
            this, &DMDetected::onDisconnected);       // 连接断开
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &DMDetected::onErrorOccurred);      // 错误发生（Qt5/6兼容写法）
    connect(m_socket, &QTcpSocket::readyRead,
            this, &DMDetected::onReadyRead);          // 接收数据

    m_connectTimer->setSingleShot(true);
    connect(m_connectTimer, &QTimer::timeout,
            this, &DMDetected::onConnectTimeout);     // 超时处理
    m_values.fill(0.0f);
    m_values[0] = 0;
    m_values[1] = 0;
    m_values[2] = 0;
}

DMDetected::~DMDetected()
{
    disconnectFromDevice();
}

void DMDetected::connectToDevice()
{
    QTcpSocket::SocketState currentState = m_socket->state();

    if (currentState == QTcpSocket::ConnectingState) {
        emit statusUpdated("正在连接中，请勿重复操作");
        return;
    }
    if (currentState == QTcpSocket::ConnectedState) {
        emit statusUpdated("已处于连接状态，无需重复连接");
        return;
    }

    const QString ip = "192.168.192.55";
    const quint16 port = 3000;
    emit statusUpdated(QString("发起连接到 %1:%2...").arg(ip).arg(port));

    m_socket->connectToHost(ip, port);
    m_connectTimer->start(2500); // 超时
}

void DMDetected::disconnectFromDevice()
{
    if (m_socket->state() == QTcpSocket::ConnectedState) {
        m_socket->disconnectFromHost();
    }
    m_connectTimer->stop();  // 断开时停止超时定时器
}

bool DMDetected::isConnected()
{
    QMutexLocker locker(&m_flagMutex);
    return m_connectedFlag;
}

QByteArray DMDetected::getCurrentData()
{
    QMutexLocker locker(&m_dataMutex);
    return m_rawData;
}

QVector<float> DMDetected::getValues()
{
    QMutexLocker locker(&m_dataMutex);
    return m_values;
}

void DMDetected::resetValues()
{
    QMutexLocker locker(&m_dataMutex);
    m_values[0] = 0;
    m_values[1] = 0;
    m_values[2] = 0;

}

QString DMDetected::getDmCode()
{
    QMutexLocker locker(&m_dataMutex);
    return m_dmCode;
}

bool DMDetected::getParseStatus()
{
    QMutexLocker locker(&m_dataMutex);
    return m_parseStatus;
}

void DMDetected::onConnected()
{
    m_connectTimer->stop();  // 连接成功，停止超时计时

    QMutexLocker locker(&m_flagMutex);
    m_connectedFlag = true;
    emit statusUpdated("已成功连接到相机");
}

void DMDetected::onDisconnected()
{
    QMutexLocker locker(&m_flagMutex);
    m_connectedFlag = false;
    emit statusUpdated("与相机的连接已断开");
}

void DMDetected::onErrorOccurred(QAbstractSocket::SocketError socketError)
{
    m_connectTimer->stop();
    qDebug() << "错误码：" << socketError
             << "，错误描述：" << m_socket->errorString();
    // 常见错误码含义：
    // 0: 连接被拒绝（相机未启动服务）
    // 1: 远程主机未找到（IP错误）
    // 2: 连接超时（网络不通或相机无响应）
    QMutexLocker locker(&m_flagMutex);
    m_connectedFlag = false;
    emit errorOccurred(m_socket->errorString());
}

void DMDetected::onReadyRead()
{
    QByteArray data = m_socket->readAll();
    QMutexLocker locker(&m_dataMutex);

    m_rawData = data;
    m_parseStatus = false;

    m_dmCode.clear();

    // 解析数据（格式：分号分隔的5个部分，前3为数值，第4为DM码）
    QString textData = QString::fromUtf8(data).trimmed();

    QString processedText = textData.trimmed();
    processedText.remove('(').remove(')');  // 移除可能的括号
    QStringList parts = processedText.split(';');

    if (parts.size() == 5) {
        bool ok1, ok2, ok3;
        float num1 = parts[0].toFloat(&ok1);
        float num2 = parts[1].toFloat(&ok2);
        float num3 = parts[2].toFloat(&ok3);
        QString dmCode = parts[3];

        if (ok1 && ok2 && ok3) {
            // 保留两位小数
            m_values[0] = std::round(num1 * 100) / 100;
            m_values[1] = std::round(num2 * 100) / 100;
            m_values[2] = std::round(num3 * 100) / 100;
            m_dmCode = dmCode;
            m_parseStatus = true;
        } else {
            qDebug() << "解析失败：前三个字段不是有效数字";
        }
    } else {
        qDebug() << "解析失败：数据格式错误（需5个部分，实际" << parts.size() << "个）";
    }
}

void DMDetected::onConnectTimeout()
{
    if (m_socket->state() == QTcpSocket::ConnectingState) {
        m_socket->abort();  // 终止连接
        emit errorOccurred("连接超时（相机无响应）");
    }
}
