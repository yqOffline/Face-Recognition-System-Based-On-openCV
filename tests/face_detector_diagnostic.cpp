#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>

#include <algorithm>
#include <iostream>
#include <vector>

int main(int argc, char *argv[])
{
    if (argc != 3) {
        std::cerr << "Usage: face_detector_diagnostic <model> <image>\n";
        return 2;
    }

    const cv::Mat original = cv::imread(argv[2]);
    if (original.empty()) {
        std::cerr << "Cannot read image.\n";
        return 3;
    }

    const std::vector<int> dimensions{1280, 960, 640, 480, 320};
    const std::vector<float> thresholds{0.9f, 0.8f, 0.7f, 0.6f};
    const std::vector<float> paddingRatios{0.0f, 0.2f, 0.4f};

    for (const float paddingRatio : paddingRatios) {
        for (const int targetDimension : dimensions) {
            const float scale = static_cast<float>(targetDimension)
                                / std::max(original.cols, original.rows);
            cv::Mat resized;
            cv::resize(original, resized, cv::Size(), scale, scale,
                       scale < 1.0f ? cv::INTER_AREA : cv::INTER_LINEAR);

            const int padX = cvRound(resized.cols * paddingRatio);
            const int padY = cvRound(resized.rows * paddingRatio);
            cv::Mat prepared;
            cv::copyMakeBorder(resized, prepared, padY, padY, padX, padX,
                               cv::BORDER_CONSTANT, cv::Scalar(127, 127, 127));

            for (const float threshold : thresholds) {
                cv::Ptr<cv::FaceDetectorYN> detector = cv::FaceDetectorYN::create(
                    argv[1], "", prepared.size(), threshold, 0.3f, 5000);
                cv::Mat faces;
                detector->detect(prepared, faces);
                if (!faces.empty()) {
                    float bestScore = 0.0f;
                    for (int row = 0; row < faces.rows; ++row) {
                        bestScore = std::max(bestScore, faces.at<float>(row, 14));
                    }
                    std::cout << "max=" << targetDimension
                              << " padding=" << paddingRatio
                              << " threshold=" << threshold
                              << " faces=" << faces.rows
                              << " best=" << bestScore << '\n';
                }
            }
        }
    }
    return 0;
}
