#ifndef FACESAMPLESTORAGE_H
#define FACESAMPLESTORAGE_H

#include <QString>
#include <opencv2/core.hpp>

namespace FaceSampleStorage
{
    cv::Mat loadImage(const QString &filePath, QString *errorMessage = nullptr);
    QString saveAlignedFace(const cv::Mat &alignedFace,QString *errorMessage = nullptr);
    QString resolveStoredPath(const QString &storedPath);
    bool deleteStoredImage(const QString &filePath);
}

#endif
