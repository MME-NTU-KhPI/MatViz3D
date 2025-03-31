#include "messagehandler.h"
#include <QTextDocument>
#include <QTextCursor>
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
    Q_UNUSED(context);
    QString logType;
    QColor textColor;
    QColor bracketColor;

    switch (type) {
    case QtDebugMsg:
        logType = "Debug";
        textColor = m_textEdit->palette().text().color(); // Колір для Debug
        bracketColor = Qt::darkGreen; // Колір для тексту в квадратних дужках
        printf("%s", GREEN);
        break;
    case QtInfoMsg:
        logType = "Info";
        textColor = m_textEdit->palette().text().color(); // Колір для Info
        bracketColor = Qt::blue; // Колір для тексту в квадратних дужках
        printf("%s", BLUE);
        break;
    case QtWarningMsg:
        logType = "Warning";
        textColor = m_textEdit->palette().text().color(); // Колір для Warning
        bracketColor = Qt::darkRed; // Колір для тексту в квадратних дужках
        printf("%s", RED);
        break;
    case QtCriticalMsg:
        logType = "Critical";
        textColor = m_textEdit->palette().text().color(); // Колір для Critical
        bracketColor = Qt::darkMagenta; // Колір для тексту в квадратних дужках
        printf("%s", MAGENTA);
        break;
    case QtFatalMsg:
        logType = "Fatal";
        textColor = m_textEdit->palette().text().color(); // Колір для Fatal
        bracketColor = Qt::darkYellow; // Колір для тексту в квадратних дужках
        printf("%s", YELLOW);
        break;
    }

    QString message = QString("[%1] %2").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz")).arg(msg);
    printf("[%s]%s:%s\n", qPrintable(logType), RESET, qPrintable(msg));
    fflush(stdout);

    if (m_instance) {
        emit m_instance->messageWrittenSignal(message);
    }

    if (m_textEdit) {
        // Додаємо текст з відповідним кольором до QTextEdit
        QTextCursor cursor(m_textEdit->document());
        cursor.movePosition(QTextCursor::End);

        QTextCharFormat textFormat;
        textFormat.setForeground(textColor); // Встановлюємо колір тексту
        QTextCharFormat bracketFormat;
        bracketFormat.setForeground(bracketColor); // Встановлюємо колір тексту в квадратних дужках

        cursor.setCharFormat(textFormat);
        cursor.insertText("[");
        cursor.setCharFormat(bracketFormat);
        cursor.insertText(logType);
        cursor.setCharFormat(textFormat);
        cursor.insertText("] ");
        cursor.setCharFormat(textFormat);
        cursor.insertText(msg);
        cursor.setCharFormat(bracketFormat);
        cursor.insertText("\n");

        m_textEdit->setTextCursor(cursor);
        m_textEdit->ensureCursorVisible(); // Переміщуємо курсор до кінця документу
    }
}
