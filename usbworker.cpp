#include "usbworker.h"
#include <iostream>
#include <sstream>
#include <regex>
#include <map>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <unistd.h>
#define BUFFER_SIZE 4096

USBWorker::USBWorker(QObject *parent) : QObject(parent), running(false), netlink_socket(-1) {}

USBWorker::~USBWorker() {
    if (netlink_socket >= 0) {
        close(netlink_socket);
    }
}

void USBWorker::startMonitoring() {
    if (running) {
        emit logMessage("Monitoring is already running.");
        return;
    }

    netlink_socket = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
    if (netlink_socket < 0) {
        perror("socket");
        emit logMessage("Error: Failed to create Netlink socket.");
        emit finished();
        return;
    }

    sockaddr_nl addr = {};
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    addr.nl_groups = 1;

    if (bind(netlink_socket, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        perror("bind");
        emit logMessage("Error: Failed to bind Netlink socket.");
        close(netlink_socket);
        netlink_socket = -1;
        emit finished();
        return;
    }

    running = true;
    emit logMessage("✅ Started monitoring USB events...");
    processEvents();
}

void USBWorker::stopMonitoring() {
    emit logMessage("⏹ Stopping monitoring...");
    running = false;
}

void USBWorker::processEvents() {
    char buffer[BUFFER_SIZE];
    while (running) {
        timeval tv{};
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(netlink_socket, &fds);

        const int ret = select(netlink_socket + 1, &fds, nullptr, nullptr, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }
        if (ret == 0) continue;

        if (const ssize_t len = recv(netlink_socket, buffer, sizeof(buffer) - 1, 0); len > 0) {
            buffer[len] = '\0';
            handleUEvent(buffer);
        }
    }

    if (netlink_socket >= 0) {
        close(netlink_socket);
        netlink_socket = -1;
    }
    emit logMessage("Monitoring stopped.");
    emit finished();
}

void USBWorker::parseUEvent(const char* buffer, std::map<std::string, std::string>& uevent_data) {
    const char* s = buffer;
    while (*s) {
        std::string line(s);
        if (const size_t equals_pos = line.find('='); equals_pos != std::string::npos) {
            uevent_data[line.substr(0, equals_pos)] = line.substr(equals_pos + 1);
        }
        s += line.length() + 1;
    }
}

std::string USBWorker::getPortId(const std::string& devpath) {
    static const std::regex port_regex(R"((\d+-\d+(\.\d+)*)\/$)");
    const std::string search_path = devpath + "/";
    if (std::smatch match; std::regex_search(search_path, match, port_regex) && !match.empty()) {
        return match[1].str();
    }
    return "";
}

void USBWorker::handleUEvent(const char* uevent_buf) {
    std::map<std::string, std::string> uevent;
    parseUEvent(uevent_buf, uevent);

    if (!uevent.contains("ACTION") || !uevent.contains("DEVPATH")) {
        return;
    }

    std::string action = uevent["ACTION"];
    std::string subsystem = uevent.contains("SUBSYSTEM") ? uevent["SUBSYSTEM"] : "";
    std::string devpath = uevent["DEVPATH"];

    if (subsystem != "usb" && subsystem != "block") {
        return;
    }

    if (subsystem == "block" && (!uevent.contains("ID_BUS") || uevent["ID_BUS"] != "usb")) {
        return;
    }

    size_t last_slash = devpath.rfind('/');
    if (last_slash == std::string::npos) return;
    std::string parent_devpath = devpath.substr(0, last_slash);

    if (subsystem == "usb") {
        if (size_t usb_pos = parent_devpath.find("/usb"); usb_pos != std::string::npos) {
            std::string after_usb = parent_devpath.substr(usb_pos + 4);
            if (int dash_count = std::ranges::count(after_usb, '-'); dash_count < 1) {
                return;
            }
        }
    }

    if (action == "add") {
        std::string new_info;
        if (subsystem == "usb" && uevent.contains("PRODUCT")) {
            std::string vendor_id, product_id;
            std::stringstream ss(uevent["PRODUCT"]);
            std::getline(ss, vendor_id, '/');
            std::getline(ss, product_id, '/');

            std::string command = "lsusb -d " + vendor_id + ":" + product_id;
            std::string result = executeCommand(command);

            if (size_t id_pos = result.find(vendor_id + ":" + product_id); id_pos != std::string::npos) {
                new_info = "Device: " + result.substr(id_pos + 9);
                new_info.erase(new_info.find_last_not_of("\n\r") + 1);
            } else {
                new_info = "Device: " + (uevent.contains("ID_MODEL") ? uevent["ID_MODEL"] : "Unknown");
            }
        }
        else if (subsystem == "block" && uevent.contains("DEVNAME")) {
            std::string devname = uevent["DEVNAME"].substr(uevent["DEVNAME"].rfind('/') + 1);
            std::string command = "lsblk -o NAME,MODEL,SIZE,FSTYPE,TRAN -l | grep " + devname;
            if (std::string result = executeCommand(command); !result.empty()) {
                new_info = "Storage: " + result;
                new_info.erase(new_info.find_last_not_of("\n\r") + 1);
            }
        }

        if (!new_info.empty()) {
            if (!connected_device_info.contains(parent_devpath)) {
                connected_device_info[parent_devpath] = new_info;
                emit deviceConnected(QString::fromStdString(new_info), QString::fromStdString(parent_devpath)+ ":" + QString::fromStdString(new_info));
            }
        }

    } else if (action == "remove") {
        if (connected_device_info.contains(parent_devpath)) {
            emit deviceDisconnected(
                QString::fromStdString(connected_device_info[parent_devpath]),
                QString::fromStdString(parent_devpath) + ":" + QString::fromStdString(connected_device_info[parent_devpath])
            );
            connected_device_info.erase(parent_devpath);
        }
    }
}

std::string USBWorker::executeCommand(const std::string& command) {
    std::stringstream result;
    char buffer[256];
    FILE* pipe = popen((command + " 2>/dev/null").c_str(), "r");
    if (!pipe) return "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) result << buffer;
    pclose(pipe);
    return result.str();
}