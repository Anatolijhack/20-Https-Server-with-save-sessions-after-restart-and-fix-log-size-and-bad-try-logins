#include "PaymentController.h"
#include "FormParser.h"
#include "CartRepository.h"
#include "Logger.h"
#include "Validation.h"
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>
#include <cmath>
#include "AuthMiddleware.h"

using json = nlohmann::json;

void PaymentController::register_routes(Router& router)
{
    router.add("POST", "/checkout", [this](const Request& req)
        {
            return checkout(req);
        });

    router.add("GET", "/orders/:id", [this](const Request& req)
        {
            return get_order(req);
        });

    router.add("POST", "/payment/callback", [this](const Request& req)
        {
            return payment_callback(req);
        });

    router.add("GET", "/users/:id/orders", [this](const Request& req)
        {
            return get_user_orders(req);
        });

    router.add("GET", "/admin/orders",
        [this](const Request& req)
        {
            return get_all_orders(req);
        }
    );

    router.add("PUT", "/admin/orders/:id/status",
        [this](const Request& req)
        {
            return update_order_status(req);
        }
    );

    router.add("GET", "/cart/preview", [this](const Request& req) { return preview_cart(req); });
    router.add("GET", "/me/orders", [this](const Request& req) { return get_my_orders(req); });
    router.add("GET", "/me", [this](const Request& req)
        {
            auto auth = require_auth(req, sessions);
            if (!auth.ok) return auth.error;

            json response = { {"user_id", auth.user_id}, { "username", auth.username }, {"role", auth.role} };
            return Response{ response.dump(), "application/json" };
        });

    router.add("PUT", "/orders/:id/cancel", [this](const Request& req)
        {
            return cancel_my_order(req);
        });

    router.add("GET", "/admin/reports/margin", [this](const Request& req)
        {
            return get_margin_report(req);
        });

    //router.add("GET", "/cart/:user_id/preview", [this](const Request& req)
    //    {
    //        return preview_cart(req);
    //    });

    
}

static std::string generate_payment_ref()
{
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist;

    std::ostringstream oss;
    oss << "PAY-" << std::hex << dist(gen);
    return oss.str();
}

Response PaymentController::cancel_my_order(const Request& req)
{
    auto auth = require_auth(req, sessions);
    if (!auth.ok) return auth.error;

    int order_id;

    try
    {
        order_id = std::stoi(req.params.at("id"));
    }
    catch (...)
    {
        return Response{ R"({"error":"Invalid order id"})", "application/json", "400 Bad Request" };
    }

    std::string result = order_repository.cancel_order_by_customer(order_id, auth.user_id);
    json result_json = json::parse(result);

    std::string status_code = "200 OK";

    if (result_json.contains("error"))
    {
        const std::string err = result_json["error"].get<std::string>();

        if (err == "Order not found") status_code = "404 Not Found";
        else if (err == "Order cannot be cancelled") status_code = "409 Conflict";
        else if (err == "Order status changed concurrently, retry") status_code = "409 Conflict";
        else if (err.find("failed") != std::string::npos) status_code = "500 Internal Server Error";
        else status_code = "409 Conflict";
    }

    return Response{ result_json.dump(), "application/json", status_code };
}

