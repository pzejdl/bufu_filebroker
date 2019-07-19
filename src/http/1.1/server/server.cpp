//------------------------------------------------------------------------------
//
// Example: HTTP server, asynchronous
//
//------------------------------------------------------------------------------

#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "listener.hpp"
#include "server.hpp"

using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

namespace http_server {

server::server(const std::string address_str, const std::string port_str, const std::string doc_root, int threads, bool debug_http_requests)
    : doc_root_(doc_root)
    , io_context_(threads)
    , threads_(threads)
    , request_handler_(doc_root_, debug_http_requests)
{

    auto const address = boost::asio::ip::make_address(address_str);
    auto const port = static_cast<unsigned short>(std::stoi(port_str));

    // Create and launch a listening port
    std::make_shared<listener>(
        io_context_,
        tcp::endpoint{ address, port },
        doc_root_,
        request_handler_)
        ->run();

    runners_.reserve(threads - 1);
}

void server::run()
{
    // The io_context::run() call will block until all asynchronous operations
    // have finished. While the server is running, there is always at least one
    // asynchronous operation outstanding: the asynchronous accept call waiting
    // for new incoming connections.

    // Run the I/O service on the requested number of threads
    for (auto i = threads_ - 1; i > 0; --i)
        runners_.emplace_back(
            [this] {
                io_context_.run();
            });
    io_context_.run();
}

} // namespace http_server
