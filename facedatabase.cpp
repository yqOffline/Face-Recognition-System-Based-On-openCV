#include "facedatabase.h"
#include "facesamplestorage.h"
#include "recognitionlogstorage.h"

#include <QHash>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QtGlobal>
#include <cstring>

namespace {
constexpr int EmbeddingDimensions = 128;
constexpr auto ModelVersion = "sface_2021dec_aligned_v1";
}

FaceDatabase::FaceDatabase(QObject *parent)
    : QObject(parent)
{
}

FaceDatabase::~FaceDatabase()
{
    if (db.isOpen()) {
        db.close();
    }
}

bool FaceDatabase::init(const QString &dbPath)
{
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        setError(tr("无法打开数据库：%1").arg(db.lastError().text()));
        return false;
    }

    QSqlQuery query(db);
    if (!query.exec("PRAGMA foreign_keys = ON")) {
        setError(tr("无法启用数据库外键：%1").arg(query.lastError().text()));
        return false;
    }
    if (!query.exec(
            "CREATE TABLE IF NOT EXISTS people ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "person_code TEXT NOT NULL UNIQUE, "
            "name TEXT NOT NULL, "
            "department TEXT NOT NULL DEFAULT '', "
            "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, "
            "updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP)")) {
        setError(tr("创建人员表失败：%1").arg(query.lastError().text()));
        return false;
    }
    if (!query.exec(
            "CREATE TABLE IF NOT EXISTS face_samples ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "person_id INTEGER NOT NULL, "
            "embedding BLOB NOT NULL, "
            "image_path TEXT NOT NULL DEFAULT '', "
            "model_version TEXT NOT NULL, "
            "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, "
            "FOREIGN KEY(person_id) REFERENCES people(id) ON DELETE CASCADE)")) {
        setError(tr("创建人脸样本表失败：%1").arg(query.lastError().text()));
        return false;
    }

    // 已有数据库没有 image_path 字段时，进行不会丢失旧特征的增量升级。
    bool hasImagePath = false;
    if (!query.exec("PRAGMA table_info(face_samples)")) {
        setError(tr("读取样本表结构失败：%1").arg(query.lastError().text()));
        return false;
    }
    while (query.next()) {
        if (query.value(1).toString() == "image_path") {
            hasImagePath = true;
            break;
        }
    }
    if (!hasImagePath
        && !query.exec(
            "ALTER TABLE face_samples ADD COLUMN image_path TEXT NOT NULL DEFAULT ''")) {
        setError(tr("升级人脸样本表失败：%1").arg(query.lastError().text()));
        return false;
    }
    if (!query.exec(
            "CREATE INDEX IF NOT EXISTS idx_face_samples_person_id "
            "ON face_samples(person_id)")) {
        setError(tr("创建样本索引失败：%1").arg(query.lastError().text()));
        return false;
    }
    if (!query.exec(
            "CREATE TABLE IF NOT EXISTS recognition_logs ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "person_id INTEGER, "
            "name_snapshot TEXT NOT NULL, "
            "similarity REAL NOT NULL, "
            "recognized_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, "
            "snapshot_path TEXT NOT NULL DEFAULT '', "
            "source_type TEXT NOT NULL DEFAULT '', "
            "FOREIGN KEY(person_id) REFERENCES people(id) ON DELETE SET NULL)")) {
        setError(tr("Failed to create recognition log table: %1")
                     .arg(query.lastError().text()));
        return false;
    }
    if (!query.exec(
            "CREATE INDEX IF NOT EXISTS idx_recognition_logs_time "
            "ON recognition_logs(recognized_at DESC)")) {
        setError(tr("Failed to create recognition log index: %1")
                     .arg(query.lastError().text()));
        return false;
    }

    errorMessage.clear();
    return true;
}

