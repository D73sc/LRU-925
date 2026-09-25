#ifndef SCStatusTcp_H
#define SCStatusTcp_H

#include <QTcpSocket>
#include <QObject>
#include <QElapsedTimer>

class SCStatusTcp : public QObject
{
    Q_OBJECT
public:
    explicit SCStatusTcp(QObject *parent = 0);

    ~SCStatusTcp();

    void releaseTcpSocket();
    QString lastError() const;

    void setLastError(const QString &lastError);

    bool writeTcpData(uint16_t sendCommand,
                      const QByteArray &jsonData,
                      const QByteArray &sendData,
                      uint16_t &number, uint8_t byte15);

    QTcpSocket *tcpSocket() const;

    QElapsedTimer time() const;

    int connectHost(const QString &ip, quint16 port);
    QString getCurrentDateTime() const;
    QString hexToQString(const QByteArray &b);
    int getTimeOut() const;
    void setTimeOut(int timeOut);

public slots:
    void receiveTcpReadyRead();
    void onStateChanged(QAbstractSocket::SocketState state);//优化后新增加的
    void onTcpError(QAbstractSocket::SocketError error);//优化后新增加的
signals:
    void sigPrintInfo(QString);
    void sigChangedText(bool ,int ,QByteArray ,QByteArray ,int ,int );
    void sigJsonData(QString);
    void sigTcpStateChanged(QAbstractSocket::SocketState state);//优化后新增加的
    void sigTcpError(QAbstractSocket::SocketError error);//优化后新增加的

    void sigTcpStateChangedOut(QAbstractSocket::SocketState state);
    void sigTcpErrorOut(QAbstractSocket::SocketError error);
private:

    QTcpSocket *_tcpSocket;
    QString _lastError;
    QString _ip; //ip
    quint16 _port; //端口号
    uint16_t _number;//序号

    QByteArray _lastMessage;//存放所有读取的数据
    QElapsedTimer _time;//用来监测时间

    int _oldSendCommand;
    int _oldNumber;
    int _timeOut;


};

#endif // SCStatusTcp_H