//Response PaymentController::checkout(const Request& req)
//{
//    auto auth = require_auth(req);
//    if (!auth.ok) return auth.error;
//    int user_id = auth.user_id;
//
//    json body = json::object();
//    if (!req.body.empty())
//    {
//        try { body = json::parse(req.body); }
//        catch (...) { return Response{ R"({"error":"Invalid JSON"})", "application/json", "400 Bad Request" }; }
//    }
//    DeliveryInfo delivery;
//
//    std::vector<OrderItemInput> items;
//    bool from_cart = false;
//
//    // ≈сли items переданы €вно в запросе Ч используем их (обратна€ совместимость)
//    if (body.contains("items") && !body.at("items").empty())
//    {
//        try
//        {
//            for (const auto& raw_item : body.at("items"))
//            {
//                OrderItemInput item;
//                item.product_id = raw_item.at("product_id").get<int>();
//                item.quantity = raw_item.at("quantity").get<int>();
//                items.push_back(item);
//            }
//        }
//        catch (...)
//        {
//            return Response{ R"({"error":"Invalid items format"})", "application/json", "400 Bad Request" };
//        }
//    }
//    else
//    {
//        // items не переданы Ч берЄм из корзины пользовател€
//        items = cart_repository.get_cart_as_order_items(user_id);
//        from_cart = true;
//    }
//
//    if (items.empty())
//    {
//        std::string msg = from_cart
//            ? R"({"error":"Cart is empty"})"
//            : R"({"error":"Order must contain at least one item"})";
//
//        return Response{ msg, "application/json", "400 Bad Request" };
//    }
//
//    // —оздаЄм заказ Ч цены подт€гиваютс€ из Ѕƒ внутри create_order
//    std::string order_result_str = order_repository.create_order(user_id, items);
//    json order_result = json::parse(order_result_str);
//
//    if (order_result.contains("error"))
//    {
//        LOG_ERROR("Order creation failed: " + order_result_str);
//        return Response{ order_result.dump(), "application/json", "400 Bad Request" };
//    }
//
//    int order_id = order_result.at("order_id").get<int>();
//    double total = order_result.at("total").get<double>();
//
//    LOG_INFO("Order created: order_id=" + std::to_string(order_id) + " total=" + std::to_string(total));
//
//    // ≈сли заказ собран из корзины Ч очищаем еЄ после успешного создани€ заказа
//    if (from_cart)
//    {
//        cart_repository.clear_cart(user_id);
//        LOG_INFO("Cart cleared for user_id=" + std::to_string(user_id));
//    }
//
//    std::string payment_ref = generate_payment_ref();
//    payment_repository.create_payment(order_id, payment_ref, total);
//
//    try
//    {
//        auto form = liqpay.create_payment(payment_ref, total, "Auto parts order #" + std::to_string(order_id), result_url, server_url);
//
//        json response = {
//            {"order_id", order_id},
//            {"payment_ref", payment_ref},
//            {"total", total},
//            {"data", form.data},
//            {"signature", form.signature},
//            {"checkout_url", "https://www.liqpay.ua/api/3/checkout"}
//        };
//
//        return Response{ response.dump(), "application/json" };
//    }
//    catch (const std::exception& e)
//    {
//        LOG_ERROR(std::string("liqpay.create_payment threw: ") + e.what());
//        return Response{ R"({"error":"Payment form generation failed"})", "application/json", "500 Internal Server Error" };
//    }
//}
Response PaymentController::checkout(const Request& req)
{
    auto auth = require_auth(req, sessions);
    if (!auth.ok) return auth.error;
    int user_id = auth.user_id;

    json body = json::object();
    if (!req.body.empty())
    {
        try { body = json::parse(req.body); }
        catch (...) { return Response{ R"({"error":"Invalid JSON"})", "application/json", "400 Bad Request" }; }
    }

    DeliveryInfo delivery;

    try
    {
        delivery.recipient_name = body.at("delivery").at("recipient_name").get<std::string>();
        delivery.phone = body.at("delivery").at("phone").get<std::string>();
        delivery.city = body.at("delivery").at("city").get<std::string>();
        delivery.address = body.at("delivery").at("address").get<std::string>();
        delivery.comment = body.at("delivery").value("comment", "");
    }
    catch (...)
    {
        return Response{ R"({"error":"Missing delivery information"})", "application/json", "400 Bad Request" };
    }

    if (auto error = validation::validate_required_text(delivery.recipient_name, "recipient_name", 100))
    {
        json err = { {"error", error->message}, {"field", error->field} };
        return Response{ err.dump(), "application/json", "400 Bad Request" };
    }

    if (auto error = validation::validate_phone(delivery.phone))
    {
        json err = { {"error", error->message}, {"field", error->field} };
        return Response{ err.dump(), "application/json", "400 Bad Request" };
    }

    if (auto error = validation::validate_required_text(delivery.city, "city", 100))
    {
        json err = { {"error", error->message}, {"field", error->field} };
        return Response{ err.dump(), "application/json", "400 Bad Request" };
    }

    if (auto error = validation::validate_required_text(delivery.address, "address", 200))
    {
        json err = { {"error", error->message}, {"field", error->field} };
        return Response{ err.dump(), "application/json", "400 Bad Request" };
    }

    std::vector<OrderItemInput> items;
    bool from_cart = false;

    // ≈сли items переданы €вно в запросе Ч используем их (обратна€ совместимость)
    if (body.contains("items") && !body.at("items").empty())
    {
        try
        {
            for (const auto& raw_item : body.at("items"))
            {
                OrderItemInput item;
                item.product_id = raw_item.at("product_id").get<int>();
                item.quantity = raw_item.at("quantity").get<int>();
                items.push_back(item);
            }
        }
        catch (...)
        {
            return Response{ R"({"error":"Invalid items format"})", "application/json", "400 Bad Request" };
        }
    }
    else
    {
        // items не переданы Ч берЄм из корзины пользовател€
        items = cart_repository.get_cart_as_order_items(user_id);
        from_cart = true;
    }

    if (items.empty())
    {
        std::string msg = from_cart
            ? R"({"error":"Cart is empty"})"
            : R"({"error":"Order must contain at least one item"})";

        return Response{ msg, "application/json", "400 Bad Request" };
    }

    // —оздаЄм заказ Ч цены подт€гиваютс€ из Ѕƒ внутри create_order
    std::string order_result_str = order_repository.create_order(user_id, items, delivery);
    json order_result = json::parse(order_result_str);

    if (order_result.contains("error"))
    {
        LOG_ERROR("Order creation failed: " + order_result_str);
        return Response{ order_result.dump(), "application/json", "400 Bad Request" };
    }

    int order_id = order_result.at("order_id").get<int>();
    double total = order_result.at("total").get<double>();

    LOG_INFO("Order created: order_id=" + std::to_string(order_id) + " total=" + std::to_string(total));

    // ≈сли заказ собран из корзины Ч очищаем еЄ после успешного создани€ заказа
    if (from_cart)
    {
        cart_repository.clear_cart(user_id);
        LOG_INFO("Cart cleared for user_id=" + std::to_string(user_id));
    }

    std::string payment_ref = generate_payment_ref();
    payment_repository.create_payment(order_id, payment_ref, total);

    try
    {
        auto form = liqpay.create_payment(payment_ref, total, "Auto parts order #" + std::to_string(order_id), result_url, server_url);

        json response = {
            {"order_id", order_id},
            {"payment_ref", payment_ref},
            {"total", total},
            {"data", form.data},
            {"signature", form.signature},
            {"checkout_url", "https://www.liqpay.ua/api/3/checkout"}
        };

        return Response{ response.dump(), "application/json" };
    }
    catch (const std::exception& e)
    {
        LOG_ERROR(std::string("liqpay.create_payment threw: ") + e.what());
        return Response{ R"({"error":"Payment form generation failed"})", "application/json", "500 Internal Server Error" };
    }
}
// GET /orders/:id Ч посмотреть состав и статус заказа
Response PaymentController::get_order(const Request& req)
{
    auto auth = require_auth(req, sessions);
    if (!auth.ok) return auth.error;

    int order_id;
    try { order_id = std::stoi(req.params.at("id")); }
    catch (...) { return Response{ R"({"error":"Invalid order id"})", "application/json", "400 Bad Request" }; }

    json result = json::parse(order_repository.get_order(order_id));

    if (result.contains("error"))
    {
        return Response{ result.dump(), "application/json", "404 Not Found" };
    }

    bool is_staff = auth.role == "admin" || auth.role == "employee";

    if (!is_staff && result.at("user_id").get<int>() != auth.user_id)
    {
        return Response{ R"({"error":"Order not found"})", "application/json", "404 Not Found" };
    }

    return Response{ result.dump(), "application/json" };
}

