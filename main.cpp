#include "mainwindow.h"
#include <QApplication>
#include <QMessageBox>
#include <exception>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("SCU");
    QCoreApplication::setApplicationName("FaceVision");
    try {
        MainWindow w;
        w.show();
        return QApplication::exec();
    } catch (const std::exception &error) {
        QMessageBox::critical(nullptr, QObject::tr("启动失败"),
                              QObject::tr("程序初始化失败：%1\n"
                                          "请检查模型文件和运行库配置。")
                                  .arg(QString::fromUtf8(error.what())));
        return 1;
    }
}
