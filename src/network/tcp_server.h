// tcp_server.h
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
namespace mts::tcp_server {
    
class TCPServer {
public:
    // 回調函式類型定義
    using ConnectionCallback = std::function<void(SOCKET clientSocket)>;
    using MessageCallback = std::function<void(SOCKET clientSocket, const std::string& message)>;
    using DisconnectionCallback = std::function<void(SOCKET clientSocket)>;
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
    explicit TCPServer(int port);
    ~TCPServer();

    // 根據 clientId 取得 socket
    SOCKET getClientSocket(int clientId) ;
    
    // 根據 socket 取得 clientId（可能用得到）
    int getClientId(SOCKET clientSocket) ;

    // ===== 回調函式設定 =====
    void setConnectionCallback(ConnectionCallback callback) ;

    void setMessageCallback(MessageCallback callback) ;
    
    void setDisconnectionCallback(DisconnectionCallback callback) ;
    
    void setErrorCallback(ErrorCallback callback) ;
    
    // ===== 服務器生命週期 =====
    bool start() ;
    
    void stop() ;
    
    // ===== 訊息發送 =====
    bool sendMessage(int clientId, const std::string& message) ;
    
    bool sendMessage(SOCKET clientSocket, const std::string& message);

    // ===== 狀態查詢 =====
    bool isRunning() const ;
    
    size_t getActiveClientCount() ;
    
    std::vector<int> getActiveClientIds() ;
    
private:
    // ===== 網路處理 =====
    void accept_loop() ;
    
    void handle_client(int client_id, SOCKET client_socket) ;
    
    void cleanup_client(int client_id, SOCKET client_socket) ;
    
    // ===== 工具方法 =====
    void notifyError(const std::string& error) ;
};

}