#ifndef FACEDATABASE_H
#define FACEDATABASE_H

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <opencv2/core.hpp>

struct PersonInfo
{
    int id = -1;
    QString personCode;
    QString name;
    QString department;
    QVector<cv::Mat> embeddings;
};

class FaceDatabase : public QObject
{
    Q_OBJECT

public:
    explicit FaceDatabase(QObject *parent = nullptr);
    ~FaceDatabase();

    bool init(const QString &dbPath = "faces.db");
    bool addPerson(const QString &personCode, const QString &name,
                   const QString &department, int *newPersonId = nullptr);
    bool updatePerson(int id, const QString &personCode, const QString &name,
                      const QString &department);
    bool deletePerson(int id);

    bool findPersonByCode(const QString &personCode, PersonInfo &person);
    bool addFaceSample(int personId, const cv::Mat &embedding);
    bool registerFaceSample(const QString &personCode, const QString &name,
                            const QString &department, const cv::Mat &embedding,
                            int *personId = nullptr,
                            bool *createdPerson = nullptr);

    QVector<PersonInfo> getAllPersons();
    QString lastError() const;

private:
    QSqlDatabase db;
    QString errorMessage;

    void setError(const QString &message);
    QByteArray matToBytes(const cv::Mat &mat) const;
    cv::Mat bytesToEmbedding(const QByteArray &data) const;
};

#endif // FACEDATABASE_H
