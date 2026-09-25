#ifndef WORKER_H
#define WORKER_H
#include <QCoreApplication>
#include <QThread>
#include <QTimer>
#include <QDebug>
#include <functional>
#include <vector>
#include <memory>

// 事件处理器接口
class EventHandler : public QObject {
    Q_OBJECT
public:
    virtual void handleEvent() = 0;
    virtual ~EventHandler() = default;
};


class Worker : public QObject
{
    Q_OBJECT
public:
    explicit Worker(QObject* parent = nullptr,int ms=1000);
    // 线程安全的处理器注册
    void registerHandler(EventHandler* handler);
    ~Worker();
signals:
    void eventTriggered(const QString& info);
    void finished();
private slots:
    void init();
    void processEvents();
    void cleanup();
private:
    QThread* m_thread;
    QTimer* m_timer;
    std::vector<EventHandler*> m_handlers;
    int m_interval;
};

#endif // WORKER_H
