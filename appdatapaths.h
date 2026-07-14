#ifndef APPDATAPATHS_H
#define APPDATAPATHS_H

#include <QString>

namespace AppDataPaths {

// 可通过 FACEVISION_DATA_DIR 覆盖，便于自动化测试和便携式部署。
QString rootDirectory();
QString databasePath();
QString faceSamplesDirectory();
QString recognitionSnapshotsDirectory();
QString resolveStoredPath(const QString &storedPath);

// 首次升级时将旧版“程序目录/当前工作目录”中的数据复制到新目录。
void migrateLegacyData();

}

#endif // APPDATAPATHS_H
