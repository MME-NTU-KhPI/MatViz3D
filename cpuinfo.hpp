#ifndef CPUINFO_HPP
#define CPUINFO_HPP

#include <QObject>
#include <QProcess>
#include <QStringList>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class CpuInfo : public QObject
{
    Q_OBJECT

public:
    explicit CpuInfo(QObject *parent = nullptr)
        : QObject(parent)
    {}

    static int getPhysicalCores()
    {
#ifdef Q_OS_WIN
        return getWindowsPhysicalCores();
#elif defined(Q_OS_LINUX)
        return getLinuxPhysicalCores();
#elif defined(Q_OS_MAC)
        return getMacPhysicalCores();
#else
        return -1;
#endif
    }

private:

#ifdef Q_OS_WIN
    static int getWindowsPhysicalCores()
    {
        DWORD length = 0;

        GetLogicalProcessorInformationEx(
            RelationProcessorCore,
            nullptr,
            &length);

        if (length == 0)
            return -1;

        auto *buffer =
            reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(
                malloc(length));

        if (!buffer)
            return -1;

        if (!GetLogicalProcessorInformationEx(
                RelationProcessorCore,
                buffer,
                &length))
        {
            free(buffer);
            return -1;
        }

        int coreCount = 0;

        auto *ptr = buffer;

        while (reinterpret_cast<BYTE*>(ptr) <
               reinterpret_cast<BYTE*>(buffer) + length)
        {
            if (ptr->Relationship == RelationProcessorCore)
                ++coreCount;

            ptr =
                reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(
                    reinterpret_cast<BYTE*>(ptr) + ptr->Size);
        }

        free(buffer);

        return coreCount;
    }
#endif

#ifdef Q_OS_LINUX
    static int getLinuxPhysicalCores()
    {
        QProcess process;

        process.start("lscpu");

        if (!process.waitForFinished())
            return -1;

        QString output = process.readAllStandardOutput();

        int coresPerSocket = 0;
        int sockets = 0;

        for (const QString &line : output.split('\n'))
        {
            if (line.startsWith("Core(s) per socket:"))
            {
                coresPerSocket =
                    line.section(':', 1).trimmed().toInt();
            }

            if (line.startsWith("Socket(s):"))
            {
                sockets =
                    line.section(':', 1).trimmed().toInt();
            }
        }

        if (coresPerSocket > 0 && sockets > 0)
            return coresPerSocket * sockets;

        return -1;
    }
#endif

#ifdef Q_OS_MAC
    static int getMacPhysicalCores()
    {
        QProcess process;

        process.start("sysctl",
                      QStringList() << "-n"
                                    << "hw.physicalcpu");

        if (!process.waitForFinished())
            return -1;

        return process
            .readAllStandardOutput()
            .trimmed()
            .toInt();
    }
#endif
};

#endif // CPUINFO_HPP
