#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <csignal>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <locale>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "file_server.hpp"
#include "middleware.hpp"
#include "request.hpp"
#include "router.hpp"

#ifndef TEZ_VERSION
#define TEZ_VERSION "1.1.0-dev"
#endif

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

namespace {
struct Options {
    std::string address = "127.0.0.1";
    unsigned short port = 8080;
    unsigned threads = std::max(1u, std::min(8u, std::thread::hardware_concurrency()));
    std::string config;
    std::string static_dir;
    unsigned timeout = 30;
    std::size_t max_connections = 128;
    std::size_t body_limit = DEFAULT_BODY_LIMIT;
};

void usage(std::ostream &out) {
    out << "Usage: Tez [options]\n"
        << "  --address ADDRESS       Bind address (default: 127.0.0.1)\n"
        << "  --port PORT             TCP port; 0 selects a free port (default: 8080)\n"
        << "  --threads COUNT         I/O workers, 1..256 (default: min(CPU threads, 8))\n"
        << "  --config PATH           Route configuration JSON\n"
        << "  --static-dir PATH       Static file directory\n"
        << "  --timeout SECONDS       Deadline per header, body, or write, 1..3600 (default: 30)\n"
        << "  --max-connections COUNT Concurrent connections, 1..65536 (default: 128)\n"
        << "  --body-limit BYTES      Request body limit, 1..10485760 (default: 1048576)\n"
        << "  --help                  Show this help\n"
        << "  --version               Show version\n";
}

std::size_t number(const std::string &value, std::size_t min, std::size_t max,
                   const std::string &option) {
    std::size_t result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (value.empty() || parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
        result < min || result > max)
        throw std::invalid_argument("Invalid value for " + option + ": " + value);
    return result;
}

Options parse_options(int argc, char *argv[]) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string option = argv[i];
        if (i + 1 >= argc)
            throw std::invalid_argument("Missing value for " + option);
        const std::string value = argv[++i];
        if (option == "--address")
            options.address = value;
        else if (option == "--port")
            options.port = static_cast<unsigned short>(number(value, 0, 65535, option));
        else if (option == "--threads")
            options.threads = static_cast<unsigned>(number(value, 1, 256, option));
        else if (option == "--config")
            options.config = value;
        else if (option == "--static-dir")
            options.static_dir = value;
        else if (option == "--timeout")
            options.timeout = static_cast<unsigned>(number(value, 1, 3600, option));
        else if (option == "--max-connections")
            options.max_connections = number(value, 1, 65536, option);
        else if (option == "--body-limit")
            options.body_limit = number(value, 1, MAX_CONTENT_LENGTH, option);
        else
            throw std::invalid_argument("Unknown option: " + option);
        if (value.empty())
            throw std::invalid_argument("Empty value for " + option);
    }
    return options;
}

std::string http_date() {
    const auto now = std::time(nullptr);
    std::tm utc{};
    gmtime_r(&now, &utc);
    std::ostringstream date;
    date.imbue(std::locale::classic());
    date << std::put_time(&utc, "%a, %d %b %Y %H:%M:%S GMT");
    return date.str();
}

bool safe_field(const std::string &value) {
    return std::all_of(value.begin(), value.end(),
                       [](unsigned char ch) { return ch == '\t' || (ch >= 0x20 && ch != 0x7f); });
}

class Session : public std::enable_shared_from_this<Session> {
  public:
    Session(tcp::socket socket, const Options &options, std::shared_ptr<std::atomic_size_t> active)
        : stream_(std::move(socket)), buffer_(MAX_HEADER_SIZE + 4096), options_(options),
          active_(std::move(active)) {
        ++*active_;
        boost::system::error_code ec;
        const auto endpoint = stream_.socket().remote_endpoint(ec);
        client_ip_ = ec ? "unknown" : endpoint.address().to_string();
    }

    ~Session() {
        --*active_;
    }

    void run() {
        // The accepted socket has a strand executor. All session state and stream
        // operations stay on it, including the initial read.
        asio::dispatch(stream_.get_executor(),
                       [self = shared_from_this()] { self->read_header(); });
    }

  private:
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    Options options_;
    std::shared_ptr<std::atomic_size_t> active_;
    std::string client_ip_;
    std::optional<http::request_parser<http::string_body>> parser_;
    std::optional<http::request_header<>> initial_header_;
    std::optional<http::response<http::string_body>> response_;
    std::optional<http::response_serializer<http::string_body>> serializer_;
    http::response<http::empty_body> interim_{http::status::continue_, 11};
    std::size_t request_count_ = 0;
    bool head_ = false;
    bool keep_alive_ = false;
    unsigned version_ = 11;

    void deadline() {
        stream_.expires_after(std::chrono::seconds(options_.timeout));
    }

