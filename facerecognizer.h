#ifndef FACERECOGNIZER_H
#define FACERECOGNIZER_H

#include <opencv2/core.hpp> //提供cv::Mat,cv::Rect,cv::Point2f
#include <opencv2/objdetect.hpp>  //提供FaceDetectorYN,FacerecognierSF
#include <array>
#include <vector>

struct FaceDetection //人脸识别检测单元
{
    cv::Rect box;
    std::array<cv::Point2f, 5> landmarks;//5个人脸关键点：左眼，右眼，鼻尖，左嘴角，右嘴角
    float confidence = 0.0f;
    cv::Mat faceData; //存放10个人脸特征（5个关键点的2个维度）
};

class FaceRecognizer
{
public:
    FaceRecognizer(const std::string &detectModelPath,const std::string &recogModelPath);//需要两个模型的路径：YuNet检测模型，SFace识别模型
    ~FaceRecognizer();

    std::vector<FaceDetection> detectFaces(const cv::Mat &frame);

    cv::Mat extractFeature(const cv::Mat &frame, const FaceDetection &face, cv::Mat *alignedFaceOutput = nullptr);

    static float cosineSimilarity(const cv::Mat &feat1, const cv::Mat &feat2);

private:
    cv::Ptr<cv::FaceDetectorYN> detector;
    cv::Ptr<cv::FaceRecognizerSF> sface;
};

#endif