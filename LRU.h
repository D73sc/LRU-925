#ifndef LRU_H
#define LRU_H

#include <QObject>
#include <QVector3D>
#include <QQuaternion>
#include <QMatrix4x4>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QTimer>
#include <QOpenGLFunctions_2_1>
#include <GL/glu.h>

class LRU : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit LRU(QWidget *parent = nullptr);
    ~LRU();

    // 加载STL模型
    bool loadSTLModel(const QString &fileName);

    //坐标绑定
    void updateModelPosition(float motorXPosition,float motorYPosition,float motorZPosition,float rotationAngleY);

signals:


private slots:

protected:
    void paintGL() override;
    void initializeGL() override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    // 从STL文件读取顶点数据
    bool readSTLFile(const QString &fileName, QVector<float> &vertices, QVector<float> &normals);

    QOpenGLShaderProgram *m_shaderProgram;
    QOpenGLBuffer m_vertexBuffer;
    QOpenGLBuffer m_normalBuffer;
    QOpenGLVertexArrayObject m_vao;
    QVector<float> m_vertices;
    QVector<float> m_normals;

    // 坐标映射参数
    QVector3D m_mappingTranslation;
    QQuaternion m_mappingRotation;
    QVector3D m_mappingScale;

    // 当前LRU坐标
    QVector3D m_currentPosition;
    QQuaternion m_currentOrientation;

    // 数据加载标志位
    bool m_dataLoaded;

    //模型范围
    float min_x;
    float max_x;
    float min_y;
    float max_y;
    float min_z;
    float max_z;

    // 鼠标相关变量
    QPoint m_lastMousePos;
    bool m_leftButtonPressed;
    bool m_middleButtonPressed;

    //坐标绑定
    float m_modelXOffset = 0.0f; // 模型在X方向的偏移量
    float m_modelYOffset = 0.0f; // 模型在Y方向的偏移量
    float m_modelZOffset = 0.0f; // 模型在Z方向的偏移量


    // 开始/停止实时更新
    void startRealTimeUpdate(bool start = true);

    // 初始化OpenGL资源
    void initializeGLResources();

    // 清理OpenGL资源）
    void cleanupGLResources();

    //更新缓冲区
    void updateBuffers();

    // 渲染函数
    void render();

    //绘制网格和轴
    void drawGridAndAxes(QPainter& painter);

    //天花板顶点数据
    QVector<float> m_ceilingVertices;
    QVector<float> m_ceilingNormals;
    bool m_ceilingInitialized = false;
    QOpenGLBuffer m_ceilingVertexBuffer;
    QOpenGLBuffer m_ceilingNormalBuffer;
    QOpenGLVertexArrayObject m_ceilingVao;

    // 初始化天花板顶点数据
    void initializeCeiling();

    // 绘制天花板
    void renderCeiling();

};

#endif // LRU_H
