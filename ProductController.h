#pragma once

#include "Router.h"
#include "ProductService.h"
#include "Validation.h"
#include "SessionRepository.h"


class ProductController
{
private:
    ProductService service;
    SessionRepository& sessions;
    Response search_products(const Request& req);

    Response get_products(const Request& req);
    Response get_product(const Request& req);
    Response add_product(const Request& req);
    Response delete_product(const Request& req);
    Response update_product(const Request& req);
    Response add_compatibility(const Request& req);
    Response get_compatibility(const Request& req);

public:
    void register_routes(Router& router);
    ProductController(ConnectionPool& pool, SessionRepository& sessions)
        : service(pool), sessions(sessions) {}
};