#include "mainwindow.h"
#include <QApplication>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QTranslator>
#include <exception>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QTranslator qtTranslator;  //Qt翻译软件，使程序更易懂
    if (qtTranslator.load("qtbase_zh_CN", QLibraryInfo::path(QLibraryInfo::TranslationsPath))) a.installTranslator(&qtTranslator);
   QCoreApplication::setOrganizationName("SCU"); //规定程序数据目录，增强可移植性(C:\Users\name\AppData\Local\SCU\FaceVision)
   QCoreApplication::setApplicationName("FaceVision");//静态函数，不写a.set…
    try
    {
        MainWindow w;
        w.show();
        return QApplication::exec();
    }
    catch (const std::exception &error)
    {
        QMessageBox::critical(nullptr, QObject::tr("启动失败"), QObject::tr("程序初始化失败：%1\n" "请检查模型文件和运行库配置。").arg(QString::fromUtf8(error.what())));
        return 1;
    }
}
