#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <functional>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <thread>
#include "router.hpp"
#include "middleware.hpp"
#include "file_server.hpp"
#include "request.hpp"
#include "thread_pool.hpp"

using boost::asio::ip::tcp;
namespace asio = boost::asio;

// Security limits
constexpr size_t MAX_CONTENT_LENGTH = 10 * 1024 * 1024;  // 10 MB
constexpr size_t MAX_HEADER_SIZE = 8 * 1024;              // 8 KB
constexpr size_t MAX_KEEPALIVE_REQUESTS = 1000;           // Max requests per connection
constexpr int REQUEST_TIMEOUT_SECONDS = 30;               // Request timeout

void handle_request(tcp::socket socket){
    size_t request_count = 0;  // Track requests per connection

    while (true) {
        auto do_read = [&socket, &request_count](){
            std::string req_headers;
            boost::system::error_code ec;

            // Read headers with size limit
            read_until(socket, asio::dynamic_buffer(req_headers),"\r\n\r\n", ec);
            if (ec) {
                if (ec == boost::asio::error::eof || ec == boost::asio::error::connection_reset || ec == boost::asio::error::operation_aborted) {
                    return;
                }
                std::cerr << "Error reading request: " << ec.message() << "\n";
                std::string error_resp = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
                boost::system::error_code ignored_ec;
                write(socket, asio::buffer(error_resp), ignored_ec);
                return;
            }

            // Validate header size to prevent memory exhaustion
            if (req_headers.size() > MAX_HEADER_SIZE) {
                std::string error_resp = "HTTP/1.1 431 Request Header Fields Too Large\r\n"
                                       "Content-Type: text/plain\r\n"
                                       "Content-Length: 34\r\n"
                                       "Connection: close\r\n\r\n"
                                       "Request headers exceed size limit";
                boost::system::error_code ignored_ec;
                write(socket, asio::buffer(error_resp), ignored_ec);
                socket.shutdown(tcp::socket::shutdown_both, ignored_ec);
                socket.close(ignored_ec);
                return;
            }

            // Parse request headers
            Request request = parse_request(req_headers);

            // Read body if Content-Length is present
            int content_length = get_content_length(request.headers);

            // Validate Content-Length to prevent memory exhaustion attack
            if (content_length < 0) {
                std::string error_resp = "HTTP/1.1 400 Bad Request\r\n"
                                       "Content-Type: text/plain\r\n"
                                       "Content-Length: 24\r\n"
                                       "Connection: close\r\n\r\n"
                                       "Invalid Content-Length";
                boost::system::error_code ignored_ec;
                write(socket, asio::buffer(error_resp), ignored_ec);
                socket.shutdown(tcp::socket::shutdown_both, ignored_ec);
                socket.close(ignored_ec);
                return;
            }

            if (content_length > static_cast<int>(MAX_CONTENT_LENGTH)) {
                std::string error_resp = "HTTP/1.1 413 Payload Too Large\r\n"
                                       "Content-Type: text/plain\r\n"
                                       "Content-Length: 44\r\n"
                                       "Connection: close\r\n\r\n"
                                       "Request body exceeds maximum allowed size";
                boost::system::error_code ignored_ec;
                write(socket, asio::buffer(error_resp), ignored_ec);
                socket.shutdown(tcp::socket::shutdown_both, ignored_ec);
                socket.close(ignored_ec);
                return;
            }

            if (content_length > 0) {
                std::string body_buffer;
                body_buffer.resize(content_length);
                boost::asio::read(socket, boost::asio::buffer(&body_buffer[0], content_length), ec);
                if (!ec) {
                    request.body = body_buffer;
                }
            }

            // Get client IP (for logging)
            std::string client_ip = socket.remote_endpoint().address().to_string();

            // Log the request (middleware)
            log_request(client_ip, request.method, request.path);

            Response response;
            if (request.path.substr(0, 8) == "/static/") {
                response = serve_file(request.path);
            } else {
                response = handle_route_with_method(request.method, request.path, request.body);
            }
            // Determine keep-alive semantics
            bool keep_alive = false;
            // Default keep-alive for HTTP/1.1 unless explicitly closed
            if (request.version == "HTTP/1.1") keep_alive = true;
            // Check Connection header from parsed request
            auto conn_it = request.headers.find("connection");
            if (conn_it != request.headers.end()) {
                std::string conn_value = conn_it->second;
                std::transform(conn_value.begin(), conn_value.end(), conn_value.begin(), [](unsigned char ch){
                    return static_cast<char>(std::tolower(ch));
                });
                if (conn_value == "close") keep_alive = false;
                else if (conn_value == "keep-alive") keep_alive = true;
            }

            std::time_t now = std::time(nullptr);
            std::tm tm = *std::gmtime(&now);
            std::ostringstream date_ss;
            date_ss << std::put_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");

            // Build and send response
            std::string resp;
            resp.reserve(128);
            resp += "HTTP/1.1 " + response.status + "\r\n";
            resp += "Content-Type: " + response.content_type + "\r\n";
            resp += "Date: " + date_ss.str() + "\r\n";
            resp += "Server: Tez\r\n";
            resp += std::string("Connection: ") + (keep_alive ? "keep-alive" : "close") + std::string("\r\n");
            if (keep_alive) {
                resp += "Keep-Alive: timeout=5, max=1000\r\n";
            }
            resp += "Content-Length: " + std::to_string(response.body.size()) + "\r\n";
            resp += "\r\n";
            resp += response.body;

            // Send response
            boost::system::error_code send_ec;
            write(socket, asio::buffer(resp), send_ec);
            if (send_ec) {
                std::cerr << "Error sending response: " << send_ec.message() << "\n";
                return;
            }

            // Increment request counter
            request_count++;

            // Check if we've exceeded max keep-alive requests
            if (request_count >= MAX_KEEPALIVE_REQUESTS) {
                keep_alive = false;  // Force close after max requests
            }

            // Break loop if not keep-alive
            if (!keep_alive) {
                boost::system::error_code ignored;
                socket.shutdown(tcp::socket::shutdown_both, ignored);
                socket.close(ignored);
                return;
            }
        };

        do_read();
        // After handling, check if socket is still open for next request
        if (!socket.is_open()) {
            break;
        }
    }
}


int main() {
    try {
        // Initialize router configuration at startup
        init_router_config();

        // Create thread pool with hardware concurrency threads
        unsigned int num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;  // Fallback to 4 threads
        ThreadPool thread_pool(num_threads);

        asio::io_context io;

        tcp::acceptor acceptor(io, {tcp::v4(), 8080});
        acceptor.set_option(asio::socket_base::reuse_address(true));

        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code&, int){
            std::cout << "Shutting down...\n";
            boost::system::error_code ignored_ec;
            acceptor.close(ignored_ec);
            io.stop();
            thread_pool.shutdown();
        });

        std::cout << "Tez server starting on port 8080 with " << num_threads << " worker threads...\n";

      // Async accept loop
      std::function<void()> do_accept;
        do_accept = [&](){
            auto socket = std::make_shared<tcp::socket>(io);
            acceptor.async_accept(*socket, [&, socket](boost::system::error_code ec){
                if(!ec){
                    // Enqueue connection handling to thread pool
                    thread_pool.enqueue([socket](){
                        handle_request(std::move(*socket));
                    });
                }
                if (ec != boost::asio::error::operation_aborted) {
                    do_accept();
                }
            });
         };
         do_accept();
         io.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