// POST /payment/callback Ч обратный вызов от LiqPay
Response PaymentController::payment_callback(const Request& req)
{
    LOG_INFO("Payment callback raw body: " + req.body);

    auto form_data = parse_form_urlencoded(req.body);

    auto data_it = form_data.find("data");
    auto sig_it = form_data.find("signature");

    if (data_it == form_data.end() || sig_it == form_data.end())
    {
        return Response{ "Bad Request", "text/plain", "400 Bad Request" };
    }

    if (!liqpay.verify_signature(data_it->second, sig_it->second))
    {
        LOG_ERROR("LiqPay callback: invalid signature Ч possible forged request");
        return Response{ "Forbidden", "text/plain", "403 Forbidden" };
    }

    json payment_data = liqpay.decode_data(data_it->second);

    std::string payment_ref = payment_data.at("order_id").get<std::string>(); // это payment_ref, а не order_id заказа!
    std::string status = payment_data.at("status").get<std::string>();
    double callback_amount = payment_data.value("amount", 0.0);

    auto payment = payment_repository.find_by_ref(payment_ref);

    if (!payment)
    {
        LOG_ERROR("Payment callback: unknown payment_ref=" + payment_ref);
        return Response{ "Payment not found", "text/plain", "404 Not Found" };
    }

    if (payment->status == "Paid")
    {
        LOG_INFO("Payment callback: payment=" + payment_ref + " already processed, skipping");
        return Response{ "OK", "text/plain" };
    }

    if (std::abs(callback_amount - payment->amount) > 0.01)
    {
        LOG_ERROR("Payment amount mismatch: payment=" + payment_ref);
        return Response{ "Amount mismatch", "text/plain", "400 Bad Request" };
    }

    const bool paid = status == "success" || status == "sandbox";
    const bool failed = status == "failure" || status == "error" || status == "reversed";

    if (!paid && !failed)
    {
        LOG_INFO("Payment callback: intermediate status '" + status + "' for " + payment_ref);
        return Response{ "OK", "text/plain" };
    }

    payment_repository.update_payment_status(payment_ref, paid ? "Paid" : "Failed");

    if (paid)
    {
        json order = json::parse(order_repository.get_order(payment->order_id));

        if (order.value("status", "") == "Cancelled")
        {
            LOG_ERROR("PAID CANCELLED ORDER order=" + std::to_string(payment->order_id)
                + " payment=" + payment_ref + " - нужен возврат средств или восстановление заказа");
        }
        else
        {
            order_repository.update_order_status(payment->order_id, "Processing");
        }
    }
    else
    {
        order_repository.cancel_order(payment->order_id);
    }

    LOG_INFO("Payment callback: payment=" + payment_ref + " order=" + std::to_string(payment->order_id)
        + " status=" + std::string(paid ? "Paid" : "Failed"));

    return Response{ "OK", "text/plain" };
}

