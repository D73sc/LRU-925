#include "LRU.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QtMath>
#include <QMouseEvent>
#include <QPainter>
#include<QPainterPath>

LRU::LRU(QWidget *parent) : QOpenGLWidget(parent)
    , m_shaderProgram(nullptr)
    , m_vertexBuffer(QOpenGLBuffer::VertexBuffer)
    , m_normalBuffer(QOpenGLBuffer::VertexBuffer)
    , m_currentPosition(0, 0, 0)
    , m_currentOrientation(1, 0, 0, 0)
    // 初始化映射参数
    , m_mappingTranslation(QVector3D(0, 0, 0))
    , m_mappingRotation(QQuaternion::fromAxisAndAngle(QVector3D(0, 1, 0), 0)) // 初始绕y轴旋转45°
    , m_mappingScale(QVector3D(1, 1, 1))
    , m_dataLoaded(false) // 初始数据加载标志位
    , m_leftButtonPressed(false)
    , m_middleButtonPressed(false)
{

}


LRU::~LRU()
{
    cleanupGLResources();
}



// 加载模型
bool LRU::loadSTLModel(const QString &fileName)
{
    QVector<float> vertices, normals;
    if (!readSTLFile(fileName, vertices, normals)) {
        qDebug() << "Failed to read STL file:" << fileName;
        return false;
    }

    m_vertices = vertices;
    m_normals = normals;
    m_dataLoaded = true; // 数据加载完成，更新标志位

    // qDebug() << "STL数据加载完成 - 顶点数：" << m_vertices.size() / 3
    //          << "法线数：" << m_normals.size() / 3; // 添加输出

    // 清理旧资源
    cleanupGLResources();

    if (!m_shaderProgram) {
        initializeGLResources(); // 重新初始化着色器和缓冲区
    } else {
        // 即使已初始化，也要更新缓冲区数据
        makeCurrent();
        updateBuffers();
        doneCurrent();
    }

    if (m_shaderProgram) {

        makeCurrent(); // 激活上下文

        m_vao.bind();

        // 创建顶点缓冲区
        if (!m_vertexBuffer.isCreated()) {
            m_vertexBuffer.create();
        }
        m_vertexBuffer.bind();
        if (!m_vertices.isEmpty()) {
            m_vertexBuffer.allocate(m_vertices.data(), m_vertices.size() * sizeof(float));
            // qDebug() << "Vertex buffer allocated. Size:" << m_vertices.size() * sizeof(float);
            // 提取X轴分量（索引0,3,6,...）
            std::vector<float> x;
            for (size_t i = 0; i < m_vertices.size(); i += 3) {
                x.push_back(m_vertices[i]);
            }
            // 提取Y轴分量（索引1,4,7,...）
            std::vector<float> y;
            for (size_t i = 1; i < m_vertices.size(); i += 3) {
                y.push_back(m_vertices[i]);
            }
            // 提取Z轴分量（索引2,5,8,...）
            std::vector<float> z;
            for (size_t i = 2; i < m_vertices.size(); i += 3) {
                z.push_back(m_vertices[i]);
            }
             min_x = *std::min_element(x.begin(), x.end());
             max_x = *std::max_element(x.begin(), x.end());
             min_y = *std::min_element(y.begin(), y.end());
             max_y = *std::max_element(y.begin(), y.end());
             min_z = *std::min_element(z.begin(), z.end());
             max_z = *std::max_element(z.begin(), z.end());
            // qDebug() << "模型X轴范围： [" << min_x << ", " << max_x << "]";
            // qDebug() << "模型y轴范围： [" << min_y << ", " << max_y << "]";
            // qDebug() << "模型z轴范围： [" << min_z << ", " << max_z << "]";

        } else {
            qDebug() << "Vertex buffer is empty.";
        }
        m_shaderProgram->enableAttributeArray("a_position");
        m_shaderProgram->setAttributeBuffer("a_position", GL_FLOAT, 0, 3, 0);
        m_vertexBuffer.release();

        // 创建法线缓冲区
        if (!m_normalBuffer.isCreated()) {
            m_normalBuffer.create();
        }
        m_normalBuffer.bind();
        if (!m_normals.isEmpty()) {
            m_normalBuffer.allocate(m_normals.data(), m_normals.size() * sizeof(float));
            // qDebug() << "Normal buffer allocated. Size:" << m_normals.size() * sizeof(float);
        } else {
            qDebug() << "Normal buffer is empty.";
        }
        m_shaderProgram->enableAttributeArray("a_normal");
        m_shaderProgram->setAttributeBuffer("a_normal", GL_FLOAT, 0, 3, 0);
        m_normalBuffer.release();

        m_vao.release();

        doneCurrent(); // 释放上下文
    }

    if (m_shaderProgram && m_vao.isCreated()) { // 确保资源已初始化
        update(); // 仅在资源有效时触发重绘
    }

    return true;
}


