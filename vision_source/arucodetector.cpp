#include "arucoDetector.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>

namespace {
QString buildImageQualityDiagnosis(const cv::Mat &image)
{
    cv::Mat gray;
    if (image.channels() == 1) {
        gray = image;
    } else if (image.channels() == 4) {
        cv::cvtColor(image, gray, cv::COLOR_BGRA2GRAY);
    } else if (image.channels() == 3) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        return QString("图像通道数=%1，无法进行亮度/清晰度诊断").arg(image.channels());
    }

    cv::Scalar meanValue;
    cv::Scalar stddevValue;
    cv::meanStdDev(gray, meanValue, stddevValue);

    cv::Mat darkMask = gray < 35;
    cv::Mat brightMask = gray > 245;
    const double pixelCount = static_cast<double>(gray.rows * gray.cols);
    const double darkRatio = pixelCount > 0.0 ? cv::countNonZero(darkMask) / pixelCount : 0.0;
    const double brightRatio = pixelCount > 0.0 ? cv::countNonZero(brightMask) / pixelCount : 0.0;

    cv::Mat laplacianImage;
    cv::Laplacian(gray, laplacianImage, CV_64F);
    cv::Scalar lapMean;
    cv::Scalar lapStddev;
    cv::meanStdDev(laplacianImage, lapMean, lapStddev);
    const double sharpness = lapStddev[0] * lapStddev[0];

    QStringList reasons;
    if (meanValue[0] < 45.0) {
        reasons << "画面偏暗，可能需要提高曝光或光源亮度";
    }
    if (meanValue[0] > 210.0 || brightRatio > 0.25) {
        reasons << "画面偏亮或存在过曝，二维码白底/反光区域可能丢失细节";
    }
    if (stddevValue[0] < 25.0) {
        reasons << "对比度偏低，二维码黑白边界可能不明显";
    }
    if (sharpness < 80.0) {
        reasons << "清晰度偏低，可能虚焦或运动模糊";
    }
    if (darkRatio > 0.60) {
        reasons << "暗像素占比过高，二维码可能处在阴影中";
    }

    QString metrics = QString("亮度均值=%1，对比度=%2，过暗像素=%3%，过曝像素=%4%，清晰度=%5")
                          .arg(meanValue[0], 0, 'f', 1)
                          .arg(stddevValue[0], 0, 'f', 1)
                          .arg(darkRatio * 100.0, 0, 'f', 1)
                          .arg(brightRatio * 100.0, 0, 'f', 1)
                          .arg(sharpness, 0, 'f', 1);

    if (reasons.isEmpty()) {
        reasons << "图像亮度、对比度和清晰度未发现明显异常，可能是二维码不在画面内、尺寸太小、角度过大或被遮挡";
    }

    return QString("%1；推断：%2").arg(metrics).arg(reasons.join("；"));
}

QString formatNumberForFileName(double value)
{
    QString text = QString::number(value, 'f', 1);
    text.replace("-", "m");
    text.replace(".", "p");
    return text;
}

int inferCameraIndexFromLiftZ(float z)
{
    if (-0.5f <= z && z <= 0.5f) {
        return 1;
    }
    if (1699.5f <= z && z <= 1700.5f) {
        return 0;
    }
    return -1;
}