Response PaymentController::get_user_orders(const Request& req)
{
    int user_id;

    try
    {
        user_id = std::stoi(req.params.at("id"));
    }
    catch (...)
    {
        return Response{ R"({"error":"Invalid user id"})", "application/json", "400 Bad Request" };
    }

    std::string result = order_repository.get_orders_by_user(user_id);
    return Response{ result, "application/json" };
}

Response PaymentController::get_all_orders(const Request& req)
{
    auto auth = require_roles(req, sessions, { "admin", "employee" });
    if (!auth.ok) return auth.error;

    std::string result = order_repository.get_all_orders();
    return Response{ result, "application/json" };
}

Response PaymentController::update_order_status(const Request& req)
{
    auto auth = require_roles(req, sessions, { "admin", "employee" });
    if (!auth.ok) return auth.error;

    int order_id;

    try
    {
        order_id = std::stoi(req.params.at("id"));
    }
    catch (...)
    {
        return Response{ R"({"error":"Invalid order id"})", "application/json", "400 Bad Request" };
    }

    json body;

    try
    {
        body = json::parse(req.body);
    }
    catch (...)
    {
        return Response{ R"({"error":"Invalid JSON"})", "application/json", "400 Bad Request" };
    }

    std::string new_status;

    try
    {
        new_status = body.at("status").get<std::string>();
    }
    catch (...)
    {
        return Response{ R"({"error":"Missing status field"})", "application/json", "400 Bad Request" };
    }

    // ѕровер€ем, что статус входит в допустимый список (совпадает с CHECK в Ѕƒ)
    static const std::vector<std::string> allowed_statuses = {
        "New", "Processing", "Shipped", "Completed", "Cancelled"
    };

    if (std::find(allowed_statuses.begin(), allowed_statuses.end(), new_status) == allowed_statuses.end())
    {
        return Response{
            R"({"error":"Invalid status. Must be one of: New, Processing, Shipped, Completed, Cancelled"})",
            "application/json", "400 Bad Request"
        };
    }

    std::string result = (new_status == "Cancelled")
        ? order_repository.cancel_order(order_id)
        : order_repository.update_order_status(order_id, new_status);

    json result_json = json::parse(result);
    std::string status_code = "200 OK";

    if (result_json.contains("error"))
    {
        const std::string err = result_json["error"].get<std::string>();
        if (err == "Order not found") status_code = "404 Not Found";
        else if (err == "Invalid status transition") status_code = "409 Conflict";
        else if (err.find("failed") != std::string::npos) status_code = "500 Internal Server Error";
        else status_code = "409 Conflict";
    }

    return Response{ result_json.dump(), "application/json", status_code };
}

