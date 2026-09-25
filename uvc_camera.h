#ifndef UVC_CAMERA_H
#define UVC_CAMERA_H

// Qt 多媒体头文件
#include <QObject>
#include <QCamera>
#include <QCameraInfo>
#include <QCameraImageCapture>
#include <QImage>
#include <QList>
#include <QString>
#include <QCameraViewfinder>

class UVC_Camera : public QObject
{
    Q_OBJECT
public:
    explicit UVC_Camera(QObject *parent = nullptr);
    ~UVC_Camera() override;

    // 枚举所有可用的UVC相机
    QList<QCameraInfo> enumUvcCameras();

    // 打开指定索引的UVC相机
    bool openCamera(int cameraIndex = 0);

    // 关闭相机
    void closeCamera();

    // 开始相机预览
    bool startPreview(QCameraViewfinder *viewfinder);

    // 停止预览
    void stopPreview();

    // 捕获一帧图像
    void captureFrame();

    // 获取相机当前状态
    bool isCameraOpened() const { return m_isOpened; }

signals:
    void frameCaptured(const QImage &frame);  // 帧捕获完成信号
    void UVCInfo(QString UVCInfo);

private slots:
    void onImageCaptured(int id, const QImage &frame); // 帧捕获回调
    void onCameraError(QCamera::Error error);

private:
    QCamera *m_camera;               // UVC相机实例
    QCameraImageCapture *m_capture;  // 图像捕获器
    bool m_isOpened;                 // 相机打开状态
};

#endif // UVC_CAMERA_H