cv::Mat buildAnnotatedDetectionImage(const cv::Mat &image,
                                     const std::vector<std::vector<cv::Point2f>> &corners,
                                     const std::vector<int> &ids)
{
    cv::Mat annotated;
    if (image.channels() == 1) {
        cv::cvtColor(image, annotated, cv::COLOR_GRAY2BGR);
    } else if (image.channels() == 4) {
        cv::cvtColor(image, annotated, cv::COLOR_BGRA2BGR);
    } else {
        annotated = image.clone();
    }

    for (size_t i = 0; i < corners.size(); ++i) {
        if (corners[i].size() != 4) {
            continue;
        }

        const cv::Scalar borderColor(0, 255, 0);
        const cv::Scalar cornerColor(0, 0, 255);
        const cv::Scalar centerColor(255, 0, 0);

        std::vector<cv::Point> polygon;
        for (const auto &point : corners[i]) {
            polygon.push_back(cv::Point(cvRound(point.x), cvRound(point.y)));
        }
        cv::polylines(annotated, polygon, true, borderColor, 3, cv::LINE_AA);

        cv::Point2f center(0.0f, 0.0f);
        for (size_t cornerIndex = 0; cornerIndex < corners[i].size(); ++cornerIndex) {
            const cv::Point point(cvRound(corners[i][cornerIndex].x), cvRound(corners[i][cornerIndex].y));
            center.x += corners[i][cornerIndex].x;
            center.y += corners[i][cornerIndex].y;
            cv::circle(annotated, point, 8, cornerColor, -1, cv::LINE_AA);
            cv::putText(annotated,
                        QString::number(cornerIndex + 1).toStdString(),
                        point + cv::Point(8, -8),
                        cv::FONT_HERSHEY_SIMPLEX,
                        0.8,
                        cornerColor,
                        2,
                        cv::LINE_AA);
        }

        center.x /= 4.0f;
        center.y /= 4.0f;
        cv::circle(annotated, cv::Point(cvRound(center.x), cvRound(center.y)), 8, centerColor, -1, cv::LINE_AA);

        QString label = i < ids.size() ? QString("ID=%1").arg(ids[i]) : "ID=?";
        cv::putText(annotated,
                    label.toStdString(),
                    polygon[0] + cv::Point(0, -14),
                    cv::FONT_HERSHEY_SIMPLEX,
                    1.0,
                    borderColor,
                    2,
                    cv::LINE_AA);
    }

    return annotated;
}

QString saveDetectionImage(const cv::Mat &image,
                           float z,
                           const std::vector<std::vector<cv::Point2f>> &corners,
                           const std::vector<int> &ids)
{
    if (image.empty()) {
        return "图片为空，未保存检测图片";
    }

    QDir saveDir(QCoreApplication::applicationDirPath());
    if (!saveDir.exists("vision_detection_images") && !saveDir.mkdir("vision_detection_images")) {
        return "创建检测图片目录失败";
    }

    const int cameraIndex = inferCameraIndexFromLiftZ(z);
    const QString cameraText = cameraIndex >= 0 ? QString("camera%1").arg(cameraIndex) : "camera_unknown";
    const QString fileName = QString("%1_%2_liftz_%3.png")
                                 .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz"))
                                 .arg(cameraText)
                                 .arg(formatNumberForFileName(z));
    const QString filePath = saveDir.filePath(QString("vision_detection_images/%1").arg(fileName));

    cv::Mat imageToSave = buildAnnotatedDetectionImage(image, corners, ids);
    QByteArray localPath = QDir::toNativeSeparators(filePath).toLocal8Bit();
    if (!cv::imwrite(localPath.constData(), imageToSave)) {
        return QString("保存检测图片失败：%1").arg(filePath);
    }

    return QString("检测图片已保存：%1，标注到%2个ArUco标记").arg(filePath).arg(ids.size());
}
}

ArucoDetector::ArucoDetector(QObject *parent) : QObject(parent)
{
    aruco_to_gapx  = 0.0f;
    aruco_to_gapy  = 0.0f;
    camera_to_lrux_50 = 0.0f;
    camera_to_lruy_50 = 0.0f;
    camera_to_lrux_16 = 0.0f;
    camera_to_lruy_16 = 0.0f;
    marker_id    = 0;
    angle_offset = 0.0f;

    x_offset  = 0.0f;
    y_offset  = 0.0f;
    rz_offset = 0.0f;

    // 1. 初始化字典
    m_arucoDict = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_4X4_50);

    // 2. 初始化检测参数（与 Python 版本一致）
    m_parameters = cv::aruco::DetectorParameters();
    m_parameters.minMarkerPerimeterRate   = 0.077;
    m_parameters.cornerRefinementMethod   = cv::aruco::CORNER_REFINE_APRILTAG; // 亚像素精炼
    m_parameters.adaptiveThreshWinSizeMin = 9;
    m_parameters.adaptiveThreshWinSizeMax = 53;
    m_parameters.adaptiveThreshConstant   = 7;

    // 3. 初始化检测器
    m_detector = cv::makePtr<cv::aruco::ArucoDetector>(m_arucoDict, m_parameters);
}

