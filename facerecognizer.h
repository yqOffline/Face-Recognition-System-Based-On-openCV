#ifndef FACERECOGNIZER_H
#define FACERECOGNIZER_H

#include <opencv2/core.hpp> //提供cv::Mat,cv::Rect,cv::Point2f
#include <opencv2/objdetect.hpp>  //提供FaceDetectorYN,FacerecognierSF
#include <array>
#include <vector>

struct FaceDetection  //人脸的完整检测结果
{
    cv::Rect box;  //人脸矩形框x,y,width,height
    std::array<cv::Point2f, 5> landmarks;//5个人脸关键点：左眼，右眼，鼻尖，左嘴角，右嘴角
    float confidence = 0.0f; //检测置信度（Yunet）
    cv::Mat faceData; //4个人脸框数据+10个关键点坐标+1个检测置信度=15个float
};

class FaceRecognizer  //人脸识别算法单元
{
    public:
        FaceRecognizer(const std::string &detectModelPath,const std::string &recogModelPath);//需要两个模型的路径：YuNet检测模型，SFace识别模型
        ~FaceRecognizer();

        std::vector<FaceDetection> detectFaces(const cv::Mat &frame);//检测人脸（有无，并确定位置）
        cv::Mat extractFeature(const cv::Mat &frame, const FaceDetection &face, cv::Mat *alignedFaceOutput = nullptr);//提取人脸特征（对齐）
        static float cosineSimilarity(const cv::Mat &feat1, const cv::Mat &feat2);//鱼线相似度比较

    private:
        cv::Ptr<cv::FaceDetectorYN> detector;//Yunet检测器
        cv::Ptr<cv::FaceRecognizerSF> sface; //SFace识别器，不用手动管理内存
};

#endif