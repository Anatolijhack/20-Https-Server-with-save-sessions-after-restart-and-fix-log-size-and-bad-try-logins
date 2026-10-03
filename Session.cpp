#include "Session.h"
#include <sstream>
#include <string>
#include <iostream>
#include "Logger.h"

static std::string to_lower(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });

    return value;
}

//void Session::do_read()
//{
//    auto self = shared_from_this();
//
//    socket.async_read_some(
//        boost::asio::buffer(temp),
//        [this, self](boost::system::error_code ec, std::size_t length)
//        {
//            if (ec)
//                return;
//
//            buffer.append(temp.data(), length);
//
//            if (buffer.size() > MAX_REQUEST_SIZE)
//            {
//                send_response_safe("Request too large", "text/plain", "413 Payload Too Large");
//                return;
//            }
//
//            if (parse())
//            {
//                process_request();
//                reset_parser();
//            }
//           
//        });
//}
void Session::do_read()
{
    auto self = shared_from_this();

    // Сначала пытаемся разобрать то,
    // что уже накопилось в buffer
    if (!buffer.empty())
    {
        if (buffer.size() > MAX_REQUEST_SIZE)
        {
            send_response_safe(
                "Request too large",
                "text/plain",
                "413 Payload Too Large"
            );
            return;
        }

        ParseResult result = parse();

        if (result == ParseResult::Complete)
        {
            process_request();
            reset_parser();
            return;
        }

        if (result == ParseResult::Error)
        {
            return;
        }
    }

    // Если полного запроса пока нет —
    // читаем новые данные из сокета
    socket.async_read_some(
        boost::asio::buffer(temp),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
            if (ec)
            {
                if (ec != boost::asio::error::eof) // EOF — это нормальное закрытие клиентом, не ошибка
                {
                    std::cerr << "Read error: " << ec.message() << std::endl;
                }
                return;
            }

            buffer.append(temp.data(), length);

            if (buffer.size() > MAX_REQUEST_SIZE)
            {
                send_response_safe(
                    "Request too large",
                    "text/plain",
                    "413 Payload Too Large"
                );
                return;
            }

            //if (parse())
            //{
            //    process_request();
            //    reset_parser();
            //}
            //else
            //{
            //    // Запрос ещё не полный.
            //    // Продолжаем читать.
            //    do_read();
            //}
            ParseResult result = parse();

            if (result == ParseResult::Complete)
            {
                process_request();
                reset_parser();
                return;
            }

            if (result == ParseResult::Error)
            {
                return;
            }

            // Incomplete
            do_read();
        });
}
void Session::reset_parser()
{
    state = ParseState::RequestLine;
    method.clear();
    path.clear();
    version.clear();
    headers.clear();
    content_length = 0;
    body.clear();
}
//bool Session::parse()
//{
//    while (true)
//    {
//        if (state == ParseState::RequestLine)
//        {
//            auto pos = buffer.find("\r\n");
//            if (pos == std::string::npos)
//                return false;
//
//            std::string line = buffer.substr(0, pos);
//            buffer.erase(0, pos + 2);
//
//            std::istringstream rl(line);
//            if (!(rl >> method >> path >> version))
//            {
//                send_response_safe("Bad Request", "text/plain", "400 Bad Request");
//                return false;
//            }
//
//            state = ParseState::Header;
//        }
//        else if (state == ParseState::Header)
//        {
//            auto pos = buffer.find("\r\n");
//            if (pos == std::string::npos)
//                return false;
//
//            std::string line = buffer.substr(0, pos);
//            buffer.erase(0, pos + 2);
//
//            if (line.empty())
//            {
//                // --- Connection ---
//                keep_alive = true;
//                auto conn = headers.find("connection");
//                if (conn != headers.end())
//                {
//                    std::string value = to_lower(conn->second);
//
//                    if (value == "close")
//                    {
//                        keep_alive = false;
//                    }
//                }
//
//                // --- Content-Length ---
//                auto it = headers.find("content-length");
//                if (it != headers.end())
//                {
//                    try
//                    {
//                        std::size_t pos = 0;
//
//                        long long value = std::stoll(it->second, &pos);
//
//                        if (pos != it->second.size() || value < 0)
//                        {
//                            send_response_safe(
//                                "Bad Request",
//                                "text/plain",
//                                "400 Bad Request"
//                            );
//                            return false;
//                        }
//
//                        if (value > static_cast<long long>(MAX_REQUEST_SIZE))
//                        {
//                            send_response_safe(
//                                "Too large",
//                                "text/plain",
//                                "413 Payload Too Large"
//                            );
//                            return false;
//                        }
//
//                        content_length = static_cast<int>(value);
//                    }
//                    catch (...)
//                    {
//                        send_response_safe(
//                            "Bad Request",
//                            "text/plain",
//                            "400 Bad Request"
//                        );
//                        return false;
//                    }
//                }
//
//                if (content_length > 0)
//                {
//                    state = ParseState::Body;
//                }
//                else
//                {
//                    return true;
//                }
//            }
//            else
//            {
//                auto sep = line.find(":");
//                if (sep == std::string::npos)
//                {
//                    send_response_safe("Bad Request", "text/plain", "400 Bad Request");
//                    return false;
//                }
//
//                std::string key = line.substr(0, sep);
//                key = to_lower(key);
//                std::string value = line.substr(sep + 1);
//
//                while (!value.empty() && value.front() == ' ')
//                    value.erase(value.begin());
//
//                headers[key] = value;
//            }
//        }
//        else if (state == ParseState::Body)
//        {
//            if (buffer.size() < content_length)
//                return false;
//
//            body = buffer.substr(0, content_length); // ✅ СОХРАНИЛИ
//            buffer.erase(0, content_length);
//
//            return true;
//        }
//    }
//}


