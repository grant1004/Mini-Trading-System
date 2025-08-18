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
            // g_tradingSystem->printStatistics();
        }
    }
}


// 替換 main.cpp 中的 simulateTestClient() 函式

void simulateTestClient() {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    std::cout << "\n🧪 Starting test client simulation..." << std::endl;
    std::cout << "基於您提供的確切 FIX 訊息進行測試" << std::endl;
    
    // 詢問使用者要執行哪個測試
    std::cout << "\n請選擇測試場景:" << std::endl;
    std::cout << "1 - CLIENT1 完整流程 (登入→買AAPL→賣MSFT→查詢→取消→登出)" << std::endl;
    std::cout << "2 - 多客戶端撮合測試 (CLIENT1, CLIENT2, CLIENT3)" << std::endl;
    std::cout << "3 - 錯誤處理測試 (CLIENT4錯誤CompID, 缺欄位, 負價格)" << std::endl;
    std::cout << "4 - 市價單和心跳測試 (CLIENT1重登+市價單+心跳)" << std::endl;
    std::cout << "5 - 執行全部測試 (按順序)" << std::endl;
    std::cout << "請輸入數字 (1-5): ";
    
    int choice = 5; // 預設執行全部
    // 由於是在另一個執行緒中，簡化為自動執行全部
    
    auto createConnection = [](const std::string& clientName) -> SOCKET {
        SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "❌ " << clientName << ": Failed to create socket" << std::endl;
            return INVALID_SOCKET;
        }
        
        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(8080);
        inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
        
        if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) != 0) {
            std::cerr << "❌ " << clientName << ": Failed to connect" << std::endl;
            closesocket(clientSocket);
            return INVALID_SOCKET;
        }
        
        std::cout << "✅ " << clientName << ": Connected" << std::endl;
        return clientSocket;
    };
    
    auto sendFixMessage = [](SOCKET socket, const std::string& message, const std::string& description) {
        if (socket == INVALID_SOCKET) return false;
        
        std::string fullMessage = message + "\r\n";
        std::cout << "📤 " << description << std::endl;
        std::cout << "    " << message << std::endl;
        
        int result = send(socket, fullMessage.c_str(), fullMessage.length(), 0);
        if (result == SOCKET_ERROR) {
            std::cerr << "❌ Send failed" << std::endl;
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(150)); // 等待處理
        return true;
    };
    
    try {
        switch (choice) {
            case 1: {
                std::cout << "\n" << std::string(60, '=') << std::endl;
                std::cout << "測試 1: CLIENT1 完整流程" << std::endl;
                std::cout << std::string(60, '=') << std::endl;
                
                SOCKET client1 = createConnection("CLIENT1");
                if (client1 != INVALID_SOCKET) {
                    sendFixMessage(client1, "8=FIX.4.4|9=70|35=A|49=CLIENT1|56=SERVER|34=1|52=20230817-10:00:00|98=0|108=30|141=Y|10=027|\r", "CLIENT1 登入");
                    sendFixMessage(client1, "8=FIX.4.4|9=115|35=D|49=CLIENT1|56=SERVER|34=2|52=20230817-10:01:00|11=BUY_001|17=EXEC_001|55=AAPL|54=1|38=100|40=2|44=150.00|59=0|10=151|\r", "CLIENT1 買入 AAPL 限價單");
                    sendFixMessage(client1, "8=FIX.4.4|9=115|35=D|49=CLIENT1|56=SERVER|34=3|52=20230817-10:02:00|11=SELL_001|17=EXEC_002|55=MSFT|54=2|38=50|40=2|44=300.00|59=0|10=200|\r", "CLIENT1 賣出 MSFT 限價單");
                    sendFixMessage(client1, "8=FIX.4.4|9=76|35=H|49=CLIENT1|56=SERVER|34=4|52=20230817-10:03:00|11=BUY_001|55=AAPL|54=1|10=006|\r", "CLIENT1 查詢 BUY_001 狀態");
                    sendFixMessage(client1, "8=FIX.4.4|9=111|35=F|49=CLIENT1|56=SERVER|34=5|52=20230817-10:04:00|11=CANCEL_001|41=BUY_001|55=AAPL|54=1|60=20230817-10:04:00|10=102|\r", "CLIENT1 取消 BUY_001");
                    sendFixMessage(client1, "8=FIX.4.4|9=69|35=5|49=CLIENT1|56=SERVER|34=6|52=20230817-10:05:00|58=Normal logout|10=169|\r", "CLIENT1 登出");
                    closesocket(client1);
                }
                break;
            }
            
            case 2: {
                std::cout << "" << std::string(60, '=') << std::endl;
                std::cout << "測試 2: 多客戶端撮合測試" << std::endl;
                std::cout << std::string(60, '=') << std::endl;
                
                SOCKET client2 = createConnection("CLIENT2");
                SOCKET client3 = createConnection("CLIENT3");
                
                if (client2 != INVALID_SOCKET) {
                    sendFixMessage(client2, "8=FIX.4.4|9=70|35=A|49=CLIENT2|56=SERVER|34=1|52=20230817-10:06:00|98=0|108=30|141=Y|10=034|\r", "CLIENT2 登入");
                    sendFixMessage(client2, "8=FIX.4.4|9=114|35=D|49=CLIENT2|56=SERVER|34=2|52=20230817-10:07:00|11=BUY_002|17=EXEC_003|55=MSFT|54=1|38=50|40=2|44=300.00|59=0|10=141|\r", "CLIENT2 買入 MSFT (應該撮合)");
                    closesocket(client2);
                }
                
                if (client3 != INVALID_SOCKET) {
                    sendFixMessage(client3, "8=FIX.4.4|9=70|35=A|49=CLIENT3|56=SERVER|34=1|52=20230817-10:08:00|98=0|108=30|141=Y|10=037|\r", "CLIENT3 登入");
                    sendFixMessage(client3, "8=FIX.4.4|9=117|35=D|49=CLIENT3|56=SERVER|34=2|52=20230817-10:09:00|11=BUY_003|17=EXEC_004|55=GOOGL|54=1|38=200|40=2|44=2500.00|59=0|10=052|\r", "CLIENT3 買入 GOOGL");
                    sendFixMessage(client3, "8=FIX.4.4|9=118|35=D|49=CLIENT3|56=SERVER|34=3|52=20230817-10:10:00|11=SELL_003|17=EXEC_005|55=GOOGL|54=2|38=150|40=2|44=2505.00|59=0|10=121|\r", "CLIENT3 賣出 GOOGL");
                    closesocket(client3);
                }
                break;
            }
            
            case 3: {
                std::cout << "" << std::string(60, '=') << std::endl;
                std::cout << "測試 3: 錯誤處理測試" << std::endl;
                std::cout << std::string(60, '=') << std::endl;
                
                SOCKET client4 = createConnection("CLIENT4");
                if (client4 != INVALID_SOCKET) {
                    sendFixMessage(client4, "8=FIX.4.4|9=69|35=A|49=CLIENT4|56=WRONG|34=1|52=20230817-10:11:00|98=0|108=30|141=Y|10=222|\r", "CLIENT4 錯誤 TargetCompID");
                    closesocket(client4);
                }
                
                SOCKET client1_err = createConnection("CLIENT1_ERROR");
                if (client1_err != INVALID_SOCKET) {
                    sendFixMessage(client1_err, "8=FIX.4.4|9=70|35=A|49=CLIENT1|56=SERVER|34=1|52=20230817-10:14:00|98=0|108=30|141=Y|10=032|\r", "CLIENT1 重新登入");
                    sendFixMessage(client1_err, "8=FIX.4.4|9=76|35=D|49=CLIENT1|56=SERVER|34=7|52=20230817-10:12:00|11=BAD_001|55=AAPL|54=1|10=220|\r", "CLIENT1 缺少數量欄位的訂單");
                    sendFixMessage(client1_err, "8=FIX.4.4|9=115|35=D|49=CLIENT1|56=SERVER|34=8|52=20230817-10:13:00|11=BAD_002|17=EXEC_006|55=AAPL|54=1|38=100|40=2|44=-10.00|59=0|10=117|\r", "CLIENT1 負價格訂單");
                    closesocket(client1_err);
                }
                break;
            }
            
            case 4: {
                std::cout << "" << std::string(60, '=') << std::endl;
                std::cout << "測試 4: 市價單和心跳測試" << std::endl;
                std::cout << std::string(60, '=') << std::endl;
                
                SOCKET client1_market = createConnection("CLIENT1_MARKET");
                if (client1_market != INVALID_SOCKET) {
                    sendFixMessage(client1_market, "8=FIX.4.4|9=70|35=A|49=CLIENT1|56=SERVER|34=1|52=20230817-10:14:00|98=0|108=30|141=Y|10=032|\r", "CLIENT1 登入");
                    sendFixMessage(client1_market, "8=FIX.4.4|9=105|35=D|49=CLIENT1|56=SERVER|34=2|52=20230817-10:15:00|11=MKT_001|17=EXEC_007|55=AAPL|54=1|38=100|40=1|59=0|10=210|\r", "CLIENT1 市價買單 AAPL");
                    sendFixMessage(client1_market, "8=FIX.4.4|9=104|35=D|49=CLIENT1|56=SERVER|34=3|52=20230817-10:16:00|11=MKT_002|17=EXEC_008|55=AAPL|54=2|38=75|40=1|59=0|10=177|\r", "CLIENT1 市價賣單 AAPL");
                    sendFixMessage(client1_market, "8=FIX.4.4|9=52|35=0|49=CLIENT1|56=SERVER|34=4|52=20230817-10:17:00|10=207|\r", "CLIENT1 心跳訊息");
                    sendFixMessage(client1_market, "8=FIX.4.4|9=65|35=1|49=CLIENT1|56=SERVER|34=5|52=20230817-10:18:00|112=TEST_123|10=221|\r", "CLIENT1 測試請求");
                    closesocket(client1_market);
                }
                break;
            }
            
            case 5:
            default: {
                std::cout << "🚀 執行全部測試場景..." << std::endl;
                
                // 測試 1: CLIENT1 完整流程
                std::cout << "" << std::string(60, '=') << std::endl;
                std::cout << "測試 1: CLIENT1 完整流程" << std::endl;
                std::cout << std::string(60, '=') << std::endl;
                
                SOCKET client1 = createConnection("CLIENT1");
                if (client1 != INVALID_SOCKET) {
                    sendFixMessage(client1, "8=FIX.4.4|9=70|35=A|49=CLIENT1|56=SERVER|34=1|52=20230817-10:00:00|98=0|108=30|141=Y|10=027|\r", "CLIENT1 登入");
                    sendFixMessage(client1, "8=FIX.4.4|9=115|35=D|49=CLIENT1|56=SERVER|34=2|52=20230817-10:01:00|11=BUY_001|17=EXEC_001|55=AAPL|54=1|38=100|40=2|44=150.00|59=0|10=151|\r", "CLIENT1 買入 AAPL");
                    sendFixMessage(client1, "8=FIX.4.4|9=115|35=D|49=CLIENT1|56=SERVER|34=3|52=20230817-10:02:00|11=SELL_001|17=EXEC_002|55=MSFT|54=2|38=50|40=2|44=300.00|59=0|10=200|\r", "CLIENT1 賣出 MSFT");
                    sendFixMessage(client1, "8=FIX.4.4|9=76|35=H|49=CLIENT1|56=SERVER|34=4|52=20230817-10:03:00|11=BUY_001|55=AAPL|54=1|10=006|\r", "CLIENT1 查詢訂單");
                    sendFixMessage(client1, "8=FIX.4.4|9=111|35=F|49=CLIENT1|56=SERVER|34=5|52=20230817-10:04:00|11=CANCEL_001|41=BUY_001|55=AAPL|54=1|60=20230817-10:04:00|10=102|\r", "CLIENT1 取消訂單");
                    sendFixMessage(client1, "8=FIX.4.4|9=69|35=5|49=CLIENT1|56=SERVER|34=6|52=20230817-10:05:00|58=Normal logout|10=169|\r", "CLIENT1 登出");
                    closesocket(client1);
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                
                // 測試 2: 多客戶端
                std::cout << "" << std::string(60, '=') << std::endl;
                std::cout << "測試 2: 多客戶端撮合測試" << std::endl;
                std::cout << std::string(60, '=') << std::endl;
                
                SOCKET client2 = createConnection("CLIENT2");
                SOCKET client3 = createConnection("CLIENT3");
                
                if (client2 != INVALID_SOCKET) {
                    sendFixMessage(client2, "8=FIX.4.4|9=70|35=A|49=CLIENT2|56=SERVER|34=1|52=20230817-10:06:00|98=0|108=30|141=Y|10=034|\r", "CLIENT2 登入");
                    sendFixMessage(client2, "8=FIX.4.4|9=114|35=D|49=CLIENT2|56=SERVER|34=2|52=20230817-10:07:00|11=BUY_002|17=EXEC_003|55=MSFT|54=1|38=50|40=2|44=300.00|59=0|10=141|\r", "CLIENT2 買入 MSFT");
                    closesocket(client2);
                }
                
                if (client3 != INVALID_SOCKET) {
                    sendFixMessage(client3, "8=FIX.4.4|9=70|35=A|49=CLIENT3|56=SERVER|34=1|52=20230817-10:08:00|98=0|108=30|141=Y|10=037|\r", "CLIENT3 登入");
                    sendFixMessage(client3, "8=FIX.4.4|9=117|35=D|49=CLIENT3|56=SERVER|34=2|52=20230817-10:09:00|11=BUY_003|17=EXEC_004|55=GOOGL|54=1|38=200|40=2|44=2500.00|59=0|10=052|\r", "CLIENT3 買入 GOOGL");
                    sendFixMessage(client3, "8=FIX.4.4|9=118|35=D|49=CLIENT3|56=SERVER|34=3|52=20230817-10:10:00|11=SELL_003|17=EXEC_005|55=GOOGL|54=2|38=150|40=2|44=2505.00|59=0|10=121|\r", "CLIENT3 賣出 GOOGL");
                    closesocket(client3);
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                
                // 測試 3: 錯誤處理
                std::cout << "" << std::string(60, '=') << std::endl;
                std::cout << "測試 3: 錯誤處理測試" << std::endl;
                std::cout << std::string(60, '=') << std::endl;
                
                SOCKET client4 = createConnection("CLIENT4");
                if (client4 != INVALID_SOCKET) {
                    sendFixMessage(client4, "8=FIX.4.4|9=69|35=A|49=CLIENT4|56=WRONG|34=1|52=20230817-10:11:00|98=0|108=30|141=Y|10=222|\r", "CLIENT4 錯誤 CompID");
                    closesocket(client4);
                }
                
                SOCKET client1_err = createConnection("CLIENT1_ERROR");
                if (client1_err != INVALID_SOCKET) {
                    sendFixMessage(client1_err, "8=FIX.4.4|9=70|35=A|49=CLIENT1|56=SERVER|34=1|52=20230817-10:14:00|98=0|108=30|141=Y|10=032|\r", "CLIENT1 重登");
                    sendFixMessage(client1_err, "8=FIX.4.4|9=76|35=D|49=CLIENT1|56=SERVER|34=7|52=20230817-10:12:00|11=BAD_001|55=AAPL|54=1|10=220|\r", "缺欄位訂單");
                    sendFixMessage(client1_err, "8=FIX.4.4|9=115|35=D|49=CLIENT1|56=SERVER|34=8|52=20230817-10:13:00|11=BAD_002|17=EXEC_006|55=AAPL|54=1|38=100|40=2|44=-10.00|59=0|10=117|\r", "負價格訂單");
                    closesocket(client1_err);
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                
                // 測試 4: 市價單和心跳
                std::cout << "" << std::string(60, '=') << std::endl;
                std::cout << "測試 4: 市價單和心跳測試" << std::endl;
                std::cout << std::string(60, '=') << std::endl;
                
                SOCKET client1_market = createConnection("CLIENT1_MARKET");
                if (client1_market != INVALID_SOCKET) {
                    sendFixMessage(client1_market, "8=FIX.4.4|9=70|35=A|49=CLIENT1|56=SERVER|34=1|52=20230817-10:14:00|98=0|108=30|141=Y|10=032|\r", "CLIENT1 登入");
                    sendFixMessage(client1_market, "8=FIX.4.4|9=105|35=D|49=CLIENT1|56=SERVER|34=2|52=20230817-10:15:00|11=MKT_001|17=EXEC_007|55=AAPL|54=1|38=100|40=1|59=0|10=210|\r", "市價買單");
                    sendFixMessage(client1_market, "8=FIX.4.4|9=104|35=D|49=CLIENT1|56=SERVER|34=3|52=20230817-10:16:00|11=MKT_002|17=EXEC_008|55=AAPL|54=2|38=75|40=1|59=0|10=177|\r", "市價賣單");
                    sendFixMessage(client1_market, "8=FIX.4.4|9=52|35=0|49=CLIENT1|56=SERVER|34=4|52=20230817-10:17:00|10=207|\r", "心跳訊息");
                    sendFixMessage(client1_market, "8=FIX.4.4|9=65|35=1|49=CLIENT1|56=SERVER|34=5|52=20230817-10:18:00|112=TEST_123|10=221|\r", "測試請求");
                    closesocket(client1_market);
                }
                break;
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 測試過程中發生錯誤: " << e.what() << std::endl;
    }
    
    std::cout << "" << std::string(60, '=') << std::endl;
    std::cout << "🎉 測試完成！請檢查系統統計資訊" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
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