bool FaceDatabase::addPerson(const QString &personCode, const QString &name,
                             const QString &department, int *newPersonId)
{
    const QString normalizedCode = personCode.trimmed();
    const QString normalizedName = name.trimmed();
    if (normalizedCode.isEmpty() || normalizedName.isEmpty()) {
        setError(tr("人员编号和姓名不能为空。"));
        return false;
    }

    QSqlQuery query(db);
    query.prepare(
        "INSERT INTO people(person_code, name, department) VALUES(?, ?, ?)");
    query.addBindValue(normalizedCode);
    query.addBindValue(normalizedName);
    query.addBindValue(department.trimmed());
    if (!query.exec()) {
        setError(tr("新增人员失败：%1").arg(query.lastError().text()));
        return false;
    }

    if (newPersonId) {
        *newPersonId = query.lastInsertId().toInt();
    }
    errorMessage.clear();
    return true;
}

bool FaceDatabase::updatePerson(int id, const QString &personCode,
                                const QString &name, const QString &department)
{
    const QString normalizedCode = personCode.trimmed();
    const QString normalizedName = name.trimmed();
    if (id < 0 || normalizedCode.isEmpty() || normalizedName.isEmpty()) {
        setError(tr("人员ID、编号和姓名不能为空。"));
        return false;
    }

    QSqlQuery query(db);
    query.prepare(
        "UPDATE people SET person_code = ?, name = ?, department = ?, "
        "updated_at = CURRENT_TIMESTAMP WHERE id = ?");
    query.addBindValue(normalizedCode);
    query.addBindValue(normalizedName);
    query.addBindValue(department.trimmed());
    query.addBindValue(id);
    if (!query.exec()) {
        setError(tr("修改人员失败：%1").arg(query.lastError().text()));
        return false;
    }
    if (query.numRowsAffected() != 1) {
        setError(tr("没有找到要修改的人员。"));
        return false;
    }

    errorMessage.clear();
    return true;
}

bool FaceDatabase::deletePerson(int id)
{
    const QVector<FaceSampleInfo> samples = getFaceSamples(id);
    QSqlQuery query(db);
    query.prepare("DELETE FROM people WHERE id = ?");
    query.addBindValue(id);
    if (!query.exec()) {
        setError(tr("删除人员失败：%1").arg(query.lastError().text()));
        return false;
    }
    if (query.numRowsAffected() != 1) {
        setError(tr("没有找到要删除的人员。"));
        return false;
    }

    for (const FaceSampleInfo &sample : samples) {
        FaceSampleStorage::deleteStoredImage(sample.imagePath);
    }

    errorMessage.clear();
    return true;
}

bool FaceDatabase::findPersonByCode(const QString &personCode,
                                    PersonInfo &person)
{
    QSqlQuery query(db);
    query.prepare(
        "SELECT id, person_code, name, department FROM people WHERE person_code = ?");
    query.addBindValue(personCode.trimmed());
    if (!query.exec()) {
        setError(tr("查询人员失败：%1").arg(query.lastError().text()));
        return false;
    }
    if (!query.next()) {
        person = PersonInfo{};
        errorMessage.clear();
        return false;
    }

    person.id = query.value(0).toInt();
    person.personCode = query.value(1).toString();
    person.name = query.value(2).toString();
    person.department = query.value(3).toString();
    errorMessage.clear();
    return true;
}

bool FaceDatabase::addFaceSample(int personId, const cv::Mat &embedding,
                                 const QString &imagePath, int *newSampleId)
{
    if (personId < 0 || embedding.empty() || embedding.type() != CV_32F
        || embedding.total() != EmbeddingDimensions) {
        setError(tr("人脸特征必须是有效的128维float向量。"));
        return false;
    }

    QSqlQuery query(db);
    query.prepare(
        "INSERT INTO face_samples(person_id, embedding, image_path, model_version) "
        "VALUES(?, ?, ?, ?)");
    query.addBindValue(personId);
    query.addBindValue(matToBytes(embedding));
    query.addBindValue(imagePath.isNull() ? QStringLiteral("") : imagePath);
    query.addBindValue(QString::fromLatin1(ModelVersion));
    if (!query.exec()) {
        setError(tr("保存人脸样本失败：%1").arg(query.lastError().text()));
        return false;
    }

    if (newSampleId) {
        *newSampleId = query.lastInsertId().toInt();
    }
    errorMessage.clear();
    return true;
}

