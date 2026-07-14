#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "facesamplestorage.h"
#include "recognitionlogstorage.h"
#include "personmanagerdialog.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QDateTime>
#include <QDebug>
#include <QInputDialog>
#include <QCoreApplication>
#include <QApplication>
#include <QDialog>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSizePolicy>
#include <QStatusBar>
#include <QVBoxLayout>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setupModernUi();

    QString modelDir = QCoreApplication::applicationDirPath() + "/models/";
    recognizer = new FaceRecognizer(
        (modelDir + "face_detection_yunet_2023mar.onnx").toStdString(),
        (modelDir + "face_recognition_sface_2021dec.onnx").toStdString());
    database = new FaceDatabase(this);
    if (!database->init("faces.db")) {
        QMessageBox::critical(this, tr("数据库错误"), database->lastError());
    }
    refreshPersonCache();
    loadRecognitionLogs();

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::processFrame);

    connect(ui->btnStartStop, &QPushButton::clicked, this, &MainWindow::onStartStop);
    connect(ui->btnLoadImage, &QPushButton::clicked, this, &MainWindow::onLoadImage);
    connect(ui->btnRegister, &QPushButton::clicked, this, &MainWindow::onRegister);
    connect(ui->btnManageDB, &QPushButton::clicked, this, &MainWindow::onManageDB);
    connect(ui->btnClearLogs, &QPushButton::clicked, this, &MainWindow::onClearLogs);
    connect(ui->logList, &QListWidget::itemDoubleClicked,
            this, &MainWindow::onLogDoubleClicked);
}

MainWindow::~MainWindow()
{
    if (cap.isOpened()) cap.release();
    delete recognizer;
    delete ui;
}

void MainWindow::onStartStop()
{
    if (!isCameraRunning) {
        ui->btnStartStop->setEnabled(false);
        ui->btnStartStop->setText(tr("正在连接..."));
        statusBar()->showMessage(tr("正在连接摄像头，请稍候..."));
        QApplication::setOverrideCursor(Qt::WaitCursor);
        // 先把“正在连接”绘制出来，再执行可能耗时的摄像头初始化。
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

        QElapsedTimer openTimer;
        openTimer.start();

        // Windows 下明确指定后端，避免 CAP_ANY 逐个探测后端造成额外等待。
        // DirectShow 通常打开较快；不可用时回退到 Media Foundation。
        bool opened = cap.open(0, cv::CAP_DSHOW);
        if (!opened) {
            cap.release();
            opened = cap.open(0, cv::CAP_MSMF);
        }

        QApplication::restoreOverrideCursor();
        ui->btnStartStop->setEnabled(true);
        if (!cap.isOpened()) {
            ui->btnStartStop->setText(tr("启动摄像头"));
            statusBar()->showMessage(tr("摄像头连接失败"), 5000);
            QMessageBox::critical(this, tr("摄像头错误"),
                                  tr("无法打开摄像头，请检查设备是否被其他程序占用。"));
            return;
        }

        // 640×480 足以进行实时人脸识别，同时比高分辨率处理更快。
        cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
        cap.set(cv::CAP_PROP_FPS, 30);
        cap.set(cv::CAP_PROP_BUFFERSIZE, 1);
        qDebug() << "Camera opened in" << openTimer.elapsed() << "ms, backend:"
                 << QString::fromStdString(cap.getBackendName());

        isImageMode = false;
        isCameraRunning = true;
        timer->start(33);
        ui->btnStartStop->setText(tr("停止摄像头"));
        statusBar()->showMessage(
            tr("摄像头运行中 · 启动耗时 %1 ms").arg(openTimer.elapsed()), 5000);
    } else {
        timer->stop();
        cap.release();
        isCameraRunning = false;
        identitiesInPreviousFrame.clear();
        ui->btnStartStop->setText(tr("启动摄像头"));
        ui->videoLabel->setText(tr("等待摄像头或图片输入"));
        ui->videoLabel->setAlignment(Qt::AlignCenter);
        statusBar()->showMessage(tr("摄像头已停止"), 3000);
    }
}