// 渲染函数
void LRU::render()
{
    // 检查OpenGL错误
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        qDebug() << "OpenGL error before rendering:" << err;
    }

    if (m_vertices.isEmpty() || m_normals.isEmpty()) {
        return;
    }

    if (!isValid()) {
        qWarning() << "OpenGL 上下文无效！";
        return;
    }

    makeCurrent();

    glViewport(0, 0, width(), height());
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // 背景颜色
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    if (!m_shaderProgram->bind()) {
        qDebug() << "Failed to bind shader program.";
        doneCurrent();
        return;
    }
    // qDebug() << "Shader program bound.";

    // 模型矩阵（缩小1000倍）
    //这里的缩放比例代表实际和屏幕显示的缩放关系
    QMatrix4x4 model;
    model.scale(0.001f * m_mappingScale);
    model.translate(m_mappingTranslation);
    model.rotate(m_mappingRotation);
    m_shaderProgram->setUniformValue("modelMatrix", model);
    // qDebug() << "Model matrix set.";

    // 计算模型中心
    float centerX = (min_x + max_x) * 0.001f / 2;
    float centerY = (min_y + max_y) * 0.001f / 2;
    float centerZ = (min_z + max_z) * 0.001f / 2;

    // 视图矩阵
    QMatrix4x4 view;
    view.lookAt(
        QVector3D(centerX, centerY, centerZ + 8.0f), // 相机在空间的位置坐标，一般在模型中心点正上方
        QVector3D(centerX, centerY, centerZ), // 相机观察点坐标，一般是模型中心点
        QVector3D(0, 1, 0)
        );
    m_shaderProgram->setUniformValue("viewMatrix", view);

    // 投影矩阵
    QMatrix4x4 projection;
    float aspect = static_cast<float>(width()) / height();
    float farPlane = 20.0f;
    projection.perspective(45.0f, aspect, 0.1f, farPlane);
    m_shaderProgram->setUniformValue("projectionMatrix", projection);

    // 设置光源位置
    m_shaderProgram->setUniformValue("lightPosition", QVector3D(10, 10, 10));

    // 绑定VAO
    m_vao.bind();

    // 绘制模型
    glDrawArrays(GL_TRIANGLES, 0, m_vertices.size() / 3);

    //绘制天花板
    renderCeiling();

    // 解绑
    m_vao.release();
    m_shaderProgram->release();

    doneCurrent(); // 释放上下文
}



