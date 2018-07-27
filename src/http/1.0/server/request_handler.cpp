//
// request_handler.cpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "request_handler.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include "mime_types.hpp"
#include "reply.hpp"
#include "request.hpp"

#include <boost/utility/string_ref.hpp> 
#include <iostream>


namespace http {
namespace server {

request_handler::request_handler(const std::string& doc_root, bool debug_http_requests)
  : doc_root_(doc_root),
    debug_http_requests_(debug_http_requests)
{
}


// TODO: move to request_parser
bool request_handler::parse_uri(request& req)
{
  boost::string_ref uri {req.uri};
  boost::string_ref path {req.uri};
  boost::string_ref query;

  // Split uri into path and query
  auto query_pos = uri.find_first_of('?');
  if (query_pos != std::string::npos) {
    path = uri.substr(0, query_pos);
    query = uri.substr(query_pos+1);
  } 

  // Count how many key-value pairs we may have and reserve a storage for them
  auto nb_keyvalues = std::count_if( query.cbegin(), query.cend(),[](char ch){return ch == '&' || ch == ';';} );
  req.query_params.reserve(nb_keyvalues);

  // Parse query attribute-value pairs
  if ( !query.empty() ) {
    do {
      query_pos = query.find_first_of("&;");

      auto keyvalue = query.substr(0, query_pos);
      if ( !keyvalue.empty() ) {
        // Split pair into key and value
        auto sep = keyvalue.find_first_of('=');

        std::string key;
        std::string value; 

        if ( 
          !url_decode( keyvalue.substr(0, sep), key ) ||
          !url_decode( (sep != std::string::npos ? keyvalue.substr(sep+1) : ""), value )
        ) {
          return false;
        }

        req.query_params.emplace_back( std::move(key), std::move(value) );
      }

      query.remove_prefix(query_pos+1);
    } while ( query_pos != std::string::npos );
  }

  if (!url_decode(path, req.path)) {
    return false;
  }

  // Request path must be absolute and not contain "..".
  if (req.path.empty() || req.path[0] != '/'
      || req.path.find("..") != std::string::npos)
  {
    return false;
  }

  return true;
}

void request_handler::handle_request(request& req, reply& rep)
{
  if ( !parse_uri(req) ) {
    rep = reply::stock_reply(reply::bad_request);
    return;   
  }

  // If path ends in slash (i.e. is a directory) then add "index.html".
  if (req.path[req.path.size() - 1] == '/')
  {
    req.path += "index.html";
  }


  // DEBUG
  if (debug_http_requests_) {
    std::cout << "HTTP DEBUG: " << req.method << " '" << req.uri << "' URL_DECODE: '" << req.path << '\'';
    std::cout << " PARAMS: [";
    auto it = req.query_params.cbegin();
    auto end = req.query_params.cend();
    if (it != end) {
      std::cout << '\'' << (*it).first << "'='" << (*it).second << '\'';
      ++it;
    }
    while (it != end) {
      std::cout << ", '" << (*it).first << "'='" << (*it).second << '\'';
      ++it;
    }
    std::cout << ']' << std::endl;

    // std::cout << "HEADERS:" << std::endl;
    // for (const auto& header: req.headers) {
    //   std::cout << "  " << header.name << '=' << header.value << std::endl;
    // }
  }


  // Find if we have a specific handler for the path given
  auto iter = std::find_if(handlers_.cbegin(), handlers_.cend(),
    [&req](const std::pair<std::string,RequestHandler_t>& handler) { return handler.first == req.path;}
    );

  if (iter == handlers_.cend()) {
    rep = reply::stock_reply(reply::not_found);
    return;
  } 

  // By default reply status is OK. Can be changed by the handler
  rep.status = reply::ok;

  // Call the handler
  iter->second(req, rep);

  // At the moment headers should not be modified directly by handlers
  assert( rep.headers.empty() );

  // Prepare the headers
  rep.headers.resize(2);
  rep.headers[0].name = "Content-Length";
  rep.headers[0].value = std::to_string(rep.content.size());
  rep.headers[1].name = "Content-Type";
  rep.headers[1].value = ( rep.content_type.size() ? std::move(rep.content_type) : "text/html" );


  // // Determine the file extension.
  // std::size_t last_slash_pos = request_path.find_last_of("/");
  // std::size_t last_dot_pos = request_path.find_last_of(".");
  // std::string extension;
  // if (last_dot_pos != std::string::npos && last_dot_pos > last_slash_pos)
  // {
  //   extension = request_path.substr(last_dot_pos + 1);
  // }

  // // Open the file to send back.
  // std::string full_path = doc_root_ + request_path;
  // std::ifstream is(full_path.c_str(), std::ios::in | std::ios::binary);
  // if (!is)
  // {
  //   rep = reply::stock_reply(reply::not_found);
  //   return;
  // }

  // // Fill out the reply to be sent to the client.
  // rep.status = reply::ok;
  // char buf[512];
  // while (is.read(buf, sizeof(buf)).gcount() > 0)
  //   rep.content.append(buf, is.gcount());
  // rep.headers.resize(2);
  // rep.headers[0].name = "Content-Length";
  // rep.headers[0].value = std::to_string(rep.content.size());
  // rep.headers[1].name = "Content-Type";
  // rep.headers[1].value = mime_types::extension_to_type(extension);
}


// TODO: move to request_parser
bool request_handler::url_decode(const boost::string_ref& in, std::string& out)
{
  out.clear();
  out.reserve(in.size());
  for (std::size_t i = 0; i < in.size(); ++i)
  {
    if (in[i] == '%')
    {
      if (i + 3 <= in.size())
      {
        int value = 0;
        //std::istringstream is(in.substr(i + 1, 2));
 
        char *input = const_cast<char *>(in.data()) + i + 1;
        char *end;
        value = std::strtol( input, &end, 16 );

        //if (is >> std::hex >> value)
        if (input != end)
        {
          out += static_cast<char>(value);
          i += 2;
        }
        else
        {
          return false;
        }
      }
      else
      {
        return false;
      }
    }
    else if (in[i] == '+')
    {
      out += ' ';
    }
    else
    {
      out += in[i];
    }
  }
  return true;
}

} // namespace server
} // namespace http
