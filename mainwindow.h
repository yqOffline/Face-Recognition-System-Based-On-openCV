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
namespace Ui { class MainWindow; } //有mainwindow.ui自动生成，保存界面控件指针
QT_END_NAMESPACE

class QListWidgetItem;

class MainWindow : public QMainWindow  //Ui {class Mainwindow}与Mainwindow不同！
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
        void onClearLogs();
        void onLogDoubleClicked(QListWidgetItem *item);
        void processFrame();

    private:
        void displayImage(const cv::Mat &mat);
        QString recognizeFace(const cv::Mat &feature, float &maxSim,int &bestPersonId);
        bool shouldAppendLog(int personId);
        void refreshPersonCache();
        void loadRecognitionLogs();
        void appendRecognitionLogItem(const RecognitionLogInfo &log);
        void setupModernUi();

        Ui::MainWindow *ui;     //AUTOUIC辅助对象；手动delete
        QTimer *timer;           //每33ms触发processFrame；父对象MainWindow
        cv::VideoCapture cap;   //控制0号物理摄像头及Windows后端
        cv::Mat currentFrame;    //当前摄像头帧或加载的静态图
        bool isCameraRunning = false;  //摄像头是否持续采帧
        bool isImageMode = false;   //静态图是否等待一次性处理
        FaceRecognizer *recognizer;    //检测/对齐/特征/相似度服务；手动删除
        FaceDatabase *database;     //SQLite服务；父对象MainWindow
        QVector<PersonInfo> personCache;    //内存人员与特征缓存，避免逐帧查库
        QHash<int, qint64> lastLogTimes;    //人员ID到最近日志时间，实施5秒冷却
        QSet<int> identitiesInPreviousFrame; //上一帧已知身份集合，用于进入事件去重

        static constexpr float RecognitionThreshold = 0.363f;
        static constexpr qint64 LogCooldownMs = 5000;
};

#endif
