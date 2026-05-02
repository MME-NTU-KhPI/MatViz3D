// ConsoleLogger.cpp
#include "ConsoleLogger.h"

ConsoleLogger* ConsoleLogger::m_instance = nullptr;

ConsoleLogger* ConsoleLogger::instance() {
    if (!m_instance)
        m_instance = new ConsoleLogger();
    return m_instance;
}

ConsoleLogger::ConsoleLogger(QObject* parent) : QObject(parent) {}

void ConsoleLogger::messageHandler(QtMsgType type, const QMessageLogContext&, const QString& msg) {
    if (!m_instance) return;

    QString msgType;
    switch (type) {
    case QtDebugMsg:    msgType = "debug";   break;
    case QtInfoMsg:     msgType = "info";    break;
    case QtWarningMsg:  msgType = "warning"; break;
    case QtCriticalMsg: msgType = "critical";break;
    case QtFatalMsg:    msgType = "fatal";   break;
    }

    emit m_instance->newMessage(msgType, msg);
}