// OrderRepository.h
//#pragma once
//#include "ConnectionPool.h"
//#include <string>
//#include <optional>
//
//class OrderRepository
//{
//public:
//    OrderRepository(ConnectionPool& pool) : pool(pool) {}
//
//    std::string create_order(const std::string& order_ref, const std::string& customer_name, double amount);
//    bool update_status(const std::string& order_ref, const std::string& status);
//
//private:
//    ConnectionPool& pool;
//};
#pragma once
#include "ConnectionPool.h"
#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct OrderItemInput
{
    int product_id;
    int quantity;
};

struct OrderItemRecord
{
    int product_id;
    std::string product_name;
    int quantity;
    double price;
};

struct OrderRecord
{
    int order_id;
    int user_id;
    std::string status;
    std::string created_at;
    std::vector<OrderItemRecord> items;
    double total;
};
struct ResolvedOrderItem
{
    int product_id;
    std::string product_name;
    int quantity;
    double price;
    int available_stock;
};
struct DeliveryInfo
{
    std::string recipient_name;
    std::string phone;
    std::string city;
    std::string address;
    std::string comment; // необязательное
};

class OrderRepository
{
public:
    OrderRepository(ConnectionPool& pool) : pool(pool) {}

    // Создаёт заказ + позиции, возвращает JSON с order_id и total (или error)
    std::string create_order(int user_id, const std::vector<OrderItemInput>& items, const DeliveryInfo& delivery);

    std::string get_order(int order_id);
    std::string get_orders_by_user(int user_id);
    std::string update_order_status(int order_id, const std::string& status);
    std::string get_orders_by_status(const std::string& status);
    std::string get_all_orders();
    std::optional<ResolvedOrderItem> resolve_item(int product_id, int quantity);
    std::string cancel_order(int order_id, bool only_unpaid = false);
    std::string cancel_order_by_customer(int order_id, int user_id);
    std::vector<int> find_expired_order_ids(int minutes);
    int cancel_expired_orders(int minutes);
    std::string get_margin_report(const std::string& date_from, const std::string& date_to);


private:
    ConnectionPool& pool;
};