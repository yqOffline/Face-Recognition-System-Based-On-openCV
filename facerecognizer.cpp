#include "facerecognizer.h"
#include <opencv2/imgproc.hpp>

FaceRecognizer::FaceRecognizer(const std::string &detectModelPath,
                               const std::string &recogModelPath)
{
    detectNet = cv::dnn::readNet(detectModelPath);
    recogNet  = cv::dnn::readNet(recogModelPath);

    // YuNet 模型要求的输入尺寸
    inputSize = cv::Size(320, 320);
}

FaceRecognizer::~FaceRecognizer() {}

std::vector<cv::Rect> FaceRecognizer::detectFaces(const cv::Mat &frame)
{
    std::vector<cv::Rect> faces;
    cv::Mat blob = cv::dnn::blobFromImage(frame, 1.0/255.0, inputSize,
                                          cv::Scalar(), true, false);
    detectNet.setInput(blob);
    cv::Mat output = detectNet.forward();  // shape: [1, N, 15]

    int rows = output.size[1];
    for (int i = 0; i < rows; ++i) {
        float confidence = output.ptr<float>(0)[i * 15 + 14];
        if (confidence > 0.5f) {   // 置信度阈值
            int x1 = static_cast<int>(output.ptr<float>(0)[i * 15 + 0] * frame.cols);
            int y1 = static_cast<int>(output.ptr<float>(0)[i * 15 + 1] * frame.rows);
            int x2 = static_cast<int>(output.ptr<float>(0)[i * 15 + 2] * frame.cols);
            int y2 = static_cast<int>(output.ptr<float>(0)[i * 15 + 3] * frame.rows);
            faces.push_back(cv::Rect(cv::Point(x1, y1), cv::Point(x2, y2)));
        }
    }
    return faces;
}

cv::Mat FaceRecognizer::extractFeature(const cv::Mat &faceROI)
{
    // SFace 要求输入 112x112 的 RGB 人脸图像
    cv::Mat resized;
    cv::resize(faceROI, resized, cv::Size(112, 112));
    cv::Mat blob = cv::dnn::blobFromImage(resized, 1.0/255.0, cv::Size(112,112),
                                          cv::Scalar(), true, false);
    recogNet.setInput(blob);
    cv::Mat feature = recogNet.forward();  // 输出 1x128
    return feature.clone(); // 返回一份拷贝
}

float FaceRecognizer::cosineSimilarity(const cv::Mat &feat1, const cv::Mat &feat2)
{
    double dot = feat1.dot(feat2);
    double norm1 = cv::norm(feat1);
    double norm2 = cv::norm(feat2);
    if (norm1 == 0 || norm2 == 0)
        return 0.0f;
    return static_cast<float>(dot / (norm1 * norm2));
}