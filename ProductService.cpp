#include "ProductService.h"

std::string ProductService::get_products()
{
	return repository.get_products();
}

std::string ProductService::get_product(int id)
{
	return repository.get_product(id);
}

std::string ProductService::add_product(const std::string& type, const std::string& name, double cost, double price, int stock,int sku,const std::string& image_url)
{
	return repository.add_product(type, name, cost, price, stock,sku,image_url);
}

std::string ProductService::update_product(int id, double price, int stock)
{
	return repository.update_product(id, price, stock);
}

std::string ProductService::delete_product(int id)
{
	return repository.delete_product(id);
}

std::string ProductService::get_products_by_type(const std::string& type)
{
	return repository.get_products_by_type(type);
}

std::string ProductService::search_products(const ProductQuery& q)
{
	return repository.search_products(q);
}

std::string ProductService::add_compatibility(int product_id, const std::string& make, const std::string& model, std::optional<int> year_from, std::optional<int> year_to)
{
	return repository.add_compatibility(product_id, make, model, year_from, year_to);
}

std::string ProductService::get_compatibility(int product_id)
{
	return repository.get_compatibility(product_id);
}
