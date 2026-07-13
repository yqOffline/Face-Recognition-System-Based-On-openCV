#ifndef FACEDATABASE_H
#define FACEDATABASE_H

#include <QObject>
#include <QSqlDatabase>
#include <QVector>
#include <opencv2/core.hpp>

struct PersonInfo {
    int id;
    QString name;
    cv::Mat embedding;  // 128维特征向量
};

class FaceDatabase : public QObject
{
    Q_OBJECT
public:
    explicit FaceDatabase(QObject *parent = nullptr);
    ~FaceDatabase();

    bool init(const QString &dbPath = "faces.db");
    bool addPerson(const QString &name, const cv::Mat &embedding);
    bool deletePerson(int id);
    QVector<PersonInfo> getAllPersons();

private:
    QSqlDatabase db;
    QByteArray matToBytes(const cv::Mat &mat);
    cv::Mat bytesToMat(const QByteArray &data, int rows, int cols, int type);
};

#endif // FACEDATABASE_H