ArucoDetector::ArucoResult ArucoDetector::detectOneArucoCode(cv::Mat &image)
{
    ArucoResult result;
    result.detected = false;

    if (m_intrinsicMatrix.empty() || m_distCoeffs.empty()) {
        arucoUpdateUI("相机参数为空！");
        return result;
    }

    if (image.empty()) {
        arucoUpdateUI("图片为空！");
        return result;
    }

    std::vector<int> ids;
    std::vector<std::vector<cv::Point2f>> corners;

    m_detector->detectMarkers(image, corners, ids);

    image.release();

    if (ids.empty()) {
        arucoUpdateUI("二维码ID为空，检测失败！");
        return result;
    }
    arucoUpdateUI("二维码ID有！");

    // 图像中心 (2592x1944)
    const cv::Point2f imageCenter(1296.0f, 972.0f);

    // 1. 收集所有 ID 匹配的二维码索引
    std::vector<size_t> matchingIndices;
    for (size_t i = 0; i < ids.size(); ++i) {
        if (ids[i] == marker_id) {
            matchingIndices.push_back(i);
        }
    }

    if (matchingIndices.empty()) {
        arucoUpdateUI(QString("未找到目标ID(%1)，检测失败！").arg(marker_id));
        return result;
    }

    int selectedIndex = -1;

    // 2. 多个匹配时，选中心离图像中心最近的那个
    if (matchingIndices.size() == 1) {
        selectedIndex = static_cast<int>(matchingIndices[0]);
    } else {
        float minDistSq = std::numeric_limits<float>::max();

        for (size_t idx : matchingIndices) {
            const auto &currentCorners = corners[idx];

            cv::Point2f markerCenter(0.0f, 0.0f);
            for (const auto &p : currentCorners) {
                markerCenter.x += p.x;
                markerCenter.y += p.y;
            }
            markerCenter.x /= 4.0f;
            markerCenter.y /= 4.0f;

            float dx = markerCenter.x - imageCenter.x;
            float dy = markerCenter.y - imageCenter.y;
            float distSq = dx * dx + dy * dy;

            if (distSq < minDistSq) {
                minDistSq = distSq;
                selectedIndex = static_cast<int>(idx);
            }
        }
    }

    std::vector<cv::Point2f> markerCorners = corners[selectedIndex];
    markerCorners = sortCornersTLTRBRBL(markerCorners);
    result.id           = ids[selectedIndex];
    result.detected     = true;
    result.cornerPixels = markerCorners;

    return result;
}

