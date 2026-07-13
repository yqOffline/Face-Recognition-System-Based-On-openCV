#ifndef FACERECOGNIZER_H
#define FACERECOGNIZER_H

#include <opencv2/dnn.hpp>
#include <opencv2/core.hpp>
#include <vector>

class FaceRecognizer
{
public:
    FaceRecognizer(const std::string &detectModelPath,
                   const std::string &recogModelPath);
    ~FaceRecognizer();

    // 检测人脸，返回每个人脸的矩形框
    std::vector<cv::Rect> detectFaces(const cv::Mat &frame);

    // 提取单个人脸的特征向量（128维）
    cv::Mat extractFeature(const cv::Mat &faceROI);

    // 计算两个特征向量的余弦相似度
    static float cosineSimilarity(const cv::Mat &feat1, const cv::Mat &feat2);

private:
    cv::dnn::Net detectNet;
    cv::dnn::Net recogNet;
    cv::Size inputSize;   // 检测模型输入尺寸
};

#endif // FACERECOGNIZER_H