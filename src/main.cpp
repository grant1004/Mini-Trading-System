#include "trading_system.h"
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>

// 全域的交易系統實例
std::unique_ptr<TradingSystem> g_tradingSystem;

// 信號處理函式 (優雅關閉)
void signalHandler(int signal) {
    std::cout << "\n🛑 Received signal " << signal << ", shutting down gracefully..." << std::endl;
    
    if (g_tradingSystem) {
        g_tradingSystem->stop();
    }
    
    exit(0);
}

// 監控執行緒 (定期印出統計資訊)
void monitoringThread() {
    while (g_tradingSystem && g_tradingSystem->isRunning()) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        
        if (g_tradingSystem && g_tradingSystem->isRunning()) {
            g_tradingSystem->printStatistics();
        }
    }
}

// 測試客戶端模擬 (供開發測試用)
void simulateTestClient() {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    std::cout << "\n🧪 Starting test client simulation..." << std::endl;
    
    // 模擬 FIX 訊息處理
    // 這裡可以加入自動化測試邏輯
    
    std::cout << "🧪 Test client simulation completed" << std::endl;
}


int main(int argc, char* argv[]) {
    std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                           MTS - Mini Trading System                          ║" << std::endl;
    std::cout << "║                          Production-Ready Demo                               ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝" << std::endl;
    
    // 解析命令列參數
    int port = 8080;
    bool enableTestClient = false;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--test") {
            enableTestClient = true;
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --port <port>    Set server port (default: 8080)" << std::endl;
            std::cout << "  --test           Enable test client simulation" << std::endl;
            std::cout << "  --help           Show this help message" << std::endl;
            return 0;
        }
    }
    
    try {
        // 設定信號處理 (Ctrl+C 優雅關閉)
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);
        
        // 建立交易系統
        g_tradingSystem = std::make_unique<TradingSystem>(port);
        
        // 啟動系統
        if (!g_tradingSystem->start()) {
            std::cerr << "❌ Failed to start trading system" << std::endl;
            return 1;
        }
        
        // 啟動監控執行緒
        std::thread monitor(monitoringThread);
        monitor.detach();
        
        // 如果啟用測試模式，啟動測試客戶端
        if (enableTestClient) {
            std::thread testClient(simulateTestClient);
            testClient.detach();
        }
        
        // 顯示系統資訊
        std::cout << "\n📋 System Information:" << std::endl;
        std::cout << "  Port: " << port << std::endl;
        std::cout << "  Test Mode: " << (enableTestClient ? "Enabled" : "Disabled") << std::endl;
        std::cout << "  PID: " << getpid() << std::endl;
        
        // 顯示操作說明
        std::cout << "\n📖 Available Commands:" << std::endl;
        std::cout << "  'stats'  - Show system statistics" << std::endl;
        std::cout << "  'help'   - Show this help" << std::endl;
        std::cout << "  'quit'   - Shutdown system" << std::endl;
        std::cout << "  Ctrl+C   - Graceful shutdown" << std::endl;
        
        std::cout << "\n🚀 Trading System is running. Waiting for connections..." << std::endl;
        std::cout << "💡 Connect using: telnet localhost " << port << std::endl;
        
        // 主迴圈 (處理控制台命令)
        std::string command;
        while (g_tradingSystem->isRunning()) {
            std::cout << "\nMTS> ";
            std::getline(std::cin, command);
            
            if (command == "quit" || command == "exit") {
                std::cout << "🛑 Initiating shutdown..." << std::endl;
                break;
            } else if (command == "stats") {
                g_tradingSystem->printStatistics();
            } else if (command == "help") {
                std::cout << "Available commands: stats, help, quit" << std::endl;
            } else if (!command.empty()) {
                std::cout << "Unknown command: " << command << std::endl;
                std::cout << "Type 'help' for available commands" << std::endl;
            }
        }
        
        // 關閉系統
        g_tradingSystem->stop();
        g_tradingSystem.reset();
        
    } catch (const std::exception& e) {
        std::cerr << "💥 Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "👋 Goodbye!" << std::endl;
    return 0;
}



// int main() { // test tcp server 
//     TCPServer server(8080);
    
//     // 設定回調函式
//     server.setConnectionCallback([](int clientId) {
//         std::cout << "✅ Client " << clientId << " connected!" << std::endl;
//     });
    
//     server.setMessageCallback([&server](int clientId, const std::string& message) {
//         std::cout << "📨 Message from " << clientId << ": " << message << std::endl;
        
//         // Echo back
//         server.sendMessage(clientId, "Echo: " + message + "\n");
//     });
    
//     server.setDisconnectionCallback([](int clientId) {
//         std::cout << "📴 Client " << clientId << " disconnected!" << std::endl;
//     });
    
//     server.setErrorCallback([](const std::string& error) {
//         std::cout << "❌ Server error: " << error << std::endl;
//     });
    
//     // 啟動服務器
//     if (server.start()) {
//         std::cout << "Server started successfully!" << std::endl;
        
//         // 等待用戶輸入
//         std::string input;
//         while (std::getline(std::cin, input)) {
//             if (input == "quit") break;
            
//             if (input == "status") {
//                 std::cout << "Active clients: " << server.getActiveClientCount() << std::endl;
//             }
//         }
//     }
    
//     return 0;
// }
// ===== 開發者備註 =====
/*
編譯指令:
g++ -std=c++17 -pthread -I./src \
    src/main.cpp \
    src/TradingSystem.cpp \
    src/core/MatchingEngine.cpp \
    src/core/Order.cpp \
    src/core/OrderBook.cpp \
    src/protocol/FixMessage.cpp \
    src/protocol/FixSession.cpp \
    src/protocol/FixMessageBuilder.cpp \
    src/network/tcp_server.cpp \
    -o mts_server

測試命令:
1. 啟動服務器: ./mts_server --port 8080 --test
2. 連線測試: telnet localhost 8080
3. 發送 FIX Logon: 8=FIX.4.2|9=73|35=A|49=CLIENT|56=SERVER|34=1|52=20250101-12:00:00|98=0|108=30|10=123|
4. 發送新訂單: 8=FIX.4.2|9=154|35=D|49=CLIENT|56=SERVER|34=2|52=20250101-12:00:01|11=ORDER123|55=AAPL|54=1|38=100|40=2|44=150.50|59=0|10=456|

架構亮點:
1. 模組化設計 - 各組件職責清晰
2. 異步處理 - 避免阻塞主執行緒
3. 錯誤處理 - 完善的異常捕獲
4. 資源管理 - 優雅的啟動和關閉
5. 監控統計 - 實時系統狀態
6. 可擴展性 - 易於新增功能

面試展示重點:
1. 展示系統啟動過程
2. 演示 FIX 協議處理
3. 說明撮合引擎邏輯
4. 展現錯誤處理機制
5. 討論效能優化策略
*/