    void close() {
        boost::system::error_code ignored;
        stream_.socket().shutdown(tcp::socket::shutdown_both, ignored);
        stream_.socket().close(ignored);
    }

    void read_header() {
        serializer_.reset();
        response_.reset();
        parser_.emplace();
        initial_header_.reset();
        parser_->header_limit(MAX_HEADER_SIZE);
        parser_->body_limit(options_.body_limit);
        head_ = false;
        keep_alive_ = false;
        version_ = 11;
        deadline();
        http::async_read_header(stream_, buffer_, *parser_,
                                [self = shared_from_this()](boost::system::error_code ec,
                                                            std::size_t) { self->on_header(ec); });
    }

    void read_failure(const boost::system::error_code &ec) {
        head_ = head_ || parser_->get().method() == http::verb::head;
        if (ec == http::error::end_of_stream || ec == asio::error::eof ||
            ec == asio::error::connection_reset || ec == asio::error::operation_aborted ||
            ec == beast::error::timeout)
            return close();
        if (ec == http::error::header_limit || ec == http::error::buffer_overflow)
            return fail(431, "Request headers exceed the size limit\n");
        if (ec == http::error::body_limit)
            return fail(413, "Request body exceeds the size limit\n");
        if (request_error_status(ec, beast::buffers_to_string(buffer_.data())) == 505)
            return fail(505, "HTTP version not supported\n");
        fail(400, "Malformed HTTP request\n");
    }

    void on_header(const boost::system::error_code &ec) {
        if (ec)
            return read_failure(ec);
        head_ = parser_->get().method() == http::verb::head;
        version_ = parser_->get().version() == 10 ? 10 : 11;
        try {
            validate_request_header(parser_->get().base());
        } catch (const RequestError &error) {
            return fail(error.status, std::string(error.what()) + "\n");
        }
        initial_header_ = parser_->get().base();
        if (parser_->get().method() == http::verb::connect)
            return fail(501, "CONNECT is not supported\n");
        if (parser_->is_done())
            return dispatch_request();
        if (parser_->get().count(http::field::expect)) {
            deadline();
            http::async_write(
                stream_, interim_,
                [self = shared_from_this()](boost::system::error_code write_ec, std::size_t) {
                    if (write_ec)
                        self->close();
                    else
                        self->read_body();
                });
        } else {
            read_body();
        }
    }

    void read_body() {
        deadline();
        http::async_read(stream_, buffer_, *parser_,
                         [self = shared_from_this()](boost::system::error_code ec, std::size_t) {
                             if (ec)
                                 self->read_failure(ec);
                             else
                                 self->dispatch_request();
                         });
    }

    void dispatch_request() {
        try {
            // Validate again after parsing trailers so they cannot inject Host,
            // Content-Length, or an unsupported transfer coding after the body.
            validate_request_header(parser_->get().base());
            validate_request_trailers(parser_->get().base(), *initial_header_);
            keep_alive_ = parser_->get().keep_alive() && ++request_count_ < MAX_KEEPALIVE_REQUESTS;
            auto request = make_request(parser_->release());
            log_request(client_ip_, request.method, request.path);
            Response result;
            if (request.path.compare(0, 8, "/static/") == 0) {
                if (request.method == "GET" || request.method == "HEAD")
                    result = serve_file(request.path);
                else {
                    result.status = "405 Method Not Allowed";
                    result.content_type = "text/plain; charset=utf-8";
                    result.body = "Method not allowed\n";
                    result.allow = "GET, HEAD";
                }
            } else {
                result = handle_route_with_method(request.method, request.path, request.body);
            }
            send(std::move(result));
        } catch (const RequestError &error) {
            fail(error.status, std::string(error.what()) + "\n");
        } catch (const std::exception &error) {
            std::cerr << "Request handling failed: " << error.what() << '\n';
            fail(500, "Internal server error\n");
        }
    }

    void fail(unsigned status, const std::string &body) {
        keep_alive_ = false;
        Response result;
        result.status = std::to_string(status) + " " +
                        std::string(http::obsolete_reason(static_cast<http::status>(status)));
        result.content_type = "text/plain; charset=utf-8";
        result.body = body;
        send(std::move(result));
    }

