#include "personmanagerdialog.h"

#include "facedatabase.h"
#include "facerecognizer.h"
#include "facesamplestorage.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListView>
#include <QMessageBox>
#include <QPushButton>
#include <QPixmap>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

PersonManagerDialog::PersonManagerDialog(FaceDatabase *database,FaceRecognizer *recognizer,QWidget *parent)
    : QDialog(parent),
      database(database),
      recognizer(recognizer),
      searchEdit(new QLineEdit(this)),
      table(new QTableWidget(this)),
      sampleList(new QListWidget(this))
{
    setWindowTitle(tr("人员管理"));
    resize(960, 760);
    setMinimumSize(820, 660);

    searchEdit->setPlaceholderText(tr("搜索人员编号、姓名或部门"));
    searchEdit->setClearButtonEnabled(true);
    searchEdit->setMinimumHeight(44);

    table->setColumnCount(5);
    table->setHorizontalHeaderLabels({tr("数据库编号"), tr("人员编号"), tr("姓名"), tr("部门"), tr("人脸样本数")});
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    table->setMinimumHeight(260);

    sampleList->setViewMode(QListView::IconMode);
    sampleList->setIconSize(QSize(112, 112));
    sampleList->setResizeMode(QListView::Adjust);
    sampleList->setMovement(QListView::Static);
    sampleList->setSelectionMode(QAbstractItemView::SingleSelection);
    sampleList->setMinimumHeight(180);

    auto *addButton = new QPushButton(tr("新增人员"), this);
    auto *editButton = new QPushButton(tr("修改信息"), this);
    auto *deleteButton = new QPushButton(tr("删除人员"), this);
    auto *addSampleButton = new QPushButton(tr("增加人脸样本"), this);
    auto *deleteSampleButton = new QPushButton(tr("删除选中样本"), this);
    auto *closeButton = new QPushButton(tr("关闭"), this);

    addButton->setProperty("primary", true);
    deleteButton->setProperty("danger", true);
    deleteSampleButton->setProperty("danger", true);
    for (QPushButton *button : {addButton, editButton, deleteButton, addSampleButton, deleteSampleButton, closeButton}) {
        button->setMinimumHeight(40);
        button->setCursor(Qt::PointingHandCursor);
    }

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(editButton);
    buttonLayout->addWidget(deleteButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 22, 24, 22);
    mainLayout->setSpacing(11);
    auto *peopleTitle = new QLabel(tr("人员列表"), this);
    peopleTitle->setProperty("sectionTitle", true);
    mainLayout->addWidget(peopleTitle);
    mainLayout->addWidget(searchEdit);
    mainLayout->addWidget(table);
    mainLayout->addLayout(buttonLayout);
    auto *sampleTitle = new QLabel(tr("所选人员的人脸样本"), this);
    sampleTitle->setProperty("sectionTitle", true);
    mainLayout->addWidget(sampleTitle);
    mainLayout->addWidget(sampleList);

    auto *sampleButtonLayout = new QHBoxLayout;
    sampleButtonLayout->addWidget(addSampleButton);
    sampleButtonLayout->addWidget(deleteSampleButton);
    sampleButtonLayout->addStretch();
    mainLayout->addLayout(sampleButtonLayout);

    setStyleSheet(R"(
        QDialog {
            color: #172033;
            background: #F3F6FA;
            font-family: "Microsoft YaHei UI";
            font-size: 13px;
        }
        QLabel[sectionTitle="true"] {
            color: #20293A;
            font-size: 17px;
            font-weight: 650;
        }
        QLineEdit {
            color: #273247;
            background: white;
            border: 1px solid #DDE4EC;
            border-radius: 11px;
            padding: 0 13px;
            selection-background-color: #DCE9F7;
        }
        QLineEdit:hover { border-color: #AFC1D7; }
        QLineEdit:focus { border: 1px solid #5D83B2; }
        QTableWidget, QListWidget {
            color: #344054;
            background: white;
            alternate-background-color: #F8FAFC;
            border: 1px solid #E2E8F0;
            border-radius: 13px;
            outline: none;
        }
        QHeaderView::section {
            color: #5C6678;
            background: #F1F5F9;
            border: none;
            border-bottom: 1px solid #E2E8F0;
            padding: 10px 8px;
            font-weight: 600;
        }
        QTableWidget::item { padding: 8px; }
        QTableWidget::item:selected, QListWidget::item:selected {
            color: #163F70;
            background: #DCE9F7;
        }
        QListWidget::item { border-radius: 10px; padding: 7px; }
        QListWidget::item:hover { background: #EEF4FA; }
        QPushButton {
            color: #344054;
            background: white;
            border: 1px solid #DCE3EB;
            border-radius: 10px;
            padding: 0 16px;
            font-weight: 600;
        }
        QPushButton:hover {
            color: #1D4ED8;
            background: #EEF4FF;
            border-color: #AFC8FF;
        }
        QPushButton:pressed { background: #DCE8FF; }
        QPushButton[primary="true"] {
            color: white;
            background: #1E3A5F;
            border-color: #1E3A5F;
        }
        QPushButton[primary="true"]:hover {
            color: white;
            background: #28517F;
        }
        QPushButton[danger="true"] {
            color: #B54747;
            background: #FFF8F7;
            border-color: #F1D1CD;
        }
        QPushButton[danger="true"]:hover {
            color: #9E2F2F;
            background: #FDECE9;
            border-color: #EABAB4;
        }
    )");

    connect(searchEdit, &QLineEdit::textChanged, this, [this](const QString &text) { applyFilter(text); });
    connect(addButton, &QPushButton::clicked, this, [this] { addPerson(); });
    connect(editButton, &QPushButton::clicked,this, [this] { editSelectedPerson(); });
    connect(deleteButton, &QPushButton::clicked,this, [this] { deleteSelectedPerson(); });
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(table, &QTableWidget::cellDoubleClicked,this, [this](int, int) { editSelectedPerson(); });
    connect(table, &QTableWidget::itemSelectionChanged, this, [this] { loadSamplesForSelectedPerson(); });
    connect(addSampleButton, &QPushButton::clicked, this, [this] { addSamplesToSelectedPerson(); });
    connect(deleteSampleButton, &QPushButton::clicked,this, [this] { deleteSelectedSample(); });
    reloadPeople();
}

void PersonManagerDialog::reloadPeople()
{
    const QVector<PersonInfo> persons = database->getAllPersons();
    table->setRowCount(persons.size());

    for (int row = 0; row < persons.size(); ++row)
    {
        const PersonInfo &person = persons[row];
        table->setItem(row, 0, new QTableWidgetItem(QString::number(person.id)));
        table->setItem(row, 1, new QTableWidgetItem(person.personCode));
        table->setItem(row, 2, new QTableWidgetItem(person.name));
        table->setItem(row, 3, new QTableWidgetItem(person.department));
        table->setItem(row, 4,new QTableWidgetItem(QString::number(person.embeddings.size())));
    }

    applyFilter(searchEdit->text());
}

void PersonManagerDialog::addPerson()
{
    QString personCode;
    QString name;
    QString department;
    if (!editPersonFields(tr("新增人员"), personCode, name, department))
    {
        return;
    }

    if (!database->addPerson(personCode, name, department))
    {
        QMessageBox::warning(this, tr("新增失败"), database->lastError());
        return;
    }
    reloadPeople();
}

void PersonManagerDialog::editSelectedPerson()
{
    const int row = table->currentRow();
    const int personId = selectedPersonId();
    if (row < 0 || personId < 0)
    {
        QMessageBox::information(this, tr("提示"), tr("请先选择一名人员。"));
        return;
    }

    QString personCode = table->item(row, 1)->text();
    QString name = table->item(row, 2)->text();
    QString department = table->item(row, 3)->text();
    if (!editPersonFields(tr("修改人员信息"), personCode, name, department)) return ;

    if (!database->updatePerson(personId, personCode, name, department))
    {
        QMessageBox::warning(this, tr("修改失败"), database->lastError());
        return;
    }
    reloadPeople();
}

void PersonManagerDialog::deleteSelectedPerson()
{
    const int row = table->currentRow();
    const int personId = selectedPersonId();
    if (row < 0 || personId < 0)
    {
        QMessageBox::information(this, tr("提示"), tr("请先选择一名人员。"));
        return;
    }

    const QString name = table->item(row, 2)->text();
    const auto answer = QMessageBox::question(this,tr("确认删除"),tr("确定删除“%1”吗？该人员的所有人脸样本也会被删除。").arg(name));
    if (answer != QMessageBox::Yes) return ;

    if (!database->deletePerson(personId))
    {
        QMessageBox::warning(this, tr("删除失败"), database->lastError());
        return;
    }
    reloadPeople();
}

void PersonManagerDialog::applyFilter(const QString &text)
{
    const QString keyword = text.trimmed();
    for (int row = 0; row < table->rowCount(); ++row)
    {
        bool matches = keyword.isEmpty();
        for (int column = 1; !matches && column <= 3; ++column)
        {
            const QTableWidgetItem *item = table->item(row, column);
            matches = item && item->text().contains(keyword, Qt::CaseInsensitive);
        }
        table->setRowHidden(row, !matches);
    }
}

void PersonManagerDialog::loadSamplesForSelectedPerson()
{
    sampleList->clear();
    const int personId = selectedPersonId();
    if (personId < 0)
    {
        return;
    }

    const QVector<FaceSampleInfo> samples = database->getFaceSamples(personId);
    for (const FaceSampleInfo &sample : samples)
    {
        auto *item = new QListWidgetItem;
        item->setData(Qt::UserRole, sample.id);
        item->setData(Qt::UserRole + 1, sample.imagePath);
        item->setText(tr("样本 #%1\n%2").arg(sample.id).arg(sample.createdAt));

        const QString resolvedPath = FaceSampleStorage::resolveStoredPath(sample.imagePath);
        const QPixmap preview(resolvedPath);
        if (!preview.isNull())
        {
            item->setIcon(QIcon(preview));
            item->setToolTip(resolvedPath);
        }
        else
        {
            item->setText(tr("样本 #%1\n无预览").arg(sample.id));
            item->setToolTip(tr("这是升级前保存的特征，没有对应缩略图。"));
        }
        sampleList->addItem(item);
    }
}

void PersonManagerDialog::addSamplesToSelectedPerson()
{
    const int personId = selectedPersonId();
    if (personId < 0)
    {
        QMessageBox::information(this, tr("提示"), tr("请先选择一名人员。"));
        return;
    }

    const QStringList filePaths = QFileDialog::getOpenFileNames(this,tr("选择人脸样本图片"),tr("图片 (*.png *.jpg *.jpeg *.bmp)"));
    if (filePaths.isEmpty()) return ;

    int successCount = 0;
    QStringList failures;
    for (const QString &filePath : filePaths)
    {
        QString error;
        const cv::Mat image = FaceSampleStorage::loadImage(filePath, &error);
        if (image.empty())
        {
            failures << tr("%1：%2").arg(QFileInfo(filePath).fileName(), error);
            continue;
        }

        const std::vector<FaceDetection> faces = recognizer->detectFaces(image);
        if (faces.size() != 1) { failures << tr("%1：检测到%2张脸，要求恰好1张").arg(QFileInfo(filePath).fileName()).arg(faces.size());
            continue;
        }

        cv::Mat alignedFace;
        const cv::Mat feature = recognizer->extractFeature(image, faces.front(), &alignedFace);
        if (feature.empty() || alignedFace.empty())
        {
            failures << tr("%1：人脸对齐或特征提取失败").arg(QFileInfo(filePath).fileName());
            continue;
        }

        const QString storedPath = FaceSampleStorage::saveAlignedFace(alignedFace, &error);
        if (storedPath.isEmpty())
        {
            failures << tr("%1：%2").arg(QFileInfo(filePath).fileName(), error);
            continue;
        }
        if (!database->addFaceSample(personId, feature, storedPath))
        {
            FaceSampleStorage::deleteStoredImage(storedPath);
            failures << tr("%1：%2").arg(QFileInfo(filePath).fileName(), database->lastError());
            continue;
        }
        ++successCount;
    }

    reloadPeople();
    selectPersonById(personId);
    loadSamplesForSelectedPerson();

    QString summary = tr("成功增加%1个人脸样本。").arg(successCount);
    if (!failures.isEmpty())
    {
        summary += tr("\n\n以下图片未加入：\n%1").arg(failures.join("\n"));
    }
    QMessageBox::information(this, tr("样本导入结果"), summary);
}

void PersonManagerDialog::deleteSelectedSample()
{
    QListWidgetItem *item = sampleList->currentItem();
    if (!item)
    {
        QMessageBox::information(this, tr("提示"), tr("请先选择一张人脸样本。"));
        return;
    }

    const int sampleId = item->data(Qt::UserRole).toInt();
    const int personId = selectedPersonId();
    const auto answer = QMessageBox::question(this, tr("确认删除"),tr("确定删除样本 #%1 吗？删除后该特征将不再参与识别。").arg(sampleId));
    if (answer != QMessageBox::Yes) return ;
    if (!database->deleteFaceSample(sampleId))
    {
        QMessageBox::warning(this, tr("删除失败"), database->lastError());
        return;
    }

    reloadPeople();
    selectPersonById(personId);
    loadSamplesForSelectedPerson();
}

void PersonManagerDialog::selectPersonById(int personId)
{
    for (int row = 0; row < table->rowCount(); ++row)
    {
        const QTableWidgetItem *idItem = table->item(row, 0);
        if (idItem && idItem->text().toInt() == personId)
        {
            table->selectRow(row);
            return;
        }
    }
}

int PersonManagerDialog::selectedPersonId() const
{
    const int row = table->currentRow();
    if (row < 0 || !table->item(row, 0))
    {
        return -1;
    }
    return table->item(row, 0)->text().toInt();
}

bool PersonManagerDialog::editPersonFields(const QString &title,QString &personCode,QString &name,QString &department)
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

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("确定"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("取消"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    auto *layout = new QVBoxLayout(&dialog);
    layout->addLayout(formLayout);
    layout->addWidget(buttons);

    while (dialog.exec() == QDialog::Accepted)
    {
        if (!codeEdit->text().trimmed().isEmpty() && !nameEdit->text().trimmed().isEmpty())
        {
            personCode = codeEdit->text().trimmed();
            name = nameEdit->text().trimmed();
            department = departmentEdit->text().trimmed();
            return true;
        }
        QMessageBox::information(&dialog, tr("信息不完整"),tr("人员编号和姓名不能为空。"));
    }
    return false;
}