bool FaceDatabase::deleteFaceSample(int sampleId)
{
    QSqlQuery lookup(db);
    lookup.prepare("SELECT image_path FROM face_samples WHERE id = ?");
    lookup.addBindValue(sampleId);
    if (!lookup.exec() || !lookup.next()) {
        setError(tr("没有找到要删除的人脸样本。"));
        return false;
    }
    const QString imagePath = lookup.value(0).toString();

    QSqlQuery query(db);
    query.prepare("DELETE FROM face_samples WHERE id = ?");
    query.addBindValue(sampleId);
    if (!query.exec() || query.numRowsAffected() != 1) {
        setError(tr("删除人脸样本失败：%1").arg(query.lastError().text()));
        return false;
    }

    FaceSampleStorage::deleteStoredImage(imagePath);
    errorMessage.clear();
    return true;
}

QVector<FaceSampleInfo> FaceDatabase::getFaceSamples(int personId)
{
    QVector<FaceSampleInfo> samples;
    QSqlQuery query(db);
    query.prepare(
        "SELECT id, person_id, image_path, created_at "
        "FROM face_samples WHERE person_id = ? ORDER BY id");
    query.addBindValue(personId);
    if (!query.exec()) {
        setError(tr("读取人脸样本失败：%1").arg(query.lastError().text()));
        return samples;
    }

    while (query.next()) {
        FaceSampleInfo sample;
        sample.id = query.value(0).toInt();
        sample.personId = query.value(1).toInt();
        sample.imagePath = query.value(2).toString();
        sample.createdAt = query.value(3).toString();
        samples.append(sample);
    }
    errorMessage.clear();
    return samples;
}

bool FaceDatabase::addRecognitionLog(int personId,
                                     const QString &nameSnapshot,
                                     float similarity,
                                     const QString &snapshotPath,
                                     const QString &sourceType,
                                     int *newLogId)
{
    if (personId < 0 || nameSnapshot.trimmed().isEmpty()) {
        setError(tr("Recognition log requires a valid person and name."));
        return false;
    }

    QSqlQuery query(db);
    query.prepare(
        "INSERT INTO recognition_logs(person_id, name_snapshot, similarity, "
        "snapshot_path, source_type, recognized_at) "
        "VALUES(?, ?, ?, ?, ?, datetime('now', 'localtime'))");
    query.addBindValue(personId);
    query.addBindValue(nameSnapshot.trimmed());
    query.addBindValue(similarity);
    query.addBindValue(snapshotPath);
    query.addBindValue(sourceType);
    if (!query.exec()) {
        setError(tr("Failed to save recognition log: %1")
                     .arg(query.lastError().text()));
        return false;
    }
    if (newLogId) {
        *newLogId = query.lastInsertId().toInt();
    }
    errorMessage.clear();
    return true;
}

QVector<RecognitionLogInfo> FaceDatabase::getRecentRecognitionLogs(int limit)
{
    QVector<RecognitionLogInfo> logs;
    QSqlQuery query(db);
    query.prepare(
        "SELECT id, person_id, name_snapshot, similarity, recognized_at, "
        "snapshot_path, source_type FROM recognition_logs "
        "ORDER BY id DESC LIMIT ?");
    query.addBindValue(qBound(1, limit, 1000));
    if (!query.exec()) {
        setError(tr("Failed to read recognition logs: %1")
                     .arg(query.lastError().text()));
        return logs;
    }

    while (query.next()) {
        RecognitionLogInfo log;
        log.id = query.value(0).toInt();
        log.personId = query.value(1).isNull() ? -1 : query.value(1).toInt();
        log.nameSnapshot = query.value(2).toString();
        log.similarity = query.value(3).toFloat();
        log.recognizedAt = query.value(4).toString();
        log.snapshotPath = query.value(5).toString();
        log.sourceType = query.value(6).toString();
        logs.append(log);
    }
    errorMessage.clear();
    return logs;
}