    void send(Response result) {
        unsigned status = 0;
        if (result.status.size() >= 4) {
            const auto parsed =
                std::from_chars(result.status.data(), result.status.data() + 3, status);
            if (parsed.ec != std::errc{} || parsed.ptr != result.status.data() + 3)
                status = 0;
        }
        if (status < 200 || status > 599 || result.status[3] != ' ' || !safe_field(result.status) ||
            !safe_field(result.content_type) || !safe_field(result.allow)) {
            keep_alive_ = false;
            result.status = "500 Internal Server Error";
            result.content_type = "text/plain; charset=utf-8";
            result.body = "Invalid application response\n";
            result.allow.clear();
            status = 500;
        }
        response_.emplace(static_cast<http::status>(status), version_);
        auto &response = *response_;
        response.reason(result.status.substr(4));
        response.set(http::field::server, "Tez");
        response.set(http::field::date, http_date());
        response.set(http::field::content_type, result.content_type);
        response.set("X-Content-Type-Options", "nosniff");
        if (!result.allow.empty())
            response.set(http::field::allow, result.allow);
        response.keep_alive(keep_alive_);
        if (!keep_alive_)
            response.set(http::field::connection, "close");
        // 204 and 304 cannot carry a message body; 204 also forbids Content-Length.
        // Omit the optional representation length on 304 rather than inventing one.
        if (status != 204 && status != 304) {
            response.body() = std::move(result.body);
            if (status == 205)
                response.body().clear();
            response.content_length(response.body().size());
        }
        serializer_.emplace(response);
        deadline();
        auto completed = [self = shared_from_this()](boost::system::error_code ec, std::size_t) {
            if (ec || !self->keep_alive_)
                self->close();
            else
                self->read_header();
        };
        if (head_)
            http::async_write_header(stream_, *serializer_, std::move(completed));
        else
            http::async_write(stream_, *serializer_, std::move(completed));
    }
};

class Listener : public std::enable_shared_from_this<Listener> {
  public:
    Listener(asio::io_context &io, const Options &options)
        : io_(io), acceptor_(asio::make_strand(io)), retry_(acceptor_.get_executor()),
          options_(options), active_(std::make_shared<std::atomic_size_t>(0)) {
        const tcp::endpoint endpoint(asio::ip::make_address(options.address), options.port);
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(asio::socket_base::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen(asio::socket_base::max_listen_connections);
    }

    unsigned short port() const {
        return acceptor_.local_endpoint().port();
    }

    void run() {
        accept();
    }

    void stop() {
        asio::dispatch(acceptor_.get_executor(), [self = shared_from_this()] {
            boost::system::error_code ignored;
            self->acceptor_.close(ignored);
            self->retry_.cancel();
            self->io_.stop();
        });
    }

  private:
    asio::io_context &io_;
    tcp::acceptor acceptor_;
    asio::steady_timer retry_;
    Options options_;
    std::shared_ptr<std::atomic_size_t> active_;

    void accept() {
        acceptor_.async_accept(asio::make_strand(io_), [self = shared_from_this()](
                                                           boost::system::error_code ec,
                                                           tcp::socket socket) {
            if (ec == asio::error::operation_aborted)
                return;
            if (ec) {
                std::cerr << "Accept failed: " << ec.message() << '\n';
                // Back off on descriptor/memory exhaustion instead of spinning.
                self->retry_.expires_after(std::chrono::seconds(1));
                self->retry_.async_wait([self](boost::system::error_code retry_ec) {
                    if (!retry_ec)
                        self->accept();
                });
                return;
            }
            if (self->active_->load() < self->options_.max_connections)
                std::make_shared<Session>(std::move(socket), self->options_, self->active_)->run();
            else {
                boost::system::error_code ignored;
                socket.close(ignored);
            }
            self->accept();
        });
    }
};
} // namespace

int main(int argc, char *argv[]) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--help") {
            usage(std::cout);
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--version") {
            std::cout << "Tez " << TEZ_VERSION << '\n';
            return 0;
        }
        const auto options = parse_options(argc, argv);
        init_router_config(options.config);
        configure_static_root(options.static_dir);
        asio::io_context io(static_cast<int>(options.threads));
        auto listener = std::make_shared<Listener>(io, options);
        asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([listener](boost::system::error_code ec, int) {
            if (!ec)
                listener->stop();
        });
        listener->run();
        std::cout << "Tez " << TEZ_VERSION << " listening on " << options.address << ':'
                  << listener->port() << " with " << options.threads << " I/O worker(s)"
                  << std::endl;

        std::atomic_bool failed{false};
        auto run = [&] {
            try {
                io.run();
            } catch (const std::exception &error) {
                failed = true;
                std::cerr << "I/O worker failed: " << error.what() << '\n';
                io.stop();
            }
        };
        std::vector<std::thread> workers;
        workers.reserve(options.threads - 1);
        try {
            for (unsigned i = 1; i < options.threads; ++i)
                workers.emplace_back(run);
        } catch (...) {
            io.stop();
            for (auto &worker : workers)
                worker.join();
            throw;
        }
        run();
        for (auto &worker : workers)
            worker.join();
        return failed ? 1 : 0;
    } catch (const std::exception &error) {
        std::cerr << "Tez: " << error.what() << '\n';
        return 1;
    }
}
