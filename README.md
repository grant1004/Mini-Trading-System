# Mini Trading System (MTS)

## 🎯 專案動機與背景

為了深入理解金融科技領域的核心技術，我決定構建一個完整的交易系統。

這個專案展示我對**高頻交易系統設計思維**、**金融業務邏輯**和**C++ 高性能程式設計**的學習成果。

## 🏗️ 技術棧與架構選擇

### **核心技術棧**
- **C++17/20**：現代 C++ 特性，RAII、智能指標、原子操作
- **多執行緒程式設計**：std::thread、std::mutex、std::atomic、condition_variable
- **網路程式設計**：Windows Socket → 未來遷移至 Boost.Asio
- **協議實作**：FIX 4.4 金融資訊交換協議
- **測試框架**：Google Test 單元測試、整合測試、效能測試
- **建置系統**：CMake + vcpkg 現代化依賴管理

### **系統架構設計**

#### **分層架構與核心類別**
```
┌─────────────────────────────────────────────────────────────────┐
│                    應用層 (Application Layer)                    │
├─────────────────────────────────────────────────────────────────┤
│ • TradingSystem          - 系統核心協調器，管理所有模組           │
│ • ClientSession          - 客戶端會話管理                        │
│ • OrderMapping           - 訂單映射與追蹤                        │
└─────────────────────────────────────────────────────────────────┘
                                   │
┌─────────────────────────────────────────────────────────────────┐
│                    協議層 (Protocol Layer)                       │
├─────────────────────────────────────────────────────────────────┤
│ • FixMessage             - FIX 協議訊息載體                      │
│ • FixSession             - FIX 會話生命週期管理                  │
│ • FixMessageBuilder      - FIX 訊息構建工具                      │
│ • FixTags                - FIX 標準欄位定義                      │
└─────────────────────────────────────────────────────────────────┘
                                   │
┌─────────────────────────────────────────────────────────────────┐
│                    網路層 (Network Layer)                        │
├─────────────────────────────────────────────────────────────────┤
│ • TCPServer              - TCP 多客戶端連線管理                  │
│ • WinSocket              - Windows Socket 抽象層                │
└─────────────────────────────────────────────────────────────────┘
                                   │
┌─────────────────────────────────────────────────────────────────┐
│                   撮合引擎 (Matching Engine)                     │
├─────────────────────────────────────────────────────────────────┤
│ • MatchingEngine         - 撮合引擎主控制器                      │
│ • ExecutionReport        - 執行報告結構                          │
│ • MarketDataSnapshot     - 市場行情快照                          │
│ • EngineStatistics       - 引擎效能統計                          │
└─────────────────────────────────────────────────────────────────┘
                                   │
┌─────────────────────────────────────────────────────────────────┐
│                     資料層 (Data Layer)                          │
├─────────────────────────────────────────────────────────────────┤
│ • Order                  - 訂單基礎資料結構                      │
│ • OrderBook              - 完整訂單簿管理                        │
│ • OrderBookSide          - 單邊訂單簿 (買/賣)                   │
│ • Trade                  - 交易記錄結構                          │
└─────────────────────────────────────────────────────────────────┘
```

#### **核心設計模式**
- **分層架構 (Layered Architecture)**: 清晰的職責分離與模組化設計
- **回調模式 (Callback Pattern)**: 事件驅動的異步處理機制
- **工廠模式 (Factory Pattern)**: FIX 訊息的動態構建
- **觀察者模式 (Observer Pattern)**: 執行報告與市場資料分發
- **生產者-消費者模式**: 撮合引擎的訊息佇列處理

#### **類別互動與資料流**
```
客戶端連線 → TCPServer → TradingSystem → FixSession
     ↓
FIX 訊息解析 → FixMessage → 業務邏輯驗證 → Order 物件創建
     ↓
MatchingEngine → OrderBook/OrderBookSide → 撮合處理
     ↓
Trade 生成 → ExecutionReport → FixMessage 回應 → 客戶端
```

**詳細互動流程**:
1. **`TCPServer`** 接受新客戶端連線，觸發 `TradingSystem::handleNewConnection()`
2. **`TradingSystem`** 為每個客戶端創建 `ClientSession` 與 `FixSession`
3. **`FixSession`** 處理 FIX 協議層面的訊息解析與會話管理
4. 業務訊息通過 `TradingSystem::convertFixToOrder()` 轉換為 `Order` 物件
5. **`MatchingEngine`** 接收 Order，分發至對應的 `OrderBook`
6. **`OrderBook`** 透過 `OrderBookSide` 進行買賣撮合，產生 `Trade`
7. **`ExecutionReport`** 記錄執行結果，經 FIX 協議回傳客戶端

#### **執行緒模型與並發設計**
```
主執行緒 (Main Thread)
├── TradingSystem 生命週期管理
└── 控制台介面處理

網路執行緒池 (Network Thread Pool) 
├── TCPServer::accept_loop()          - 監聽新連線
├── TCPServer::handle_client()        - 每客戶端獨立執行緒
└── ClientSession 訊息處理

撮合引擎執行緒 (Matching Thread)
├── MatchingEngine::processingLoop()  - 專用撮合處理
├── 內部訊息佇列 (std::queue + mutex)
└── 原子統計更新 (std::atomic)

健康檢查執行緒 (Health Check Thread)
├── TradingSystem::performSessionHealthCheck()
└── 週期性清理無效會話
```

