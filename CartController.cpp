#include "CartController.h"
#include "AuthMiddleware.h"
#include "Validation.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

void CartController::register_routes(Router& router)
{
    //router.add("POST", "/cart/add", [this](const Request& req)
    //    {
    //        return add_item(req);
    //    });

    //router.add("GET", "/cart/:user_id", [this](const Request& req)
    //    {
    //        return get_cart(req);
    //    });

    //router.add("PUT", "/cart/update", [this](const Request& req)
    //    {
    //        return update_item(req);
    //    });

    //router.add("DELETE", "/cart/:user_id/remove/:product_id", [this](const Request& req)
    //    {
    //        return remove_item(req);
    //    });
    router.add("POST", "/cart/add", [this](const Request& req) { return add_item(req); });
    router.add("GET", "/cart", [this](const Request& req) { return get_cart(req); });
    router.add("PUT", "/cart/update", [this](const Request& req) { return update_item(req); });
    router.add("DELETE", "/cart/remove/:product_id", [this](const Request& req) { return remove_item(req); });
}

Response CartController::add_item(const Request& req)
{
    auto auth = require_auth(req, sessions);
    if (!auth.ok) return auth.error;
    json body;

    
    try { body = json::parse(req.body); }
    catch (...) { return Response{ R"({"error":"Invalid JSON"})", "application/json", "400 Bad Request" }; }

    int product_id, quantity;

    try
    {
       
        product_id = body.at("product_id").get<int>();
        quantity = body.at("quantity").get<int>();
    }
    catch (...)
    {
        return Response{ R"({"error":"Missing user_id, product_id or quantity"})", "application/json", "400 Bad Request" };
    }

    if (quantity <= 0)
    {
        return Response{ R"({"error":"Quantity must be positive"})", "application/json", "400 Bad Request" };
    }

    std::string result = cart_repository.add_item(auth.user_id, product_id, quantity);
    json result_json = json::parse(result);

    std::string status_code = result_json.contains("error") ? "400 Bad Request" : "201 Created";

    return Response{ result_json.dump(), "application/json", status_code };
}

Response CartController::get_cart(const Request& req)
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

   

    std::string result = cart_repository.get_cart(user_id);
    return Response{ result, "application/json" };
}

Response CartController::update_item(const Request& req)
{
    auto auth = require_auth(req, sessions);
    if (!auth.ok) return auth.error;


    json body;

    try
    {
        body = json::parse(req.body);
    }
    catch (...)
    {
        return Response{ R"({"error":"Invalid JSON"})", "application/json", "400 Bad Request" };
    }

    int product_id, quantity;

    try
    {
       
        product_id = body.at("product_id").get<int>();
        quantity = body.at("quantity").get<int>();
    }
    catch (...)
    {
        return Response{ R"({"error":"Missing user_id, product_id or quantity"})", "application/json", "400 Bad Request" };
    }

    if (quantity <= 0)
    {
        return Response{ R"({"error":"Quantity must be positive. Use DELETE to remove item"})", "application/json", "400 Bad Request" };
    }

    std::string result = cart_repository.update_item(auth.user_id, product_id, quantity);
    json result_json = json::parse(result);

    std::string status_code = "200 OK";

    if (result_json.contains("error"))
    {
        std::string error_msg = result_json["error"].get<std::string>();
        status_code = (error_msg == "Item not found in cart") ? "404 Not Found" : "500 Internal Server Error";
    }

    return Response{ result_json.dump(), "application/json", status_code };
}

Response CartController::remove_item(const Request& req)
{
    auto auth = require_auth(req, sessions);
    if (!auth.ok) return auth.error;
    int  product_id;

    try
    {
       
        product_id = std::stoi(req.params.at("product_id"));
    }
    catch (...)
    {
        return Response{ R"({"error":"Invalid user_id or product_id"})", "application/json", "400 Bad Request" };
    }

    std::string result = cart_repository.remove_item(auth.user_id, product_id);
    json result_json = json::parse(result);

    std::string status_code = result_json.contains("error") ? "404 Not Found" : "200 OK";

    return Response{ result_json.dump(), "application/json", status_code };
}