void MainWindow::onLoadImage()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Chose picture", "", "Images (*.png *.jpg *.bmp)");
    if (fileName.isEmpty()) return;
    QString imageError;
    cv::Mat img = FaceSampleStorage::loadImage(fileName, &imageError);
    if (img.empty()) {
        QMessageBox::warning(this, tr("图片错误"), imageError);
        return;
    }

    // 静态图片和摄像头共用同一个显示区域。载入图片时先停止摄像头，
    // 防止下一次定时器触发后摄像头画面立刻覆盖刚载入的图片。
    if (isCameraRunning) {
        timer->stop();
        cap.release();
        isCameraRunning = false;
        ui->btnStartStop->setText(tr("启动摄像头"));
    }
    currentFrame = img.clone();
    identitiesInPreviousFrame.clear();
    isImageMode = true;
    processFrame();
    statusBar()->showMessage(tr("已加载图片：%1").arg(fileName), 5000);
}

void MainWindow::setupModernUi()
{
    setWindowTitle(tr("FaceVision · 智能人脸识别系统"));
    resize(1180, 760);
    setMinimumSize(1020, 680);
    ui->menubar->hide();

    auto addShadow = [](QWidget *widget, int blurRadius = 30) {
        auto *shadow = new QGraphicsDropShadowEffect(widget);
        shadow->setBlurRadius(blurRadius);
        shadow->setOffset(0, 7);
        shadow->setColor(QColor(15, 23, 42, 28));
        widget->setGraphicsEffect(shadow);
    };

    auto *rootLayout = new QVBoxLayout(ui->centralwidget);
    rootLayout->setContentsMargins(28, 22, 28, 20);
    rootLayout->setSpacing(18);

    auto *headerLayout = new QHBoxLayout;
    auto *titleLayout = new QVBoxLayout;
    titleLayout->setSpacing(2);
    ui->label->setText(tr("FaceVision"));
    ui->label->setObjectName("pageTitle");
    auto *subtitle = new QLabel(
        tr("智能人脸识别 · 人员管理 · 识别记录"), ui->centralwidget);
    subtitle->setObjectName("pageSubtitle");
    titleLayout->addWidget(ui->label);
    titleLayout->addWidget(subtitle);
    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();
    auto *systemState = new QLabel(tr("●  SYSTEM READY"), ui->centralwidget);
    systemState->setObjectName("systemState");
    headerLayout->addWidget(systemState, 0, Qt::AlignVCenter);
    rootLayout->addLayout(headerLayout);

    auto *contentLayout = new QHBoxLayout;
    contentLayout->setSpacing(18);

    auto *videoCard = new QFrame(ui->centralwidget);
    videoCard->setProperty("card", true);
    auto *videoLayout = new QVBoxLayout(videoCard);
    videoLayout->setContentsMargins(18, 16, 18, 18);
    videoLayout->setSpacing(12);
    auto *previewTitle = new QLabel(tr("实时预览"), videoCard);
    previewTitle->setProperty("sectionTitle", true);
    auto *previewHint = new QLabel(
        tr("支持摄像头实时识别与本地静态图片分析"), videoCard);
    previewHint->setProperty("sectionHint", true);
    videoLayout->addWidget(previewTitle);
    videoLayout->addWidget(previewHint);
    ui->videoLabel->setObjectName("videoLabel");
    ui->videoLabel->setText(tr("等待摄像头或图片输入"));
    ui->videoLabel->setAlignment(Qt::AlignCenter);
    ui->videoLabel->setMinimumSize(500, 430);
    ui->videoLabel->setSizePolicy(QSizePolicy::Expanding,
                                  QSizePolicy::Expanding);
    videoLayout->addWidget(ui->videoLabel, 1);
    addShadow(videoCard);
    contentLayout->addWidget(videoCard, 6);

    auto *actionCard = new QFrame(ui->centralwidget);
    actionCard->setProperty("card", true);
    actionCard->setMinimumWidth(190);
    actionCard->setMaximumWidth(220);
    auto *actionLayout = new QVBoxLayout(actionCard);
    actionLayout->setContentsMargins(16, 18, 16, 18);
    actionLayout->setSpacing(12);
    auto *actionTitle = new QLabel(tr("快捷操作"), actionCard);
    actionTitle->setProperty("sectionTitle", true);
    auto *actionHint = new QLabel(tr("选择一种输入或管理数据"), actionCard);
    actionHint->setProperty("sectionHint", true);
    actionHint->setWordWrap(true);
    actionLayout->addWidget(actionTitle);
    actionLayout->addWidget(actionHint);
    actionLayout->addSpacing(12);
    ui->btnStartStop->setText(tr("启动摄像头"));
    ui->btnStartStop->setProperty("primary", true);
    ui->btnLoadImage->setText(tr("导入图片"));
    ui->btnRegister->setText(tr("注册人脸"));
    ui->btnManageDB->setText(tr("人员管理"));
    for (QPushButton *button : {ui->btnStartStop, ui->btnLoadImage,
                                ui->btnRegister, ui->btnManageDB}) {
        button->setMinimumHeight(52);
        button->setCursor(Qt::PointingHandCursor);
        actionLayout->addWidget(button);
    }
    actionLayout->addStretch();
    auto *tip = new QLabel(
        tr("提示：载入图片后可直接注册，双击记录可查看抓拍。"), actionCard);
    tip->setObjectName("actionTip");
    tip->setWordWrap(true);
    actionLayout->addWidget(tip);
    addShadow(actionCard);
    contentLayout->addWidget(actionCard, 2);

    auto *logCard = new QFrame(ui->centralwidget);
    logCard->setProperty("card", true);
    logCard->setMinimumWidth(250);
    logCard->setMaximumWidth(300);
    auto *logLayout = new QVBoxLayout(logCard);
    logLayout->setContentsMargins(16, 18, 16, 18);
    logLayout->setSpacing(12);
    ui->label_2->setText(tr("识别记录"));
    ui->label_2->setProperty("sectionTitle", true);
    auto *logHint = new QLabel(tr("最近 200 条识别事件"), logCard);
    logHint->setProperty("sectionHint", true);
    logLayout->addWidget(ui->label_2);
    logLayout->addWidget(logHint);
    ui->logList->setSpacing(5);
    ui->logList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    logLayout->addWidget(ui->logList, 1);
    ui->btnClearLogs->setText(tr("清空记录"));
    ui->btnClearLogs->setProperty("danger", true);
    ui->btnClearLogs->setMinimumHeight(42);
    ui->btnClearLogs->setCursor(Qt::PointingHandCursor);
    logLayout->addWidget(ui->btnClearLogs);
    addShadow(logCard);
    contentLayout->addWidget(logCard, 3);

    rootLayout->addLayout(contentLayout, 1);

    setStyleSheet(R"(
        QWidget {
            font-family: "Microsoft YaHei UI";
        }
        QMainWindow, QWidget#centralwidget {
            background: #F3F6FA;
            color: #172033;
            font-size: 13px;
        }
        QLabel#pageTitle {
            color: #111827;
            font-size: 27px;
            font-weight: 700;
        }
        QLabel#pageSubtitle, QLabel[sectionHint="true"] {
            color: #7A8496;
            font-size: 12px;
        }
        QLabel#systemState {
            color: #168A66;
            background: #E5F7F0;
            border: 1px solid #C9EDDF;
            border-radius: 15px;
            padding: 6px 12px;
            font-size: 11px;
            font-weight: 600;
        }
        QFrame[card="true"] {
            background: #FFFFFF;
            border: 1px solid #E7EBF1;
            border-radius: 18px;
        }
        QLabel[sectionTitle="true"] {
            color: #20293A;
            font-size: 16px;
            font-weight: 650;
        }
        QLabel#videoLabel {
            color: #8E9AAF;
            background: #101827;
            border: 2px solid #263449;
            border-radius: 14px;
            font-size: 15px;
        }
        QPushButton {
            color: #344054;
            background: #F8FAFC;
            border: 1px solid #DFE5EC;
            border-radius: 12px;
            padding: 0 16px;
            font-weight: 600;
            text-align: left;
        }
        QPushButton:hover {
            color: #1D4ED8;
            background: #EEF4FF;
            border: 1px solid #AFC8FF;
        }
        QPushButton:pressed {
            color: #1E40AF;
            background: #DCE8FF;
            border-color: #7DA5F5;
        }
        QPushButton[primary="true"] {
            color: white;
            background: #1E3A5F;
            border: 1px solid #1E3A5F;
        }
        QPushButton[primary="true"]:hover {
            background: #28517F;
            border-color: #3C6B9E;
        }
        QPushButton[primary="true"]:pressed {
            background: #142A46;
        }
        QPushButton[danger="true"] {
            color: #B54747;
            background: #FFF8F7;
            border-color: #F4D8D5;
            text-align: center;
        }
        QPushButton[danger="true"]:hover {
            color: #9E2F2F;
            background: #FDECE9;
            border-color: #EABAB4;
        }
        QListWidget {
            color: #344054;
            background: #F8FAFC;
            border: 1px solid #E4E9F0;
            border-radius: 12px;
            padding: 7px;
            outline: none;
        }
        QListWidget::item {
            background: transparent;
            border-radius: 9px;
            padding: 9px 8px;
        }
        QListWidget::item:hover {
            background: #EDF3FA;
        }
        QListWidget::item:selected {
            color: #163F70;
            background: #DCE9F7;
        }
        QLabel#actionTip {
            color: #667085;
            background: #F4F7FA;
            border-radius: 10px;
            padding: 10px;
            font-size: 11px;
        }
        QScrollBar:vertical {
            width: 8px;
            background: transparent;
            margin: 8px 1px;
        }
        QScrollBar::handle:vertical {
            min-height: 24px;
            background: #CBD3DE;
            border-radius: 4px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QStatusBar {
            color: #6B7280;
            background: #F3F6FA;
            border: none;
            padding-left: 24px;
        }
    )");
    statusBar()->showMessage(tr("系统就绪"));
}

