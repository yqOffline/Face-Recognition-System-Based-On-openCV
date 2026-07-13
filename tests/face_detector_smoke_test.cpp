#include "facerecognizer.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>

int main(int argc, char *argv[])
{
    if (argc != 4 && argc != 5) {
        std::cerr << "Usage: face_detector_smoke_test <detect-model> "
                     "<recognition-model> <image> [visualization-output]\n";
        return 2;
    }

    try {
        const cv::Mat image = cv::imread(argv[3]);
        if (image.empty()) {
            std::cerr << "Failed to load test image: " << argv[3] << '\n';
            return 3;
        }

        FaceRecognizer recognizer(argv[1], argv[2]);
        const std::vector<FaceDetection> faces = recognizer.detectFaces(image);

        cv::Mat visualized = image.clone();
        std::cout << "Detected face count: " << faces.size() << '\n';
        for (std::size_t index = 0; index < faces.size(); ++index) {
            const FaceDetection &face = faces[index];
            std::cout << "Face " << index
                      << ": box=(" << face.box.x << ", " << face.box.y
                      << ", " << face.box.width << ", " << face.box.height
                      << "), confidence=" << face.confidence << '\n';

            cv::rectangle(visualized, face.box, cv::Scalar(0, 255, 0), 2);
            for (const cv::Point2f &landmark : face.landmarks) {
                cv::circle(visualized, landmark, 2, cv::Scalar(0, 0, 255), -1);
            }
        }

        if (faces.size() != 1) {
            std::cerr << "Expected exactly one face in the regression image.\n";
            return 1;
        }
        if (argc == 5 && !cv::imwrite(argv[4], visualized)) {
            std::cerr << "Failed to save the visualized detection result.\n";
            return 6;
        }
        return 0;
    } catch (const cv::Exception &error) {
        std::cerr << "OpenCV error: " << error.what() << '\n';
        return 4;
    } catch (const std::exception &error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 5;
    }
}
