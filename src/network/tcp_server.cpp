// enhanced_tcp_server.h
#pragma once
#include "win_socket.h"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <string>

/*  
┌─────────────────┐
│   TCPServer     │
├─────────────────┤
│ • 監聽連線       │
│ • 管理客戶端     │
│ • 事件分發       │
└─────────────────┘
         │
    ┌─────┴─────┐
    │  回調介面  │
    └─────┬─────┘
         │
┌────────┴────────┐
│   使用者程式     │
│ (TradingSystem) │
└─────────────────┘

*/
class TCPServer {
public:
    // 回調函式類型定義
    using ConnectionCallback = std::function<void(int clientSocket)>;
    using MessageCallback = std::function<void(int clientSocket, const std::string& message)>;
    using DisconnectionCallback = std::function<void(int clientSocket)>;
    using ErrorCallback = std::function<void(const std::string& error)>;

private:
    SOCKET listen_socket_ = INVALID_SOCKET;
    int port_;
    std::atomic<bool> running_{false};
    std::vector<std::thread> client_threads_;
    
    // 客戶端管理
    std::unordered_map<int, SOCKET> active_clients_;
    std::mutex clients_mutex_;
    std::atomic<int> next_client_id_{1};
    
    // 回調函式
    ConnectionCallback on_connection_;
    MessageCallback on_message_;
    DisconnectionCallback on_disconnection_;
    ErrorCallback on_error_;
    
public:
    TCPServer(int port) : port_(port) {
        std::cout << "🌐 Enhanced TCP Server created on port " << port << std::endl;
    }
    
    ~TCPServer() {
        stop();
    }
    
    // ===== 回調函式設定 =====
    void setConnectionCallback(ConnectionCallback callback) {
        on_connection_ = std::move(callback);
    }
    
    void setMessageCallback(MessageCallback callback) {
        on_message_ = std::move(callback);
    }
    
    void setDisconnectionCallback(DisconnectionCallback callback) {
        on_disconnection_ = std::move(callback);
    }
    
    void setErrorCallback(ErrorCallback callback) {
        on_error_ = std::move(callback);
    }
    
