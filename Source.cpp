#include "Server.h"
#include <iostream>
#include "ProductController.h"
#include "ConnectionPool.h"
#include "Logger.h"
#include "Config.h"
#include "AuthController.h"
#include "PaymentController.h"
#include "CartController.h"
#include "SessionStore.h"
#include "OrderCleaner.h"
#include "SessionRepository.h"
#include "SessionCleaner.h"

using json = nlohmann::json;

int main()
{
    
    Logger::instance().init("server.log");
    LOG_INFO("Server starting...");
    Config config = Config::load("config.json");

    Router router;
    ConnectionPool db_pool(
        config.database.pool_size,
        config.database.to_connection_string()
    );
    SessionRepository session_repository(db_pool, std::chrono::minutes(30), std::chrono::hours(12));

    ProductController product_controller(db_pool, session_repository);

    PaymentController payment_controller(
        db_pool, session_repository,
        config.liqpay.public_key,
        config.liqpay.private_key,
        config.liqpay.result_url,
        config.liqpay.server_url
    );
    payment_controller.register_routes(router);

    AuthController auth_controller(db_pool, session_repository);
    auth_controller.register_routes(router);

    product_controller.register_routes(router);

    CartController cart_controller(db_pool, session_repository);   // исправлено
    cart_controller.register_routes(router);

    OrderRepository cleaner_repo(db_pool);
    OrderCleaner cleaner(cleaner_repo, 30);
    SessionCleaner session_cleaner(session_repository);

    router.add("GET", "/", [](const Request& req)
        {
            return Response{
                "<html><body><h1>Автозапчасти</h1></body></html>",
                "text/html"
            };
        });

    router.add("POST", "/order", [](const Request& req)
        {
            json request_json;

            try
            {
                request_json = json::parse(req.body);
            }
            catch (...)
            {
                return Response{
                    R"({"error":"Invalid JSON"})",
                    "application/json",
                    "400 Bad Request"
                };
            }

            json response = {
                {"status", "created"},
                {"data", request_json}
            };

            return Response{
                response.dump(),
                "application/json"
            };
        });
    ThreadPool pool(4);

    try
    {
        boost::asio::io_context io;

        Server server(io, 8080, pool, router);

        std::cout << "Server started on port 8080\n";

        io.run();
    }
    catch (std::exception& e)
    {
        std::cout << "Error: " << e.what() << std::endl;
    }
}