**執行緒安全機制**:
- **`std::atomic<T>`**: 無鎖統計資訊更新
- **`std::mutex`**: 關鍵資源保護 (會話映射、訊息佇列)
- **`std::shared_mutex`**: 讀寫分離 (OrderBook 查詢 vs 更新)
- **`std::condition_variable`**: 生產者-消費者同步

## 📚 深度學習成果

### **金融交易系統核心概念**

#### **FIX 協議深度理解**
- **訊息結構**：BeginString、BodyLength、MsgType、CheckSum 的作用
- **業務訊息**：NewOrderSingle (35=D)、ExecutionReport (35=8)、OrderCancelRequest (35=F)
- **會話管理**：Logon/Logout 流程、心跳機制、序列號管理
- **錯誤處理**：Reject 訊息、重送機制、會話恢復

#### **撮合引擎核心邏輯**
- **價格-時間優先原則**：全球交易所通用的撮合演算法
- **訂單生命週期**：New → PartiallyFilled → Filled 的狀態轉換
- **市場資料生成**：Best Bid/Ask、市場深度、成交資訊
- **風險管理**：價格檢查、數量限制、符號驗證

#### **訂單類型與業務邏輯**
- **限價單 (Limit Order)**：指定價格的掛單機制
- **市價單 (Market Order)**：立即以最佳價格成交
- **停損單 (Stop Order)**：條件觸發的風險控制
- **時效性管理**：Day、GTC、IOC、FOK 的實作差異

### **C++ 高性能程式設計精進**

#### **多執行緒並發設計**
```cpp
// 執行緒安全的併發處理
class MatchingEngine {
    std::atomic<bool> running_{false};                    // 原子狀態管理
    std::mutex messageQueueMutex_;                        // 互斥鎖保護共享資源
    std::condition_variable messageQueueCV_;              // 條件變數實現等待/通知
    std::queue<InternalMessagePtr> incomingMessages_;     // 執行緒間訊息傳遞
    
    void processingLoop();  // 專用執行緒處理撮合邏輯
};
```

#### **記憶體管理與效能優化**
- **智能指標應用**：std::shared_ptr、std::unique_ptr 自動資源管理
- **移動語意**：std::move 減少不必要的拷貝開銷
- **RAII 原則**：資源取得即初始化，確保資源正確釋放
- **原子操作**：std::atomic 實現無鎖併發，提升效能

#### **現代 C++ 特性運用**
- **Lambda 表達式**：回調函式的優雅實作
- **auto 類型推導**：提升程式碼可讀性
- **range-based for**：現代化的迭代器使用
- **constexpr**：編譯期常數優化

#### **測試驅動開發**
```cpp
// Google Test 完整測試覆蓋
TEST_F(OrderBookTest, BasicMatching) {
    // 單元測試：驗證撮合邏輯正確性
}

TEST_F(FixMessageTest, SerializationRoundTrip) {
    // 協議測試：確保訊息解析的正確性
}
```

### **系統設計與架構思維**

#### **事件驅動架構**
- **回調機制**：std::function 實現組件間解耦
- **訊息佇列**：異步處理提升系統響應性
- **觀察者模式**：執行報告與市場資料的事件分發

#### **分層設計原則**
- **單一職責**：每個類別專注於特定功能
- **依賴注入**：介面抽象化，提升可測試性
- **模組化設計**：組件可獨立開發、測試、部署

#### **併發與同步**
- **執行緒池模式**：每客戶端獨立執行緒處理
- **生產者-消費者模式**：撮合引擎的訊息處理機制
- **讀寫分離**：市場資料查詢與訂單處理分離

## 🎖️ 專案成果與價值

### **技術深度展示**
- **完整的端到端實作**：從 TCP 連線到業務邏輯的全棧開發
- **企業級程式碼品質**：完整的測試覆蓋、錯誤處理、資源管理
- **效能意識**：微秒級延遲測量、吞吐量統計、記憶體優化

### **業務理解展現**
- **金融協議精通**：FIX 4.4 標準的深度實作
- **交易邏輯掌握**：撮合演算法、風險管理、市場資料處理
- **系統思維**：可擴展性、高可用性、監控機制的設計考量

### **工程實踐能力**
- **現代化開發流程**：Git 版本控制、CMake 建置、自動化測試

## 🚀 與真實交易所的對標

雖然在**規模**和**效能**上與生產級交易所存在差距，但在**核心概念**和**設計思維**上保持一致：

| 概念領域 | MTS 實作 | 交易所標準 | 一致性 |
|---------|---------|-----------|--------|
| **協議處理** | FIX 4.4 完整實作 | FIX 4.4 標準 | ✅ 完全一致 |
| **撮合演算法** | 價格-時間優先 | 全球標準演算法 | ✅ 邏輯相同 |
| **訂單管理** | 完整生命週期 | 標準狀態機 | ✅ 流程一致 |
| **架構設計** | 分層事件驅動 | 企業級架構 | ✅ 模式相同 |
| **併發處理** | 多執行緒安全 | 高併發設計 | ✅ 原則相同 |
