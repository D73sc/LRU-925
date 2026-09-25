#ifndef LRUDATADEF_H
#define LRUDATADEF_H

#include <QString>
#include <QMap>
#include <QMetaType>

// 内层结构体：10个float参数（纯聚合结构体，无构造函数，最简洁）
//六自由度平台坐标系下的参数
struct LRUInnerParams {
    float x_gap;//
    float y_gap;
    float final_z;
    float aruco_to_gapx;
    float aruco_to_gapy;
    float camera_to_lrux_50;
    float camera_to_lruy_50;
    float camera_to_lrux_16;
    float camera_to_lruy_16;
    float offset_x_50;
    float offset_y_50;
    float offset_x_16;
    float offset_y_16;
    int marker_id;
    float angle;
    float target_rx;
    float target_ry;
    int camera0_exposureTime;
    int camera1_exposureTime;
};
Q_DECLARE_METATYPE(LRUInnerParams)

// ==============================================
// 【唯一修改点】用内联函数返回静态常量数据
// 消除 non-POD static 警告，且线程安全、初始化安全
// ==============================================
inline const QMap<QString, LRUInnerParams>& LRUpresetData()
{
    // 函数内 static：第一次调用时初始化，绝对安全
    static const QMap<QString, LRUInnerParams> LRU_DATA = {
   // 格式：{"字符串名称", {p1,p2,p3,p4,p5,p6,p7,p8,p9,p10}}
   {"安装车右片架",   {7.0f,4.0f,
                     2655.0f,
                     41.73f,71.95f,
                     62.5f,84.0f,
                     17.5f,84.0f,
                     5.0f,-12.0f,
                     0.0f,0.0f,
                     1,
                     0.0f,
                     -0.11f,0.57f,
                     500000,300000}},

   {"安装车右右隔板玻璃",   {7.0f,30.0f,
                           2655.0f,
                           41.73f,71.95f,
                           57.5f,112.0f,
                           12.5f,112.0f,
                           -8.5f,0.0f,
                           -6.0f,0.0f,
                           1,
                           0.0f,
                           -0.11f,0.57f,
                           500000,300000}},


   {"安装车右左隔板玻璃",   {7.0f,20.0f,
                           2675.0f,
                           41.73f,71.95f,
                           57.5f,112.0f,
                           12.5f,112.0f,
                           -8.5f,0.0f,
                           -6.0f,0.0f,
                           1,
                           0.0f,
                           -0.11f,0.57f,
                           500000,300000}},


   {"安装车右中灯箱",   {12.0f,3.0f,
                       2695.0f,
                       41.73f,605.95f,
                       52.5f,620.0f,
                       7.5f,620.0f,
                       -2.17f,-9.5f,
                       -1.88f,-6.66f,
                       1,
                       0.0f,
                       -0.11f,0.57f,
                       500000,300000}},


   {"安装车右侧灯箱",   {12.0f,5.0f,
                       2695.0f,
                       45.9f,-159.9f,
                       52.5f,-157.0f,
                       7.5f,-157.0f,
                       -2.0f,-10.0f,
                       -2.34f,-8.62f,
                       0,
                       0.0f,
                       -0.11f,0.57f,
                       500000,300000}},


   {"安装车右片窗",   {7.25f,12.0f,
                     2600.0f,
                     295.73f-32.0f,30.95f,
                     202.5f,52.0f,
                     157.5f,52.0f,
                     -3.0f,-8.0f,
                     -7.0f,0.0f,
                     1,
                     0.0f,
                     -0.11f,0.57f,
                     500000,300000}},

    {"安装TM1TM2",   {44.95f,22.5f,
                      2690.0f,
                      43.45f,-833.45f,
                      47.5f,-786.5f,
                      2.5f,-786.5f,
                      0.0f,0.0f,
                      0.0f,0.0f,
                      1,
                      0.0f,
                      -0.2f,0.69f,
                      50000,300000}},
   };
    return LRU_DATA;
}

#endif // LRUDATADEF_H
