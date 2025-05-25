Order Book Trading System
A high-performance C++ implementation of a financial order book with real-time matching engine and visualization.

![OrderBook Table](https://github.com/user-attachments/assets/dfaf5a17-516d-4f7e-82b4-d5cf8d5173f0)

## Overview
This project implements a complete electronic trading system featuring:
- Order Management: Support for different order types and lifecycle management
- Matching Engine: Real-time order matching with price-time priority
- Market Data: Live order book visualization with bid/ask levels
- Trade Execution: Automatic trade generation and reporting
## Features
### Order Types
- Good Till Cancel (GTC): Orders remain active until filled or cancelled
- Fill and Kill (FAK): Orders execute immediately or are cancelled
### Order Management
- Order Placement: Add new orders to the book
- Order Modification: Update existing order parameters
- Order Cancellation: Remove orders from the book
- Trade Matching: Automatic execution when bid/ask prices cross
### Market Data
- Level 2 Data: Full order book depth with aggregated quantities
- Real-time Updates: Live visualization of market changes
- Trade Reports: Detailed execution information
## Technical Architecture
### Core Components
Order Class
- Manages individual order lifecycle
- Tracks filled/remaining quantities
- Supports partial fills
Orderbook Class
- Maintains separate bid/ask price levels
- Implements price-time priority matching
- Provides market data snapshots
Matching Engine
- Processes orders in real-time
- Generates trade confirmations
- Maintains market integrity
### Data Structures
- Price-Time Priority: Orders sorted by price, then arrival time
- STL Containers: Efficient use of maps and lists for O(log n) operations
- Smart Pointers: Memory-safe order management
## Building and Running
### Prerequisites
- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)
- CMake 3.15+ (optional)
- Terminal with ANSI color support
### Compilation
bash
# Direct compilation
g++ -std=c++20 -O3 -o orderbook main.cpp
# Or with CMake
mkdir build && cd build
cmake ..
make

### Execution
bash
./orderbook

## Configuration
### Simulation Parameters
cpp
const int delayMs = 5;          // Delay between orders (milliseconds)
const int numOrders = 5000;     // Total orders to simulate

### Price/Quantity Ranges
cpp
std::uniform_int_distribution<int> priceDist(1, 1000);    // Price range
std::uniform_int_distribution<int> qtyDist(1, 1000);      // Quantity range

### Display Depth
cpp
OrderbookPrinter::Print(orderbook.GetOrderInfos(), 6);    // Show top 6 levels

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