void ArucoDetector::printDetectionDebugInfo(const cv::Mat &image, int cameraIndex, float liftZ, float sixDofZ)
{
    arucoUpdateUI(QString("检测姿态诊断：当前使用%1号相机，二级升降z=%2，六自由度平台z=%3")
                      .arg(cameraIndex)
                      .arg(liftZ, 0, 'f', 2)
                      .arg(sixDofZ, 0, 'f', 2));

    if (image.empty()) {
        arucoUpdateUI("检测姿态诊断：抓取图片为空");
        return;
    }

    arucoUpdateUI(QString("检测姿态诊断：图片尺寸=%1x%2，通道数=%3")
                      .arg(image.cols)
                      .arg(image.rows)
                      .arg(image.channels()));

    std::vector<int> ids;
    std::vector<std::vector<cv::Point2f>> corners;
    std::vector<std::vector<cv::Point2f>> rejectedCandidates;
    m_detector->detectMarkers(image, corners, ids, rejectedCandidates);

    if (ids.empty()) {
        arucoUpdateUI("检测姿态诊断：未检测到任何ArUco标记");
        arucoUpdateUI(QString("检测姿态诊断：拒绝候选数量=%1").arg(rejectedCandidates.size()));
        if (!rejectedCandidates.empty()) {
            arucoUpdateUI("检测姿态诊断：存在候选轮廓但未能解码，可能是二维码边界不完整、反光、污损、角度过大或画面模糊");
        }
        arucoUpdateUI(QString("检测姿态诊断：失败原因推断：%1").arg(buildImageQualityDiagnosis(image)));
        return;
    }

    QStringList idTexts;
    bool hasTargetId = false;
    for (int id : ids) {
        idTexts << QString::number(id);
        if (id == marker_id) {
            hasTargetId = true;
        }
    }

    arucoUpdateUI(QString("检测姿态诊断：检测到ArUco ID：%1，目标ID=%2")
                      .arg(idTexts.join(", "))
                      .arg(marker_id));

    if (!hasTargetId) {
        arucoUpdateUI(QString("检测姿态诊断：检测到ID=%1，但目标ID=%2")
                          .arg(idTexts.join(", "))
                          .arg(marker_id));
        arucoUpdateUI("检测姿态诊断：失败原因推断：画面中有二维码，但不是当前LRU参数要求的目标ID；请检查二维码类型、LRU类型选择或相机画面内容");
    }

    for (size_t i = 0; i < ids.size(); ++i) {
        if (i >= corners.size() || corners[i].size() != 4) {
            arucoUpdateUI(QString("检测姿态诊断：ID=%1 角点数量异常").arg(ids[i]));
            continue;
        }

        cv::Point2f center(0.0f, 0.0f);
        for (const auto &point : corners[i]) {
            center.x += point.x;
            center.y += point.y;
        }
        center.x /= static_cast<float>(corners[i].size());
        center.y /= static_cast<float>(corners[i].size());

        QStringList cornerTexts;
        for (const auto &point : corners[i]) {
            cornerTexts << QString("(%1,%2)")
            .arg(point.x, 0, 'f', 1)
                .arg(point.y, 0, 'f', 1);
        }

        arucoUpdateUI(QString("检测姿态诊断：ID=%1 中心=(%2,%3) 角点=%4")
                          .arg(ids[i])
                          .arg(center.x, 0, 'f', 1)
                          .arg(center.y, 0, 'f', 1)
                          .arg(cornerTexts.join(" ")));
    }
}

