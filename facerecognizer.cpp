#include "facerecognizer.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace {
constexpr int MaxDetectionDimension = 1280;
}

FaceRecognizer::FaceRecognizer(const std::string &detectModelPath,
                               const std::string &recogModelPath)
{
    detector = cv::FaceDetectorYN::create(
        detectModelPath,
        "",
        cv::Size(320, 320),
        0.9f,  // 检测置信度阈值
        0.3f,  // 非极大值抑制阈值
        5000);
    sface = cv::FaceRecognizerSF::create(recogModelPath, "");

    if (detector.empty()) {
        throw std::runtime_error("Failed to create YuNet face detector.");
    }
    if (sface.empty()) {
        throw std::runtime_error("Failed to load SFace recognition model.");
    }
}

FaceRecognizer::~FaceRecognizer() {}

std::vector<FaceDetection> FaceRecognizer::detectFaces(const cv::Mat &frame)
{
    std::vector<FaceDetection> detections;
    if (frame.empty() || detector.empty()) {
        return detections;
    }

    // 超大照片直接送入 YuNet 不仅计算量大，也可能超出模型适合的尺度。
    // 检测时等比例缩小，稍后再把坐标映射回原始高清图。
    cv::Mat detectionFrame = frame;
    float detectionScale = 1.0f;
    const int largestDimension = std::max(frame.cols, frame.rows);
    if (largestDimension > MaxDetectionDimension) {
        detectionScale = static_cast<float>(MaxDetectionDimension)
                         / static_cast<float>(largestDimension);
        cv::resize(frame, detectionFrame, cv::Size(),
                   detectionScale, detectionScale, cv::INTER_AREA);
    }

    detector->setInputSize(detectionFrame.size());

    cv::Mat faces;
    detector->detect(detectionFrame, faces);
    if (faces.empty()) {
        return detections;
    }

    const cv::Rect imageBounds(0, 0, frame.cols, frame.rows);
    for (int row = 0; row < faces.rows; ++row) {
        cv::Mat originalFaceData = faces.row(row).clone();
        if (detectionScale != 1.0f) {
            float *coordinates = originalFaceData.ptr<float>(0);
            for (int column = 0; column < 14; ++column) {
                coordinates[column] /= detectionScale;
            }
        }
        const float *values = originalFaceData.ptr<float>(0);

        cv::Rect box(
            cvRound(values[0]),
            cvRound(values[1]),
            cvRound(values[2]),
            cvRound(values[3]));
        box &= imageBounds;
        if (box.empty()) {
            continue;
        }

        FaceDetection detection;
        detection.box = box;
        detection.confidence = values[14];
        for (int point = 0; point < 5; ++point) {
            detection.landmarks[point] = cv::Point2f(
                values[4 + point * 2],
                values[5 + point * 2]);
        }
        detection.faceData = originalFaceData;
        detections.push_back(std::move(detection));
    }

    return detections;
}

cv::Mat FaceRecognizer::extractFeature(const cv::Mat &frame,
                                       const FaceDetection &face)
{
    if (frame.empty() || face.faceData.empty() || sface.empty()) {
        return {};
    }
    if (face.faceData.rows != 1 || face.faceData.cols != 15
        || face.faceData.type() != CV_32F) {
        return {};
    }

    // alignCrop 根据双眼、鼻尖和嘴角把脸旋转、缩放到标准姿态。
    cv::Mat alignedFace;
    sface->alignCrop(frame, face.faceData, alignedFace);
    if (alignedFace.empty()) {
        return {};
    }

    cv::Mat feature;
    sface->feature(alignedFace, feature);
    if (feature.empty()) {
        return {};
    }

    // 归一化后向量长度为 1，后续余弦相似度比较更稳定。
    cv::Mat normalizedFeature;
    cv::normalize(feature, normalizedFeature, 1.0, 0.0, cv::NORM_L2);
    return normalizedFeature.clone();
}

float FaceRecognizer::cosineSimilarity(const cv::Mat &feat1, const cv::Mat &feat2)
{
    if (feat1.empty() || feat2.empty()
        || feat1.type() != CV_32F || feat2.type() != CV_32F
        || feat1.total() != feat2.total()) {
        return 0.0f;
    }

    double dot = feat1.dot(feat2);
    double norm1 = cv::norm(feat1);
    double norm2 = cv::norm(feat2);
    if (norm1 == 0 || norm2 == 0)
        return 0.0f;
    return static_cast<float>(dot / (norm1 * norm2));
}
