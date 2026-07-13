#include "facedatabase.h"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <cmath>
#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) {
        std::cerr << "Failed to create temporary test directory.\n";
        return 1;
    }

    FaceDatabase database;
    if (!database.init(temporaryDirectory.filePath("test_faces.db"))) {
        std::cerr << database.lastError().toStdString() << '\n';
        return 2;
    }

    cv::Mat firstFeature = cv::Mat::ones(1, 128, CV_32F);
    cv::normalize(firstFeature, firstFeature, 1.0, 0.0, cv::NORM_L2);
    cv::Mat secondFeature = cv::Mat::ones(1, 128, CV_32F) * 2.0f;
    cv::normalize(secondFeature, secondFeature, 1.0, 0.0, cv::NORM_L2);

    int personId = -1;
    bool created = false;
    if (!database.registerFaceSample("S001", "Alice", "Computer Science",
                                     firstFeature, &personId, &created)
        || !created || personId < 0) {
        std::cerr << "Failed to create person and first sample: "
                  << database.lastError().toStdString() << '\n';
        return 3;
    }

    if (!database.registerFaceSample("S001", "Ignored", "Ignored",
                                     secondFeature, nullptr, &created)
        || created) {
        std::cerr << "Failed to append second sample.\n";
        return 4;
    }

    QVector<PersonInfo> persons = database.getAllPersons();
    if (persons.size() != 1 || persons.front().embeddings.size() != 2) {
        std::cerr << "Expected one person with two samples.\n";
        return 5;
    }

    if (!database.updatePerson(personId, "S001", "Alice Zhang", "AI Lab")) {
        std::cerr << database.lastError().toStdString() << '\n';
        return 6;
    }
    persons = database.getAllPersons();
    if (persons.front().name != "Alice Zhang"
        || persons.front().department != "AI Lab") {
        std::cerr << "Updated person information was not persisted.\n";
        return 7;
    }

    if (!database.deletePerson(personId) || !database.getAllPersons().isEmpty()) {
        std::cerr << "Person deletion or cascading sample deletion failed.\n";
        return 8;
    }

    std::cout << "Database CRUD and multi-sample test passed.\n";
    return 0;
}
