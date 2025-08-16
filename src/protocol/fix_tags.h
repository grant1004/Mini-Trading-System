// ============================================================================
// 3. fix_tags.h - 專門定義業務相關的 FIX Tags
// ============================================================================
#pragma once

namespace mts {
namespace protocol {

// 業務相關的 FIX Tags
namespace FixTags {
    // 訂單相關
    constexpr int ClOrdID = 11;       // Client Order ID
    constexpr int Symbol = 55;        // 交易標的
    constexpr int Side = 54;          // 買賣方向
    constexpr int OrderQty = 38;      // 訂單數量
    constexpr int OrdType = 40;       // 訂單類型
    constexpr int Price = 44;         // 價格
    constexpr int TimeInForce = 59;   // 時效性

    // 執行回報相關
    constexpr int ExecID = 17;        // 執行ID
    constexpr int ExecType = 150;     // 執行類型
    constexpr int OrdStatus = 39;     // 訂單狀態
    constexpr int LeavesQty = 151;    // 剩餘數量
    constexpr int CumQty = 14;        // 累計成交數量
    constexpr int LastQty = 32;       // 最後成交數量
    constexpr int LastPx = 31;        // 最後成交價格

    // 取消相關
    constexpr int OrigClOrdID = 41;   // 原始客戶訂單ID

    // Session 相關
    constexpr int Username = 553;     // 登入用戶名
    constexpr int Password = 554;     // 登入密碼
    constexpr int TestReqID = 112;    // 測試請求ID
    constexpr int Text = 58;          // 文字訊息
}

// FIX 常數值
namespace FixValues {
    // Side 值
    constexpr char BUY = '1';
    constexpr char SELL = '2';

    // OrdType 值
    constexpr char MARKET = '1';
    constexpr char LIMIT = '2';
    constexpr char STOP = '3';
    constexpr char STOP_LIMIT = '4';

    // TimeInForce 值
    constexpr char DAY = '0';
    constexpr char GTC = '1';  // Good Till Cancel
    constexpr char IOC = '3';  // Immediate Or Cancel
    constexpr char FOK = '4';  // Fill Or Kill

    // ExecType 值
    constexpr char NEW = '0';
    constexpr char PARTIAL_FILL = '1';
    constexpr char FILL = '2';
    constexpr char CANCELED = '4';
    constexpr char REJECTED = '8';

    // OrdStatus 值
    constexpr char ORDER_NEW = '0';
    constexpr char ORDER_PARTIALLY_FILLED = '1';
    constexpr char ORDER_FILLED = '2';
    constexpr char ORDER_CANCELED = '4';
    constexpr char ORDER_REJECTED = '8';
}

} // namespace protocol
} // namespace mts