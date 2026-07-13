#include "personmanagerdialog.h"

#include "facedatabase.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

PersonManagerDialog::PersonManagerDialog(FaceDatabase *database,
                                         QWidget *parent)
    : QDialog(parent),
      database(database),
      searchEdit(new QLineEdit(this)),
      table(new QTableWidget(this))
{
    setWindowTitle(tr("人员数据库管理"));
    resize(760, 480);

    searchEdit->setPlaceholderText(tr("按编号、姓名或部门搜索"));

    table->setColumnCount(5);
    table->setHorizontalHeaderLabels(
        {tr("ID"), tr("人员编号"), tr("姓名"), tr("部门"), tr("人脸样本数")});
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    auto *addButton = new QPushButton(tr("新增人员"), this);
    auto *editButton = new QPushButton(tr("修改信息"), this);
    auto *deleteButton = new QPushButton(tr("删除人员"), this);
    auto *closeButton = new QPushButton(tr("关闭"), this);

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(editButton);
    buttonLayout->addWidget(deleteButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(new QLabel(tr("人员列表"), this));
    mainLayout->addWidget(searchEdit);
    mainLayout->addWidget(table);
    mainLayout->addLayout(buttonLayout);

    connect(searchEdit, &QLineEdit::textChanged,
            this, [this](const QString &text) { applyFilter(text); });
    connect(addButton, &QPushButton::clicked,
            this, [this] { addPerson(); });
    connect(editButton, &QPushButton::clicked,
            this, [this] { editSelectedPerson(); });
    connect(deleteButton, &QPushButton::clicked,
            this, [this] { deleteSelectedPerson(); });
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(table, &QTableWidget::cellDoubleClicked,
            this, [this](int, int) { editSelectedPerson(); });

    reloadPeople();
}

void PersonManagerDialog::reloadPeople()
{
    const QVector<PersonInfo> persons = database->getAllPersons();
    table->setRowCount(persons.size());

    for (int row = 0; row < persons.size(); ++row) {
        const PersonInfo &person = persons[row];
        table->setItem(row, 0, new QTableWidgetItem(QString::number(person.id)));
        table->setItem(row, 1, new QTableWidgetItem(person.personCode));
        table->setItem(row, 2, new QTableWidgetItem(person.name));
        table->setItem(row, 3, new QTableWidgetItem(person.department));
        table->setItem(row, 4,
                       new QTableWidgetItem(QString::number(person.embeddings.size())));
    }

    applyFilter(searchEdit->text());
}

void PersonManagerDialog::addPerson()
{
    QString personCode;
    QString name;
    QString department;
    if (!editPersonFields(tr("新增人员"), personCode, name, department)) {
        return;
    }

    if (!database->addPerson(personCode, name, department)) {
        QMessageBox::warning(this, tr("新增失败"), database->lastError());
        return;
    }
    reloadPeople();
}

void PersonManagerDialog::editSelectedPerson()
{
    const int row = table->currentRow();
    const int personId = selectedPersonId();
    if (row < 0 || personId < 0) {
        QMessageBox::information(this, tr("提示"), tr("请先选择一名人员。"));
        return;
    }

    QString personCode = table->item(row, 1)->text();
    QString name = table->item(row, 2)->text();
    QString department = table->item(row, 3)->text();
    if (!editPersonFields(tr("修改人员信息"), personCode, name, department)) {
        return;
    }

    if (!database->updatePerson(personId, personCode, name, department)) {
        QMessageBox::warning(this, tr("修改失败"), database->lastError());
        return;
    }
    reloadPeople();
}

void PersonManagerDialog::deleteSelectedPerson()
{
    const int row = table->currentRow();
    const int personId = selectedPersonId();
    if (row < 0 || personId < 0) {
        QMessageBox::information(this, tr("提示"), tr("请先选择一名人员。"));
        return;
    }

    const QString name = table->item(row, 2)->text();
    const auto answer = QMessageBox::question(
        this,
        tr("确认删除"),
        tr("确定删除“%1”吗？该人员的所有人脸样本也会被删除。").arg(name));
    if (answer != QMessageBox::Yes) {
        return;
    }

    if (!database->deletePerson(personId)) {
        QMessageBox::warning(this, tr("删除失败"), database->lastError());
        return;
    }
    reloadPeople();
}

void PersonManagerDialog::applyFilter(const QString &text)
{
    const QString keyword = text.trimmed();
    for (int row = 0; row < table->rowCount(); ++row) {
        bool matches = keyword.isEmpty();
        for (int column = 1; !matches && column <= 3; ++column) {
            const QTableWidgetItem *item = table->item(row, column);
            matches = item && item->text().contains(keyword, Qt::CaseInsensitive);
        }
        table->setRowHidden(row, !matches);
    }
}

int PersonManagerDialog::selectedPersonId() const
{
    const int row = table->currentRow();
    if (row < 0 || !table->item(row, 0)) {
        return -1;
    }
    return table->item(row, 0)->text().toInt();
}

bool PersonManagerDialog::editPersonFields(const QString &title,
                                           QString &personCode,
                                           QString &name,
                                           QString &department)
{
    QDialog dialog(this);
    dialog.setWindowTitle(title);

    auto *codeEdit = new QLineEdit(personCode, &dialog);
    auto *nameEdit = new QLineEdit(name, &dialog);
    auto *departmentEdit = new QLineEdit(department, &dialog);

    auto *formLayout = new QFormLayout;
    formLayout->addRow(tr("人员编号："), codeEdit);
    formLayout->addRow(tr("姓名："), nameEdit);
    formLayout->addRow(tr("部门："), departmentEdit);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    auto *layout = new QVBoxLayout(&dialog);
    layout->addLayout(formLayout);
    layout->addWidget(buttons);

    while (dialog.exec() == QDialog::Accepted) {
        if (!codeEdit->text().trimmed().isEmpty()
            && !nameEdit->text().trimmed().isEmpty()) {
            personCode = codeEdit->text().trimmed();
            name = nameEdit->text().trimmed();
            department = departmentEdit->text().trimmed();
            return true;
        }
        QMessageBox::information(&dialog, tr("信息不完整"),
                                 tr("人员编号和姓名不能为空。"));
    }
    return false;
}
