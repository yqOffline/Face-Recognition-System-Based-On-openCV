#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QDateTime>
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
    database->init("faces.db");

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
    if (ui->modeCombo->currentIndex() == 0) {
        if (!isCameraRunning) {
            cap.open(0);
            if (!cap.isOpened()) {
                QMessageBox::critical(this, "错误", "无法打开摄像头");
                return;
            }
            timer->start(30);
            isCameraRunning = true;
            ui->btnStartStop->setText("停止摄像头");
        } else {
            timer->stop();
            cap.release();
            isCameraRunning = false;
            ui->btnStartStop->setText("开始摄像头");
            ui->videoLabel->setText("视频画面");
        }
    }
}

void MainWindow::onLoadImage()
{
    QString fileName = QFileDialog::getOpenFileName(this, "选择图片", "", "Images (*.png *.jpg *.bmp)");
    if (fileName.isEmpty()) return;
    cv::Mat img = cv::imread(fileName.toLocal8Bit().toStdString());
    if (img.empty()) {
        QMessageBox::warning(this, "错误", "无法加载图片");
        return;
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
    std::vector<cv::Rect> faces = recognizer->detectFaces(displayFrame);
    QVector<PersonInfo> persons = database->getAllPersons();

    for (const auto &rect : faces) {
        cv::Rect safeRect = rect & cv::Rect(0,0,displayFrame.cols,displayFrame.rows);
        if (safeRect.area() == 0) continue;

        cv::Mat faceROI = displayFrame(safeRect);
        cv::Mat feature = recognizer->extractFeature(faceROI);

        float maxSim = 0.0f;
        QString identity = recognizeFace(feature, maxSim);

        cv::rectangle(displayFrame, rect, cv::Scalar(0,255,0), 2);
        std::string label = identity.toStdString() + " " + std::to_string(maxSim).substr(0,4);
        cv::putText(displayFrame, label, cv::Point(rect.x, rect.y-5),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0,255,0), 2);

        if (!identity.isEmpty() && identity != "陌生人") {
            QString logEntry = QDateTime::currentDateTime().toString("hh:mm:ss") + " - " + identity;
            ui->logList->insertItem(0, logEntry);
        }
    }
    displayImage(displayFrame);
}

QString MainWindow::recognizeFace(const cv::Mat &feature, float &maxSim)
{
    maxSim = 0.0f;
    QString bestName = "陌生人";
    QVector<PersonInfo> persons = database->getAllPersons();
    for (const auto &p : persons) {
        float sim = FaceRecognizer::cosineSimilarity(feature, p.embedding);
        if (sim > maxSim) {
            maxSim = sim;
            bestName = p.name;
        }
    }
    if (maxSim < 0.45f) bestName = "陌生人";
    return bestName;
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
        QMessageBox::information(this, "提示", "请先获取一帧图像");
        return;
    }
    std::vector<cv::Rect> faces = recognizer->detectFaces(currentFrame);
    if (faces.empty()) {
        QMessageBox::information(this, "提示", "未检测到人脸");
        return;
    }
    bool ok;
    QString name = QInputDialog::getText(this, "注册", "输入姓名：", QLineEdit::Normal, "", &ok);
    if (!ok || name.isEmpty()) return;

    cv::Rect faceRect = faces[0] & cv::Rect(0,0,currentFrame.cols,currentFrame.rows);
    if (faceRect.area() == 0) {
        QMessageBox::warning(this, "错误", "人脸区域无效，请重新获取图像");
        return;
    }
    cv::Mat faceROI = currentFrame(faceRect);
    cv::Mat feature = recognizer->extractFeature(faceROI);
    if (database->addPerson(name, feature))
        QMessageBox::information(this, "成功", "人脸注册成功！");
    else
        QMessageBox::warning(this, "错误", "数据库写入失败");
}

void MainWindow::onManageDB()
{
    QVector<PersonInfo> persons = database->getAllPersons();
    QStringList names;
    for (const auto &p : persons)
        names << QString::number(p.id) + ": " + p.name;
    if (names.isEmpty()) {
        QMessageBox::information(this, "数据库", "暂无人员");
        return;
    }
    bool ok;
    QString item = QInputDialog::getItem(this, "管理数据库", "选择删除的人员", names, 0, false, &ok);
    if (ok && !item.isEmpty()) {
        int id = item.split(":").first().toInt();
        if (database->deletePerson(id))
            QMessageBox::information(this, "成功", "已删除");
    }
}