// =============================================================================
//                    PnP 位姿求解
// =============================================================================
ArucoDetector::PoseResult
ArucoDetector::solvePnPPose(const std::vector<cv::Point2f> &cornerPixels,
                            float arucoSizeMm)
{
    PoseResult pose;
    pose.valid = false;
    pose.reprojectionError = std::numeric_limits<double>::max();

    // 参数检查
    if (cornerPixels.size() != 4) {
        arucoUpdateUI("PnP失败：角点数量必须为4");
        return pose;
    }
    if (m_intrinsicMatrix.empty() || m_distCoeffs.empty()) {
        arucoUpdateUI("PnP失败：相机内参或畸变系数为空");
        return pose;
    }

    cv::Point2f mid1 = (cornerPixels[0] + cornerPixels[1]) * 0.5f;
    cv::Point2f mid2 = (cornerPixels[2] + cornerPixels[3]) * 0.5f;
    float dx = mid2.x - mid1.x;
    float dy = mid2.y - mid1.y;
    arucoUpdateUI(QString("cornerPixels[0](%1)！").arg(cornerPixels[0].x));
    arucoUpdateUI(QString("cornerPixels[0](%1)！").arg(cornerPixels[0].y));
    arucoUpdateUI(QString("cornerPixels[1](%1)！").arg(cornerPixels[1].x));
    arucoUpdateUI(QString("cornerPixels[1](%1)！").arg(cornerPixels[1].y));

    arucoUpdateUI(QString("cornerPixels[2](%1)！").arg(cornerPixels[2].x));
    arucoUpdateUI(QString("cornerPixels[2](%1)！").arg(cornerPixels[2].y));
    arucoUpdateUI(QString("cornerPixels[3](%1)！").arg(cornerPixels[3].x));
    arucoUpdateUI(QString("cornerPixels[3](%1)！").arg(cornerPixels[3].y));
    // arucoUpdateUI(QString("dx(%1)！").arg(dx));
    // arucoUpdateUI(QString("dy(%1)！").arg(dy));
    // 与 y 轴的夹角，单位：弧度
    double angleRad = std::atan2(dx, dy);
    arucoUpdateUI(QString("angleRad(%1)！").arg(angleRad));
    // 转成角度
    double angleDeg = angleRad * 180.0 / CV_PI;

    // 1. 构造 marker 平面 4 个 3D 点（IPPE_SQUARE 要求的固定顺序）
    //    左上、右上、右下、左下（Y-up 标准 marker 坐标系）
    const float half = arucoSizeMm / 2.0f;
    std::vector<cv::Point3f> objectPoints = {
        cv::Point3f(-half,  half, 0.f),
        cv::Point3f( half,  half, 0.f),
        cv::Point3f( half, -half, 0.f),
        cv::Point3f(-half, -half, 0.f)
    };

    // 2. 调用 solvePnPGeneric 拿到 IPPE_SQUARE 的多解（通常 2 个）
    std::vector<cv::Mat> rvecs, tvecs;
    int solutions = 0;
    try {
        solutions = cv::solvePnPGeneric(
            objectPoints,
            cornerPixels,
            m_intrinsicMatrix,
            m_distCoeffs,
            rvecs,
            tvecs,
            false,                      // 不使用 extrinsicGuess
            cv::SOLVEPNP_IPPE_SQUARE
            );
    } catch (const cv::Exception &e) {
        arucoUpdateUI(QString("solvePnPGeneric 异常：%1").arg(e.what()));
        return pose;
    }

    if (solutions <= 0 || rvecs.empty() || tvecs.empty()) {
        arucoUpdateUI("PnP求解失败：无有效解");
        return pose;
    }

    cv::Vec3d bestRvec, bestTvec;
    double    bestErr   = std::numeric_limits<double>::max();
    double    bestR22   = -2.0;   // R(2,2) 取值范围 [-1,1]，初始给一个不可能值
    bool      foundPositiveZ = false;
    cv::Vec3d fallbackRvec, fallbackTvec;
    double    fallbackErr = std::numeric_limits<double>::max();
    for (size_t i = 0; i < rvecs.size(); ++i) {
        cv::Vec3d rv(rvecs[i].at<double>(0),
                     rvecs[i].at<double>(1),
                     rvecs[i].at<double>(2));
        cv::Vec3d tv(tvecs[i].at<double>(0),
                     tvecs[i].at<double>(1),
                     tvecs[i].at<double>(2));
        // 重投影误差
        std::vector<cv::Point2f> projected;
        cv::projectPoints(objectPoints, rv, tv,
                          m_intrinsicMatrix, m_distCoeffs, projected);
        double sumErr = 0.0;
        for (size_t k = 0; k < projected.size(); ++k) {
            double dx = projected[k].x - cornerPixels[k].x;
            double dy = projected[k].y - cornerPixels[k].y;
            sumErr += std::sqrt(dx * dx + dy * dy);
        }
        double meanErr = sumErr / static_cast<double>(projected.size());
        // 计算 marker 法向朝向相机的程度：旋转矩阵 R(2,2)
        cv::Mat Ri;
        cv::Rodrigues(rv, Ri);
        double r22 = Ri.at<double>(2, 2);   // 越接近 1，越正对相机
        // fallback：所有解里误差最小的
        if (meanErr < fallbackErr) {
            fallbackErr  = meanErr;
            fallbackRvec = rv;
            fallbackTvec = tv;
        }
        // 只考虑 Z>0 的物理合法解
        if (tv[2] <= 0) continue;
        // ★ 选解策略：
        //   1) 若 r22 明显更大（差 > 0.05，约 18°）→ 直接选它
        //   2) 若 r22 差不多 → 选 reprojErr 更小的
        bool takeIt = false;
        if (!foundPositiveZ) {
            takeIt = true;
        } else {
            double dR = r22 - bestR22;
            if (dR > 0.05) {
                takeIt = true;                          // 法向明显更朝向相机
            } else if (std::abs(dR) <= 0.05 && meanErr < bestErr) {
                takeIt = true;                          // 法向接近，比误差
            }
        }
        if (takeIt) {
            bestRvec = rv;
            bestTvec = tv;
            bestErr  = meanErr;
            bestR22  = r22;
            foundPositiveZ = true;
        }
    }

    if (foundPositiveZ) {
        pose.rvec              = bestRvec;
        pose.tvec              = bestTvec;
        pose.reprojectionError = bestErr;
    } else {
        pose.rvec              = fallbackRvec;
        pose.tvec              = fallbackTvec;
        pose.reprojectionError = fallbackErr;
    }

    // 4. 旋转向量 -> 旋转矩阵
    cv::Rodrigues(pose.rvec, pose.rotationMatrix);

    // 5. 旋转矩阵 -> 欧拉角（度）
    rotationVectorToEuler(pose.rvec, pose.roll, pose.pitch, pose.yaw);
    pose.yaw = angleDeg;

    pose.valid = true;
    return pose;
}