// 初始化资源
void LRU::initializeGLResources()
{
    makeCurrent(); // 激活上下文

    // 创建着色器程序
    m_shaderProgram = new QOpenGLShaderProgram(this);

    // 顶点着色器
    const char *vertexShaderSource =
        "#version 330 core\n"
        "layout (location = 0) in vec3 a_position;\n"
        "layout (location = 1) in vec3 a_normal;\n"
        "uniform mat4 modelMatrix;\n"
        "uniform mat4 viewMatrix;\n"
        "uniform mat4 projectionMatrix;\n"
        "out vec3 v_normal;\n"
        "out vec3 v_position;\n"
        "void main() {\n"
        "    v_normal = mat3(modelMatrix) * a_normal;\n"
        "    v_position = vec3(modelMatrix * vec4(a_position, 1.0));\n"
        "    gl_Position = projectionMatrix * viewMatrix * vec4(v_position, 1.0);\n"
        "}\n";

    const char *fragmentShaderSource =
        "#version 330 core\n"
        "uniform vec3 lightPosition;\n"
        "in vec3 v_normal;\n"
        "in vec3 v_position;\n"
        "out vec4 FragColor;\n"
        "void main() {\n"
        "    vec3 normal = normalize(v_normal);\n"
        "    vec3 lightDir = normalize(lightPosition - v_position);\n"
        "    float diff = max(dot(normal, lightDir), 0.1);\n"
        "    vec3 color = vec3(0.2, 0.6, 0.8);\n"
        "    FragColor = vec4(color * diff, 1.0);\n"
        "}\n";

    // 编译并链接着色器
    if (!m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource)) {
        qDebug() << "Vertex shader compilation error:" << m_shaderProgram->log();
        doneCurrent(); // 释放上下文
        return;
    } else {
        // qDebug() << "Vertex shader compiled successfully.";
    }

    if (!m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource)) {
        qDebug() << "Fragment shader compilation error:" << m_shaderProgram->log();
        doneCurrent(); // 释放上下文
        return;
    } else {
        // qDebug() << "Fragment shader compiled successfully.";
    }

    if (!m_shaderProgram->link()) {
        qDebug() << "Shader program linking error:" << m_shaderProgram->log();
        doneCurrent(); // 释放上下文
        return;
    } else {
        // qDebug() << "Shader program linked successfully.";
    }

    // 创建顶点数组对象
    m_vao.create();
    m_vao.bind();

    // 创建顶点缓冲区
    m_vertexBuffer.create();
    m_vertexBuffer.bind();
    if (!m_vertices.isEmpty())
        m_vertexBuffer.allocate(m_vertices.data(), m_vertices.size() * sizeof(float));

    // 设置顶点属性
    m_shaderProgram->enableAttributeArray("a_position");
    m_shaderProgram->setAttributeBuffer("a_position", GL_FLOAT, 0, 3, 0);

    // 创建法线缓冲区
    m_normalBuffer.create();
    m_normalBuffer.bind();
    if (!m_normals.isEmpty())
        m_normalBuffer.allocate(m_normals.data(), m_normals.size() * sizeof(float));

    // 设置法线属性
    m_shaderProgram->enableAttributeArray("a_normal");
    m_shaderProgram->setAttributeBuffer("a_normal", GL_FLOAT, 0, 3, 0);

    // 解绑VAO
    m_vao.release();

    doneCurrent(); // 释放上下文
}



//清理资源
void LRU::cleanupGLResources() {
    makeCurrent(); // 确保上下文激活
    if (m_vao.isCreated()) m_vao.destroy();
    if (m_vertexBuffer.isCreated()) m_vertexBuffer.destroy();
    if (m_normalBuffer.isCreated()) m_normalBuffer.destroy();
    // 清理天花板资源
    if (m_ceilingVao.isCreated()) m_ceilingVao.destroy();
    if (m_ceilingVertexBuffer.isCreated()) m_ceilingVertexBuffer.destroy();
    if (m_ceilingNormalBuffer.isCreated()) m_ceilingNormalBuffer.destroy();
    m_ceilingInitialized = false; //重置标志位
    delete m_shaderProgram;
    m_shaderProgram = nullptr;
    doneCurrent(); // 释放上下文 
}



// STL 读取
bool LRU::readSTLFile(const QString &fileName, QVector<float> &vertices, QVector<float> &normals)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open STL file:" << fileName;
        return false;
    }

    char header[80];
    file.read(header, 80);
    quint32 numTriangles;
    file.read(reinterpret_cast<char*>(&numTriangles), 4);
    file.seek(0);

    bool isBinary = false;
    if (file.size() == 84 + numTriangles * 50) {
        isBinary = true;
    }

    vertices.clear();
    normals.clear();

    if (isBinary) {
        // 二进制读取逻辑
        file.seek(84);
        for (quint32 i = 0; i < numTriangles; ++i) {
            float nx, ny, nz;
            file.read(reinterpret_cast<char*>(&nx), 4);
            file.read(reinterpret_cast<char*>(&ny), 4);
            file.read(reinterpret_cast<char*>(&nz), 4);
            for (int j = 0; j < 3; ++j) {
                float x, y, z;
                file.read(reinterpret_cast<char*>(&x), 4);
                file.read(reinterpret_cast<char*>(&y), 4);
                file.read(reinterpret_cast<char*>(&z), 4);
                vertices.append(x);
                vertices.append(y);
                vertices.append(z);
                normals.append(nx);
                normals.append(ny);
                normals.append(nz);
            }
            file.seek(file.pos() + 2);
        }
        // qDebug() << "Binary STL file read. Number of vertices:" << vertices.size() << "Number of normals:" << normals.size();
    } else {
        // ASCII 读取逻辑
        QTextStream in(&file);
        QString line;
        while (!in.atEnd()) {
            line = in.readLine().trimmed();
            if (line.startsWith("facet normal")) {
                QStringList parts = line.split(" ");
                float nx = parts[2].toFloat();
                float ny = parts[3].toFloat();
                float nz = parts[4].toFloat();
                for (int i = 0; i < 3; ++i) {
                    while (!in.atEnd()) {
                        line = in.readLine().trimmed();
                        if (line.startsWith("vertex")) {
                            QStringList vParts = line.split(" ");
                            float x = vParts[1].toFloat();
                            float y = vParts[2].toFloat();
                            float z = vParts[3].toFloat();
                            vertices.append(x);
                            vertices.append(y);
                            vertices.append(z);
                            normals.append(nx);
                            normals.append(ny);
                            normals.append(nz);
                            break;
                        }
                    }
                }
            }
        }
        qDebug() << "ASCII STL file read. Number of vertices:" << vertices.size() << "Number of normals:" << normals.size();
    }
    file.close();
    return true;
}


