//
// request_handler.hpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef HTTP_REQUEST_HANDLER_HPP
#define HTTP_REQUEST_HANDLER_HPP

#include <string>
#include <functional>   // std::function
#include <utility>      // std::pair
#include <vector>

 #include <boost/utility/string_ref.hpp> 


namespace http {
namespace server {

struct reply;
struct request;

typedef std::function<void(const request& req, reply& rep)> RequestHandler_t;


/// The common handler for all incoming requests.
class request_handler
{
public:
  request_handler(const request_handler&) = delete;
  request_handler& operator=(const request_handler&) = delete;

  /// Construct with a directory containing files to be served.
  explicit request_handler(const std::string& doc_root, bool debug_http_requests);

  /// Handle a request and produce a reply.
  void handle_request(request& req, reply& rep);

  // Register a handler for the specific path
  void add(std::string&& path, RequestHandler_t&& handler)
  {
    handlers_.emplace_back( std::move(path), std::move(handler) );
  }

private:
  /// The directory containing the files to be served.
  std::string doc_root_;

  /// DEBUG: Prints http request
  bool debug_http_requests_;

  std::vector< std::pair<std::string,RequestHandler_t> > handlers_;

  // Splits URI into path and query
  bool parse_uri(request& req);

  /// Perform URL-decoding on a string. Returns false if the encoding was
  /// invalid.
  static bool url_decode(const boost::string_ref& in, std::string& out);
};

} // namespace server
} // namespace http

#endif // HTTP_REQUEST_HANDLER_HPP
