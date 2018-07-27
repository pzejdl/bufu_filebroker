#ifndef HTTP_REQUEST_HANDLER_HPP
#define HTTP_REQUEST_HANDLER_HPP

#include <boost/beast/http.hpp>
#include <boost/optional.hpp>
#include <string>
#include <vector>

#include "request.hpp"

//using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
namespace http = boost::beast::http; // from <boost/beast/http.hpp>

namespace http_server {

typedef http::response<http::string_body> response_t;

/// Request handler callback
typedef std::function<void(const request_t& req, response_t& rep)> request_handler_t;

/// The common handler for all incoming requests.
class request_handler {
public:
    request_handler(const request_handler&) = delete;
    request_handler& operator=(const request_handler&) = delete;

    /// Construct with a directory containing files to be served.
    explicit request_handler(const std::string& doc_root, bool debug_http_requests);

    /// Handle a request and produce a reply.

    // This function produces an HTTP response for the given
    // request. The type of the response object depends on the
    // contents of the request, so the interface requires the
    // caller to pass a generic lambda for receiving the response.
    template <class Send>
    void handle_request(
        boost::beast::string_view doc_root,
        http_server::request_t&& req,
        Send&& send) const;

    // Register a request handler for the specific path
    void add(std::string&& path, request_handler_t&& handler)
    {
        handlers_.emplace_back(std::move(path), std::move(handler));
    }

    // Returns a request handler for the specific path
    boost::optional<request_handler_t> handler(const std::string& path) const
    {
        auto iter = std::find_if(handlers_.cbegin(), handlers_.cend(),
            [&path](const std::pair<std::string, request_handler_t>& handler) { return handler.first == path; });

        if (iter == handlers_.cend()) {
            return boost::none;
        }
        return iter->second;
    }

private:
    /// The directory containing the files to be served.
    std::string doc_root_;

    /// DEBUG: Prints http request details
    bool debug_http_requests_;

    /// Store all handlers (Note: vector as a map is the fastest solution for small amount of elements)
    std::vector<std::pair<std::string, request_handler_t>> handlers_;
};

} // namespace http_server

#include "request_handler.ipp"

#endif // HTTP_REQUEST_HANDLER_HPP
