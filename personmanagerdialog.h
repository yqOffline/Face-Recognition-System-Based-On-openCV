#ifndef PERSONMANAGERDIALOG_H
#define PERSONMANAGERDIALOG_H

#include <QDialog>

class FaceDatabase;
class FaceRecognizer;
class QLineEdit;
class QListWidget;
class QTableWidget;

class PersonManagerDialog : public QDialog
{
    public:
        explicit PersonManagerDialog(FaceDatabase *database,FaceRecognizer *recognizer,QWidget *parent = nullptr);

    private:
        void reloadPeople();
        void addPerson();
        void editSelectedPerson();
        void deleteSelectedPerson();
        void applyFilter(const QString &text);
        void loadSamplesForSelectedPerson();
        void addSamplesToSelectedPerson();
        void deleteSelectedSample();
        void selectPersonById(int personId);
        int selectedPersonId() const;
        bool editPersonFields(const QString &title,QString &personCode,QString &name, QString &department);

        FaceDatabase *database;
        FaceRecognizer *recognizer;
        QLineEdit *searchEdit;
        QTableWidget *table;
        QListWidget *sampleList;
};

#endif
