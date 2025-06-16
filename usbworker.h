#ifndef USBWORKER_H
#define USBWORKER_H

#include <QObject>
#include <string>
#include <unordered_map>
#include <atomic>
#include <map>

class USBWorker final : public QObject {
    Q_OBJECT

public:
    explicit USBWorker(QObject *parent = nullptr);
    ~USBWorker() override;

    signals:
        void deviceConnected(const QString &deviceInfo, const QString &port);
    void deviceDisconnected(const QString &deviceInfo, const QString &port);
    void logMessage(const QString &message);
    void finished();

public slots:
    void startMonitoring();
    void stopMonitoring();

private:
    void processEvents();
    void handleUEvent(const char* uevent_buf);

    static void parseUEvent(const char* buffer, std::map<std::string, std::string>& uevent_data);

    static std::string getPortId(const std::string& devpath);

    static std::string executeCommand(const std::string& command);

    std::atomic<bool> running;
    int netlink_socket;
    std::unordered_map<std::string, std::string> connected_device_info;
};

#endif