// =============================================================================
//   旋转向量 -> 欧拉角（内旋 X-Y-Z，等价于外旋 Z-Y-X），与 Python 版本保持一致
//   roll  = 绕 X 轴
//   pitch = 绕 Y 轴
//   yaw   = 绕 Z 轴
// =============================================================================
void ArucoDetector::rotationVectorToEuler(const cv::Vec3d &rvec,
                                          double &roll,
                                          double &pitch,
                                          double &yaw)
{
    cv::Mat R;
    cv::Rodrigues(rvec, R);

    double r00 = R.at<double>(0, 0);
    double r10 = R.at<double>(1, 0);
    double r20 = R.at<double>(2, 0);
    double r21 = R.at<double>(2, 1);
    double r22 = R.at<double>(2, 2);
    double r11 = R.at<double>(1, 1);
    double r12 = R.at<double>(1, 2);

    double sy = std::sqrt(r00 * r00 + r10 * r10);
    bool singular = sy < 1e-6;

    double x, y, z;
    if (!singular) {
        x = std::atan2(r21, r22);
        y = std::atan2(-r20, sy);
        z = std::atan2(r10, r00);
    } else {
        // gimbal lock：pitch ≈ ±90°
        x = std::atan2(-r12, r11);
        y = std::atan2(-r20, sy);
        z = 0.0;
    }

    // 弧度 -> 度
    roll  = x * 180.0 / M_PI;
    pitch = y * 180.0 / M_PI;
    yaw   = z * 180.0 / M_PI;
}

