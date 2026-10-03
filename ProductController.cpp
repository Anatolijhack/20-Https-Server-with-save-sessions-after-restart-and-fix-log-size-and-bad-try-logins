#include "ProductController.h"
#include "AuthMiddleware.h"
using json = nlohmann::json;


void ProductController::register_routes(Router& router)
{
    router.add(
        "GET",
        "/products",
        [this](const Request& req)
        {
            return get_products(req);
        }
    );
    router.add("GET", "/products/search", [this](const Request& req)
        {
            std::cerr << ">>> SEARCH PRODUCTS HANDLER\n";
            return search_products(req);
        });

    router.add(
        "GET",
        "/products/:id",
        [this](const Request& req)
        {   
            std::cerr << ">>> GET PRODUCT HANDLER, path = ";
            return get_product(req);
        }
    );

    router.add(
        "POST",
        "/products",
        [this](const Request& req)
        {
            return add_product(req);
        }
    );

    router.add(
        "DELETE",
        "/products/:id",
        [this](const Request& req)
        {
            return delete_product(req);
        }
    );

    router.add(
        "PUT",
        "/products/:id",
        [this](const Request& req)
        {
            return update_product(req);
        }
    );
    router.add("GET", "/products/:id/compatibility", [this](const Request& req)
        {
            return get_compatibility(req);
        });

    router.add("POST", "/products/:id/compatibility", [this](const Request& req)
        {
            return add_compatibility(req);
        });

    router.add("GET", "/products/type/:type", [this](const Request& req)
        {
            std::string type = req.params.at("type");
            return Response{ service.get_products_by_type(type), "application/json" };
        });



}

Response ProductController::search_products(const Request& req)
{
    ProductQuery q;

    auto it = req.params.find("search");
    if (it != req.params.end() && !it->second.empty()) q.search = it->second;

    it = req.params.find("type");
    if (it != req.params.end() && !it->second.empty()) q.type = it->second;

    it = req.params.find("sort_by");
    if (it != req.params.end()) q.sort_by = it->second;

    it = req.params.find("sort_dir");
    if (it != req.params.end()) q.sort_dir = it->second;

    it = req.params.find("limit");
    if (it != req.params.end())
    {
        try { q.limit = std::clamp(std::stoi(it->second), 1, 100); }
        catch (...) {}
    }

    it = req.params.find("offset");
    if (it != req.params.end())
    {
        try { q.offset = max(0, std::stoi(it->second)); }
        catch (...) {}
    }
    it = req.params.find("make");
    if (it != req.params.end() && !it->second.empty()) q.make = it->second;

    it = req.params.find("model");
    if (it != req.params.end() && !it->second.empty()) q.model = it->second;

    it = req.params.find("year");
    if (it != req.params.end())
    {
        try { q.year = std::stoi(it->second); }
        catch (...) {}
    }

    return Response{ service.search_products(q), "application/json" };
}
Response ProductController::get_products(const Request& req)
{
    return Response{
        service.get_products(),
        "application/json"
    };
}


Response ProductController::get_product(const Request& req)
{
    try
    {
        int id = std::stoi(req.params.at("id"));

        return Response{
            service.get_product(id),
            "application/json"
        };
    }
    catch (...)
    {
        return Response{
            R"({"error":"Invalid product id"})",
            "application/json",
            "400 Bad Request"
        };
    }
}


    Response ProductController::add_product(const Request& req)
    {
        auto auth = require_roles(req, sessions, { "admin", "employee" });
        if (!auth.ok) return auth.error;

        try
        {
            auto data = nlohmann::json::parse(req.body);

            std::string type =
                data.at("type").get<std::string>();

            std::string name =
                data.at("name").get<std::string>();
            int sku = data.at("sku").get<int>();
            std::string image_url = data.at("image_url").get<std::string>();

            double cost =
                data.at("cost").get<double>();

            double price =
                data.at("price").get<double>();

            int stock =
                data.at("stock_quantity").get<int>();

            return Response{
                service.add_product(
                    type,
                    name,
                    cost,
                    price,
                    stock,
                    sku,
                    image_url
                ),
                "application/json",
                "201 Created"
            };
        }
        catch (...)
        {
            return Response{
                R"({"error":"Invalid JSON or missing fields"})",
                "application/json",
                "400 Bad Request"
            };
        }
    }


