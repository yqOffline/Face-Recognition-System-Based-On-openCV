#include "facerecognizer.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace   //运用匿名命名空间，只对此文件可见，保存“宏”
{
    constexpr int MaxDetectionDimension = 1280;
    constexpr int CloseUpDetectionDimension = 640;
    constexpr float PrimaryScoreThreshold = 0.9f;
    constexpr float ProfileScoreThreshold = 0.85f;
}

FaceRecognizer::FaceRecognizer(const std::string &detectModelPath,const std::string &recogModelPath)
{
    detector = cv::FaceDetectorYN::create(detectModelPath,"",cv::Size(320, 320),0.9f, 0.3f, 5000);
    sface = cv::FaceRecognizerSF::create(recogModelPath, "");

    if (detector.empty())
    {
        throw std::runtime_error("无法创建 YuNet 人脸检测器。");
    }
    if (sface.empty())
    {
        throw std::runtime_error("无法加载 SFace 人脸识别模型。");
    }
}

FaceRecognizer::~FaceRecognizer() {}

//FaceDetection，人脸识别数据结构体
//detections：人脸数据结构体的vector
std::vector<FaceDetection> FaceRecognizer::detectFaces(const cv::Mat &frame)//人脸检测
{
    std::vector<FaceDetection> detections;
    if (frame.empty() || detector.empty())
    {
        return detections;
    }

    const int largestDimension = std::max(frame.cols, frame.rows);
    cv::Mat faces;
    float detectionScale = 1.0f; //如果后续尺寸过大，需要进行缩放

    struct DetectionPass
    {
        int maximumDimension;
        float scoreThreshold;
    };

    const std::array<DetectionPass, 3> passes
    {{
        {MaxDetectionDimension, PrimaryScoreThreshold}, //普通图片高置信度检测
        {CloseUpDetectionDimension, PrimaryScoreThreshold},  //打人脸缩小进入模型
        {MaxDetectionDimension, ProfileScoreThreshold}//校准人脸方向
    }};

    for (const DetectionPass &pass : passes)
    {
        detectionScale = std::min(1.0f,static_cast<float>(pass.maximumDimension)/ static_cast<float>(largestDimension));

        cv::Mat detectionFrame;
        if (detectionScale < 1.0f)
        {
            cv::resize(frame, detectionFrame, cv::Size(),detectionScale, detectionScale, cv::INTER_AREA);//INTER_AREA，一种缩放图片的方法
        }
        else
        {
            detectionFrame = frame;
        }

        detector->setInputSize(detectionFrame.size());//检测器输入尺寸，匹配尺寸
        detector->setScoreThreshold(pass.scoreThreshold);//本轮阈值
        detector->detect(detectionFrame, faces);//检测并输出faces,人脸数*15个float
        if (!faces.empty())
        {
            break;
        }
    }

    if (faces.empty())
    {
        return detections;
    }

    const cv::Rect imageBounds(0, 0, frame.cols, frame.rows);//恢复到原来大小，进入识别
    for (int row = 0; row < faces.rows; ++row)//有多少个人脸
    {
        cv::Mat originalFaceData = faces.row(row).clone();
        if (detectionScale != 1.0f)
        {
            float *coordinates = originalFaceData.ptr<float>(0);
            for (int column = 0; column < 14; ++column)
            {
                coordinates[column] /= detectionScale;//还原坐标
            }
        }
        const float *values = originalFaceData.ptr<float>(0);

        cv::Rect box(cvRound(values[0]),cvRound(values[1]),cvRound(values[2]), cvRound(values[3]));
        box &= imageBounds;//构造人脸矩形框,cvround将浮点数变为整数,并通过&=取交集，防止ROI裁剪越界
        if (box.empty())//交集为空，人脸无效
        {
            continue;
        }

        FaceDetection detection;
        detection.box = box;
        detection.confidence = values[14];
        for (int point = 0; point < 5; ++point)
        {
            detection.landmarks[point] = cv::Point2f(values[4 + point * 2],values[5 + point * 2]);
        }
        detection.faceData = originalFaceData;
        detections.push_back(std::move(detection));
    }

    return detections;
}

cv::Mat FaceRecognizer::extractFeature(const cv::Mat &frame,const FaceDetection &face, cv::Mat *alignedFaceOutput)//对齐并提取特征
{
    if (frame.empty() || face.faceData.empty() || sface.empty())
    {
        return {};
    }
    if (face.faceData.rows != 1 || face.faceData.cols != 15|| face.faceData.type() != CV_32F)
    {
        return {};
    }

    cv::Mat alignedFace;//保存对齐后的人脸
    sface->alignCrop(frame, face.faceData, alignedFace);
    if (alignedFace.empty())
    {
        return {};
    }
    if (alignedFaceOutput)
    {
        *alignedFaceOutput = alignedFace.clone();
    }

    cv::Mat feature;
    sface->feature(alignedFace, feature);
    if (feature.empty())
    {
        return {};
    }


    cv::Mat normalizedFeature;
    cv::normalize(feature, normalizedFeature, 1.0, 0.0, cv::NORM_L2);
    return normalizedFeature.clone();
}

float FaceRecognizer::cosineSimilarity(const cv::Mat &feat1, const cv::Mat &feat2)//余弦相似度比较
{
    if (feat1.empty() || feat2.empty()|| feat1.type() != CV_32F || feat2.type() != CV_32F|| feat1.total() != feat2.total())
    {
        return 0.0f;
    }

    double dot = feat1.dot(feat2);
    double norm1 = cv::norm(feat1);
    double norm2 = cv::norm(feat2);
    if (norm1 == 0 || norm2 == 0)return 0.0f;
    return static_cast<float>(dot / (norm1 * norm2));
}
