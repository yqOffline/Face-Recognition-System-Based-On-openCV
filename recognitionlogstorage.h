#ifndef RECOGNITIONLOGSTORAGE_H
#define RECOGNITIONLOGSTORAGE_H

#include <QString>
#include <opencv2/core.hpp>

namespace RecognitionLogStorage {

QString saveSnapshot(const cv::Mat &frame, const cv::Rect &faceBox,
                     QString *errorMessage = nullptr);
QString resolveStoredPath(const QString &storedPath);
bool deleteStoredSnapshot(const QString &storedPath);

}

#endif // RECOGNITIONLOGSTORAGE_H
