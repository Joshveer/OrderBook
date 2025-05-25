#include <iostream>
#include <map>
#include <list>
#include <string>
#include <vector>
#include <numeric>
#include <memory>
#include <stdexcept>
#include <format>
#include <unordered_map>
#include <chrono>
#include <random>
#include <thread>
#include <iomanip>

enum class OrderType { GoodTillCancel, FillAndKill };
enum class Side { Buy, Sell };

using Price = std::int32_t;
using Quantity = std::uint32_t;
using OrderId = std::uint64_t;

struct LevelInfo {
    Price price_;
    Quantity quantity_;
};

OrderId GenerateOrderId() {
    static OrderId currentId = 1000;
    return currentId++;
}

using LevelInfos = std::vector<LevelInfo>;
using OrderPointer = std::shared_ptr<class Order>;
using OrderPointers = std::list<OrderPointer>;

class Order {
public:
    Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity)
        : orderType_(orderType), orderId_(orderId), side_(side), price_(price),
          initialQuantity_(quantity), remainingQuantity_(quantity) {}

    OrderId GetOrderId() const { return orderId_; }
    Side GetSide() const { return side_; }
    Price GetPrice() const { return price_; }
    OrderType GetOrderType() const { return orderType_; }
    Quantity GetInitialQuantity() const { return initialQuantity_; }
    Quantity GetRemainingQuantity() const { return remainingQuantity_; }
    Quantity GetFilledQuantity() const { return GetInitialQuantity() - GetRemainingQuantity(); }
    bool IsFilled() const { return GetRemainingQuantity() == 0; }

    void Fill(Quantity quantity) {
        if (quantity > GetRemainingQuantity())
            throw std::logic_error(std::format("Order ({}) cannot be filled for more than its remaining quantity.", GetOrderId()));
        remainingQuantity_ -= quantity;
    }

private:
    OrderType orderType_;
    OrderId orderId_;
    Side side_;
    Price price_;
    Quantity initialQuantity_;
    Quantity remainingQuantity_;
};

class OrderModify {
public:
    OrderModify(OrderId orderId, Side side, Price price, Quantity quantity)
        : orderId_(orderId), price_(price), side_(side), quantity_(quantity) {}

    OrderId GetOrderId() const { return orderId_; }
    Price GetPrice() const { return price_; }
    Side GetSide() const { return side_; }
    Quantity GetQuantity() const { return quantity_; }

    OrderPointer ToOrderPointer(OrderType type) const {
        return std::make_shared<Order>(type, GetOrderId(), GetSide(), GetPrice(), GetQuantity());
    }

private:
    OrderId orderId_;
    Price price_;
    Side side_;
    Quantity quantity_;
};

struct TradeInfo {
    OrderId orderId_;
    Price price_;
    Quantity quantity_;
};

class Trade {
public:
    Trade(const TradeInfo& bidTrade, const TradeInfo& askTrade)
        : bidTrade_(bidTrade), askTrade_(askTrade) {}

    const TradeInfo& GetBidTrade() const { return bidTrade_; }
    const TradeInfo& GetAskTrade() const { return askTrade_; }

private:
    TradeInfo bidTrade_;
    TradeInfo askTrade_;
};

using Trades = std::vector<Trade>;

class OrderbookLevelInfos {
public:
    OrderbookLevelInfos(const LevelInfos& bids, const LevelInfos& asks) : bids_(bids), asks_(asks) {}

    const LevelInfos& GetBids() const { return bids_; }
    const LevelInfos& GetAsks() const { return asks_; }

private:
    LevelInfos bids_;
    LevelInfos asks_;
};

class Orderbook {
private:
    struct OrderEntry {
        OrderPointer order_{nullptr};
        OrderPointers::iterator location_;
    };

    std::map<Price, OrderPointers, std::greater<Price>> bids_;
    std::map<Price, OrderPointers, std::less<Price>> asks_;
    std::unordered_map<OrderId, OrderEntry> orders_;

    bool CanMatch(Side side, Price price) const {
        if (side == Side::Buy) {
            if (asks_.empty()) return false;
            const auto& [bestAsk, _] = *asks_.begin();
            return price >= bestAsk;
        } else {
            if (bids_.empty()) return false;
            const auto& [bestBid, _] = *bids_.begin();
            return price <= bestBid;
        }
    }

    Trades MatchOrders() {
        Trades trades;

        while (!bids_.empty() && !asks_.empty()) {
            auto bidIt = bids_.begin();
            auto askIt = asks_.begin();

            if (bidIt->first < askIt->first) break;

            auto& bids = bidIt->second;
            auto& asks = askIt->second;

            while (!bids.empty() && !asks.empty()) {
                auto& bid = bids.front();
                auto& ask = asks.front();

                Quantity quantity = std::min(bid->GetRemainingQuantity(), ask->GetRemainingQuantity());
                bid->Fill(quantity);
                ask->Fill(quantity);

                trades.push_back(Trade{
                    TradeInfo{bid->GetOrderId(), bid->GetPrice(), quantity},
                    TradeInfo{ask->GetOrderId(), ask->GetPrice(), quantity}
                });

                if (bid->IsFilled()) {
                    bids.pop_front();
                    orders_.erase(bid->GetOrderId());
                }

                if (ask->IsFilled()) {
                    asks.pop_front();
                    orders_.erase(ask->GetOrderId());
                }
            }

            if (bids.empty()) bids_.erase(bidIt);
            if (asks.empty()) asks_.erase(askIt);
        }

        return trades;
    }

public:
    Trades AddOrder(OrderPointer order) {
        if (orders_.contains(order->GetOrderId())) return {};

        if (order->GetOrderType() == OrderType::FillAndKill && !CanMatch(order->GetSide(), order->GetPrice()))
            return {};

        OrderPointers::iterator iterator;

        if (order->GetSide() == Side::Buy) {
            auto& orders = bids_[order->GetPrice()];
            orders.push_back(order);
            iterator = std::prev(orders.end());
        } else {
            auto& orders = asks_[order->GetPrice()];
            orders.push_back(order);
            iterator = std::prev(orders.end());
        }

        orders_.insert({order->GetOrderId(), OrderEntry{order, iterator}});
        return MatchOrders();
    }

