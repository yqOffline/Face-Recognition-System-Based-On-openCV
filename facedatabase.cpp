#include "facedatabase.h"

#include <QHash>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
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
            "model_version TEXT NOT NULL, "
            "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, "
            "FOREIGN KEY(person_id) REFERENCES people(id) ON DELETE CASCADE)")) {
        setError(tr("创建人脸样本表失败：%1").arg(query.lastError().text()));
        return false;
    }
    if (!query.exec(
            "CREATE INDEX IF NOT EXISTS idx_face_samples_person_id "
            "ON face_samples(person_id)")) {
        setError(tr("创建样本索引失败：%1").arg(query.lastError().text()));
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

bool FaceDatabase::addFaceSample(int personId, const cv::Mat &embedding)
{
    if (personId < 0 || embedding.empty() || embedding.type() != CV_32F
        || embedding.total() != EmbeddingDimensions) {
        setError(tr("人脸特征必须是有效的128维float向量。"));
        return false;
    }

    QSqlQuery query(db);
    query.prepare(
        "INSERT INTO face_samples(person_id, embedding, model_version) "
        "VALUES(?, ?, ?)");
    query.addBindValue(personId);
    query.addBindValue(matToBytes(embedding));
    query.addBindValue(QString::fromLatin1(ModelVersion));
    if (!query.exec()) {
        setError(tr("保存人脸样本失败：%1").arg(query.lastError().text()));
        return false;
    }

    errorMessage.clear();
    return true;
}

bool FaceDatabase::registerFaceSample(const QString &personCode,
                                      const QString &name,
                                      const QString &department,
                                      const cv::Mat &embedding,
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

    if (!addFaceSample(targetPersonId, embedding)) {
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
