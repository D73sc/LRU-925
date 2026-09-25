#include "uvc_camera.h"
#include <QDebug>
#include <QTimer>

UVC_Camera::UVC_Camera(QObject *parent)
    : QObject(parent)
    , m_camera(nullptr)
    , m_capture(nullptr)
    , m_isOpened(false)
{
}

UVC_Camera::~UVC_Camera()
{
    // 析构时自动关闭相机，释放资源
    closeCamera();
}

// 枚举所有UVC相机设备
QList<QCameraInfo> UVC_Camera::enumUvcCameras()
{
    return QCameraInfo::availableCameras();
}

// 打开指定索引的UVC相机
bool UVC_Camera::openCamera(int cameraIndex)
{
    if (m_isOpened) {
        emit UVCInfo("相机已打开，请勿重复操作");
        return false;
    }
    if (m_camera || m_capture) {
        closeCamera();
    }

    // 获取相机列表
    QList<QCameraInfo> cameras = enumUvcCameras();
    if (cameras.isEmpty()) {
        emit UVCInfo("未检索到UVC相机");
        return false;
    }

    emit UVCInfo(QString("检索到UVC相机%1个").arg(cameras.size()));
    if (cameraIndex < 0 || cameraIndex >= cameras.size()) {
        emit UVCInfo(QString("相机索引%1无效").arg(cameraIndex));
        return false;
    }

    // 创建相机实例
    m_camera = new QCamera(cameras[cameraIndex]);
    connect(m_camera, QOverload<QCamera::Error>::of(&QCamera::error),
            this, &UVC_Camera::onCameraError);

    // 降低分辨率，减少USB带宽占用
    QCameraViewfinderSettings settings;
    settings.setResolution(640, 480);
    settings.setMinimumFrameRate(30);
    m_camera->setViewfinderSettings(settings);

    // 创建图像捕获器
    m_capture = new QCameraImageCapture(m_camera);
    m_capture->setCaptureDestination(QCameraImageCapture::CaptureToBuffer); // 捕获到内存
    connect(m_capture, &QCameraImageCapture::imageCaptured, this, &UVC_Camera::onImageCaptured);

    // 加载相机
    m_camera->load();
    if (m_camera->error() != QCamera::NoError || m_camera->status() == QCamera::UnavailableStatus) {
        QString errorText = m_camera->errorString().isEmpty()
        ? "相机不可用，可能被占用或读取失败"
        : m_camera->errorString();
        emit UVCInfo(QString("UVC相机加载失败：%1").arg(errorText));
        closeCamera();
        return false;
    }

    m_isOpened = true;
    QString Info = QString("UVC相机加载成功：%1" ).arg(cameras[cameraIndex].description());
    emit UVCInfo(Info);
    return true;
}

// 关闭相机
void UVC_Camera::closeCamera()
{
    if (!m_camera && !m_capture) {
        m_isOpened = false;
        return;
    }

    if (m_camera) {
        m_camera->stop();
        m_camera->unload();
    }
    delete m_capture;
    delete m_camera;
    m_capture = nullptr;
    m_camera = nullptr;
    m_isOpened = false;

    emit UVCInfo("UVC相机已关闭");
}

// 开始预览
bool UVC_Camera::startPreview(QCameraViewfinder *viewfinder)
{
    if (!m_isOpened || !m_camera) {
        emit UVCInfo("请先打开相机！");
        return false;
    }
    if (!viewfinder) {
        emit UVCInfo("预览窗口为空，无法启动监测");
        return false;
    }

    m_camera->stop();
    // 设置预览控件
    m_camera->setViewfinder(viewfinder);
    // 启动预览
    m_camera->start();
    if (m_camera->error() != QCamera::NoError || m_camera->status() == QCamera::UnavailableStatus) {
        QString errorText = m_camera->errorString().isEmpty()
        ? "相机不可用，可能被占用或读取失败"
        : m_camera->errorString();
        emit UVCInfo(QString("相机开始监测失败：%1").arg(errorText));
        closeCamera();
        return false;
    }

    emit UVCInfo("相机开始监测！");
    return true;
}

// 停止预览
void UVC_Camera::stopPreview()
{
    if (m_camera) m_camera->stop();
    emit UVCInfo("相机停止监测！");
}

// 捕获单帧
void UVC_Camera::captureFrame()
{
    if (!m_isOpened || !m_capture || !m_capture->isReadyForCapture()) {
        emit UVCInfo("相机未准备好，无法捕获帧");
        return;
    }
    // 触发帧捕获
    m_capture->capture();
}


// 帧捕获完成回调
void UVC_Camera::onImageCaptured(int id, const QImage &frame)
{
    Q_UNUSED(id);
    // 发送信号，将帧数据传递给外部
    emit frameCaptured(frame);
}

void UVC_Camera::onCameraError(QCamera::Error error)
{
    if (error == QCamera::NoError) {
        return;
    }

    QString errorText = m_camera && !m_camera->errorString().isEmpty()
                            ? m_camera->errorString()
                            : "相机读取失败，可能被占用或已断开";
    emit UVCInfo(QString("UVC相机错误：%1").arg(errorText));
    QTimer::singleShot(0, this, &UVC_Camera::closeCamera);
}