    // ===== 服務器生命週期 =====
    bool start() {
        try {
            std::cout << "🚀 Starting Enhanced TCP Server on port " << port_ << std::endl;
            
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
                notifyError("getaddrinfo failed: " + std::to_string(res));
                return false;
            }
            
            // 建立 socket
            listen_socket_ = socket(result->ai_family, 
                                   result->ai_socktype, 
                                   result->ai_protocol);
            
            if (listen_socket_ == INVALID_SOCKET) {
                notifyError("socket failed: " + std::to_string(WSAGetLastError()));
                freeaddrinfo(result);
                return false;
            }
            
            // 設定 socket 選項
            char opt = 1;
            setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
            
            // Bind
            res = bind(listen_socket_, result->ai_addr, (int)result->ai_addrlen);
            if (res == SOCKET_ERROR) {
                notifyError("bind failed: " + std::to_string(WSAGetLastError()));
                closesocket(listen_socket_);
                freeaddrinfo(result);
                return false;
            }
            
            freeaddrinfo(result);
            
            // Listen
            if (listen(listen_socket_, SOMAXCONN) == SOCKET_ERROR) {
                notifyError("listen failed: " + std::to_string(WSAGetLastError()));
                closesocket(listen_socket_);
                return false;
            }
            
            running_ = true;
            std::cout << "✅ Server listening on port " << port_ << std::endl;
            
            // 啟動 accept 執行緒
            std::thread accept_thread(&TCPServer::accept_loop, this);
            accept_thread.detach();
            
            return true;
            
        } catch (const std::exception& e) {
            notifyError("Server start exception: " + std::string(e.what()));
            return false;
        }
    }
    
    void stop() {
        if (!running_.load()) {
            return;
        }
        
        std::cout << "🛑 Stopping Enhanced TCP Server..." << std::endl;
        running_ = false;
        
        // 關閉監聽 socket
        if (listen_socket_ != INVALID_SOCKET) {
            closesocket(listen_socket_);
            listen_socket_ = INVALID_SOCKET;
        }
        
        // 關閉所有客戶端連線
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            for (const auto& pair : active_clients_) {
                closesocket(pair.second);
                std::cout << "📴 Closed client connection: " << pair.first << std::endl;
            }
            active_clients_.clear();
        }
        
        // 等待所有客戶端執行緒結束
        for (auto& t : client_threads_) {
            if (t.joinable()) {
                t.join();
            }
        }
        client_threads_.clear();
        
        std::cout << "✅ Enhanced TCP Server stopped" << std::endl;
    }
    
    // ===== 訊息發送 =====
    bool sendMessage(int clientId, const std::string& message) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        
        auto it = active_clients_.find(clientId);
        if (it == active_clients_.end()) {
            std::cerr << "❌ Client " << clientId << " not found" << std::endl;
            return false;
        }
        
        try {
            int result = send(it->second, message.c_str(), message.length(), 0);
            if (result == SOCKET_ERROR) {
                std::cerr << "❌ Send failed for client " << clientId << ": " << WSAGetLastError() << std::endl;
                return false;
            }
            
            std::cout << "📤 Sent to client " << clientId << ": " << message.substr(0, 50) << "..." << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Send exception for client " << clientId << ": " << e.what() << std::endl;
            return false;
        }
    }
    
    // ===== 狀態查詢 =====
    bool isRunning() const {
        return running_.load();
    }
    
    size_t getActiveClientCount(){
        std::lock_guard<std::mutex> lock(clients_mutex_);
        return active_clients_.size();
    }
    
    std::vector<int> getActiveClientIds(){
        std::lock_guard<std::mutex> lock(clients_mutex_);
        std::vector<int> ids;
        for (const auto& pair : active_clients_) {
            ids.push_back(pair.first);
        }
        return ids;
    }
    
