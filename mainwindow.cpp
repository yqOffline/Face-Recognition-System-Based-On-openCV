#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "personmanagerdialog.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QDateTime>
#include <QDebug>
#include <QInputDialog>
#include <QCoreApplication>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QString modelDir = QCoreApplication::applicationDirPath() + "/models/";
    recognizer = new FaceRecognizer(
        (modelDir + "face_detection_yunet_2023mar.onnx").toStdString(),
        (modelDir + "face_recognition_sface_2021dec.onnx").toStdString());
    database = new FaceDatabase(this);
    if (!database->init("faces.db")) {
        QMessageBox::critical(this, tr("数据库错误"), database->lastError());
    }
    refreshPersonCache();

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::processFrame);

    connect(ui->btnStartStop, &QPushButton::clicked, this, &MainWindow::onStartStop);
    connect(ui->btnLoadImage, &QPushButton::clicked, this, &MainWindow::onLoadImage);
    connect(ui->btnRegister, &QPushButton::clicked, this, &MainWindow::onRegister);
    connect(ui->btnManageDB, &QPushButton::clicked, this, &MainWindow::onManageDB);
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
        cap.open(0);
        if (!cap.isOpened()) {
            QMessageBox::critical(this, "ERROR!", "Fail to Open the Cam.!");
            return;
        }
        isImageMode = false;
        timer->start(30);
        isCameraRunning = true;
        ui->btnStartStop->setText("Stop Camera");
    } else {
        timer->stop();
        cap.release();
        isCameraRunning = false;
        identitiesInPreviousFrame.clear();
        ui->btnStartStop->setText("Start Camera");
        ui->videoLabel->setText("VIDEO");
    }
}

void MainWindow::onLoadImage()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Chose picture", "", "Images (*.png *.jpg *.bmp)");
    if (fileName.isEmpty()) return;
    cv::Mat img = cv::imread(fileName.toLocal8Bit().toStdString());
    if (img.empty()) {
        QMessageBox::warning(this, "ERROR", "Fail to load Images!");
        return;
    }

    // 静态图片和摄像头共用同一个显示区域。载入图片时先停止摄像头，
    // 防止下一次定时器触发后摄像头画面立刻覆盖刚载入的图片。
    if (isCameraRunning) {
        timer->stop();
        cap.release();
        isCameraRunning = false;
        ui->btnStartStop->setText("Start Camera");
    }
    currentFrame = img.clone();
    isImageMode = true;
    processFrame();
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
    QSet<QString> recognizedThisFrame;

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
        QString identity = recognizeFace(feature, maxSim);

        cv::rectangle(displayFrame, rect, cv::Scalar(0,255,0), 2);
        std::string label = identity.toStdString() + " " + std::to_string(maxSim).substr(0,4);
        cv::putText(displayFrame, label, cv::Point(rect.x, rect.y-5),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0,255,0), 2);

        if (!identity.isEmpty() && identity != "Stranger") {
            recognizedThisFrame.insert(identity);
            const bool justAppeared = !identitiesInPreviousFrame.contains(identity);
            if (justAppeared && shouldAppendLog(identity)) {
                QString logEntry = QDateTime::currentDateTime().toString("hh:mm:ss") + " - " + identity;
                ui->logList->insertItem(0, logEntry);
                while (ui->logList->count() > 200) {
                    delete ui->logList->takeItem(ui->logList->count() - 1);
                }
            }
        }
    }
    identitiesInPreviousFrame = recognizedThisFrame;
    displayImage(displayFrame);
}

QString MainWindow::recognizeFace(const cv::Mat &feature, float &maxSim)
{
    maxSim = 0.0f;
    QString bestName = "Stranger";
    for (const PersonInfo &person : personCache) {
        for (const cv::Mat &embedding : person.embeddings) {
            const float sim = FaceRecognizer::cosineSimilarity(feature, embedding);
            if (sim > maxSim) {
                maxSim = sim;
                bestName = person.name;
            }
        }
    }
    if (maxSim < RecognitionThreshold) bestName = "Stranger";
    return bestName;
}

bool MainWindow::shouldAppendLog(const QString &identity)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 lastTime = lastLogTimes.value(identity, 0);
    if (lastTime > 0 && now - lastTime < LogCooldownMs) {
        return false;
    }

    lastLogTimes.insert(identity, now);
    return true;
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
    cv::Mat feature = recognizer->extractFeature(currentFrame, faces[0]);
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

    bool createdPerson = false;
    if (!database->registerFaceSample(personCode, name, department, feature,
                                      nullptr, &createdPerson)) {
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
    PersonManagerDialog dialog(database, this);
    dialog.exec();
    refreshPersonCache();
}
