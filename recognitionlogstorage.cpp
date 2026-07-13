#include "recognitionlogstorage.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>
#include <opencv2/imgcodecs.hpp>

namespace {

QString storageDirectory()
{
    return QCoreApplication::applicationDirPath() + "/recognition_snapshots";
}

}

QString RecognitionLogStorage::saveSnapshot(const cv::Mat &frame,
                                             const cv::Rect &faceBox,
                                             QString *errorMessage)
{
    if (frame.empty() || faceBox.empty()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("No valid recognition snapshot is available.");
        }
        return {};
    }

    // Keep some surrounding context instead of saving only a tightly cropped face.
    const int horizontalPadding = faceBox.width / 4;
    const int verticalPadding = faceBox.height / 3;
    const cv::Rect expanded(faceBox.x - horizontalPadding,
                            faceBox.y - verticalPadding,
                            faceBox.width + horizontalPadding * 2,
                            faceBox.height + verticalPadding * 2);
    const cv::Rect safeBox = expanded & cv::Rect(0, 0, frame.cols, frame.rows);
    if (safeBox.empty()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("The recognized face is outside the image.");
        }
        return {};
    }

    QDir directory;
    const QString targetDirectory = storageDirectory();
    if (!directory.mkpath(targetDirectory)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Cannot create snapshot directory: %1")
                                .arg(targetDirectory);
        }
        return {};
    }

    const QString fileName = QString("log_%1.jpg")
                                 .arg(QUuid::createUuid().toString(
                                     QUuid::WithoutBraces));
    const QString absolutePath = QDir(targetDirectory).filePath(fileName);
    if (!cv::imwrite(absolutePath.toLocal8Bit().constData(), frame(safeBox),
                     {cv::IMWRITE_JPEG_QUALITY, 90})) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Cannot save recognition snapshot: %1")
                                .arg(absolutePath);
        }
        return {};
    }
    return QString("recognition_snapshots/%1").arg(fileName);
}

QString RecognitionLogStorage::resolveStoredPath(const QString &storedPath)
{
    if (storedPath.isEmpty()) {
        return {};
    }
    const QFileInfo info(storedPath);
    if (info.isAbsolute()) {
        return QDir::cleanPath(storedPath);
    }
    return QDir::cleanPath(
        QDir(QCoreApplication::applicationDirPath()).filePath(storedPath));
}

bool RecognitionLogStorage::deleteStoredSnapshot(const QString &storedPath)
{
    if (storedPath.isEmpty()) {
        return true;
    }

    const QString root = QDir::fromNativeSeparators(
                             QDir::cleanPath(storageDirectory())) + "/";
    const QString target = QDir::fromNativeSeparators(
        resolveStoredPath(storedPath));
    if (!target.startsWith(root, Qt::CaseInsensitive)) {
        return false;
    }
    return !QFile::exists(target) || QFile::remove(target);
}
