#include "request_handler.hpp"

namespace http_server {

request_handler::request_handler(const std::string& doc_root, bool debug_http_requests)
  : doc_root_(doc_root),
    debug_http_requests_(debug_http_requests)
{
}

} // namespace http_server
