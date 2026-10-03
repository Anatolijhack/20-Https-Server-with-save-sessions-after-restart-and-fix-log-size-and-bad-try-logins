#pragma once
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <memory>
#include <unordered_map>
#include "ThreadPool.h"
#include <queue>
#include "Router.h"
using boost::asio::ip::tcp;
class Session : public std::enable_shared_from_this<Session>
{
private:
	enum class ParseState
	{
		RequestLine,
		Header,
		Body
	};
	enum class ParseResult
	{
		Incomplete,
		Complete,
		Error
	};
	void do_shutdown();
	ParseState state = ParseState::RequestLine;
	std::string path;
	std::string version;
	std::string method;
	std::unordered_map<std::string, std::string> headers;
	std::array<char, 4096> temp;
	int content_lenght = 0;
	std::deque<std::shared_ptr<std::string>> write_queue;
	bool writing = false;
	boost::asio::ssl::stream<tcp::socket> socket;
	std::string buffer;
	Router& router;
	std::string body;
	bool keep_alive = true;
	int content_length = 0;
	static constexpr std::size_t MAX_REQUEST_SIZE = 1024 * 1024;
	ThreadPool& pool;
	void do_write();
	void do_read();
	/*bool parse();*/
	ParseResult parse();
	void process_request();
	void reset_parser();
	void send_response(const std::string& body,
		const std::string& type,
		const std::string& status);
	void send_response_safe(const std::string& body,
		const std::string& type,
		const std::string& status);
public:
	Session(tcp::socket socket, ThreadPool& pool, Router& router, boost::asio::ssl::context& ssl_context);
	void start();
};