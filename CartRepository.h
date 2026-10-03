#pragma once
#include "ConnectionPool.h"
#include "OrderRepository.h"
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class CartRepository
{
public:

    CartRepository(ConnectionPool& pool) : pool(pool) {}
    std::vector<OrderItemInput> get_cart_as_order_items(int user_id);
    std::string add_item(int user_id, int product_id, int quantity);
    std::string get_cart(int user_id);
    std::string update_item(int user_id, int product_id, int quantity);
    std::string remove_item(int user_id, int product_id);
    std::string clear_cart(int user_id);

private:
    
    ConnectionPool& pool;
};