#include "recognitionlogstorage.h"
#include "appdatapaths.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QUuid>
#include <opencv2/imgcodecs.hpp>
#include <vector>

namespace
{

    QString storageDirectory()
    {
        return AppDataPaths::recognitionSnapshotsDirectory();
    }

}

QString RecognitionLogStorage::saveSnapshot(const cv::Mat &frame,const cv::Rect &faceBox, QString *errorMessage)
{
    if (frame.empty() || faceBox.empty())
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("没有可保存的有效识别抓拍。");
        }
        return {};
    }

    // Keep some surrounding context instead of saving only a tightly cropped face.
    const int horizontalPadding = faceBox.width / 4;
    const int verticalPadding = faceBox.height / 3;
    const cv::Rect expanded(faceBox.x - horizontalPadding,faceBox.y - verticalPadding,faceBox.width + horizontalPadding * 2,faceBox.height + verticalPadding * 2);
    const cv::Rect safeBox = expanded & cv::Rect(0, 0, frame.cols, frame.rows);
    if (safeBox.empty())
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("识别到的人脸区域超出图片范围。");
        }
        return {};
    }

    QDir directory;
    const QString targetDirectory = storageDirectory();
    if (!directory.mkpath(targetDirectory))
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("无法创建识别抓拍目录：%1").arg(targetDirectory);
        }
        return {};
    }

    const QString fileName = QString("log_%1.jpg").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QString absolutePath = QDir(targetDirectory).filePath(fileName);
    std::vector<unsigned char> encoded;
    if (!cv::imencode(".jpg", frame(safeBox), encoded,{cv::IMWRITE_JPEG_QUALITY, 90})) {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("无法保存识别抓拍：%1").arg(absolutePath);
        }
        return {};
    }
    QSaveFile output(absolutePath);
    if (!output.open(QIODevice::WriteOnly)|| output.write(reinterpret_cast<const char *>(encoded.data()),static_cast<qint64>(encoded.size()))!= static_cast<qint64>(encoded.size()) || !output.commit()) {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("无法写入识别抓拍：%1").arg(absolutePath);
        }
        return {};
    }
    return QString("recognition_snapshots/%1").arg(fileName);
}

QString RecognitionLogStorage::resolveStoredPath(const QString &storedPath)
{
    if (storedPath.isEmpty())
    {
        return {};
    }
    const QFileInfo info(storedPath);
    if (info.isAbsolute())
    {
        return QDir::cleanPath(storedPath);
    }
    return AppDataPaths::resolveStoredPath(storedPath);
}

bool RecognitionLogStorage::deleteStoredSnapshot(const QString &storedPath)
{
    if (storedPath.isEmpty())
    {
        return true;
    }

    const QString root = QDir::fromNativeSeparators(QDir::cleanPath(storageDirectory())) + "/";
    const QString target = QDir::fromNativeSeparators( resolveStoredPath(storedPath));
    if (!target.startsWith(root, Qt::CaseInsensitive))
    {
        return false;
    }
    return !QFile::exists(target) || QFile::remove(target);
}
