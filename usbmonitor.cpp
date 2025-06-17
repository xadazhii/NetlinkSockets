#include "usbmonitor.h"
#include <QHBoxLayout>
#include <QHeaderView>
#include <QTime>
#include <QApplication>
#include <QIcon>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QAbstractAnimation>
#include <QEasingCurve>

// Constructor: Initializes the USBMonitorGUI object and sets up the UI and styles.
USBMonitorGUI::USBMonitorGUI(QWidget *parent)
    : QMainWindow(parent), workerThread(nullptr), worker(nullptr) {
    setupUI();
    applyStyles();
}

// Destructor: Ensures monitoring is stopped and resources are cleaned up.
USBMonitorGUI::~USBMonitorGUI() {
    stopMonitoring();
}

// Sets up the user interface components for the USB monitor.
void USBMonitorGUI::setupUI() {
    setWindowTitle("USB Device Monitor");
    setWindowIcon(QIcon::fromTheme("drive-removable-media-usb"));
    resize(850, 600);

    // Create the central widget and layout.
    auto *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    auto *layout = new QVBoxLayout(centralWidget);
    layout->setSpacing(10);
    layout->setContentsMargins(15, 15, 15, 15);

    // Create the header layout with status label and buttons.
    auto *headerLayout = new QHBoxLayout();
    statusLabel = new QLabel("Ready to monitor", this);
    statusLabel->setObjectName("statusLabel");

    startButton = new QPushButton(" Start Monitoring", this);
    stopButton = new QPushButton(" Stop Monitoring", this);
    auto *exitButton = new QPushButton(" Exit", this);

    headerLayout->addWidget(statusLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(startButton);
    headerLayout->addWidget(stopButton);
    layout->addLayout(headerLayout);

    stopButton->setEnabled(false);

    // Create the device table for displaying connected devices.
    deviceTable = new QTableWidget(this);
    deviceTable->setColumnCount(4);
    deviceTable->setHorizontalHeaderLabels({"", "Device Path", "Information", "Timestamp"});
    deviceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    deviceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    deviceTable->setShowGrid(false);
    deviceTable->verticalHeader()->setVisible(false);

    QHeaderView *header = deviceTable->horizontalHeader();
    header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(1, QHeaderView::Stretch);
    header->setSectionResizeMode(2, QHeaderView::Stretch);
    header->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    layout->addWidget(deviceTable);

    // Create the console output for logging messages.
    consoleOutput = new QTextEdit(this);
    consoleOutput->setReadOnly(true);
    consoleOutput->setMaximumHeight(120);
    layout->addWidget(consoleOutput);
    layout->addWidget(exitButton);

    // Connect button signals to their respective slots.
    connect(startButton, &QPushButton::clicked, this, &USBMonitorGUI::startMonitoring);
    connect(stopButton, &QPushButton::clicked, this, &USBMonitorGUI::stopMonitoring);
    connect(exitButton, &QPushButton::clicked, qApp, &QApplication::quit);
}

// Applies custom styles to the UI components.
void USBMonitorGUI::applyStyles() {
    setStyleSheet(R"(
        QMainWindow {
            background-color: #34495E;
        }
        #statusLabel {
            font-size: 20px;
            font-weight: bold;
            color: #ECF0F1;
        }
        QTableWidget {
            background-color: #2C3E50;
            color: #ECF0F1;
            border: 1px solid #466280;
            border-radius: 5px;
            font-size: 14px;
            alternate-background-color: #314457;
        }
        QTableWidget::item {
            padding: 8px;
            border-bottom: 1px solid #466280;
        }
        QTableWidget::item:selected {
            background-color: #3498DB;
            color: white;
        }
        QHeaderView::section {
            background-color: #466280;
            color: white;
            padding: 8px;
            border: none;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton {
            background-color: #3498DB;
            color: white;
            padding: 10px 15px;
            border: none;
            border-radius: 5px;
            font-size: 14px;
            font-weight: bold;
            min-width: 150px;
        }
        QPushButton:hover {
            background-color: #4DA9E4;
        }
        QPushButton:pressed {
            background-color: #2980B9;
        }
        QPushButton:disabled {
            background-color: #95A5A6;
            color: #BDC3C7;
        }
        QTextEdit {
            background-color: #212F3D;
            color: #AABBCB;
            font-family: "Monospace";
            font-size: 13px;
            border: 1px solid #466280;
            border-radius: 5px;
        }
        QScrollBar:vertical {
            border: none;
            background: #2C3E50;
            width: 10px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #5D6D7E;
            min-height: 20px;
            border-radius: 5px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
    )");
}

// Starts monitoring USB devices by creating a worker thread.
void USBMonitorGUI::startMonitoring() {
    if (workerThread && workerThread->isRunning()) return;

    workerThread = new QThread(this);
    worker = new USBWorker();
    worker->moveToThread(workerThread);

    // Connect worker signals to GUI slots.
    connect(workerThread, &QThread::started, worker, &USBWorker::startMonitoring);
    connect(worker, &USBWorker::finished, this, &USBMonitorGUI::cleanupThread);
    connect(worker, &USBWorker::logMessage, this, &USBMonitorGUI::logToConsole);
    connect(worker, &USBWorker::deviceConnected, this, &USBMonitorGUI::onDeviceConnected);
    connect(worker, &USBWorker::deviceDisconnected, this, &USBMonitorGUI::onDeviceDisconnected);

    workerThread->start();

    startButton->setEnabled(false); // Disable start button while monitoring.
    stopButton->setEnabled(true);  // Enable stop button.
    statusLabel->setText("Monitoring Active...");
}

// Stops monitoring USB devices and updates the UI.
void USBMonitorGUI::stopMonitoring() const {
    if (worker) {
        worker->stopMonitoring();
    }
    startButton->setEnabled(true); // Enable start button.
    stopButton->setEnabled(false); // Disable stop button.
    statusLabel->setText("Monitoring Stopped");
}

// Handles the event when a device is connected.
void USBMonitorGUI::onDeviceConnected(const QString &deviceInfo, const QString &port) const {
    logToConsole(QString("✅ UPDATE/CONNECT on port %1").arg(port));

    const QString typeToolTip;
    const QIcon statusIcon = QIcon::fromTheme("multimedia-player");

    const int row = deviceTable->rowCount();
    deviceTable->insertRow(row);
    deviceTable->setRowHeight(row, 50);

    // Add device information to the table.
    auto *iconItem = new QTableWidgetItem();
    iconItem->setIcon(statusIcon);
    iconItem->setTextAlignment(Qt::AlignCenter);
    iconItem->setToolTip(typeToolTip);

    deviceTable->setItem(row, 0, iconItem);
    deviceTable->setItem(row, 1, new QTableWidgetItem(port));
    deviceTable->setItem(row, 2, new QTableWidgetItem(deviceInfo));
    deviceTable->setItem(row, 3, new QTableWidgetItem(QTime::currentTime().toString("HH:mm:ss")));
    deviceTable->scrollToBottom();
}

// Handles the event when a device is disconnected.
void USBMonitorGUI::onDeviceDisconnected(const QString &deviceInfo, const QString &port) {
    logToConsole(QString("❌ DISCONNECTED from port %1").arg(port));

    // Find and remove the disconnected device from the table.
    for (int i = 0; i < deviceTable->rowCount(); ++i) {
        if (deviceTable->item(i, 1) && deviceTable->item(i, 1)->text() == port) {
            removeRowWithAnimation(i);
            break;
        }
    }
}

// Removes a row from the table with a fade-out animation.
void USBMonitorGUI::removeRowWithAnimation(int row) {
    const auto rowWidget = new QWidget();
    auto* effect = new QGraphicsOpacityEffect(rowWidget);
    rowWidget->setGraphicsEffect(effect);

    auto* animation = new QPropertyAnimation(effect, "opacity");
    animation->setDuration(500);
    animation->setStartValue(1.0);
    animation->setEndValue(0.0);
    animation->setEasingCurve(QEasingCurve::OutQuad);

    // Remove the row after the animation finishes.
    connect(animation, &QPropertyAnimation::finished, [this, row]() {
        if (row < deviceTable->rowCount()) {
             deviceTable->removeRow(row);
        }
    });

    animation->start(QAbstractAnimation::DeleteWhenStopped);
    deviceTable->setIndexWidget(deviceTable->model()->index(row, 0), rowWidget);
}

// Logs messages to the console output with a timestamp.
void USBMonitorGUI::logToConsole(const QString &message) const {
    consoleOutput->append(QString("[%1] %2").arg(QTime::currentTime().toString("HH:mm:ss"), message));
}

// Cleans up the worker thread and deletes associated resources.
void USBMonitorGUI::cleanupThread() {
    if (workerThread) {
        workerThread->quit();
        workerThread->wait();
        delete workerThread;
        workerThread = nullptr;
        delete worker;
        worker = nullptr;
    }
}
