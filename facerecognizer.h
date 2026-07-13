#ifndef FACERECOGNIZER_H
#define FACERECOGNIZER_H

#include <opencv2/core.hpp>
#include <opencv2/objdetect.hpp>
#include <array>
#include <vector>

// YuNet 对一张人脸的完整检测结果。
// 除了矩形框，SFace 在后续进行人脸对齐时还需要 5 个关键点。
struct FaceDetection
{
    cv::Rect box;
    std::array<cv::Point2f, 5> landmarks;
    float confidence = 0.0f;
    cv::Mat faceData; // OpenCV FaceDetectorYN 返回的 1x15、CV_32F 数据
};

class FaceRecognizer
{
public:
    FaceRecognizer(const std::string &detectModelPath,
                   const std::string &recogModelPath);
    ~FaceRecognizer();

    // 检测人脸，返回矩形框、5 个关键点、置信度和原始检测数据。
    std::vector<FaceDetection> detectFaces(const cv::Mat &frame);

    // 根据 YuNet 的 5 个关键点对齐人脸，再提取 128 维 SFace 特征。
    cv::Mat extractFeature(const cv::Mat &frame, const FaceDetection &face,
                           cv::Mat *alignedFaceOutput = nullptr);

    // 计算两个特征向量的余弦相似度
    static float cosineSimilarity(const cv::Mat &feat1, const cv::Mat &feat2);

private:
    cv::Ptr<cv::FaceDetectorYN> detector;
    cv::Ptr<cv::FaceRecognizerSF> sface;
};

#endif // FACERECOGNIZER_H
