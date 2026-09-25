#include "CrashHandler.h"
#include "qcoreapplication.h"

void CrashHandler::install() {
    SetUnhandledExceptionFilter(CrashHandler::exceptionFilter);
    qInstallMessageHandler(CrashHandler::qtMessageHandler);
}

LONG WINAPI CrashHandler::exceptionFilter(EXCEPTION_POINTERS* exceptionInfo) {
    QFile file("crash_log.txt");
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << "\n=== Crash Occurred at " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << " ===\n";
        out << QString("Exception Code: 0x%1\n")
                   .arg(exceptionInfo->ExceptionRecord->ExceptionCode, 0, 16);
        out << QString("Exception Address: 0x%1\n")
                   .arg(reinterpret_cast<quintptr>(exceptionInfo->ExceptionRecord->ExceptionAddress), 0, 16);
        out << "Call stack not available (DbgHelp not used).\n";
        out << "============================================================\n";
        file.close();
    }

    // 重启应用
//    restartApplication();

    return EXCEPTION_EXECUTE_HANDLER;
}

void CrashHandler::qtMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    QFile file("crash_log.txt");
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        QString level;

        switch (type) {
        case QtDebugMsg:    level = "Debug"; break;
        case QtInfoMsg:     level = "Info"; break;
        case QtWarningMsg:  level = "Warning"; break;
        case QtCriticalMsg: level = "Critical"; break;
        case QtFatalMsg:    level = "Fatal"; break;
        }

        out << QString("[%1] [%2] %3 (%4:%5, %6)\n")
                   .arg(time)
                   .arg(level)
                   .arg(msg)
                   .arg(context.file ? context.file : "")
                   .arg(context.line)
                   .arg(context.function ? context.function : "");

        file.close();
    }

    if (type == QtFatalMsg) {
        abort(); // Fatal 要终止程序
    }
}
void CrashHandler::restartApplication() {
    TCHAR moduleName[MAX_PATH];
    GetModuleFileName(NULL, moduleName, MAX_PATH);

    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    if (CreateProcess(
            moduleName,
            NULL,
            NULL,
            NULL,
            FALSE,
            0,
            NULL,
            NULL,
            &si,
            &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}
void CrashHandler::restartApplication2() {
    QString appPath = QCoreApplication::applicationFilePath().replace("/", "\\");
    STARTUPINFO si = { sizeof(STARTUPINFO) };
    PROCESS_INFORMATION pi;

    if (CreateProcessW((LPCWSTR)appPath.utf16(), NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}
