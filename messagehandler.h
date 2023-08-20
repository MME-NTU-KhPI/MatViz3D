#ifndef MESSAGEHANDLER_H
#define MESSAGEHANDLER_H

#include <QObject>
#include <QTextEdit>

class MessageHandler : public QObject
{
    Q_OBJECT

public:
    explicit MessageHandler(QTextEdit *textEdit, QObject *parent = nullptr);
    static void installMessageHandler(QTextEdit *textEdit);

signals:
    void messageWrittenSignal(const QString &message);

private:
    static void messageHandlerFunction(QtMsgType type, const QMessageLogContext &context, const QString &msg);
    static MessageHandler *m_instance;
    static QTextEdit *m_textEdit;
};

#endif // MESSAGEHANDLER_H