private:
    // ===== 網路處理 =====
    void accept_loop() {
        std::cout << "🔄 Accept loop started" << std::endl;
        
        while (running_) {
            SOCKET client_socket = accept(listen_socket_, nullptr, nullptr);
            
            if (client_socket == INVALID_SOCKET) {
                if (running_) {
                    notifyError("accept failed: " + std::to_string(WSAGetLastError()));
                }
                continue;
            }
            
            // 設定 TCP_NODELAY (關閉 Nagle 演算法，減少延遲)
            char nodelay = 1;
            setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, 
                      &nodelay, sizeof(nodelay));
            
            // 設定 SO_KEEPALIVE (保持連線檢測)
            char keepalive = 1;
            setsockopt(client_socket, SOL_SOCKET, SO_KEEPALIVE, 
                      &keepalive, sizeof(keepalive));
            
            // 分配客戶端 ID
            int client_id = next_client_id_.fetch_add(1);
            
            // 註冊客戶端
            {
                std::lock_guard<std::mutex> lock(clients_mutex_);
                active_clients_[client_id] = client_socket;
            }
            
            std::cout << "📞 New client connected: ID=" << client_id << ", Socket=" << client_socket << std::endl;
            
            // 通知新連線
            if (on_connection_) {
                try {
                    on_connection_(client_id);
                } catch (const std::exception& e) {
                    std::cerr << "❌ Connection callback error: " << e.what() << std::endl;
                }
            }
            
            // 建立客戶端處理執行緒
            client_threads_.emplace_back(&TCPServer::handle_client, this, client_id, client_socket);
        }
        
        std::cout << "🔄 Accept loop ended" << std::endl;
    }
    
    void handle_client(int client_id, SOCKET client_socket) {
        std::cout << "🔗 Client handler started for ID=" << client_id << std::endl;
        
        char buffer[4096];
        std::string message_buffer; // 用於處理不完整的訊息
        
        while (running_) {
            int result = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            
            if (result > 0) {
                buffer[result] = '\0'; // 確保字串結尾
                
                // 處理接收到的資料
                message_buffer += std::string(buffer, result);
                
                // 處理完整的訊息 (以換行符分隔)
                size_t pos = 0;
                while ((pos = message_buffer.find('\n')) != std::string::npos || 
                       (pos = message_buffer.find('\r')) != std::string::npos) {
                    
                    std::string complete_message = message_buffer.substr(0, pos);
                    message_buffer.erase(0, pos + 1);
                    
                    if (!complete_message.empty()) {
                        // 清理訊息 (移除多餘的空白字符)
                        complete_message.erase(
                            std::remove(complete_message.begin(), complete_message.end(), '\r'), 
                            complete_message.end()
                        );
                        
                        std::cout << "📨 Received from client " << client_id << ": " << complete_message << std::endl;
                        
                        // 通知訊息回調
                        if (on_message_) {
                            try {
                                on_message_(client_id, complete_message);
                            } catch (const std::exception& e) {
                                std::cerr << "❌ Message callback error: " << e.what() << std::endl;
                            }
                        }
                    }
                }
                
                // 如果緩衝區太大，清理掉
                if (message_buffer.size() > 8192) {
                    std::cout << "⚠️ Message buffer too large for client " << client_id << ", clearing" << std::endl;
                    message_buffer.clear();
                }
                
            } else if (result == 0) {
                std::cout << "📴 Client " << client_id << " disconnected normally" << std::endl;
                break;
            } else {
                std::cerr << "❌ recv failed for client " << client_id << ": " << WSAGetLastError() << std::endl;
                break;
            }
        }
        
        // 清理客戶端
        cleanup_client(client_id, client_socket);
    }
    
    void cleanup_client(int client_id, SOCKET client_socket) {
        std::cout << "🧹 Cleaning up client " << client_id << std::endl;
        
        // 從活躍客戶端列表中移除
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            active_clients_.erase(client_id);
        }
        
        // 關閉 socket
        closesocket(client_socket);
        
        // 通知斷線回調
        if (on_disconnection_) {
            try {
                on_disconnection_(client_id);
            } catch (const std::exception& e) {
                std::cerr << "❌ Disconnection callback error: " << e.what() << std::endl;
            }
        }
        
        std::cout << "✅ Client " << client_id << " cleanup completed" << std::endl;
    }
    
    // ===== 工具方法 =====
    void notifyError(const std::string& error) {
        std::cerr << "🚨 TCP Server Error: " << error << std::endl;
        
        if (on_error_) {
            try {
                on_error_(error);
            } catch (const std::exception& e) {
                std::cerr << "❌ Error callback exception: " << e.what() << std::endl;
            }
        }
    }
};

// ===== 使用範例 =====
/*
int main() {
    TCPServer server(8080);
    
    // 設定回調函式
    server.setConnectionCallback([](int clientId) {
        std::cout << "✅ Client " << clientId << " connected!" << std::endl;
    });
    
    server.setMessageCallback([&server](int clientId, const std::string& message) {
        std::cout << "📨 Message from " << clientId << ": " << message << std::endl;
        
        // Echo back
        server.sendMessage(clientId, "Echo: " + message + "\n");
    });
    
    server.setDisconnectionCallback([](int clientId) {
        std::cout << "📴 Client " << clientId << " disconnected!" << std::endl;
    });
    
    server.setErrorCallback([](const std::string& error) {
        std::cout << "❌ Server error: " << error << std::endl;
    });
    
    // 啟動服務器
    if (server.start()) {
        std::cout << "Server started successfully!" << std::endl;
        
        // 等待用戶輸入
        std::string input;
        while (std::getline(std::cin, input)) {
            if (input == "quit") break;
            
            if (input == "status") {
                std::cout << "Active clients: " << server.getActiveClientCount() << std::endl;
            }
        }
    }
    
    return 0;
}
*/