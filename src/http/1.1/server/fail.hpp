#ifndef HTTPD_FAIL_HPP
#define HTTPD_FAIL_HPP

#include <iostream>
#include <string>
#include <boost/asio.hpp>

#include "tools/log.h"
#include "tools/exception.h"


#define FAIL(ec, what)              http_fail( ec, what, TOOLS_DEBUG_INFO, false )
#define THROW_FAIL(ec, what)        http_fail( ec, what, TOOLS_DEBUG_INFO, true )

namespace http_server {

/* 
 * Report a failure
 */
void http_fail(boost::system::error_code ec, const char *what, const char *where, bool throw_exception)
{
    std::ostringstream os;
    //std::cerr << "ERROR: " << what << ": " << ec.message() << "\n";
    os << where << ": HTTP SERVER ERROR: " << what << ": " << ec.message();
    LOG(WARNING) << os.str();
    if (throw_exception) {
        throw std::runtime_error(os.str());
    }
}

} // namespace http_server

#endif // HTTPD_FAIL_HPP
