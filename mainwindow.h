#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include "facerecognizer.h"
#include "facedatabase.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onStartStop();
    void onLoadImage();
    void onRegister();
    void onManageDB();
    void processFrame();

private:
    void displayImage(const cv::Mat &mat);
    QString recognizeFace(const cv::Mat &feature, float &maxSim);

    Ui::MainWindow *ui;
    QTimer *timer;
    cv::VideoCapture cap;
    cv::Mat currentFrame;
    bool isCameraRunning = false;
    bool isImageMode = false;
    FaceRecognizer *recognizer;
    FaceDatabase *database;
};

#endif