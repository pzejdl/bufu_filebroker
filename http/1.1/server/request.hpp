#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <boost/beast/http.hpp>
#include <boost/utility/string_view.hpp>

// Before C++17 we use string_view from boost
using string_view = boost::string_view;

namespace http = boost::beast::http; // from <boost/beast/http.hpp>

namespace http_server {

struct request;
typedef http_server::request request_t;

//typedef http::request<http::string_body> request_detail_t;

typedef std::vector<std::pair<std::string, std::string>> query_params_t;

/// A request received from a client.
//template<class Body>
struct request : http::request<http::string_body> {

    // Inherit constructor(s)
    using http::request<http::string_body>::request;

    /// Parses URI into path and quary parameters
    bool parse_uri();

    // TODO: should be named differently and put inside query_params...
    /// Returns a value for query parameter specificied as a key
    bool query(const std::string& key, std::string& value) const;

private:
    friend class request_handler;

    /// Storage for query parameters
    query_params_t query_params_;

    /// URI path (without query postfix, i.e. before '?' character)
    std::string path_;
};

bool url_decode(const string_view& in, std::string& out);

} // namespace http_server

#endif // HTTP_REQUEST_HPP