bool ArucoDetector::readCameraParamsFromJson(const QString &jsonPath)
{
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << QString("Cannot open file: %1").arg(jsonPath);
        arucoUpdateUI("无法打开文件");
        return false;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        arucoUpdateUI("JSON parsing failed");
        return false;
    }

    QJsonObject obj = doc.object();

    // 读取内参矩阵
    if (obj.contains("camera_matrix") && obj["camera_matrix"].isArray()) {
        QJsonArray arr = obj["camera_matrix"].toArray();
        m_intrinsicMatrix = cv::Mat::eye(3, 3, CV_64F);
        int rows = std::min(3, arr.size());
        for (int i = 0; i < rows; ++i) {
            QJsonArray row = arr[i].toArray();
            int cols = std::min(3, row.size());
            for (int j = 0; j < cols; ++j) {
                m_intrinsicMatrix.at<double>(i, j) = row[j].toDouble();
            }
        }
    } else {
        arucoUpdateUI("camera_matrix not found in JSON");
        return false;
    }

    // 读取畸变系数（二维数组形式 [[k1,k2,p1,p2,k3]]）
    if (obj.contains("distortion_coefficients") && obj["distortion_coefficients"].isArray()) {
        QJsonArray outerArr = obj["distortion_coefficients"].toArray();
        if (outerArr.size() > 0 && outerArr[0].isArray()) {
            QJsonArray innerArr = outerArr[0].toArray();
            m_distCoeffs = cv::Mat::zeros(innerArr.size(), 1, CV_64F);
            for (int i = 0; i < innerArr.size(); ++i) {
                m_distCoeffs.at<double>(i) = innerArr[i].toDouble();
            }
        } else {
            qDebug() << "distortion_coefficients format error (expected 2D array)";
            return false;
        }
    } else {
        qDebug() << "distortion_coefficients not found in JSON";
        return false;
    }

    // 读取旋转矩阵
    if (obj.contains("rotation_matrix") && obj["rotation_matrix"].isArray()) {
        QJsonArray arr = obj["rotation_matrix"].toArray();
        m_rotationMatrix = cv::Mat::eye(3, 3, CV_64F);
        int rows = std::min(3, arr.size());
        for (int i = 0; i < rows; ++i) {
            QJsonArray row = arr[i].toArray();
            int cols = std::min(3, row.size());
            for (int j = 0; j < cols; ++j) {
                m_rotationMatrix.at<double>(i, j) = row[j].toDouble();
            }
        }
    } else {
        qDebug() << "rotation_matrix not found in JSON";
        return false;
    }

    // 读取平移向量
    if (obj.contains("translation_vector") && obj["translation_vector"].isArray()) {
        QJsonArray arr = obj["translation_vector"].toArray();
        m_translationVector = cv::Mat::zeros(arr.size(), 1, CV_64F);
        for (int i = 0; i < arr.size(); ++i) {
            m_translationVector.at<double>(i) = arr[i].toDouble();
        }
    } else {
        qDebug() << "translation_vector not found in JSON";
        return false;
    }

    return true;
}

QString ArucoDetector::getCameraParamsFile(float z)
{
    if (-0.5 <= z && z <= 0.5) {
        return "vision/resource/calibration_results_opcv/camera_calibration_50.json";
    } else if(1699.5 <=z && z<= 1700.5) {
        return "vision/resource/calibration_results_opcv/camera_calibration_16.json";
    }
}

ArucoDetector::PlanarPose ArucoDetector::processImage(cv::Mat &image, float z)
{
    PlanarPose result{false, 0.0, 0.0, 0.0};
    std::vector<int> saveIds;
    std::vector<std::vector<cv::Point2f>> saveCorners;
    if (!image.empty()) {
        m_detector->detectMarkers(image, saveCorners, saveIds);
    }
    arucoUpdateUI(saveDetectionImage(image, z, saveCorners, saveIds));

    QString paramsFile = getCameraParamsFile(z);
    if (!readCameraParamsFromJson(paramsFile)) {
        arucoUpdateUI("读取相机参数错误");
        return result;
    }
    // 1. 检测 ArUco 码
    ArucoResult detection = detectOneArucoCode(image);
    if (!detection.detected) {
        arucoUpdateUI("检测二维码失败");
        return result;
    }
    // 2. PnP 解算
    PoseResult pose = solvePnPPose(detection.cornerPixels, 40.0f);
    if (!pose.valid) {
        arucoUpdateUI("PnP位姿计算失败");
        return result;
    }
    // 3. 打印位姿信息（保留原日志）
    QString info = QString("PnP成功 | 重投影误差: %1 px\n"
                           "tvec(mm): X=%2  Y=%3  Z=%4\n"
                           "Euler(deg): Roll=%5  Pitch=%6  Yaw=%7")
                       .arg(pose.reprojectionError, 0, 'f', 4)
                       .arg(pose.tvec[0], 0, 'f', 2)
                       .arg(pose.tvec[1], 0, 'f', 2)
                       .arg(pose.tvec[2], 0, 'f', 2)
                       .arg(pose.roll,  0, 'f', 2)
                       .arg(pose.pitch, 0, 'f', 2)
                       .arg(pose.yaw,   0, 'f', 2);
    arucoUpdateUI(info);
    // 4. 仅返回需要的字段
    //视觉检测的结果，相机坐标系下的
    //
    result.valid = true;
    result.yaw = pose.yaw;


    if(-0.5<=z && z <= 0.5){
        // pose.tvec[0] = pose.tvec[0]+7.25;
        // pose.tvec[1] = pose.tvec[1]+4.8;

        pose.tvec[0] = pose.tvec[0]-0.3;
        pose.tvec[1] = pose.tvec[1]+7.1;
    }else if (1699.5<=z && z <= 1700.5) {
        pose.tvec[0] = pose.tvec[0]+7.3;
        pose.tvec[1] = pose.tvec[1]+3.3;
    }


    float x_platform = -pose.tvec[1];
    float y_platform = pose.tvec[0];

    arucoUpdateUI(QString("平台坐标计算结果：x_platform=%1 mm，y_platform=%2 mm")
                      .arg(x_platform, 0, 'f', 2)
                      .arg(y_platform, 0, 'f', 2));

    //result.x是x方向的缝隙宽度，六自由度平台坐标系
    //result.y是y方向的缝隙宽度，六自由度平台坐标系
    if(-0.5<=z && z <= 0.5){
        result.x =(aruco_to_gapx + x_platform) - camera_to_lrux_50 + offset_x_50;
        result.y = camera_to_lruy_50 - (aruco_to_gapy + y_platform) + offset_y_50;


        // result.x =(aruco_to_gapx + x_platform) - camera_to_lrux_50;
        // result.y = camera_to_lruy_50 - (aruco_to_gapy + y_platform);

    }else if (1699.5<=z && z <= 1700.5) {
        result.x = (aruco_to_gapx + x_platform)-camera_to_lrux_16 + offset_x_16;
        result.y = camera_to_lruy_16 - (aruco_to_gapy + y_platform) + offset_y_16;
        // result.x = (aruco_to_gapx + x_platform)-camera_to_lrux_16 ;
        // result.y = camera_to_lruy_16 - (aruco_to_gapy + y_platform) ;
    }
    return result;
}