void LRU::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 渲染模型
    render();

    // 使用QPainter绘制网格、坐标轴和标签
    QPainter painter(this);
    drawGridAndAxes(painter);
    painter.end();
}

void LRU::initializeGL() {
    initializeOpenGLFunctions(); // 必须首先初始化函数指针

    qDebug() << "OpenGL Version:" << (const char*)glGetString(GL_VERSION);

    glEnable(GL_DEPTH_TEST);     // 启用深度测试

    initializeGLResources(); // 在正确上下文中初始化资源
}


void LRU::updateBuffers() {
    m_vao.bind();

    // 更新顶点缓冲区
    m_vertexBuffer.bind();
    if (!m_vertices.isEmpty()) {
        m_vertexBuffer.allocate(m_vertices.constData(), m_vertices.size() * sizeof(float));
        qDebug() << "顶点缓冲区更新完成，大小:" << m_vertexBuffer.size();
    } else {
        qDebug() << "顶点数据为空，无法更新缓冲区";
    }
    m_vertexBuffer.release();

    // 更新法线缓冲区
    m_normalBuffer.bind();
    if (!m_normals.isEmpty()) {
        m_normalBuffer.allocate(m_normals.constData(), m_normals.size() * sizeof(float));
        qDebug() << "法线缓冲区更新完成，大小:" << m_normalBuffer.size();
    } else {
        qDebug() << "法线数据为空，无法更新缓冲区";
    }
    m_normalBuffer.release();

    m_vao.release();
}


void LRU::mousePressEvent(QMouseEvent *event)
{
    m_lastMousePos = event->pos();
    if (event->button() == Qt::LeftButton) {
        m_leftButtonPressed = true;
    } else if (event->button() == Qt::MiddleButton) {
        m_middleButtonPressed = true;
    }
}


void LRU::mouseMoveEvent(QMouseEvent *event)
{
    int dx = event->x() - m_lastMousePos.x();
    int dy = event->y() - m_lastMousePos.y();

    if (m_leftButtonPressed) {
        // 左键拖动：上下移动模型
        float sensitivity = 0.01f;
        m_mappingTranslation.setZ(m_mappingTranslation.z() - dy * sensitivity);
    } else if (m_middleButtonPressed) {
        // 中键拖动：绕y轴旋转模型视角
        float sensitivity = 1.0f;
        QQuaternion rotation = QQuaternion::fromAxisAndAngle(QVector3D(0, 1, 0), dx * sensitivity);
        m_mappingRotation = rotation * m_mappingRotation;
    }

    m_lastMousePos = event->pos();
    update(); // 触发重绘
}


void LRU::wheelEvent(QWheelEvent *event)
{
    // 获取滚轮滚动的角度
    int delta = event->angleDelta().y();

    // 缩放灵敏度
    float sensitivity = 0.001f;

    // 根据滚轮滚动方向计算缩放因子
    float scaleFactor = 1.0f + delta * sensitivity;

    // 更新缩放参数
    m_mappingScale *= scaleFactor;

    update(); // 触发重绘
}


