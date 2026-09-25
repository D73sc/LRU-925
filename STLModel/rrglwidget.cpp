#include "STLModel/rrglwidget.h"
#include <QPainter>
#include <cmath>

//鼠标旋转、缩小、平移、网格、坐标系
RRGLWidget::RRGLWidget(QWidget *parent)
    : QGLWidget(parent)
    , xRot(0.0)
    , yRot(0.0)
    , zRot(0.0)
    , scaleFactor(1.0)
    , curButton(Qt::NoButton)
    , isMouseDragging(false)
{
    setFocusPolicy(Qt::StrongFocus); // 确保焦点捕获
    setMouseTracking(false);         // 仅拖拽时跟踪鼠标
}

RRGLWidget::~RRGLWidget()
{

}

// 角度归一化
double RRGLWidget::normalizeAngle(double angle, double min, double max) {
    angle = fmod(angle, 360.0);
    if (angle < min) angle = max + (angle - min);
    if (angle > max) angle = min + (angle - max);
    return angle;
}

// X轴旋转
void RRGLWidget::setXRotation(double angle) {
    angle = normalizeAngle(angle, -90, 90);
    if (qFuzzyCompare(xRot, angle)) return;
    xRot = angle;
    updateGL();
}

// Y轴旋转
void RRGLWidget::setYRotation(double angle) {
    angle = normalizeAngle(angle, -90, 90);
    if (qFuzzyCompare(yRot, angle)) return;
    yRot = angle;
    updateGL();
}

// Z轴旋转
void RRGLWidget::setZRotation(double angle) {
    angle = normalizeAngle(angle, 0.0, 360.0);
    if (qFuzzyCompare(zRot, angle)) return;
    zRot = angle;
    updateGL();
}

// 缩放
void RRGLWidget::setScaleFactor(double factor) {
    factor = qBound(0.1, factor, 10.0);
    if (qFuzzyCompare(scaleFactor, factor)) return;
    scaleFactor = factor;
    updateGL();
}


void RRGLWidget::initializeGL()
{
    glEnable(GL_DEPTH_TEST);    // 开启深度缓冲

    // 光源配置
    GLfloat ambientLight[] = {0.7f, 0.7f, 0.7f, 1.0f};
    GLfloat diffuseLight[] = {0.7f, 0.7f, 0.7f, 1.0f};
    GLfloat specularLight[] = {0.4f, 0.4f, 0.4f, 1.0f};
    GLfloat positionLight[] = {20.0f, 20.0f, 20.0f, 0.0f};

    glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specularLight);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 1.0);
    glLightfv(GL_LIGHT0, GL_POSITION, positionLight);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    glClearDepth(1.0);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_NORMALIZE);

    glClearColor(0.0, 0.0, 0.0, 1.0);
}

void RRGLWidget::resizeGL(int w, int h)
{
    if (w < 0 || h < 0) {
        return;
    }

    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    GLfloat zNear = 1.0;
    GLfloat zFar = 20000.0;
    GLfloat aspect = (GLfloat)w / (GLfloat)h;
    GLfloat fH = tan(GLfloat(70.0 / 360.0 * 3.14159)) * zNear;
    GLfloat fW = fH * aspect;
    glFrustum(-fW, fW, -fH, fH, zNear, zFar);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslated(0.0, 0.0, -1500.0);//调整相机高度-40->-1500，使得视角变大
}

// 鼠标按下：记录初始状态
void RRGLWidget::mousePressEvent(QMouseEvent *event) {
    if (!isMouseDragging) {
        curButton = event->button();
        lastMousePos = event->pos();
        isMouseDragging = true;
        setCursor(Qt::ClosedHandCursor);
    }
}

// 鼠标释放：重置状态
void RRGLWidget::mouseReleaseEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    curButton = Qt::NoButton;
    isMouseDragging = false;
    lastMousePos = QPoint();
    unsetCursor();
}

// 鼠标移动
void RRGLWidget::mouseMoveEvent(QMouseEvent *event) {
    if (!isMouseDragging || curButton == Qt::NoButton) return;

    const QPoint delta = event->pos() - lastMousePos;
    const double rotateSensitivity = 0.1;

    if (delta.isNull()) return;

    switch (curButton) {
    case Qt::LeftButton:
        // 左键：绕X/Y轴旋转
        setXRotation(xRot + delta.y() * rotateSensitivity);
        setYRotation(yRot + delta.x() * rotateSensitivity);
        break;

    case Qt::RightButton:
        // 右键：绕Z轴旋转
        setZRotation(zRot + delta.x() * rotateSensitivity);
        break;

    default:
        break;
    }

    lastMousePos = event->pos();
}

