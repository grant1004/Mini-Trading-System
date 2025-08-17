# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

This is a C++17 CMake-based project with the following structure:

### Dependencies
- **GTest**: For unit testing framework
- **Boost**: system and thread libraries for networking and threading
- **nlohmann_json**: JSON parsing and serialization
- **Platform**: Supports Windows (WSL2) and Linux

### Build Commands
```bash
# Create build directory
mkdir build && cd build

# Configure with debug options enabled
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Build the project
make -j$(nproc)

# Build specific targets
make mts_app      # Main trading system executable
make mts_lib      # Core library

# Run tests
ctest
# Or run individual test executables
./test_order_book
./test_fix_message
./test_order
./test_server
```

### FIX Protocol Debug Options
The build system includes granular FIX protocol debugging options:
- `ENABLE_FIX_DEBUG`: General FIX debug output
- `ENABLE_FIX_CHECKSUM_DEBUG`: Checksum validation debug
- `ENABLE_FIX_PARSE_DEBUG`: Message parsing debug
- `ENABLE_FIX_SERIALIZE_DEBUG`: Serialization debug
- `ENABLE_FIX_VALIDATION_DEBUG`: Field validation debug
- `ENABLE_FIX_FACTORY_DEBUG`: Message factory debug

These are enabled by default in the CMake configuration.

## Architecture Overview

### Core Components

**TradingSystem** (`src/trading_system.h/cpp`): Central orchestrator that coordinates between network layer and matching engine. Manages client sessions, handles FIX protocol messages, and maintains order mappings.

**MatchingEngine** (`src/core/matching_engine.h/cpp`): High-performance order matching engine with configurable matching modes (Continuous, Auction, CallAuction). Includes risk management, order book management, and execution reporting.

**OrderBook** (`src/core/order_book.h/cpp`): Price-time priority order book implementation with separate bid/ask sides. Handles order insertion, cancellation, and matching logic.

**TCPServer** (`src/network/tcp_server.h/cpp`): Multi-threaded TCP server for client connections. Provides callback-based event handling for new connections, messages, and disconnections.

**FIX Protocol Layer** (`src/protocol/`): Complete FIX protocol implementation including:
- `fix_message.h/cpp`: Message parsing and validation
- `fix_session.h/cpp`: Session management and state tracking
- `fix_message_builder.h/cpp`: Message construction utilities
- `fix_tags.h`: FIX tag definitions

### Data Flow
1. Client connects via TCP to TradingSystem
2. TradingSystem creates FIX session for protocol handling
3. Incoming FIX messages parsed and converted to internal Order objects
4. Orders submitted to MatchingEngine for processing
5. MatchingEngine processes orders through OrderBooks
6. Execution reports generated and sent back as FIX messages
7. Market data updates distributed to subscribed clients

### Threading Model
- **Main Thread**: Console interface and system lifecycle management
- **TCP Accept Thread**: Handles new client connections
- **Client Handler Threads**: One per connected client for message processing
- **Matching Engine Thread**: Dedicated thread for order processing
- **Health Check Thread**: Periodic session validation and cleanup

## Running the System

### Main Application
```bash
# Run with default settings (port 8080)
./mts_app

# Run with custom port
./mts_app --port 9090

# Run with test client simulation
./mts_app --test

# Show help
./mts_app --help
```

### Testing Client Connection
```bash
# Connect using telnet
telnet localhost 8080

# Send FIX messages (example new order)
8=FIX.4.4|9=XXX|35=D|49=CLIENT|56=MTS|11=ORDER123|55=AAPL|54=1|38=100|40=2|44=150.50|10=XXX|
```

### Runtime Commands
- `stats`: Display system statistics
- `help`: Show available commands  
- `quit`/`exit`: Graceful shutdown
- `Ctrl+C`: Signal-based graceful shutdown

## Key Implementation Details

### Order Processing Flow
Orders move through validation → risk checking → order book insertion → matching → execution reporting. All processing is asynchronous with thread-safe message queues.

### FIX Message Format
Messages use pipe (`|`) as field separators instead of SOH for easier testing. The system validates checksums, sequence numbers, and required fields according to FIX 4.4 specification.

### Risk Management
Built-in risk checks include maximum order price/quantity limits, per-symbol order count limits, and basic order validation (positive prices/quantities, valid symbols).

### Session Management
Each client connection gets a dedicated FIX session with heartbeat monitoring, sequence number tracking, and automatic cleanup on disconnection.

### Performance Considerations
- Lock-free statistics using atomics
- Shared mutexes for read-heavy order book access
- Dedicated processing threads to avoid blocking
- Configurable processing time limits and monitoring