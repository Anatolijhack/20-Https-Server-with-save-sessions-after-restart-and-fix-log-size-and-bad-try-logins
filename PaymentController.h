#pragma once
#include "OrderRepository.h"
#include "PaymentRepository.h"
#include "LiqPayHelper.h"
#include "CartRepository.h"
#include "Router.h"
#include "SessionRepository.h"

class PaymentController
{
public:
    PaymentController(ConnectionPool& pool, SessionRepository& sessions,
        const std::string& public_key, const std::string& private_key,
        const std::string& result_url, const std::string& server_url)
        : order_repository(pool), payment_repository(pool), cart_repository(pool),
        sessions(sessions), liqpay(public_key, private_key),
        result_url(result_url), server_url(server_url) {}

    void register_routes(Router& router);

private:
    Response cancel_my_order(const Request& req);
    Response checkout(const Request& req);
    Response payment_callback(const Request& req);
    Response get_order(const Request& req);
    Response get_user_orders(const Request& req);
    Response get_all_orders(const Request& req);
    Response preview_cart(const Request& req);
    Response get_my_orders(const Request& req);
    Response get_margin_report(const Request& req);
    Response update_order_status(const Request& req);

    OrderRepository order_repository;
    PaymentRepository payment_repository;
    CartRepository cart_repository;
    LiqPayHelper liqpay;
    SessionRepository& sessions;
    std::string result_url;
    std::string server_url;
};