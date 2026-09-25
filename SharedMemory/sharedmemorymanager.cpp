#include "sharedmemorymanager.h"

SharedMemoryManager::SharedMemoryManager(const QString& key)
    : m_key(key)
{
    m_sharedMemory.setKey(key);
}

SharedMemoryManager::~SharedMemoryManager()
{
    release();
}

bool SharedMemoryManager::writeImage(const QImage& image)
{
    // 若已挂载，先分离
    if (m_sharedMemory.isAttached()) {
        if (!m_sharedMemory.detach()) return false;
    }

    // 图像转换为字节数组
    QBuffer buffer;
    buffer.open(QBuffer::WriteOnly);
    image.save(&buffer, "PNG");
    QByteArray byteArray = buffer.data();

    // 创建共享内存段
    if (!m_sharedMemory.create(byteArray.size())) return false;

    // 加锁写入数据
    m_sharedMemory.lock();
    char* to = static_cast<char*>(m_sharedMemory.data());
    const char* from = byteArray.constData();
    memcpy(to, from, byteArray.size());
    m_sharedMemory.unlock();

    return true;
}

QImage SharedMemoryManager::readImage()
{
    if (!m_sharedMemory.isAttached()) {
        if (!m_sharedMemory.attach()) return QImage();
    }

    // 加锁读取数据
    m_sharedMemory.lock();
    QByteArray byteArray(static_cast<char*>(m_sharedMemory.data()), m_sharedMemory.size());
    m_sharedMemory.unlock();

    // 反转化为图像
    QImage image;
    image.loadFromData(byteArray, "PNG");
    return image;
}

void SharedMemoryManager::release()
{
    if (m_sharedMemory.isAttached()) {
        m_sharedMemory.detach();
    }
}
