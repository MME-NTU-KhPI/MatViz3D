#ifndef CPUINFO_HPP
#define CPUINFO_HPP

#include <QObject>
#include <QProcess>
#include <QStringList>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class CpuInfo : public QObject {
    Q_OBJECT

public:
    explicit CpuInfo(QObject *parent = nullptr) : QObject(parent) {}

    static int getPhysicalCores() {
#ifdef Q_OS_WIN
        return getWindowsPhysicalCores();
#elif defined(Q_OS_LINUX)
        return getLinuxPhysicalCores();
#elif defined(Q_OS_MAC)
        return getMacPhysicalCores();
#else
        return -1; // Unsupported OS
#endif
    }

private:
#ifdef Q_OS_WIN
    static int getWindowsPhysicalCores() {
        DWORD length = 0;
        GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length);
        if (length == 0) return -1;

        SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *buffer =
            (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *)malloc(length);
        if (!buffer) return -1;

        if (!GetLogicalProcessorInformationEx(RelationProcessorCore, buffer, &length)) {
            free(buffer);
            return -1;
        }

        int coreCount = 0;
        SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *ptr = buffer;
        while ((BYTE *)ptr < (BYTE *)buffer + length) {
            if (ptr->Relationship == RelationProcessorCore) {
                coreCount++;
            }
            ptr = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *)((BYTE *)ptr + ptr->Size);
        }

        free(buffer);
        return coreCount;
    }
#endif

#ifdef Q_OS_LINUX
    static int getLinuxPhysicalCores() {
        QProcess process;
        process.start("lscpu");
        process.waitForFinished();
        QString output = process.readAllStandardOutput();
        for (const QString &line : output.split("\n")) {
            if (line.startsWith("Core(s) per socket:")) {
                return line.split(":").last().trimmed().toInt();
            }
        }
        return -1;
    }
#endif

#ifdef Q_OS_MAC
    static int getMacPhysicalCores() {
        QProcess process;
        process.start("sysctl -n hw.physicalcpu");
        process.waitForFinished();
        return process.readAllStandardOutput().trimmed().toInt();
    }
#endif
};

#endif // CPUINFO_HPP
