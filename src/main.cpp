#include <boost/asio.hpp>
#include <iostream>
#include <string>

using boost::asio::ip::tcp;
namespace asio = boost::asio;

int main() {
    try {
        asio::io_context io;

        tcp::acceptor acceptor(io, {tcp::v4(), 8080});
        acceptor.set_option(asio::socket_base::reuse_address(true));

        std::cout << "Tez server starting on port 8080...\n";

        for (;;) {
            tcp::socket socket(io);
            acceptor.accept(socket);

            // Read request line + headers
            std::string req;
            boost::system::error_code ec;
            read_until(socket, asio::dynamic_buffer(req), "\r\n\r\n", ec);
            if (ec) {
                std::cerr << "Error reading request: " << ec.message() << "\n";
                continue;
            }

            // Very simple response
            std::string body = "Hello from Tez!\r\n";
            std::string resp;
            resp.reserve(128);
            resp += "HTTP/1.1 200 OK\r\n";
            resp += "Content-Type: text/plain; charset=utf-8\r\n";
            resp += "Connection: close\r\n";
            resp += "Content-Length: " + std::to_string(body.size()) + "\r\n";
            resp += "\r\n";
            resp += body;

            write(socket, asio::buffer(resp), ec);
            if (ec) {
                std::cerr << "Error sending response: " << ec.message() << "\n";
            }

            boost::system::error_code ignored;
            socket.shutdown(tcp::socket::shutdown_both, ignored);
            socket.close(ignored);
        }
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
}
