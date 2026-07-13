#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QHash>
#include <QTimer>
#include <QSet>
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
    bool shouldAppendLog(const QString &identity);
    void refreshPersonCache();

    Ui::MainWindow *ui;
    QTimer *timer;
    cv::VideoCapture cap;
    cv::Mat currentFrame;
    bool isCameraRunning = false;
    bool isImageMode = false;
    FaceRecognizer *recognizer;
    FaceDatabase *database;
    QVector<PersonInfo> personCache;
    QHash<QString, qint64> lastLogTimes;
    QSet<QString> identitiesInPreviousFrame;

    static constexpr float RecognitionThreshold = 0.363f;
    static constexpr qint64 LogCooldownMs = 5000;
};

#endif
