#include "facedatabase.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QFile>

FaceDatabase::FaceDatabase(QObject *parent) : QObject(parent) {}

FaceDatabase::~FaceDatabase()
{
    if (db.isOpen())
        db.close();
}

bool FaceDatabase::init(const QString &dbPath)
{
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        qWarning() << "数据库打开失败：" << db.lastError().text();
        return false;
    }
    QSqlQuery query;
    bool success = query.exec(
        "CREATE TABLE IF NOT EXISTS persons ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "name TEXT NOT NULL, "
        "embedding BLOB NOT NULL, "
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP)");
    if (!success)
        qWarning() << "建表失败：" << query.lastError().text();
    return success;
}

bool FaceDatabase::addPerson(const QString &name, const cv::Mat &embedding)
{
    QSqlQuery query;
    query.prepare("INSERT INTO persons (name, embedding) VALUES (?, ?)");
    query.addBindValue(name);
    query.addBindValue(matToBytes(embedding));
    if (!query.exec()) {
        qWarning() << "插入失败：" << query.lastError().text();
        return false;
    }
    return true;
}

bool FaceDatabase::deletePerson(int id)
{
    QSqlQuery query;
    query.prepare("DELETE FROM persons WHERE id = ?");
    query.addBindValue(id);
    return query.exec();
}

QVector<PersonInfo> FaceDatabase::getAllPersons()
{
    QVector<PersonInfo> persons;
    QSqlQuery query("SELECT id, name, embedding FROM persons");
    while (query.next()) {
        PersonInfo p;
        p.id = query.value(0).toInt();
        p.name = query.value(1).toString();
        QByteArray data = query.value(2).toByteArray();
        // 128维，float32，一行
        p.embedding = bytesToMat(data, 1, 128, CV_32F);
        persons.append(p);
    }
    return persons;
}

QByteArray FaceDatabase::matToBytes(const cv::Mat &mat)
{
    QByteArray bytes;
    if (mat.isContinuous()) {
        bytes = QByteArray(reinterpret_cast<const char*>(mat.data), mat.total() * mat.elemSize());
    }
    return bytes;
}

cv::Mat FaceDatabase::bytesToMat(const QByteArray &data, int rows, int cols, int type)
{
    cv::Mat mat(rows, cols, type);
    if (data.size() == mat.total() * mat.elemSize()) {
        memcpy(mat.data, data.constData(), data.size());
    }
    return mat;
}