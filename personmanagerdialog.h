#ifndef PERSONMANAGERDIALOG_H
#define PERSONMANAGERDIALOG_H

#include <QDialog>

class FaceDatabase;
class QLineEdit;
class QTableWidget;

class PersonManagerDialog : public QDialog
{
public:
    explicit PersonManagerDialog(FaceDatabase *database,
                                 QWidget *parent = nullptr);

private:
    void reloadPeople();
    void addPerson();
    void editSelectedPerson();
    void deleteSelectedPerson();
    void applyFilter(const QString &text);
    int selectedPersonId() const;
    bool editPersonFields(const QString &title,
                          QString &personCode,
                          QString &name,
                          QString &department);

    FaceDatabase *database;
    QLineEdit *searchEdit;
    QTableWidget *table;
};

#endif // PERSONMANAGERDIALOG_H