void LRU::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_leftButtonPressed = false;
    } else if (event->button() == Qt::MiddleButton) {
        m_middleButtonPressed = false;
    }
}


//坐标绑定
void LRU::updateModelPosition(float motorXPosition,float motorYPosition,float motorZPosition,float rotationAngleY) {
    //模型偏移量,1:1更新模型位姿
    m_modelXOffset = motorXPosition;
    m_modelZOffset = motorZPosition;
    m_modelYOffset = motorYPosition;

    //缩放逻辑，超出上限，缩小模型，待写！！

    // 更新模型的平移矩阵
    m_mappingTranslation.setX(-m_modelXOffset);//缝隙1
    m_mappingTranslation.setZ(-m_modelZOffset);//缝隙2
    m_mappingTranslation.setY(m_modelYOffset);//电机坐标

    // 更新模型的旋转矩阵，绕Y轴旋转
    QQuaternion rotation = QQuaternion::fromAxisAndAngle(QVector3D(0, 1, 0), rotationAngleY);
    m_mappingRotation = rotation;

    update(); // 触发重绘
}


// 绘制网格和坐标系
void LRU::drawGridAndAxes(QPainter& painter) {

    float gridY = 1.0f;

    // 模型-视图-投影矩阵
    QMatrix4x4 model;
    model.setToIdentity();
    model.scale(0.1f * m_mappingScale); // 使用与模型相同的缩放
    model.rotate(m_mappingRotation);    // 使用与模型相同的旋转

    QMatrix4x4 view;
    float centerX = (min_x + max_x) * 0.001f / 2;
    float centerY = (min_y + max_y) * 0.001f / 2;
    float centerZ = (min_z + max_z) * 0.001f / 2;
    view.lookAt(
        QVector3D(centerX, centerY, centerZ + 5.0f),
        QVector3D(centerX, centerY, centerZ),
        QVector3D(0, 1, 0)
        );

    QMatrix4x4 projection;
    float aspect = static_cast<float>(width()) / height();
    projection.perspective(45.0f, aspect, 0.1f, 20.0f);

    // 将 3D 点转换为 2D 屏幕坐标
    auto project = [&](const QVector3D& point3D) -> QPointF {
        QVector4D homogenousPoint(point3D, 1.0f);
        QVector4D projected = projection * view * model * homogenousPoint;
        if (projected.w() == 0) return QPointF();

        // 归一化坐标
        float x = (projected.x() / projected.w() + 1.0f) * 0.5f * width();
        float y = (1.0f - (projected.y() / projected.w())) * 0.5f * height();
        return QPointF(x, y);
    };

    // 绘制坐标系
    QPen pen(Qt::white);
    pen.setWidth(2); // 设置坐标轴的粗细
    painter.setPen(pen);

    // 绘制XZ平面网格
    const int gridSize = 6.0;
    const float step = 1.5f;

    // x方向网格线
    for (int i = -gridSize; i <= gridSize; ++i) {
        QVector3D lineStart(centerX - gridSize * step, gridY, centerZ + i * step);
        QVector3D lineEnd(centerX + gridSize * step, gridY, centerZ + i * step);
        QPointF p1 = project(lineStart);
        QPointF p2 = project(lineEnd);
        painter.drawLine(p1, p2);
    }

    // z方向网格线
    for (int i = -gridSize; i <= gridSize; ++i) {
        QVector3D lineStart(centerX + i * step, gridY, centerZ - gridSize * step);
        QVector3D lineEnd(centerX + i * step, gridY, centerZ + gridSize * step);
        QPointF p1 = project(lineStart);
        QPointF p2 = project(lineEnd);
        painter.drawLine(p1, p2);
    }

    // 绘制箭头和标签
    auto drawAxis = [&](const QPointF& start, const QPointF& end, const QColor& color, const QString& label) {
        pen.setColor(color);
        painter.setPen(pen);
        painter.drawLine(start, end);

        // 绘制箭头
        QPainterPath arrow;
        QLineF line(start, end);
        double arrowSize = 10.0;
        double angle = std::atan2(-line.dy(), line.dx());
        QPointF arrowP1 = end - QPointF(sin(angle + M_PI / 3) * arrowSize,
                                        cos(angle + M_PI / 3) * arrowSize);
        QPointF arrowP2 = end - QPointF(sin(angle + M_PI - M_PI / 3) * arrowSize,
                                        cos(angle + M_PI - M_PI / 3) * arrowSize);

        arrow.moveTo(end);
        arrow.lineTo(arrowP1);
        arrow.lineTo(arrowP2);
        arrow.closeSubpath();
        painter.fillPath(arrow, color);

        // 绘制标签
        painter.setPen(color);
        painter.drawText(end, label);
    };

    // X轴（红色）
    QPointF startX = project(QVector3D(centerX , gridY, centerZ));
    QPointF endX = project(QVector3D(centerX + 10.0f, gridY, centerZ));
    drawAxis(startX, endX, Qt::red, "X");

    // Y轴（绿色）
    QPointF startY = project(QVector3D(centerX, gridY, centerZ));
    QPointF endY = project(QVector3D(centerX, gridY + 10.0f, centerZ));
    drawAxis(startY, endY, Qt::green, "Y");

    // Z轴（蓝色）
    QPointF startZ = project(QVector3D(centerX, gridY, centerZ ));
    QPointF endZ = project(QVector3D(centerX, gridY, centerZ + 10.0f));
    drawAxis(startZ, endZ, Qt::blue, "Z");
}


