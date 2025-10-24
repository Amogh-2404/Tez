#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include "router.hpp"
#include "middleware.hpp"
#include "file_server.hpp"

using boost::asio::ip::tcp;
namespace asio = boost::asio;

void handle_request(tcp::socket socket){
    auto do_read = [&socket](std::string req){
        boost::system::error_code ec;
        read_until(socket, asio::dynamic_buffer(req),"\r\n\r\n", ec);
        if(ec){
            std::cerr<<"Error reading: "<<ec.message()<<"\n";
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

        // Build and send response
        std::string resp;
        resp.reserve(128);
        resp += "HTTP/1.1 " + response.status + "\r\n";
        resp += "Content-Type: " + response.content_type + "\r\n";
        resp += "Connection: close\r\n";
        resp += "Content-Length: " + std::to_string(response.body.size()) + "\r\n";
        resp += "\r\n";
        resp += response.body;

        // Send response
        write(socket, asio::buffer(resp), ec);
        if(ec){
            std::cerr<<"Error sending response: "<<ec.message()<<"\n";
        }

        // Close connection
        boost::system::error_code ignored;
        socket.shutdown(tcp::socket::shutdown_both, ignored);
        socket.close(ignored);
    };

    std::string req;
    do_read(req);
}


int main() {
    try {
        asio::io_context io;

        tcp::acceptor acceptor(io, {tcp::v4(), 8080});
        acceptor.set_option(asio::socket_base::reuse_address(true));

        std::cout << "Tez server starting on port 8080...\n";

      // Async accept loop   
      std::function<void()> do_accept;
        do_accept = [&](){
            auto socket = std::make_shared<tcp::socket>(io);
            acceptor.async_accept(*socket, [&, socket](boost::system::error_code ec){
                if(!ec){
                    handle_request(std::move(*socket));
                }
                do_accept();
            });
         };
         do_accept();
         io.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
