#ifndef MESSAGEHANDLER_H
#define MESSAGEHANDLER_H

#include <QObject>
#include <QTextEdit>


// the following are UBUNTU/LINUX, and MacOS ONLY terminal color codes.
// from: https://stackoverflow.com/questions/9158150/colored-output-in-c/9158263
#define RESET   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */
#define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */


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
