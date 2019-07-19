#ifndef HTTPD_LISTENER_HPP
#define HTTPD_LISTENER_HPP

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <memory>
#include <string>

using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace http_server {

// Forward declarations
class request_handler;

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this<listener> {
    boost::asio::io_context& ioc_;
    tcp::acceptor acceptor_;
    std::string const& doc_root_;
    const request_handler& request_handler_;

public:
    listener(const listener&) = delete;
    listener& operator=(const listener&) = delete;

    explicit listener(boost::asio::io_context& ioc, tcp::endpoint endpoint, std::string const& doc_root, const request_handler& req_handler);

    // Start accepting incoming connections
    void run();

    void do_accept();

    void on_accept(boost::system::error_code ec, tcp::socket socket);
};

} // namespace http_server

#endif // HTTPD_LISTENER_HPP