Response PaymentController::preview_cart(const Request& req)
{
    auto auth = require_auth(req, sessions);
    if (!auth.ok) return auth.error;

 


    auto cart_items = cart_repository.get_cart_as_order_items(auth.user_id);

    if (cart_items.empty())
    {
        return Response{ R"({"error":"Cart is empty"})", "application/json", "400 Bad Request" };
    }

    json items = json::array();
    double total = 0.0;
    bool has_issues = false;

    for (const auto& cart_item : cart_items)
    {
        auto resolved = order_repository.resolve_item(cart_item.product_id, cart_item.quantity);

        if (!resolved)
        {
            json item = { {"product_id", cart_item.product_id}, {"error", "Product not found"} };
            items.push_back(item);
            has_issues = true;
            continue;
        }

        json item;
        item["product_id"] = resolved->product_id;
        item["product_name"] = resolved->product_name;
        item["quantity"] = resolved->quantity;
        item["price"] = resolved->price;
        item["subtotal"] = resolved->price * resolved->quantity;
        item["available_stock"] = resolved->available_stock;

        if (resolved->available_stock < resolved->quantity)
        {
            item["warning"] = "Insufficient stock";
            has_issues = true;
        }
        else
        {
            total += resolved->price * resolved->quantity;
        }

        items.push_back(item);
    }

    json response;
    response["items"] = items;
    response["total"] = total;
    response["has_issues"] = has_issues;

    return Response{ response.dump(), "application/json" };
}
Response PaymentController::get_my_orders(const Request& req)
{
    auto auth = require_auth(req, sessions);
    if (!auth.ok) return auth.error;

    return Response{ order_repository.get_orders_by_user(auth.user_id), "application/json" };
}

Response PaymentController::get_margin_report(const Request& req)
{
    auto auth = require_roles(req, sessions, { "admin" });
    if (!auth.ok) return auth.error;

    std::string date_from = "1900-01-01";
    std::string date_to = "2100-01-01";

    auto it = req.params.find("from");
    if (it != req.params.end() && !it->second.empty()) date_from = it->second;

    it = req.params.find("to");
    if (it != req.params.end() && !it->second.empty()) date_to = it->second;

    return Response{ order_repository.get_margin_report(date_from, date_to), "application/json" };
}