void MainWindow::processFrame()
{
    if (isCameraRunning) {
        cap >> currentFrame;
        if (currentFrame.empty()) return;
    } else if (isImageMode) {
        isImageMode = false;
    } else {
        return;
    }

    cv::Mat displayFrame = currentFrame.clone();
    std::vector<FaceDetection> faces = recognizer->detectFaces(displayFrame);
    QSet<int> recognizedThisFrame;

    for (const auto &face : faces) {
        const cv::Rect &rect = face.box;
        cv::Rect safeRect = rect & cv::Rect(0,0,displayFrame.cols,displayFrame.rows);
        if (safeRect.area() == 0) continue;

        cv::Mat feature = recognizer->extractFeature(currentFrame, face);
        if (feature.empty()) {
            cv::rectangle(displayFrame, rect, cv::Scalar(0, 0, 255), 2);
            continue;
        }

        float maxSim = 0.0f;
        int personId = -1;
        QString identity = recognizeFace(feature, maxSim, personId);

        cv::rectangle(displayFrame, rect, cv::Scalar(0,255,0), 2);
        std::string label = identity.toStdString() + " " + std::to_string(maxSim).substr(0,4);
        cv::putText(displayFrame, label, cv::Point(rect.x, rect.y-5),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0,255,0), 2);

        if (personId >= 0 && identity != "Stranger") {
            recognizedThisFrame.insert(personId);
            const bool justAppeared = !identitiesInPreviousFrame.contains(personId);
            // A loaded image is an explicit user action, so each load gets a log.
            // Camera frames still use a cooldown to prevent rapid duplicate events.
            if (justAppeared
                && (!isCameraRunning || shouldAppendLog(personId))) {
                QString snapshotError;
                const QString snapshotPath = RecognitionLogStorage::saveSnapshot(
                    currentFrame, safeRect, &snapshotError);
                const QString sourceType = isCameraRunning ? "Camera" : "Image";
                int logId = -1;
                if (database->addRecognitionLog(personId, identity, maxSim,
                                                snapshotPath, sourceType, &logId)) {
                    RecognitionLogInfo log;
                    log.id = logId;
                    log.personId = personId;
                    log.nameSnapshot = identity;
                    log.similarity = maxSim;
                    log.recognizedAt = QDateTime::currentDateTime().toString(
                        "yyyy-MM-dd HH:mm:ss");
                    log.snapshotPath = snapshotPath;
                    log.sourceType = sourceType;
                    appendRecognitionLogItem(log);
                } else {
                    RecognitionLogStorage::deleteStoredSnapshot(snapshotPath);
                    qWarning() << "Failed to persist recognition log:"
                               << database->lastError();
                }
            }
        }
    }
    identitiesInPreviousFrame = recognizedThisFrame;
    displayImage(displayFrame);
}

