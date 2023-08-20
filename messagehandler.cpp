#include "messagehandler.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDebug>

MessageHandler* MessageHandler::m_instance = nullptr;
QTextEdit* MessageHandler::m_textEdit = nullptr;

MessageHandler::MessageHandler(QTextEdit *textEdit, QObject *parent)
    : QObject(parent)
{
    m_textEdit = textEdit;
    qInstallMessageHandler(messageHandlerFunction);
}

void MessageHandler::installMessageHandler(QTextEdit *textEdit)
{
    if (!m_instance)
        m_instance = new MessageHandler(textEdit);
}

void MessageHandler::messageHandlerFunction(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString logType;
    switch (type) {
    case QtDebugMsg:
        logType = "Debug";
        break;
    case QtInfoMsg:
        logType = "Info";
        break;
    case QtWarningMsg:
        logType = "Warning";
        break;
    case QtCriticalMsg:
        logType = "Critical";
        break;
    case QtFatalMsg:
        logType = "Fatal";
        break;
    }

    QString message = QString("[%1] %2").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz")).arg(msg);

    if (m_instance) {
        emit m_instance->messageWrittenSignal(message);
    }

    if (m_textEdit) {
        m_textEdit->append(QString("[%1] %2").arg(logType, msg));
    }
}