void ArucoDetector::onParamsReceived(const LRUInnerParams &params)
{
    aruco_to_gapx  = params.aruco_to_gapx;
    aruco_to_gapy  = params.aruco_to_gapy;
    camera_to_lrux_50 = params.camera_to_lrux_50;
    camera_to_lruy_50 = params.camera_to_lruy_50;
    camera_to_lrux_16 = params.camera_to_lrux_16;
    camera_to_lruy_16 = params.camera_to_lruy_16;
    offset_x_50 = params.offset_x_50;
    offset_y_50 = params.offset_y_50;
    offset_x_16 = params.offset_x_16;
    offset_y_16 = params.offset_y_16;
    marker_id = params.marker_id;
}




std::vector<cv::Point2f> ArucoDetector::sortCornersTLTRBRBL(const std::vector<cv::Point2f>& pts)
{
    std::vector<cv::Point2f> ordered(4);
    // 1. 找左上和右下：用 x + y
    int tlIndex = 0, brIndex = 0;
    float minSum = pts[0].x + pts[0].y;
    float maxSum = minSum;
    for (int i = 1; i < 4; ++i) {
        float sum = pts[i].x + pts[i].y;
        if (sum < minSum) {
            minSum = sum;
            tlIndex = i;
        }
        if (sum > maxSum) {
            maxSum = sum;
            brIndex = i;
        }
    }
    // 2. 找右上和左下：用 x - y
    int trIndex = 0, blIndex = 0;
    float minDiff = pts[0].x - pts[0].y;
    float maxDiff = minDiff;
    for (int i = 1; i < 4; ++i) {
        float diff = pts[i].x - pts[i].y;
        if (diff < minDiff) {
            minDiff = diff;
            blIndex = i;
        }
        if (diff > maxDiff) {
            maxDiff = diff;
            trIndex = i;
        }
    }
    ordered[0] = pts[tlIndex]; // 左上
    ordered[1] = pts[trIndex]; // 右上
    ordered[2] = pts[brIndex]; // 右下
    ordered[3] = pts[blIndex]; // 左下
    return ordered;
}