QString MainWindow::recognizeFace(const cv::Mat &feature, float &maxSim,
                                  int &bestPersonId)
{
    maxSim = 0.0f;
    bestPersonId = -1;
    QString bestName = "Stranger";
    for (const PersonInfo &person : personCache) {
        for (const cv::Mat &embedding : person.embeddings) {
            const float sim = FaceRecognizer::cosineSimilarity(feature, embedding);
            if (sim > maxSim) {
                maxSim = sim;
                bestName = person.name;
                bestPersonId = person.id;
            }
        }
    }
    if (maxSim < RecognitionThreshold) {
        bestName = "Stranger";
        bestPersonId = -1;
    }
    return bestName;
}

bool MainWindow::shouldAppendLog(int personId)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 lastTime = lastLogTimes.value(personId, 0);
    if (lastTime > 0 && now - lastTime < LogCooldownMs) {
        return false;
    }

    lastLogTimes.insert(personId, now);
    return true;
}

void MainWindow::loadRecognitionLogs()
{
    ui->logList->clear();
    const QVector<RecognitionLogInfo> logs = database->getRecentRecognitionLogs(200);
    // appendRecognitionLogItem inserts at row 0, so replay oldest to newest.
    for (auto iterator = logs.crbegin(); iterator != logs.crend(); ++iterator) {
        appendRecognitionLogItem(*iterator);
    }
}

