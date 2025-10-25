#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <functional>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include "router.hpp"
#include "middleware.hpp"
#include "file_server.hpp"

using boost::asio::ip::tcp;
namespace asio = boost::asio;

void handle_request(tcp::socket socket){
    while (true) {
        auto do_read = [&socket](std::string req){
            boost::system::error_code ec;
            read_until(socket, asio::dynamic_buffer(req),"\r\n\r\n", ec);
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

            // Get client IP (for logging)
            std::string client_ip = socket.remote_endpoint().address().to_string();

            // Parse and route
            std::string method, path , version;
            std::istringstream iss(req);
            iss >> method >> path >> version;

            // Log the request (middleware)
            log_request(client_ip, method, path);

            std::string body;
            std::string status = "200 OK";
            std::string content_type = "text/plain; charset=utf-8";

            Response response;
            if (path.substr(0, 8) == "/static/") {
                response = serve_file(path);
            } else {
                response = handle_route(path);
            }
            // Determine keep-alive semantics
            bool keep_alive = false;
            // Default keep-alive for HTTP/1.1 unless explicitly closed
            if (version == "HTTP/1.1") keep_alive = true;
            // Case-insensitive search for Connection header
            std::string req_lower = req;
            std::transform(req_lower.begin(), req_lower.end(), req_lower.begin(), [](unsigned char ch){ return static_cast<char>(std::tolower(ch)); });
            size_t conn_pos = req_lower.find("connection:");
            if (conn_pos != std::string::npos) {
                size_t end_pos = req.find("\r\n", conn_pos);
                size_t value_start = conn_pos + std::string("connection:").size();
                std::string conn_value = req.substr(value_start, end_pos - value_start);
                auto ltrim = [](std::string& s){ s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c){ return !std::isspace(c); })); };
                auto rtrim = [](std::string& s){ s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c){ return !std::isspace(c); }).base(), s.end()); };
                ltrim(conn_value); rtrim(conn_value);
                std::string conn_value_lower = conn_value;
                std::transform(conn_value_lower.begin(), conn_value_lower.end(), conn_value_lower.begin(), [](unsigned char ch){ return static_cast<char>(std::tolower(ch)); });
                if (conn_value_lower == "close") keep_alive = false;
                else if (conn_value_lower == "keep-alive") keep_alive = true;
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

            // Break loop if not keep-alive
            if (!keep_alive) {
                boost::system::error_code ignored;
                socket.shutdown(tcp::socket::shutdown_both, ignored);
                socket.close(ignored);
                return;
            }
        };

        std::string req;
        do_read(req);
        // After handling, check if socket is still open for next request
        if (!socket.is_open()) {
            break;
        }
    }
}


int main() {
    try {
        asio::io_context io;

        tcp::acceptor acceptor(io, {tcp::v4(), 8080});
        acceptor.set_option(asio::socket_base::reuse_address(true));

        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code&, int){
            std::cout << "Shutting down...\n";
            boost::system::error_code ignored_ec;
            acceptor.close(ignored_ec);
            io.stop();
        });

        std::cout << "Tez server starting on port 8080...\n";

      // Async accept loop   
      std::function<void()> do_accept;
        do_accept = [&](){
            auto socket = std::make_shared<tcp::socket>(io);
            acceptor.async_accept(*socket, [&, socket](boost::system::error_code ec){
                if(!ec){
                    handle_request(std::move(*socket));
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