bool FaceDatabase::clearRecognitionLogs()
{
    QVector<QString> snapshotPaths;
    QSqlQuery lookup(db);
    if (!lookup.exec("SELECT snapshot_path FROM recognition_logs")) {
        setError(tr("Failed to read snapshots before clearing logs: %1")
                     .arg(lookup.lastError().text()));
        return false;
    }
    while (lookup.next()) {
        snapshotPaths.append(lookup.value(0).toString());
    }

    QSqlQuery query(db);
    if (!query.exec("DELETE FROM recognition_logs")) {
        setError(tr("Failed to clear recognition logs: %1")
                     .arg(query.lastError().text()));
        return false;
    }
    for (const QString &path : snapshotPaths) {
        RecognitionLogStorage::deleteStoredSnapshot(path);
    }
    errorMessage.clear();
    return true;
}

bool FaceDatabase::registerFaceSample(const QString &personCode,
                                      const QString &name,
                                      const QString &department,
                                      const cv::Mat &embedding,
                                      const QString &imagePath,
                                      int *personId,
                                      bool *createdPerson)
{
    if (!db.transaction()) {
        setError(tr("无法开始注册事务：%1").arg(db.lastError().text()));
        return false;
    }

    PersonInfo existing;
    bool created = false;
    int targetPersonId = -1;
    if (findPersonByCode(personCode, existing)) {
        targetPersonId = existing.id;
    } else {
        if (!errorMessage.isEmpty()
            || !addPerson(personCode, name, department, &targetPersonId)) {
            db.rollback();
            return false;
        }
        created = true;
    }

    if (!addFaceSample(targetPersonId, embedding, imagePath)) {
        db.rollback();
        return false;
    }
    if (!db.commit()) {
        setError(tr("提交注册数据失败：%1").arg(db.lastError().text()));
        db.rollback();
        return false;
    }

    if (personId) {
        *personId = targetPersonId;
    }
    if (createdPerson) {
        *createdPerson = created;
    }
    errorMessage.clear();
    return true;
}

QVector<PersonInfo> FaceDatabase::getAllPersons()
{
    QVector<PersonInfo> persons;
    QHash<int, int> personIndexes;

    QSqlQuery query(db);
    if (!query.exec(
            "SELECT p.id, p.person_code, p.name, p.department, fs.embedding "
            "FROM people p LEFT JOIN face_samples fs ON fs.person_id = p.id "
            "ORDER BY p.id, fs.id")) {
        setError(tr("读取人员列表失败：%1").arg(query.lastError().text()));
        return persons;
    }

    while (query.next()) {
        const int personId = query.value(0).toInt();
        int index = personIndexes.value(personId, -1);
        if (index < 0) {
            PersonInfo person;
            person.id = personId;
            person.personCode = query.value(1).toString();
            person.name = query.value(2).toString();
            person.department = query.value(3).toString();
            persons.append(person);
            index = persons.size() - 1;
            personIndexes.insert(personId, index);
        }

        const QByteArray bytes = query.value(4).toByteArray();
        if (!bytes.isEmpty()) {
            cv::Mat embedding = bytesToEmbedding(bytes);
            if (!embedding.empty()) {
                persons[index].embeddings.append(embedding);
            }
        }
    }

    errorMessage.clear();
    return persons;
}

QString FaceDatabase::lastError() const
{
    return errorMessage;
}

void FaceDatabase::setError(const QString &message)
{
    errorMessage = message;
}

QByteArray FaceDatabase::matToBytes(const cv::Mat &mat) const
{
    const cv::Mat continuous = mat.isContinuous() ? mat : mat.clone();
    return QByteArray(reinterpret_cast<const char *>(continuous.data),
                      static_cast<qsizetype>(continuous.total()
                                             * continuous.elemSize()));
}

cv::Mat FaceDatabase::bytesToEmbedding(const QByteArray &data) const
{
    cv::Mat embedding(1, EmbeddingDimensions, CV_32F);
    const qsizetype expectedSize = static_cast<qsizetype>(
        embedding.total() * embedding.elemSize());
    if (data.size() != expectedSize) {
        return {};
    }

    std::memcpy(embedding.data, data.constData(),
                static_cast<std::size_t>(data.size()));
    return embedding;
}
