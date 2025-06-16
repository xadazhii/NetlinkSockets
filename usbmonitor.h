#ifndef USBMONITOR_H
#define USBMONITOR_H

#include <QMainWindow>
#include <QLabel>
#include <QTableWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QThread>
#include "usbworker.h"

class USBMonitorGUI final : public QMainWindow {
    Q_OBJECT

public:
    explicit USBMonitorGUI(QWidget *parent = nullptr);
    ~USBMonitorGUI() override;

private slots:
    void startMonitoring();
    void stopMonitoring() const;
    void onDeviceConnected(const QString &deviceInfo, const QString &port) const;
    void onDeviceDisconnected(const QString &deviceInfo, const QString &port);
    void logToConsole(const QString &message) const;
    void cleanupThread();
    void removeRowWithAnimation(int row);

private:
    void setupUI();
    void applyStyles();

    QLabel *statusLabel{};
    QTableWidget *deviceTable{};
    QPushButton *startButton{};
    QPushButton *stopButton{};
    QTextEdit *consoleOutput{};

    QThread *workerThread;
    USBWorker *worker;
};

#endif