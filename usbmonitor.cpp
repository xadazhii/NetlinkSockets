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

USBMonitorGUI::USBMonitorGUI(QWidget *parent)
    : QMainWindow(parent), workerThread(nullptr), worker(nullptr) {
    setupUI();
    applyStyles();
}

USBMonitorGUI::~USBMonitorGUI() {
    stopMonitoring();
}

void USBMonitorGUI::setupUI() {
    setWindowTitle("USB Device Monitor");
    setWindowIcon(QIcon::fromTheme("drive-removable-media-usb"));
    resize(850, 600);

    auto *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    auto *layout = new QVBoxLayout(centralWidget);
    layout->setSpacing(10);
    layout->setContentsMargins(15, 15, 15, 15);

    auto *headerLayout = new QHBoxLayout();
    statusLabel = new QLabel("Ready to monitor", this);
    statusLabel->setObjectName("statusLabel");

    startButton = new QPushButton(QIcon::fromTheme("media-playback-start", QIcon(":/icons/start.png")), " Start Monitoring", this);
    stopButton = new QPushButton(QIcon::fromTheme("media-playback-stop", QIcon(":/icons/stop.png")), " Stop Monitoring", this);
    auto *exitButton = new QPushButton(QIcon::fromTheme("application-exit", QIcon(":/icons/exit.png")), " Exit", this);

    headerLayout->addWidget(statusLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(startButton);
    headerLayout->addWidget(stopButton);
    layout->addLayout(headerLayout);

    stopButton->setEnabled(false);

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

    consoleOutput = new QTextEdit(this);
    consoleOutput->setReadOnly(true);
    consoleOutput->setMaximumHeight(120);
    layout->addWidget(consoleOutput);
    layout->addWidget(exitButton);

    connect(startButton, &QPushButton::clicked, this, &USBMonitorGUI::startMonitoring);
    connect(stopButton, &QPushButton::clicked, this, &USBMonitorGUI::stopMonitoring);
    connect(exitButton, &QPushButton::clicked, qApp, &QApplication::quit);
}

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

void USBMonitorGUI::startMonitoring() {
    if (workerThread && workerThread->isRunning()) return;

    workerThread = new QThread(this);
    worker = new USBWorker();
    worker->moveToThread(workerThread);

    connect(workerThread, &QThread::started, worker, &USBWorker::startMonitoring);
    connect(worker, &USBWorker::finished, this, &USBMonitorGUI::cleanupThread);
    connect(worker, &USBWorker::logMessage, this, &USBMonitorGUI::logToConsole);
    connect(worker, &USBWorker::deviceConnected, this, &USBMonitorGUI::onDeviceConnected);
    connect(worker, &USBWorker::deviceDisconnected, this, &USBMonitorGUI::onDeviceDisconnected);

    workerThread->start();

    startButton->setEnabled(false);
    stopButton->setEnabled(true);
    statusLabel->setText("Monitoring Active...");
}

void USBMonitorGUI::stopMonitoring() const {
    if (worker) {
        worker->stopMonitoring();
    }
    startButton->setEnabled(true);
    stopButton->setEnabled(false);
    statusLabel->setText("Monitoring Stopped");
}

void USBMonitorGUI::onDeviceConnected(const QString &deviceInfo, const QString &port) const {
    logToConsole(QString("✅ UPDATE/CONNECT on port %1").arg(port));

    QIcon statusIcon;
    QString typeToolTip;

    if (deviceInfo.contains("Storage:", Qt::CaseInsensitive)) {
        statusIcon = QIcon::fromTheme("drive-removable-media", QIcon(":/icons/drive.png"));
        typeToolTip = "Storage Device";
    } else if (deviceInfo.contains("Keyboard", Qt::CaseInsensitive)) {
        statusIcon = QIcon::fromTheme("input-keyboard", QIcon(":/icons/keyboard.png"));
        typeToolTip = "Keyboard";
    } else if (deviceInfo.contains("Mouse", Qt::CaseInsensitive)) {
        statusIcon = QIcon::fromTheme("input-mouse", QIcon(":/icons/mouse.png"));
        typeToolTip = "Mouse";
    } else if (deviceInfo.contains("Hub", Qt::CaseInsensitive)) {
        statusIcon = QIcon::fromTheme("network-hub", QIcon(":/icons/hub.png"));
        typeToolTip = "USB Hub";
    } else {
        statusIcon = QIcon::fromTheme("multimedia-player", QIcon(":/icons/usb.png")); // Generic device icon
        typeToolTip = "Generic USB Device";
    }

    for (int i = 0; i < deviceTable->rowCount(); ++i) {
        if (deviceTable->item(i, 1) && deviceTable->item(i, 1)->text() == port) {
            deviceTable->item(i, 0)->setIcon(statusIcon);
            deviceTable->item(i, 0)->setToolTip(typeToolTip);
            deviceTable->item(i, 2)->setText(deviceInfo);
            deviceTable->item(i, 3)->setText(QTime::currentTime().toString("HH:mm:ss"));
            return;
        }
    }

    const int row = deviceTable->rowCount();
    deviceTable->insertRow(row);
    deviceTable->setRowHeight(row, 50);

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

void USBMonitorGUI::onDeviceDisconnected(const QString &deviceInfo, const QString &port) {
    logToConsole(QString("❌ DISCONNECTED from port %1").arg(port));

    for (int i = 0; i < deviceTable->rowCount(); ++i) {
        if (deviceTable->item(i, 1) && deviceTable->item(i, 1)->text() == port) {
            removeRowWithAnimation(i);
            break;
        }
    }
}

void USBMonitorGUI::removeRowWithAnimation(int row) {
    const auto rowWidget = new QWidget();
    auto* effect = new QGraphicsOpacityEffect(rowWidget);
    rowWidget->setGraphicsEffect(effect);

    auto* animation = new QPropertyAnimation(effect, "opacity");
    animation->setDuration(500);
    animation->setStartValue(1.0);
    animation->setEndValue(0.0);
    animation->setEasingCurve(QEasingCurve::OutQuad);

    connect(animation, &QPropertyAnimation::finished, [this, row]() {
        if (row < deviceTable->rowCount()) {
             deviceTable->removeRow(row);
        }
    });

    animation->start(QAbstractAnimation::DeleteWhenStopped);
    deviceTable->setIndexWidget(deviceTable->model()->index(row, 0), rowWidget);
}


void USBMonitorGUI::logToConsole(const QString &message) const {
    consoleOutput->append(QString("[%1] %2").arg(QTime::currentTime().toString("HH:mm:ss"), message));
}

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