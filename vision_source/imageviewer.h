#ifndef IMAGEVIEWER_H
#define IMAGEVIEWER_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QPixmap>
#include <QMouseEvent>
#include <QWheelEvent>

class ImageViewer : public QGraphicsView
{
    Q_OBJECT
public:
    explicit ImageViewer(QWidget *parent = nullptr);
    void resetImage();
public slots:
    void setQImage(QImage image);
    void setPixmap(QPixmap pixmap);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    bool m_isPan;
    QPoint m_prevPan;
    QGraphicsScene *scene;
    QPixmap m_image;
    // 新增状态保存变量
    bool m_isFirstFrame;       // 是否是第一帧
    QTransform m_lastTransform;// 上一帧的变换矩阵
    int m_lastHScroll;         // 水平滚动条位置
    int m_lastVScroll;         // 垂直滚动条位置

    void initFirstShow();      // 首次显示初始化
    void updateFrameShow();    // 后续帧更新
    void zoom(QPoint factor);
    void pan(const QPoint &panTo);
    void togglePan(bool pan, const QPoint &startPos = QPoint());
};

#endif // IMAGEVIEWER_H
