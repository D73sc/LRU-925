#include "STLModel/stlfileloader.h"
#include <QDebug>
#include <QTextStream>
#include <cstring>
#include <QGL>


STLFileLoader::STLFileLoader(const QString& filename, float ratio)
    : mRatio(ratio)
{
    if (!filename.isEmpty()) {
        loadStl(filename);
    }
}

STLFileLoader::~STLFileLoader()
{
    clear();
}

void STLFileLoader::loadStl(const QString& filename)
{
    QFile file(filename);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray arr = file.read(5);
        file.close();

        // 判断是 ASCII 还是二进制格式
        if (arr == "solid") {
            loadBinaryStl(filename);
        } else {
             loadTextStl(filename);
        }
    } else {
        qDebug() << filename << "不存在";
    }
}

void STLFileLoader::loadTextStl(const QString& filename)
{
    qDebug() << "load text file:" << filename;
    clear();

    QList<QVector3D> triangle;
    STLTriangle tSTLTriangle;

    QFile file(filename);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            QStringList words = line.split(' ', QString::SkipEmptyParts);

            if (words.isEmpty()) continue;

            if (words[0] == "facet") {
                triangle.clear();
                tSTLTriangle.reset();
                if (words.size() >= 5) {
                    tSTLTriangle.setNormal(
                        words[2].toFloat(),
                        words[3].toFloat(),
                        words[4].toFloat()
                        );
                }
            } else if (words[0] == "vertex") {
                if (words.size() >= 4) {
                    triangle.append(QVector3D(
                        words[1].toFloat(),
                        words[2].toFloat(),
                        words[3].toFloat()
                        ));
                }
            } else if (words[0] == "endloop") {
                if (triangle.length() == 3) {
                    for (int i = 0; i < 3; ++i) {
                        tSTLTriangle.setVertex(i, triangle[i]);
                    }
                    model.append(tSTLTriangle);
                }
            }
        }
        file.close();
        qDebug() << filename << "解析到三角形数量：" << model.size();
    }
}

void STLFileLoader::loadBinaryStl(const QString& filename)
{
    qDebug() << "load Binary file:" << filename;
    clear();

    QList<QVector3D> triangle;
    STLTriangle tSTLTriangle;

    QFile STL_file(filename);
    int fileSize = STL_file.size();
    char *buf = (char *)malloc(sizeof(char) * fileSize);
    bool isOk = STL_file.open(QIODevice::ReadOnly);
    if (!isOk) {
        free(buf);
        return;
    }

    QDataStream stream(&STL_file);
    stream.readRawData(buf, fileSize);
    STL_file.close();

    const char *p = buf;
    char name[80];       // 起始 80 个字节 文件名
    int triangle_num;    // 4 个字节 三角形个数
    float n1, n2, n3;    // 法向量
    float v1, v2, v3;    // 顶点

    memcpy(name, p, 80); // 记录文件名
    p += 80;             // 跳过文件名

    memcpy(&triangle_num, p, 4); // 记录三角形个数
    p += 4;              // 跳过个数标识

    for (int i = 0; i < triangle_num; i++) {
        // 读取法向量
        memcpy(&n1, p, 4);
        p += 4;
        memcpy(&n2, p, 4);
        p += 4;
        memcpy(&n3, p, 4);
        p += 4;

        triangle.clear();
        tSTLTriangle.reset();
        tSTLTriangle.setNormal(n1, n2, n3);

        for (int j = 0; j < 3; j++) {
            // 读取顶点信息
            memcpy(&v1, p, 4);
            p += 4;
            memcpy(&v2, p, 4);
            p += 4;
            memcpy(&v3, p, 4);
            p += 4;

            triangle.append(QVector3D(v1, v2, v3));
        }

        if (triangle.length() == 3) {
            for (int i = 0; i < 3; ++i) {
                tSTLTriangle.setVertex(i, triangle[i]);
            }
            model.append(tSTLTriangle);
        }
        p += 2; // 跳过尾部标志 两字节
    }
    qDebug() << filename << "解析到三角形数量：" << model.size();
    free(buf);
}

void STLFileLoader::draw()
{
    QList<STLTriangle> triangles = model;
    QVector3D normal;
    QVector3D vertex;

    glBegin(GL_TRIANGLES);
    foreach(STLTriangle tri, triangles) {
        normal = tri.getNormal();
        glNormal3f(
            mRatio * normal.x(),
            mRatio * normal.y(),
            mRatio * normal.z()
            );

        for (int i = 0; i < 3; ++i) {
            vertex = tri.getVertex(i);
            glVertex3f(
                mRatio * vertex.x(),
                mRatio * vertex.y(),
                mRatio * vertex.z()
                );
        }
    }
    glEnd();
}

void STLFileLoader::clear()
{
    model.clear();
}
