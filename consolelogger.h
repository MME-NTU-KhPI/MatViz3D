// ConsoleLogger.h
#pragma once
#include <QObject>
#include <QStringList>

class ConsoleLogger : public QObject {
    Q_OBJECT
public:
    static ConsoleLogger* instance();
    static void messageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg);

public slots:
    void requestClear() { emit clearRequested(); }

signals:
    void newMessage(const QString& type, const QString& text);
    void clearRequested();

private:
    explicit ConsoleLogger(QObject* parent = nullptr);
    static ConsoleLogger* m_instance;
};