Response ProductController::delete_product(const Request& req)
{
    auto auth = require_roles(req, sessions, {"admin"});

    if (!auth.ok)
    {
        return auth.error;
    }

    int id;

    try
    {
        id = std::stoi(req.params.at("id"));
    }
    catch (...)
    {
        return Response{
            R"({"error":"Invalid product id"})",
            "application/json",
            "400 Bad Request"
        };
    }

    std::string result =
        service.delete_product(id);

    json result_json;

    try
    {
        result_json = json::parse(result);
    }
    catch (...)
    {
        return Response{
            R"({"error":"Invalid service response"})",
            "application/json",
            "500 Internal Server Error"
        };
    }

    std::string status_code = "200 OK";

    if (result_json.contains("error"))
    {
        if (result_json["error"] == "Product not found")
        {
            status_code = "404 Not Found";
        }
        else
        {
            status_code = "400 Bad Request";
        }
    }

    return Response{
        result_json.dump(),
        "application/json",
        status_code
    };
}


Response ProductController::update_product(const Request& req)
{

    auto auth = require_roles(req, sessions, {"admin", "employee"});
    if (!auth.ok) return auth.error;
    int id;

    try
    {
        id = std::stoi(req.params.at("id"));
    }
    catch (...)
    {
        return Response{
            R"({"error":"Invalid product id"})",
            "application/json",
            "400 Bad Request"
        };
    }

    json body;

    try
    {
        body = json::parse(req.body);
    }
    catch (...)
    {
        return Response{
            R"({"error":"Invalid JSON"})",
            "application/json",
            "400 Bad Request"
        };
    }

    try
    {
        double price =
            body.at("price").get<double>();

        int stock =
            body.at("stock_quantity").get<int>();

        std::string result =
            service.update_product(
                id,
                price,
                stock
            );

        json result_json =
            json::parse(result);

        std::string status_code = "200 OK";

        if (result_json.contains("error"))
        {
            if (result_json["error"] == "Product not found")
            {
                status_code = "404 Not Found";
            }
            else
            {
                status_code = "400 Bad Request";
            }
        }

        return Response{
            result_json.dump(),
            "application/json",
            status_code
        };
    }
    catch (...)
    {
        return Response{
            R"({"error":"Missing or invalid fields: price, stock_quantity"})",
            "application/json",
            "400 Bad Request"
        };
    }
}
Response ProductController::add_compatibility(const Request& req)
{
    auto auth = require_roles(req, sessions, { "admin", "employee" });
    if (!auth.ok) return auth.error;

    int product_id;
    try { product_id = std::stoi(req.params.at("id")); }
    catch (...) { return Response{ R"({"error":"Invalid product id"})", "application/json", "400 Bad Request" }; }

    json body;
    try { body = json::parse(req.body); }
    catch (...) { return Response{ R"({"error":"Invalid JSON"})", "application/json", "400 Bad Request" }; }

    std::string make, model;
    std::optional<int> year_from, year_to;

    try
    {
        make = body.at("make").get<std::string>();
        model = body.at("model").get<std::string>();
        if (body.contains("year_from")) year_from = body.at("year_from").get<int>();
        if (body.contains("year_to")) year_to = body.at("year_to").get<int>();
    }
    catch (...)
    {
        return Response{ R"({"error":"Missing make or model"})", "application/json", "400 Bad Request" };
    }

    std::string result = service.add_compatibility(product_id, make, model, year_from, year_to);
    json result_json = json::parse(result);

    return Response{ result_json.dump(), "application/json",
        result_json.contains("error") ? "500 Internal Server Error" : "201 Created" };
}

Response ProductController::get_compatibility(const Request& req)
{
    int product_id;
    try { product_id = std::stoi(req.params.at("id")); }
    catch (...) { return Response{ R"({"error":"Invalid product id"})", "application/json", "400 Bad Request" }; }

    return Response{ service.get_compatibility(product_id), "application/json" };
}