#ifndef TCPSENSOR_H
#define TCPSENSOR_H

#include <QObject>
#include <QTcpSocket>
#include <QHostAddress>
#include <QByteArray>
#include <QVector>

class SixAxisSensor : public QObject
{
    Q_OBJECT
public:
    explicit SixAxisSensor(QObject *parent = nullptr);

    bool initialize(const QString &ip, quint16 port);
    void startContinuousMeasurement();
    void sensorZeroCalibration();
    void closeConnection();

signals:
    void dataReceived(const QVector<float>& forces);
    void errorOccurred(const QString &error);

private slots:
    void onReadyRead();
    void onErrorOccurred(QAbstractSocket::SocketError error);

private:
    QTcpSocket *m_socket;
    QByteArray m_buffer;
    QVector<float> parseSensorData(const QByteArray &data);
    quint32 m_expectedLength = 0;
};

#endif // TCPSENSOR_H