// 滚轮
void RRGLWidget::wheelEvent(QWheelEvent *event) {
    const double scaleStep = 0.05;
    double newFactor = scaleFactor;

    if (isMouseDragging) {
        event->ignore();
        return;
    }

    if (event->angleDelta().y() > 0) {
        newFactor += scaleStep;
    } else {
        newFactor -= scaleStep;
    }

    setScaleFactor(newFactor);
    event->accept();
}

void RRGLWidget::drawGrid()
{
    glPushMatrix();

    GLfloat color[] = {8.0f / 255, 108.0f / 255, 162.0f / 255};
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);

    int step = 50;
    int num = 25;

    glBegin(GL_LINES);
    for (int i = -num; i < num + 1; i++) {
        glVertex3f(i * step, -num * step, 0);
        glVertex3f(i * step, num * step, 0);
        glVertex3f(-num * step, i * step, 0);
        glVertex3f(num * step, i * step, 0);
    }
    glEnd();

    glPopMatrix();
}

void RRGLWidget::drawCoordinates()
{
    glPushMatrix();
    glLineWidth(2.0f);
    setupColor(255, 255, 255);
    glBegin(GL_LINES);

    // X轴
    glVertex3f(-900, 0, 0);
    glVertex3f(900, 0, 0);
    // Y轴
    glVertex3f(0, -900, 0);
    glVertex3f(0, 900, 0);
    // Z轴
    glVertex3f(0, 0, 0);
    glVertex3f(0, 0, 700);

    glEnd();

    // 标签
    qglColor(QColor::fromRgbF(1, 0, 0));
    renderText(-900, 0, 0, "-X", QFont("helvetica", 12, QFont::Bold, true));
    renderText(900, 0, 0, "+X", QFont("helvetica", 12, QFont::Bold, true));

    qglColor(QColor::fromRgbF(0, 1, 0));
    renderText(0, -900, 0, "-Y", QFont("helvetica", 12, QFont::Bold, true));
    renderText(0, 900, 0, "+Y", QFont("helvetica", 12, QFont::Bold, true));

    qglColor(QColor::fromRgbF(0, 0, 1));
    renderText(0, 0, 700, "+Z", QFont("helvetica", 12, QFont::Bold, true));

    glLineWidth(1.0f);
    glPopMatrix();
}

void RRGLWidget::drawSTLCoordinates(int r, int g, int b)
{
    glPushMatrix();
    glLineWidth(1.5f);
    setupColor(r, g, b);
    glBegin(GL_LINES);

    glVertex3f(-300, 0, 0);
    glVertex3f(300, 0, 0);
    glVertex3f(0, -300, 0);
    glVertex3f(0, 300, 0);
    glVertex3f(0, 0, 0);
    glVertex3f(0, 0, 500);
    glEnd();

    // 标签
    qglColor(QColor::fromRgbF(1, 0, 0));
    renderText(-300, 0, 0, "-X", QFont("helvetica", 12, QFont::Bold, true));
    renderText(300, 0, 0, "+X", QFont("helvetica", 12, QFont::Bold, true));
    qglColor(QColor::fromRgbF(0, 1, 0));
    renderText(0, -300, 0, "-Y", QFont("helvetica", 12, QFont::Bold, true));
    renderText(0, 300, 0, "+Y", QFont("helvetica", 12, QFont::Bold, true));
    qglColor(QColor::fromRgbF(0, 0, 1));
    renderText(0, 0, 500, "+Z", QFont("helvetica", 12, QFont::Bold, true));

    glLineWidth(1.0f);
    glPopMatrix();
}

void RRGLWidget::setupColor(int r, int g, int b)
{
    setupColor(
        static_cast<float>(r) / 255.0f,
        static_cast<float>(g) / 255.0f,
        static_cast<float>(b) / 255.0f
        );
}

void RRGLWidget::setupColor(float r, float g, float b)
{
    GLfloat color[] = {r, g, b, 1.0f};
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);
}

void RRGLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 每次重绘 重置矩阵
    glMatrixMode(GL_MODELVIEW);//对视图矩阵做变换
    glLoadIdentity();
    //恢复相机位置
    glTranslated(0.0, 0.0, -1500.0);

    glScaled(scaleFactor, scaleFactor, scaleFactor);
    glRotated(xRot, 1.0, 0.0, 0.0);
    glRotated(yRot, 0.0, 1.0, 0.0);
    glRotated(zRot, 0.0, 0.0, 1.0);

    drawGL();
}
