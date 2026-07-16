#include "facedatabase.h"
#include "facesamplestorage.h"
#include "recognitionlogstorage.h"
#include "appdatapaths.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <cmath>
#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid())
    {
        std::cerr << "Failed to create temporary test directory.\n";
        return 1;
    }
    qputenv("FACEVISION_DATA_DIR",QFile::encodeName(temporaryDirectory.filePath("appdata")));

    FaceDatabase database;
    if (!database.init(temporaryDirectory.filePath("test_faces.db")))
    {
        std::cerr << database.lastError().toStdString() << '\n';
        return 2;
    }

    cv::Mat firstFeature = cv::Mat::ones(1, 128, CV_32F);
    cv::normalize(firstFeature, firstFeature, 1.0, 0.0, cv::NORM_L2);
    cv::Mat secondFeature = cv::Mat::ones(1, 128, CV_32F) * 2.0f;
    cv::normalize(secondFeature, secondFeature, 1.0, 0.0, cv::NORM_L2);
    const cv::Mat previewImage(112, 112, CV_8UC3, cv::Scalar(80, 120, 160));
    QString storageError;
    const QString previewPath = FaceSampleStorage::saveAlignedFace(previewImage, &storageError);
    if (previewPath.isEmpty())
    {
        std::cerr << storageError.toStdString() << '\n';
        return 10;
    }

    int personId = -1;
    bool created = false;
    if (!database.registerFaceSample("S001", "Alice", "Computer Science",firstFeature, previewPath, &personId, &created)|| !created || personId < 0) {
        std::cerr << "Failed to create person and first sample: " << database.lastError().toStdString() << '\n';
        return 3;
    }

    if (!database.registerFaceSample("S001", "Ignored", "Ignored",secondFeature, QString(), nullptr, &created)|| created)
    {
        std::cerr << "Failed to append second sample.\n";
        return 4;
    }

    QVector<PersonInfo> persons = database.getAllPersons();
    if (persons.size() != 1 || persons.front().embeddings.size() != 2)
    {
        std::cerr << "Expected one person with two samples.\n";
        return 5;
    }
    if (database.getFaceSamples(personId).size() != 2)
    {
        std::cerr << "Expected two manageable face sample records.\n";
        return 9;
    }

    if (!database.updatePerson(personId, "S001", "Alice Zhang", "AI Lab"))
    {
        std::cerr << database.lastError().toStdString() << '\n';
        return 6;
    }
    persons = database.getAllPersons();
    if (persons.front().name != "Alice Zhang"|| persons.front().department != "AI Lab")
    {
        std::cerr << "Updated person information was not persisted.\n";
        return 7;
    }

    const QString snapshotPath = RecognitionLogStorage::saveSnapshot(previewImage, cv::Rect(20, 20, 60, 60), &storageError);
    if (snapshotPath.isEmpty()|| !database.addRecognitionLog(personId, "Alice Zhang", 0.82f, snapshotPath, "Camera")) {
        std::cerr << "Failed to save recognition history: " << database.lastError().toStdString() << '\n';
        return 12;
    }
    QVector<RecognitionLogInfo> logs = database.getRecentRecognitionLogs();
    if (logs.size() != 1 || logs.front().personId != personId|| logs.front().nameSnapshot != "Alice Zhang"|| std::abs(logs.front().similarity - 0.82f) > 0.001f)
    {
        std::cerr << "Recognition history was not read back correctly.\n";
        return 13;
    }

    if (!database.deletePerson(personId) || !database.getAllPersons().isEmpty())
    {
        std::cerr << "Person deletion or cascading sample deletion failed.\n";
        return 8;
    }
    if (QFile::exists(FaceSampleStorage::resolveStoredPath(previewPath)))
    {
        std::cerr << "Stored preview image was not deleted with its person.\n";
        return 11;
    }

    logs = database.getRecentRecognitionLogs();
    if (logs.size() != 1 || logs.front().personId != -1|| logs.front().nameSnapshot != "Alice Zhang")
    {
        std::cerr << "Recognition history should survive person deletion.\n";
        return 14;
    }
    if (!database.clearRecognitionLogs() || !database.getRecentRecognitionLogs().isEmpty())
    {
        std::cerr << "Failed to clear recognition history.\n";
        return 15;
    }
    if (QFile::exists(RecognitionLogStorage::resolveStoredPath(snapshotPath)))
    {
        std::cerr << "Snapshot was not deleted with recognition history.\n";
        return 16;
    }

    std::cout << "Database, samples, and recognition log tests passed.\n";
    return 0;
}
