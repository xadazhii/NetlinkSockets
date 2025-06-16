#include <QApplication>
#include "usbmonitor.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    USBMonitorGUI window;
    window.show();

    return app.exec();
}