//
// request.hpp
// ~~~~~~~~~~~
//
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <string>
#include <vector>
#include <algorithm>
#include "header.hpp"

namespace http {
namespace server {

typedef std::vector< std::pair<std::string,std::string> > query_params_t;

/// A request received from a client.
struct request
{

  // TODO: should be named differently and put inside query_params...
  bool getParam(const std::string& key, std::string& value) const {

    // Find if we have a specific handler for the path given
    auto iter = std::find_if(query_params.cbegin(), query_params.cend(),
      [&key](const std::pair<std::string,std::string>& keyvalue) { return keyvalue.first == key;}
      );

    if (iter != query_params.cend()) {
      value = iter->second;
      return true;
    } 
    return false;
  }

  std::string method;
  std::string uri;
  int http_version_major;
  int http_version_minor;
  std::vector<header> headers;
  std::string path;
  query_params_t query_params;
};

} // namespace server
} // namespace http

#endif // HTTP_REQUEST_HPP
