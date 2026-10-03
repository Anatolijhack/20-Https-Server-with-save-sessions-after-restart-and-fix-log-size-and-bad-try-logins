#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include "ConnectionPool.h"
//#include <windows.h> 
//#include <sql.h>
//#include <sqlext.h>
//struct databaseconnection
//{
//    sqlhenv env = nullptr;
//    sqlhdbc dbc = nullptr;
//};
struct ProductRecord
{
    int id;
    std::string type;
    std::string product_name;
    double price;
    int stock_quantity;
};
struct ProductQuery
{
    std::optional<std::string> search;
    std::optional<std::string> type;
    std::optional<std::string> make;    // новое
    std::optional<std::string> model;   // новое
    std::optional<int> year;            // новое
    std::string sort_by = "id";
    std::string sort_dir = "ASC";
    int limit = 20;
    int offset = 0;
};
class ProductRepository
{
public:
    ProductRepository(ConnectionPool& pool) : pool(pool) {}

    std::string get_products();
    std::string get_product(int id);
    std::string add_product(const std::string& type, const std::string& name, double cost, double price, int stock, int sku, const std::string& image_url);
    std::string update_product(int id, double price, int stock);
    std::string delete_product(int id);
    std::string get_products_by_type(const std::string& type);
    std::string search_products(const ProductQuery& q);
    std::string add_compatibility(int product_id, const std::string& make, const std::string& model,
        std::optional<int> year_from, std::optional<int> year_to);
    std::string get_compatibility(int product_id);


private:
    ConnectionPool& pool;
};