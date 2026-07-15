#include "appdatapaths.h"
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>

namespace
{
    QString cleanAbsolutePath(const QString &path)
    {
        return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    }

    bool copyDirectoryMissingFiles(const QString &sourcePath,const QString &targetPath)
    {
        if (!QDir(sourcePath).exists())
        {
            return true;
        }
        if (!QDir().mkpath(targetPath))
        {
            return false;
        }

        QDirIterator iterator(sourcePath, QDir::Files | QDir::NoDotAndDotDot,QDirIterator::Subdirectories);
        while (iterator.hasNext())
        {
            const QString sourceFile = iterator.next();
            const QString relativePath = QDir(sourcePath).relativeFilePath(sourceFile);
            const QString targetFile = QDir(targetPath).filePath(relativePath);
            if (!QDir().mkpath(QFileInfo(targetFile).absolutePath()))
            {
                return false;
            }
            if (!QFile::exists(targetFile) && !QFile::copy(sourceFile, targetFile))
            {
                return false;
            }
        }

        return true;
    }

}

QString AppDataPaths::rootDirectory()
{
    const QByteArray overridePath = qgetenv("FACEVISION_DATA_DIR");
    QString root;
    if (!overridePath.isEmpty())
    {
        root = QFile::decodeName(overridePath);
    }
    else
    {
        root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    }

    if (root.isEmpty())
    {
        root = QCoreApplication::applicationDirPath() + "/data";
    }
    root = cleanAbsolutePath(root);
    QDir().mkpath(root);
    return root;
}

QString AppDataPaths::databasePath()
{
    return QDir(rootDirectory()).filePath("faces.db");
}

QString AppDataPaths::faceSamplesDirectory()
{
    return QDir(rootDirectory()).filePath("face_samples");
}

QString AppDataPaths::recognitionSnapshotsDirectory()
{
    return QDir(rootDirectory()).filePath("recognition_snapshots");
}

QString AppDataPaths::resolveStoredPath(const QString &storedPath)
{
    if (storedPath.isEmpty())
    {
        return {};
    }
    if (QFileInfo(storedPath).isAbsolute())
    {
        return QDir::cleanPath(storedPath);
    }
    return QDir::cleanPath(QDir(rootDirectory()).filePath(storedPath));
}

void AppDataPaths::migrateLegacyData()
{
    const QString targetRoot = rootDirectory();
    QSet<QString> legacyRoots;
    legacyRoots.insert(cleanAbsolutePath(QDir::currentPath()));
    legacyRoots.insert(cleanAbsolutePath(QCoreApplication::applicationDirPath()));
    legacyRoots.remove(cleanAbsolutePath(targetRoot));

    for (const QString &legacyRoot : legacyRoots)
    {
        const QString legacyDatabase = QDir(legacyRoot).filePath("faces.db");
        const QString targetDatabase = databasePath();
        if (!QFile::exists(targetDatabase) && QFile::exists(legacyDatabase))
        {
            QFile::copy(legacyDatabase, targetDatabase);
        }

        copyDirectoryMissingFiles(QDir(legacyRoot).filePath("face_samples"),faceSamplesDirectory());
        copyDirectoryMissingFiles(QDir(legacyRoot).filePath("recognition_snapshots"),recognitionSnapshotsDirectory());
    }
}
