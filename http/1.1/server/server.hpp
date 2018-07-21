#ifndef HTTPD_SERVER_HPP
#define HTTPD_SERVER_HPP

#include <boost/asio.hpp>
#include "request_handler.hpp"

namespace http_server {

/// The top-level class of the HTTP server.
class server {
public:
    server(const server&) = delete;
    server& operator=(const server&) = delete;

    /// Construct the server to listen on the specified TCP address and port, and
    /// serve up files from the given directory.
    explicit server(const std::string& address, const std::string& port,
        const std::string& doc_root, int threads, bool debug_http_requests = false);

    class request_handler& request_handler()
    {
        return request_handler_;
    }

    /// Run the server's io_service loop.
    void run();

private:
    /// The io_context used to perform asynchronous operations.
    boost::asio::io_context io_context_;

    /// Number of threads we have
    int threads_;

    /// thread storage
    std::vector<std::thread> runners_;

    /// The handler for all incoming requests.
    class request_handler request_handler_;
};

} // namespace http_server

#endif // HTTPD_SERVER_HPP