Session::ParseResult Session::parse()
{
    while (true)
    {
        if (state == ParseState::RequestLine)
        {
            auto pos = buffer.find("\r\n");

            if (pos == std::string::npos)
                return ParseResult::Incomplete;

            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 2);

            std::istringstream rl(line);

            if (!(rl >> method >> path >> version))
            {
                send_response_safe(
                    "Bad Request",
                    "text/plain",
                    "400 Bad Request"
                );

                return ParseResult::Error;
            }

            state = ParseState::Header;
        }

        else if (state == ParseState::Header)
        {
            auto pos = buffer.find("\r\n");

            if (pos == std::string::npos)
                return ParseResult::Incomplete;

            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 2);

            if (line.empty())
            {
                keep_alive = true;

                auto conn = headers.find("connection");

                if (conn != headers.end())
                {
                    std::string value = to_lower(conn->second);

                    if (value == "close")
                    {
                        keep_alive = false;
                    }
                }

                auto it = headers.find("content-length");

                if (it != headers.end())
                {
                    try
                    {
                        std::size_t pos = 0;

                        long long value =
                            std::stoll(it->second, &pos);

                        if (pos != it->second.size() || value < 0)
                        {
                            send_response_safe(
                                "Bad Request",
                                "text/plain",
                                "400 Bad Request"
                            );

                            return ParseResult::Error;
                        }

                        if (value > static_cast<long long>(MAX_REQUEST_SIZE))
                        {
                            send_response_safe(
                                "Too large",
                                "text/plain",
                                "413 Payload Too Large"
                            );

                            return ParseResult::Error;
                        }

                        content_length =
                            static_cast<int>(value);
                    }
                    catch (...)
                    {
                        send_response_safe(
                            "Bad Request",
                            "text/plain",
                            "400 Bad Request"
                        );

                        return ParseResult::Error;
                    }
                }

                if (content_length > 0)
                {
                    state = ParseState::Body;
                }
                else
                {
                    return ParseResult::Complete;
                }
            }
            else
            {
                auto sep = line.find(":");

                if (sep == std::string::npos)
                {
                    send_response_safe(
                        "Bad Request",
                        "text/plain",
                        "400 Bad Request"
                    );

                    return ParseResult::Error;
                }

                std::string key = line.substr(0, sep);
                key = to_lower(key);

                std::string value = line.substr(sep + 1);

                while (!value.empty() && value.front() == ' ')
                    value.erase(value.begin());

                headers[key] = value;
            }
        }

        else if (state == ParseState::Body)
        {
            if (buffer.size() < content_length)
                return ParseResult::Incomplete;

            body = buffer.substr(0, content_length);
            buffer.erase(0, content_length);

            return ParseResult::Complete;
        }
    }
}

