#include "facesamplestorage.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>
#include <opencv2/imgcodecs.hpp>
#include <vector>

namespace {

QString storageDirectory()
{
    return QCoreApplication::applicationDirPath() + "/face_samples";
}

}

cv::Mat FaceSampleStorage::loadImage(const QString &filePath,
                                     QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("无法读取图片：%1").arg(filePath);
        }
        return {};
    }

    const QByteArray bytes = file.readAll();
    const auto *begin = reinterpret_cast<const unsigned char *>(bytes.constData());
    const std::vector<unsigned char> encoded(begin, begin + bytes.size());
    cv::Mat image = cv::imdecode(encoded, cv::IMREAD_COLOR);
    if (image.empty() && errorMessage) {
        *errorMessage = QObject::tr("图片格式无法解析：%1").arg(filePath);
    }
    return image;
}

QString FaceSampleStorage::saveAlignedFace(const cv::Mat &alignedFace,
                                           QString *errorMessage)
{
    if (alignedFace.empty()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("没有可保存的对齐人脸。 ");
        }
        return {};
    }

    QDir directory;
    const QString targetDirectory = storageDirectory();
    if (!directory.mkpath(targetDirectory)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("无法创建人脸样本目录：%1")
                                .arg(targetDirectory);
        }
        return {};
    }

    const QString fileName = QString("sample_%1.jpg")
                                 .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QString filePath = QDir(targetDirectory).filePath(fileName);
    if (!cv::imwrite(filePath.toLocal8Bit().constData(), alignedFace,
                     {cv::IMWRITE_JPEG_QUALITY, 95})) {
        if (errorMessage) {
            *errorMessage = QObject::tr("无法保存人脸样本图片：%1").arg(filePath);
        }
        return {};
    }
    return QString("face_samples/%1").arg(fileName);
}

QString FaceSampleStorage::resolveStoredPath(const QString &storedPath)
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

bool FaceSampleStorage::deleteStoredImage(const QString &filePath)
{
    if (filePath.isEmpty()) {
        return true;
    }

    const QString root = QDir::fromNativeSeparators(
                             QDir::cleanPath(storageDirectory())) + "/";
    const QString target = QDir::fromNativeSeparators(
        resolveStoredPath(filePath));
    if (!target.startsWith(root, Qt::CaseInsensitive)) {
        return false;
    }
    return !QFile::exists(target) || QFile::remove(target);
}
