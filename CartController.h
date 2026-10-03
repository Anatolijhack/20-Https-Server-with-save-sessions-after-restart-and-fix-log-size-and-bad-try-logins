#pragma once
#include "CartRepository.h"
#include "Router.h"
#include "SessionRepository.h"

class CartController
{
public:
    CartController(
        ConnectionPool& pool,
        SessionRepository& session_repository)
        : cart_repository(pool),
        sessions(session_repository)
    {
    }

    void register_routes(Router& router);

private:
    Response add_item(const Request& req);
    Response get_cart(const Request& req);
    Response update_item(const Request& req);
    Response remove_item(const Request& req);

    CartRepository cart_repository;
    SessionRepository& sessions;
};