#include "Worker.h"

Worker::Worker(QObject* parent,int ms) :QObject(parent),m_thread(new QThread(this)), m_timer(new QTimer(this))
{
    m_interval =ms;
    // 将 Worker 移动到子线程
    moveToThread(m_thread);

    // 初始化定时器（属于子线程）
    m_timer->moveToThread(m_thread);

    // 连接线程启动逻辑
    connect(m_thread, &QThread::started, this, &Worker::init);
    connect(m_thread, &QThread::finished, this, &Worker::cleanup);
    // 启动线程
    m_thread->start();
}

 Worker::~Worker()
{
    m_thread->quit();
    //m_thread->wait();
    // 等待线程完全退出
    if (!m_thread->wait(3000)) {
        qWarning() << "线程未正常退出，强制终止";
        m_thread->terminate();
    }
}

void Worker::registerHandler(EventHandler* handler)
{
    QMetaObject::invokeMethod(this, [this, handler]() {
            m_handlers.emplace_back(handler);
            qDebug() << "[Worker" << m_thread->currentThreadId()
                     << "] 注册处理器:" << handler->metaObject()->className();
        }, Qt::QueuedConnection);
}
void Worker::init()
{
    connect(m_timer, &QTimer::timeout, this, &Worker::processEvents);
    m_timer->start(m_interval); // 1秒触发一次
}

void Worker::processEvents()
{
    for (auto& handler : m_handlers)
    {
        handler->handleEvent();
    }
    emit eventTriggered(
        QString("在线程 %1 处理了 %2 个任务")
            .arg(reinterpret_cast<quintptr>(m_thread->currentThreadId()))
            .arg(m_handlers.size())
        );
}

void Worker::cleanup()
{
    // 在子线程中安全停止定时器
    if (m_timer && m_timer->isActive()) {
        m_timer->stop();
    }
    m_timer->deleteLater();  // 必须使用deleteLater
    m_timer = nullptr;

    emit finished();
}
