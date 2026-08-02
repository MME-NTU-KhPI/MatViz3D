// consolelogger.cpp
#include "consolelogger.h"
#include <cstdio>

ConsoleLogger* ConsoleLogger::m_instance = nullptr;

ConsoleLogger* ConsoleLogger::instance() {
    if (!m_instance)
        m_instance = new ConsoleLogger();
    return m_instance;
}

ConsoleLogger::ConsoleLogger(QObject* parent) : QObject(parent) {}

void ConsoleLogger::messageHandler(QtMsgType type, const QMessageLogContext&, const QString& msg) {
    QString msgType;
    const char* prefix;
    switch (type) {
    case QtDebugMsg:    msgType = "debug";    prefix = "[DBG] "; break;
    case QtInfoMsg:     msgType = "info";     prefix = "[INF] "; break;
    case QtWarningMsg:  msgType = "warning";  prefix = "[WRN] "; break;
    case QtCriticalMsg: msgType = "critical"; prefix = "[CRT] "; break;
    case QtFatalMsg:    msgType = "fatal";    prefix = "[FTL] "; break;
    }

    // Classic console output (stdout for debug/info, stderr for warning/critical/fatal)
    // so the log is still visible when running MatViz3D from a terminal, not just
    // in the in-app console widget.
    FILE* stream = (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) ? stderr : stdout;
    fprintf(stream, "%s%s\n", prefix, qPrintable(msg));
    fflush(stream);

    if (m_instance)
        emit m_instance->newMessage(msgType, msg);
}