//void Session::process_request()
//{
//    auto self = shared_from_this();
//
//    std::cout << method << " " << path << std::endl;
//
//    pool.submit(0, [this, self, method = this->method, path = this->path]()
//        {
//            std::string response_body;
//            std::string content_type = "text/plain";
//            std::string status = "200 OK";
//
//            if (method == "GET" && path == "/") {
//                response_body =
//                    "<html><body>"
//                    "<h1>Автозапчасти</h1>"
//                    "<a href='/products'>Каталог</a>"
//                    "</body></html>";
//                content_type = "text/html";
//            }
//            else if (method == "GET" && path == "/products") {
//                response_body =
//                    "[{\"id\":1,\"name\":\"Фильтр\",\"price\":500},"
//                    "{\"id\":2,\"name\":\"Колодки\",\"price\":3000}]";
//                content_type = "application/json";
//            }
//            else if (method == "POST" && path == "/order") {
//                response_body = "Order created";
//            }
//            else if (method == "GET" && path == "/favicon.ico") {
//                response_body = "";
//                status = "204 No Content";
//            }
//            else {
//                response_body = "404 Not Found";
//                status = "404 Not Found";
//            }
//
//            boost::asio::post(socket.get_executor(),
//                [this, self, response_body, content_type, status]()
//                {
//                    send_response_safe(response_body, content_type, status);
//                });
//        });
//}
void Session::process_request()
{
    auto self = shared_from_this();

    std::cout << "=== PROCESS REQUEST ===" << std::endl;
    std::cout << "METHOD: [" << method << "]\n";
    std::cout << "PATH: [" << path << "]\n";
    std::cout << "BODY: [" << body << "]\n";
    std::cout << "BODY SIZE: " << body.size() << "\n";


    Request req{ method, path, body, {}, headers };

    pool.submit(0, [this, self, req]() mutable
        {
            std::cout << "INSIDE WORKER" << std::endl;

            Response res = router.route(req);

            std::cout << "ROUTER DONE" << std::endl;
            std::cout << "STATUS: " << res.status << std::endl;
            std::cout << "BODY: " << res.body << std::endl;

            boost::asio::post(socket.get_executor(),
                [this, self, res]()
                {
                    std::cout << "BEFORE RESPONSE" << std::endl;

                    send_response_safe(
                        res.body,
                        res.content_type,
                        res.status
                    );
                });
        });
}

void Session::send_response(const std::string& body,
    const std::string& type,
    const std::string& status)
{
    /*auto self = shared_from_this();

    auto response = std::make_shared<std::string>(
        "HTTP/1.1 " + status + "\r\n" +
        "Content-Length: " + std::to_string(body.size()) + "\r\n" +
        "Content-Type: " + type + "\r\n" +
        "Connection: " + std::string(keep_alive ? "keep-alive" : "close") + "\r\n" +
        "\r\n" +
        body
    );

    write_queue.push_back(response);

    if (!writing)
    {
        do_write();
    }*/
    auto self = shared_from_this();

    auto response = std::make_shared<std::string>(
        "HTTP/1.1 " + status + "\r\n" +
        "Content-Length: " + std::to_string(body.size()) + "\r\n" +
        "Content-Type: " + type + "\r\n" +
        "Connection: " + std::string(keep_alive ? "keep-alive" : "close") + "\r\n" +
        "Access-Control-Allow-Origin: *\r\n" +
        "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n" +
        "Access-Control-Allow-Headers: Content-Type, Authorization\r\n" +
        "\r\n" +
        body
    );

    write_queue.push_back(response);

    if (!writing)
    {
        do_write();
    }
}
void Session::do_write()
{
    if (write_queue.empty())
    {
        writing = false;

        if (!keep_alive)
        {
            do_shutdown(); 
            return;
        }

        do_read();
        return;
    }

    writing = true;

    auto self = shared_from_this();
    auto response = write_queue.front();

    boost::asio::async_write(socket,
        boost::asio::buffer(*response),
        [this, self, response](boost::system::error_code ec, std::size_t)
        {
            if (ec)
            {
                LOG_ERROR("Write error: " + ec.message());

                writing = false;
                write_queue.clear(); // очищаем очередь — нет смысла пытаться писать дальше

                do_shutdown();
                return;
            }

            write_queue.pop_front();
            do_write();
        });
}
void Session::send_response_safe(const std::string& body,
    const std::string& type,
    const std::string& status)
{
    auto self = shared_from_this();

    boost::asio::post(socket.get_executor(),
        [this, self, body, type, status]()
        {
            send_response(body, type, status);
        });
}
Session::Session(tcp::socket socket, ThreadPool& pool, Router& router, boost::asio::ssl::context& ssl_context)
    : socket(std::move(socket), ssl_context), pool(pool), router(router) {}

void Session::start()
{
    auto self = shared_from_this();

    socket.async_handshake(boost::asio::ssl::stream_base::server,
        [this, self](boost::system::error_code ec)
        {
            if (ec)
            {
                std::cerr << "TLS handshake failed: " << ec.message() << std::endl;
                return;
            }

            do_read();
        });
}
void Session::do_shutdown()
{
    auto self = shared_from_this();

    socket.async_shutdown(
        [this, self](boost::system::error_code ec)
        {
            // Ошибки shutdown обычно не критичны — клиент мог уже закрыть соединение
            if (ec && ec != boost::asio::error::eof)
            {
                LOG_WARNING("TLS shutdown warning: " + ec.message());
            }

            boost::system::error_code close_ec;
            socket.next_layer().close(close_ec);
        });
}



