#include "listener.hpp"
#include "session.hpp"
#include "fail.hpp"

namespace http_server {

listener::listener(
        boost::asio::io_context& ioc,
        tcp::endpoint endpoint,
        std::string const& doc_root,
        const request_handler& req_handler)
        : ioc_(ioc)
        , acceptor_(boost::asio::make_strand(ioc))
        , doc_root_(doc_root)
        , request_handler_(req_handler)
{
    boost::system::error_code ec;

    // Open the acceptor
    acceptor_.open(endpoint.protocol(), ec);
    if(ec)
    {
        THROW_FAIL(ec, "open");
        return;
    }

    // Allow address reuse
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
    if(ec)
    {
        THROW_FAIL(ec, "set_option");
        return;
    }

    // Bind to the server address
    acceptor_.bind(endpoint, ec);
    if(ec)
    {
        THROW_FAIL(ec, "bind");
        return;
    }

    // Start listening for connections
    acceptor_.listen(
        boost::asio::socket_base::max_listen_connections, ec);
    if(ec)
    {
        THROW_FAIL(ec, "listen");
        return;
    }
}


void listener::run()
{
    do_accept();
}

void listener::do_accept()
{
    acceptor_.async_accept(
        boost::asio::make_strand(ioc_),
        boost::beast::bind_front_handler(
            &listener::on_accept,
            shared_from_this()));
}

void listener::on_accept(boost::system::error_code ec, tcp::socket socket)
{
    if(ec)
    {
        FAIL(ec, "accept");
    }
    else
    {
        // Create the session and run it
        std::make_shared<session>(
            std::move(socket),
            doc_root_,
            request_handler_)->run();
    }

    // Accept another connection
    do_accept();
}

} // namespace http_server
