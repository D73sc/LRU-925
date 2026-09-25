#ifndef ARUCODETECTOR_H
#define ARUCODETECTOR_H
#pragma execution_character_set("utf-8")
#include <QObject>
#include <QImage>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <limits>
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>
#include <opencv2/calib3d.hpp>
#include <QDebug>
#include <QMessageBox>
#include <QStringList>
#include <opencv2/imgproc.hpp>
#include <cmath>
#include <algorithm>
#include <QElapsedTimer>
#include "LRUdatadef.h"


#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


class ArucoDetector : public QObject
{
    Q_OBJECT
public:
    struct ArucoResult {
        bool detected;
        int id;
        std::vector<cv::Point2f> cornerPixels;
    };

    struct PoseResult {
        bool valid;
        cv::Vec3d rvec;             // 旋转向量
        cv::Vec3d tvec;             // 平移向量（mm，相机系）
        cv::Mat   rotationMatrix;   // 3x3 旋转矩阵
        double    roll;             // 绕X轴，单位：度
        double    pitch;            // 绕Y轴，单位：度
        double    yaw;              // 绕Z轴，单位：度
        double    reprojectionError;// 平均重投影误差（像素）
    };
    struct PlanarPose {
        bool   valid;   // 是否有效
        double x;       // 平移 X（mm，相机系）
        double y;       // 平移 Y（mm，相机系）
        double yaw;     // 绕Z轴旋转角（度）
    };

    explicit ArucoDetector(QObject *parent = nullptr);

    ArucoResult detectOneArucoCode(cv::Mat &image);
    ArucoDetector::PlanarPose processImage(cv::Mat &image, float z);
    void printDetectionDebugInfo(const cv::Mat &image, int cameraIndex, float liftZ, float sixDofZ);

    // 基于 IPPE_SQUARE 的 PnP 位姿求解（对齐 Python 版本）
    PoseResult solvePnPPose(const std::vector<cv::Point2f> &cornerPixels,
                            float arucoSizeMm = 40.0f);

    // 旋转向量 → 欧拉角（内旋 X-Y-Z，即外旋 Z-Y-X，单位：度）
    static void rotationVectorToEuler(const cv::Vec3d &rvec,
                                      double &roll, double &pitch, double &yaw);

    std::vector<cv::Point2f> sortCornersTLTRBRBL(const std::vector<cv::Point2f>& pts);

    float x_offset;
    float y_offset;
    float rz_offset;

public slots:
    void onParamsReceived(const LRUInnerParams &params);

private:
    bool readCameraParamsFromJson(const QString &jsonPath);
    QString getCameraParamsFile(float z);

private:
    cv::Mat m_intrinsicMatrix;
    cv::Mat m_distCoeffs;
    cv::Mat m_rotationMatrix;
    cv::Mat m_translationVector;

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
    int   marker_id;
    float angle_offset;

    cv::aruco::Dictionary         m_arucoDict;
    cv::aruco::DetectorParameters m_parameters;
    cv::Ptr<cv::aruco::ArucoDetector> m_detector;

signals:
    void arucoUpdateUI(QString str);
};

#endif // ARUCODETECTOR_H
