#ifndef SHARED_MEMORY_MANAGER_H
#define SHARED_MEMORY_MANAGER_H

#include <QSharedMemory>
#include <QImage>
#include <QBuffer>
#include <QDataStream>

class SharedMemoryManager
{
public:
    explicit SharedMemoryManager(const QString& key);
    ~SharedMemoryManager();

    // 写入图像到共享内存
    bool writeImage(const QImage& image);
    // 从共享内存读取图像
    QImage readImage();
    // 释放共享内存
    void release();

private:
    QSharedMemory m_sharedMemory;
    QString m_key;
};

#endif // SHARED_MEMORY_MANAGER_H