//绘制天花板
void LRU::initializeCeiling() {
    if (m_ceilingInitialized) return;

    float outerSize = 1000.0f;  // 外框大小

    // 计算天花板高度：模型最底部(min_y)上方3000mm
    float height = min_y + 3000.0f;

    // 外框四个顶点
    QVector3D outerTop[4] = {
        QVector3D(-outerSize, height, -outerSize),
        QVector3D(outerSize, height, -outerSize),
        QVector3D(outerSize, height, outerSize),
        QVector3D(-outerSize, height, outerSize)
    };

    // 法线方向（向上）
    QVector3D normal(0, 1, 0);

    // 构建外框四边形（两个三角形组成）
    m_ceilingVertices << outerTop[0].x() << outerTop[0].y() << outerTop[0].z();
    m_ceilingVertices << outerTop[1].x() << outerTop[1].y() << outerTop[1].z();
    m_ceilingVertices << outerTop[2].x() << outerTop[2].y() << outerTop[2].z();
    m_ceilingVertices << outerTop[0].x() << outerTop[0].y() << outerTop[0].z();
    m_ceilingVertices << outerTop[2].x() << outerTop[2].y() << outerTop[2].z();
    m_ceilingVertices << outerTop[3].x() << outerTop[3].y() << outerTop[3].z();

    // 添加法线数据
    for (int i = 0; i < m_ceilingVertices.size() / 3; ++i) {
        m_ceilingNormals << normal.x() << normal.y() << normal.z();
    }

    // 初始化天花板的VAO和缓冲区
    m_ceilingVao.create();
    m_ceilingVao.bind();

    m_ceilingVertexBuffer.create();
    m_ceilingVertexBuffer.bind();
    m_ceilingVertexBuffer.allocate(m_ceilingVertices.data(), m_ceilingVertices.size() * sizeof(float));

    m_shaderProgram->enableAttributeArray("a_position");
    m_shaderProgram->setAttributeBuffer("a_position", GL_FLOAT, 0, 3, 0);

    m_ceilingNormalBuffer.create();
    m_ceilingNormalBuffer.bind();
    m_ceilingNormalBuffer.allocate(m_ceilingNormals.data(), m_ceilingNormals.size() * sizeof(float));

    m_shaderProgram->enableAttributeArray("a_normal");
    m_shaderProgram->setAttributeBuffer("a_normal", GL_FLOAT, 0, 3, 0);

    m_ceilingVao.release();
    m_ceilingVertexBuffer.release();
    m_ceilingNormalBuffer.release();

    m_ceilingInitialized = true;
}


void LRU::renderCeiling() {
    if (!m_ceilingInitialized) {
        initializeCeiling();
    }

    if (m_ceilingVertices.isEmpty() || !m_ceilingVao.isCreated()) return;

    // 绑定天花板的VAO并绘制
    m_ceilingVao.bind();
    glDrawArrays(GL_TRIANGLES, 0, m_ceilingVertices.size() / 3);
    m_ceilingVao.release();
}