void MainWindow::appendRecognitionLogItem(const RecognitionLogInfo &log)
{
    const QString source = log.sourceType == "Camera" ? tr("Camera") : tr("Image");
    const QString time = log.recognizedAt.size() >= 16
                             ? log.recognizedAt.mid(5, 11)
                             : log.recognizedAt;
    auto *item = new QListWidgetItem(
        QString("%1  %2\n%3  %4")
            .arg(time, log.nameSnapshot,
                 tr("Similarity %1").arg(log.similarity, 0, 'f', 2), source));
    item->setData(Qt::UserRole, log.snapshotPath);
    item->setData(Qt::UserRole + 1, log.recognizedAt);
    item->setToolTip(tr("Double-click to view the recognition snapshot."));
    ui->logList->insertItem(0, item);
    while (ui->logList->count() > 200) {
        delete ui->logList->takeItem(ui->logList->count() - 1);
    }
}

void MainWindow::onLogDoubleClicked(QListWidgetItem *item)
{
    if (!item) {
        return;
    }
    const QString storedPath = item->data(Qt::UserRole).toString();
    const QString absolutePath = RecognitionLogStorage::resolveStoredPath(storedPath);
    QPixmap snapshot(absolutePath);
    if (snapshot.isNull()) {
        QMessageBox::information(this, tr("Recognition Snapshot"),
                                 tr("This log has no available snapshot."));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Recognition Snapshot"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *details = new QLabel(item->text(), &dialog);
    auto *imageLabel = new QLabel(&dialog);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setPixmap(snapshot.scaled(640, 480, Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation));
    layout->addWidget(details);
    layout->addWidget(imageLabel);
    dialog.exec();
}

void MainWindow::onClearLogs()
{
    if (ui->logList->count() == 0) {
        return;
    }
    if (QMessageBox::question(
            this, tr("Clear Recognition Logs"),
            tr("Clear all recognition records and their snapshots?"))
        != QMessageBox::Yes) {
        return;
    }
    if (!database->clearRecognitionLogs()) {
        QMessageBox::warning(this, tr("Clear Failed"), database->lastError());
        return;
    }
    ui->logList->clear();
    lastLogTimes.clear();
}

void MainWindow::refreshPersonCache()
{
    personCache = database->getAllPersons();
}

void MainWindow::displayImage(const cv::Mat &mat)
{
    cv::Mat rgb;
    cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
    QImage qimg(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
    ui->videoLabel->setPixmap(QPixmap::fromImage(qimg).scaled(ui->videoLabel->size(), Qt::KeepAspectRatio));
}

void MainWindow::onRegister()
{
    if (currentFrame.empty()) {
        QMessageBox::information(this, "WARN", "Please obtain a frame of image");
        return;
    }
    std::vector<FaceDetection> faces = recognizer->detectFaces(currentFrame);
    qDebug() << "Detected faces for registration:" << faces.size();
    for (const auto &face : faces)
    {
        const cv::Rect &rect = face.box;
        qDebug() << "  Rect:" << rect.x << rect.y << rect.width << rect.height
                 << "confidence:" << face.confidence;
    }

    if (faces.empty()) {
        QMessageBox::information(this, "WARN", "No faces");
        return;
    }
    if (faces.size() > 1) {
        QMessageBox::information(this, "WARN", "Please keep exactly one face in the image");
        return;
    }
    const cv::Rect faceRect = faces[0].box
                              & cv::Rect(0,0,currentFrame.cols,currentFrame.rows);
    if (faceRect.empty()) {
        QMessageBox::warning(this, "ERROR", "Invalid Face region!Please re-capture the image");
        return;
    }
    cv::Mat alignedFace;
    cv::Mat feature = recognizer->extractFeature(currentFrame, faces[0],
                                                 &alignedFace);
    if (feature.empty()) {
        QMessageBox::warning(this, "ERROR", "Fail to align face or extract feature");
        return;
    }

    bool ok = false;
    const QString personCode = QInputDialog::getText(
        this, tr("注册人脸"), tr("请输入人员编号："),
        QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || personCode.isEmpty()) {
        return;
    }

    PersonInfo existingPerson;
    const bool personExists = database->findPersonByCode(personCode, existingPerson);
    if (!personExists && !database->lastError().isEmpty()) {
        QMessageBox::warning(this, tr("查询失败"), database->lastError());
        return;
    }

    QString name;
    QString department;
    if (personExists) {
        const auto answer = QMessageBox::question(
            this,
            tr("追加人脸样本"),
            tr("编号 %1 已属于“%2”。是否把当前人脸作为该人员的新样本？")
                .arg(personCode, existingPerson.name));
        if (answer != QMessageBox::Yes) {
            return;
        }
        name = existingPerson.name;
        department = existingPerson.department;
    } else {
        name = QInputDialog::getText(
            this, tr("注册人脸"), tr("请输入姓名："),
            QLineEdit::Normal, QString(), &ok).trimmed();
        if (!ok || name.isEmpty()) {
            return;
        }
        department = QInputDialog::getText(
            this, tr("注册人脸"), tr("请输入部门（可留空）："),
            QLineEdit::Normal, QString(), &ok).trimmed();
        if (!ok) {
            return;
        }
    }

    QString storageError;
    const QString sampleImagePath = FaceSampleStorage::saveAlignedFace(
        alignedFace, &storageError);
    if (sampleImagePath.isEmpty()) {
        QMessageBox::warning(this, tr("保存样本失败"), storageError);
        return;
    }

    bool createdPerson = false;
    if (!database->registerFaceSample(personCode, name, department, feature,
                                      sampleImagePath, nullptr, &createdPerson)) {
        FaceSampleStorage::deleteStoredImage(sampleImagePath);
        QMessageBox::warning(this, tr("注册失败"), database->lastError());
        return;
    }

    refreshPersonCache();
    const PersonInfo updatedPerson = [&]() {
        for (const PersonInfo &person : personCache) {
            if (person.personCode == personCode) {
                return person;
            }
        }
        return PersonInfo{};
    }();
    QMessageBox::information(
        this,
        tr("注册成功"),
        createdPerson
            ? tr("人员“%1”注册成功，当前有1个人脸样本。").arg(name)
            : tr("已为“%1”追加人脸样本，当前共有%2个样本。")
                  .arg(name)
                  .arg(updatedPerson.embeddings.size()));
}

void MainWindow::onManageDB()
{
    PersonManagerDialog dialog(database, recognizer, this);
    dialog.exec();
    refreshPersonCache();
}