    void CancelOrder(OrderId orderId) {
        if (!orders_.contains(orderId)) return;

        const auto& [order, iterator] = orders_.at(orderId);
        orders_.erase(orderId);

        auto price = order->GetPrice();
        if (order->GetSide() == Side::Sell) {
            auto& orders = asks_.at(price);
            orders.erase(iterator);
            if (orders.empty()) asks_.erase(price);
        } else {
            auto& orders = bids_.at(price);
            orders.erase(iterator);
            if (orders.empty()) bids_.erase(price);
        }
    }

    Trades MatchOrder(OrderModify order) {
        if (!orders_.contains(order.GetOrderId())) return {};
        const auto& [existingOrder, _] = orders_.at(order.GetOrderId());
        CancelOrder(order.GetOrderId());
        return AddOrder(order.ToOrderPointer(existingOrder->GetOrderType()));
    }

    std::size_t Size() const { return orders_.size(); }

    OrderbookLevelInfos GetOrderInfos() const {
        LevelInfos bidInfos, askInfos;

        auto CreateLevelInfo = [](Price price, const OrderPointers& orders) {
            return LevelInfo{price, std::accumulate(orders.begin(), orders.end(), Quantity(0),
                                                    [](Quantity sum, const OrderPointer& order) {
                                                        return sum + order->GetRemainingQuantity();
                                                    })};
        };

        for (const auto& [price, orders] : bids_) bidInfos.push_back(CreateLevelInfo(price, orders));
        for (const auto& [price, orders] : asks_) askInfos.push_back(CreateLevelInfo(price, orders));

        return OrderbookLevelInfos(bidInfos, askInfos);
    }
};

#include <iomanip>

class OrderbookPrinter {
public:
    static void Print(const OrderbookLevelInfos& info, std::size_t depth = 6) {
        auto bids = info.GetBids();
        auto asks = info.GetAsks();

        std::cout << "\033[2J\033[1;1H";

        std::cout << "\033[33m┌─────────────┬─────────────┐\033[0m\n";
        std::cout << "\033[33m│  \033[1mBIDS (BUY)\033[0;33m │ \033[1mASKS (SELL)\033[0;33m │\033[0m\n";
        std::cout << "\033[33m├──────┬──────┼──────┬──────┤\033[0m\n";

        for (size_t i = 0; i < depth; ++i) {
            std::string bidStr = "      │      ";
            std::string askStr = "      │      ";

            if (i < bids.size()) {
                std::ostringstream bidStream;
                bidStream << "\033[32m" << std::setw(6) << bids[i].price_ << "\033[0m│\033[32m"
                          << std::setw(6) << bids[i].quantity_ << "\033[0m";
                bidStr = bidStream.str();
            }

            if (i < asks.size()) {
                std::ostringstream askStream;
                askStream << "\033[31m" << std::setw(6) << asks[i].price_ << "\033[0m│\033[31m"
                          << std::setw(6) << asks[i].quantity_ << "\033[0m";
                askStr = askStream.str();
            }

            std::cout << "│" << bidStr << "│" << askStr << "│\n";
        }

        std::cout << "\033[33m└──────┴──────┴──────┴──────┘\033[0m\n";
    }
};


int main() {
    Orderbook orderbook;
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> priceDist(1, 1000);
    std::uniform_int_distribution<int> qtyDist(1, 1000);
    std::uniform_int_distribution<int> sideDist(0, 1);
    std::uniform_int_distribution<int> typeDist(0, 1);

    const int delayMs = 5;

    for (int i = 0; i < 5000; ++i) {
        Side side = sideDist(rng) == 0 ? Side::Buy : Side::Sell;
        OrderType type = typeDist(rng) == 0 ? OrderType::GoodTillCancel : OrderType::FillAndKill;
        Price price = priceDist(rng);
        Quantity qty = qtyDist(rng);
        OrderId id = GenerateOrderId();

        auto order = std::make_shared<Order>(type, id, side, price, qty);

        std::cout << "Order Placed: ID=" << id
                  << " Type=" << (type == OrderType::GoodTillCancel ? "GTC" : "FAK")
                  << " Side=" << (side == Side::Buy ? "Buy" : "Sell")
                  << " Price=" << price
                  << " Quantity=" << qty
                  << '\n';

        Trades trades = orderbook.AddOrder(order);
        for (const auto& trade : trades) {
            std::cout << "Trade Executed: "
                      << "Buy ID=" << trade.GetBidTrade().orderId_
                      << " Sell ID=" << trade.GetAskTrade().orderId_
                      << " Price=" << trade.GetBidTrade().price_
                      << " Quantity=" << trade.GetBidTrade().quantity_
                      << '\n';
        }

        OrderbookPrinter::Print(orderbook.GetOrderInfos());

        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }

    std::cout << "\nFinal Orderbook State:\n";
    auto bookInfo = orderbook.GetOrderInfos();

    std::cout << "Bids:\n";
    for (const auto& level : bookInfo.GetBids()) {
        std::cout << "  Price: " << level.price_ << ", Quantity: " << level.quantity_ << '\n';
    }

    std::cout << "Asks:\n";
    for (const auto& level : bookInfo.GetAsks()) {
        std::cout << "  Price: " << level.price_ << ", Quantity: " << level.quantity_ << '\n';
    }

    return 0;
}