#include "win_socket.h"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>

class TCPServer {
private:
    SOCKET listen_socket_ = INVALID_SOCKET;
    int port_;
    std::atomic<bool> running_{false};
    std::vector<std::thread> client_threads_;
    
public:
    TCPServer(int port) : port_(port) {}
    
    ~TCPServer() {
        stop();
    }
    
    bool start() {
        struct addrinfo hints = {0};
        struct addrinfo* result = nullptr;
        
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = AI_PASSIVE;
        
        // 解析地址
        std::string port_str = std::to_string(port_);
        int res = getaddrinfo(nullptr, port_str.c_str(), &hints, &result);
        if (res != 0) {
            std::cerr << "getaddrinfo failed: " << res << std::endl;
            return false;
        }
        
        // 建立 socket
        listen_socket_ = socket(result->ai_family, 
                               result->ai_socktype, 
                               result->ai_protocol);
        
        if (listen_socket_ == INVALID_SOCKET) {
            std::cerr << "socket failed: " << WSAGetLastError() << std::endl;
            freeaddrinfo(result);
            return false;
        }
        
        // 設定 reuse address
        char opt = 1;
        setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        // Bind
        res = bind(listen_socket_, result->ai_addr, (int)result->ai_addrlen);
        if (res == SOCKET_ERROR) {
            std::cerr << "bind failed: " << WSAGetLastError() << std::endl;
            closesocket(listen_socket_);
            freeaddrinfo(result);
            return false;
        }
        
        freeaddrinfo(result);
        
        // Listen
        if (listen(listen_socket_, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "listen failed: " << WSAGetLastError() << std::endl;
            closesocket(listen_socket_);
            return false;
        }
        
        running_ = true;
        std::cout << "Server listening on port " << port_ << std::endl;
        
        // 啟動 accept 執行緒
        std::thread accept_thread(&TCPServer::accept_loop, this);
        accept_thread.detach();
        
        return true;
    }
    
    void stop() {
        running_ = false;
        
        if (listen_socket_ != INVALID_SOCKET) {
            closesocket(listen_socket_);
            listen_socket_ = INVALID_SOCKET;
        }
        
        // 等待所有客戶端執行緒結束
        for (auto& t : client_threads_) {
            if (t.joinable()) {
                t.join();
            }
        }
    }
    
private:
    void accept_loop() {
        while (running_) {
            SOCKET client_socket = accept(listen_socket_, nullptr, nullptr);
            
            if (client_socket == INVALID_SOCKET) {
                if (running_) {
                    std::cerr << "accept failed: " << WSAGetLastError() << std::endl;
                }
                continue;
            }
            
            // 設定 TCP_NODELAY (關閉 Nagle 演算法)
            char nodelay = 1;
            setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, 
                      &nodelay, sizeof(nodelay));
            
            // 建立新執行緒處理客戶端
            client_threads_.emplace_back(&TCPServer::handle_client, this, client_socket);
        }
    }
    
    void handle_client(SOCKET client_socket) {
        std::cout << "Client connected" << std::endl;
        
        char buffer[4096];
        int result;
        
        do {
            result = recv(client_socket, buffer, sizeof(buffer), 0);
            
            if (result > 0) {
                // Echo back
                send(client_socket, buffer, result, 0);
                
                // 印出收到的訊息
                std::string msg(buffer, result);
                std::cout << "Received: " << msg << std::endl;
            }
            else if (result == 0) {
                std::cout << "Client disconnected" << std::endl;
            }
            else {
                std::cerr << "recv failed: " << WSAGetLastError() << std::endl;
            }
            
        } while (result > 0);
        
        closesocket(client_socket);
    }
};