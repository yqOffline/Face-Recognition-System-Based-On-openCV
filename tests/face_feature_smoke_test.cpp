#include "facerecognizer.h"

#include <opencv2/imgcodecs.hpp>
#include <cmath>
#include <iostream>

int main(int argc, char *argv[])
{
    if (argc != 4) {
        std::cerr << "Usage: face_feature_smoke_test <detect-model> "
                     "<recognition-model> <image>\n";
        return 2;
    }

    try {
        const cv::Mat image = cv::imread(argv[3]);
        if (image.empty()) {
            std::cerr << "Failed to load test image.\n";
            return 3;
        }

        FaceRecognizer recognizer(argv[1], argv[2]);
        const std::vector<FaceDetection> faces = recognizer.detectFaces(image);
        if (faces.size() != 1) {
            std::cerr << "Expected exactly one face, got " << faces.size() << ".\n";
            return 1;
        }

        const cv::Mat feature = recognizer.extractFeature(image, faces.front());
        if (feature.empty()) {
            std::cerr << "Feature extraction returned an empty matrix.\n";
            return 4;
        }
        if (feature.rows != 1 || feature.cols != 128 || feature.type() != CV_32F) {
            std::cerr << "Unexpected feature format: rows=" << feature.rows
                      << ", cols=" << feature.cols
                      << ", type=" << feature.type() << ".\n";
            return 5;
        }

        const double length = cv::norm(feature, cv::NORM_L2);
        const float selfSimilarity = FaceRecognizer::cosineSimilarity(feature, feature);
        std::cout << "Feature shape: " << feature.rows << "x" << feature.cols << '\n'
                  << "Feature L2 norm: " << length << '\n'
                  << "Self cosine similarity: " << selfSimilarity << '\n';

        if (std::abs(length - 1.0) > 1e-4) {
            std::cerr << "Feature was not L2-normalized.\n";
            return 6;
        }
        if (std::abs(selfSimilarity - 1.0f) > 1e-4f) {
            std::cerr << "A feature should have cosine similarity 1 with itself.\n";
            return 7;
        }

        return 0;
    } catch (const cv::Exception &error) {
        std::cerr << "OpenCV error: " << error.what() << '\n';
        return 8;
    } catch (const std::exception &error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 9;
    }
}
