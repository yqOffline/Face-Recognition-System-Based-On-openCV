#ifndef APPDATAPATHS_H
#define APPDATAPATHS_H

#include <QString>

namespace AppDataPaths
{
    QString rootDirectory();
    QString databasePath();
    QString faceSamplesDirectory();
    QString recognitionSnapshotsDirectory();
    QString resolveStoredPath(const QString &storedPath);
    void migrateLegacyData();
}

#endif