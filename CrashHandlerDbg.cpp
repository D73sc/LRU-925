#include "CrashHandlerDbg.h"

void CrashHandlerDbg::install() {
    // 安装 Windows 异常捕获
    SetUnhandledExceptionFilter(CrashHandlerDbg::exceptionFilter);

    // 安装 Qt 消息处理器
    qInstallMessageHandler(CrashHandlerDbg::qtMessageHandler);
}

LONG WINAPI CrashHandlerDbg::exceptionFilter(EXCEPTION_POINTERS* exceptionInfo) {
    QFile file("crash_log.txt");
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << "\n=== Crash Occurred at " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << " ===\n";
        out << QString("Exception Code: 0x%1\n").arg(exceptionInfo->ExceptionRecord->ExceptionCode, 0, 16);
        out << QString("Exception Address: 0x%1\n").arg(reinterpret_cast<quintptr>(exceptionInfo->ExceptionRecord->ExceptionAddress), 0, 16);

        writeCallStack(exceptionInfo, out);

        out << "============================================================\n";
        file.close();
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

void CrashHandlerDbg::writeCallStack(EXCEPTION_POINTERS* exceptionInfo, QTextStream& stream) {
    HANDLE hProcess = GetCurrentProcess();
    HANDLE hThread = GetCurrentThread();

    SymInitialize(hProcess, NULL, TRUE);

    CONTEXT context = *exceptionInfo->ContextRecord;

    STACKFRAME64 stackFrame;
    memset(&stackFrame, 0, sizeof(STACKFRAME64));

#if defined(_M_X64)
    DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
    stackFrame.AddrPC.Offset = context.Rip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context.Rbp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context.Rsp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
#else
    DWORD machineType = IMAGE_FILE_MACHINE_I386;
    stackFrame.AddrPC.Offset = context.Eip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context.Ebp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context.Esp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
#endif

    stream << "Call Stack:\n";

    for (int i = 0; i < 30; ++i) {
        if (!StackWalk64(machineType, hProcess, hThread, &stackFrame, &context, NULL,
                         SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {
            break;
        }

        DWORD64 addr = stackFrame.AddrPC.Offset;
        if (addr == 0) break;

        char symbolBuffer[sizeof(SYMBOL_INFO) + 256];
        SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 255;

        DWORD64 displacement = 0;
        if (SymFromAddr(hProcess, addr, &displacement, symbol)) {
            stream << QString("    [%1] %2 + 0x%3\n")
                          .arg(i, 2)
                          .arg(symbol->Name)
                          .arg(displacement, 0, 16);
        } else {
            stream << QString("    [%1] 0x%2\n")
                          .arg(i, 2)
                          .arg(addr, 0, 16);
        }
    }

    SymCleanup(hProcess);
}

void CrashHandlerDbg::qtMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
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
