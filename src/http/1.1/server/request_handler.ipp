#ifndef HTTP_REQUEST_HANDLER_IPP
#define HTTP_REQUEST_HANDLER_IPP

#include <iostream>
#include <boost/beast/core.hpp>
#include <boost/beast/version.hpp>

namespace http_server {

// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template<class Send>
void request_handler::handle_request(
    boost::beast::string_view doc_root,
    http_server::request_t && req,
    Send&& send) const
{

    // Suppress warning for doc_root not being used
    (void)doc_root;

    //TODO: Move these lambdas to a stock_reply

    // Returns a bad request response
    auto const bad_request =
    [&req](boost::beast::string_view why)
    {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = std::string(why);
        res.prepare_payload();
        return res;
    };

    // Returns a not found response
    auto const not_found =
    [&req](boost::beast::string_view target)
    {
        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "The resource '" + std::string(target) + "' was not found.";
        res.prepare_payload();
        return res;
    };

    // Returns a server error response
    /* NOT USED now
    auto const server_error =
    [&req](boost::beast::string_view what)
    {
        http::response<http::string_body> res{http::status::internal_server_error, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "An error occurred: '" + what.to_string() + "'";
        res.prepare_payload();
        return res;
    };
    */

    // Make sure we can handle the method
    if( req.method() != http::verb::get) {
        return send(bad_request("Unsupported HTTP-method"));
    }

    if ( !req.parse_uri() ) {
        std::ostringstream os;
        os << "URI parser failed parsing '" << req.target() << '\'';
        return send(bad_request( os.str() ));  
    }

    // DEBUG
    // TODO: Don't access req.path_ and req.query_params_ directly
    if (debug_http_requests_) {
        std::cout << "HTTP DEBUG: " << req.method_string() << " '" << req.target() << "' URL_DECODE: '" << req.path_ << '\'';

        std::cout << " PARAMS: [";
        auto it = req.query_params_.cbegin();
        auto end = req.query_params_.cend();
        if (it != end) {
            std::cout << '\'' << (*it).first << "'='" << (*it).second << '\'';
            ++it;
        }
        while (it != end) {
            std::cout << ", '" << (*it).first << "'='" << (*it).second << '\'';
            ++it;
        }
        std::cout << ']' << std::endl;

        std::cout << "HEADERS:" << '\n';
        for(auto const& field : req) {
            std::cout << field.name() << "=" << field.value() << '\n';
        }
    }

    std::string path = req.path_;

    // Request path must be absolute and not contain "..".
    if (path.empty() || path[0] != '/' || path.find("..") != std::string::npos) {
        return send(bad_request("Illegal request-target"));
    }

    // If path ends in slash (i.e. is a directory) then add "index.html".
    if (path.back() == '/') {
        path.append("index.html");
    }

    // Find a request handler for the path
    auto request_handler_func = handler(path);
    if (!request_handler_func) {
        return send(not_found(path));
    } 

    // Prepare the response
    response_t res{http::status::ok, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());

    // Call the particular request handler
    (*request_handler_func)( req, res );

    res.prepare_payload();
    return send(std::move(res));
}

} // namespace http_server


#endif // HTTP_REQUEST_HANDLER_IPP
