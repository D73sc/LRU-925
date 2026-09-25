#ifndef CRASHHANDLERDBG_H
#define CRASHHANDLERDBG_H


#include <QObject>
#include <Windows.h>
#include <DbgHelp.h>
#include <QFile>
#include <QDateTime>
#include <QTextStream>

class CrashHandlerDbg : public QObject {
    Q_OBJECT
public:
    static void install();  // 安装异常捕获

private:
    static LONG WINAPI exceptionFilter(EXCEPTION_POINTERS* exceptionInfo); // Windows异常
    static void writeCallStack(EXCEPTION_POINTERS* exceptionInfo, QTextStream& stream); // 写调用栈
    static void qtMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg); // Qt日志
};
#endif // CRASHHANDLERDBG_H
