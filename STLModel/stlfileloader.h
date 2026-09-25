#ifndef STLFILELOADER_H
#define STLFILELOADER_H

#include <QFile>
#include <QVector3D>
#include <QString>
#include <QList>
#include <QDataStream>

// STL 三角形结构
class STLTriangle {
public:
    QVector3D normal;
    QVector3D vertices[3];

    void reset() {
        normal = QVector3D(0, 0, 0);
        for (int i = 0; i < 3; ++i) {
            vertices[i] = QVector3D(0, 0, 0);
        }
    }

    void setNormal(float x, float y, float z) {
        normal = QVector3D(x, y, z);
    }

    void setVertex(int index, const QVector3D& v) {
        if (index >= 0 && index < 3) {
            vertices[index] = v;
        }
    }

    QVector3D getNormal() const {
        return normal;
    }

    QVector3D getVertex(int index) const {
        if (index >= 0 && index < 3) {
            return vertices[index];
        }
        return QVector3D(0, 0, 0);
    }
};

// STL 文件加载器类
class STLFileLoader {
public:
    STLFileLoader(const QString& filename = "", float ratio = 1.0f);
    ~STLFileLoader();

    // 加载 STL 文件
    void loadStl(const QString& filename);

    // 绘制模型
    void draw();

    // 清空模型数据
    void clear();

    // 设置缩放比例
    void setRatio(float ratio) { mRatio = ratio; }
    float getRatio() const { return mRatio; }

private:
    // 加载 ASCII 格式 STL
    void loadTextStl(const QString& filename);

    // 加载二进制格式 STL
    void loadBinaryStl(const QString& filename);

private:
    QList<STLTriangle> model;  // 模型三角形列表
    float mRatio;              // 缩放比例
};

#endif // STLFILELOADER_H
