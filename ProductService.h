#pragma once
#include <string>
#include "ProductRepository.h"

class ProductService
{
private:
    ProductRepository repository;
public:
    ProductService(ConnectionPool& pool) : repository(pool) {}
    std::string get_products();
    std::string get_product(int id);
    std::string add_product(const std::string& type, const std::string& name, double cost, double price, int stock,int sku,const std::string& image_url);
    std::string update_product(int id, double price, int stock);
    std::string delete_product(int id);
    std::string get_products_by_type(const std::string& type);
    std::string search_products(const ProductQuery& q);
    std::string add_compatibility(int product_id, const std::string& make, const std::string& model,
        std::optional<int> year_from, std::optional<int> year_to);
    std::string get_compatibility(int product_id);


};