#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <stdexcept>

#pragma comment(lib, "Ws2_32.lib")

class WinSocketInit {

public:
    WinSocketInit() {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            throw std::runtime_error("WSAStartup failed: " + std::to_string(result));
        }
    }
    
    ~WinSocketInit() {
        WSACleanup();
    }
    
    // 刪除複製
    WinSocketInit(const WinSocketInit&) = delete;
    WinSocketInit& operator=(const WinSocketInit&) = delete;

};

// 全域初始化
static WinSocketInit g_winsock_init;