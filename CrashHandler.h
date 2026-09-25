#ifndef CRASHHANDLER_H
#define CRASHHANDLER_H

#include <QObject>
#include <Windows.h>
#include <QFile>
#include <QDateTime>
#include <QTextStream>

class CrashHandler : public QObject {
    Q_OBJECT
public:
    static void install();

private:
    static LONG WINAPI exceptionFilter(EXCEPTION_POINTERS* exceptionInfo);
    static void qtMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);
    static void restartApplication();  // 自动重启
    static void restartApplication2();
};

#endif // CRASHHANDLER_H
