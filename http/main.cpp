//
// main.cpp
// ~~~~~~~~
//
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include "server.hpp"


int main(int argc, char* argv[])
{
  try
  {
    // Check command line arguments.
    if (argc != 4)
    {
      std::cerr << "Usage: http_server <address> <port> <doc_root>\n";
      std::cerr << "  For IPv4, try:\n";
      std::cerr << "    receiver 0.0.0.0 80 .\n";
      std::cerr << "  For IPv6, try:\n";
      std::cerr << "    receiver 0::0 80 .\n";
      return 1;
    }

    // Initialise the server.
    http::server::server s(argv[1], argv[2], argv[3]);


    s.request_handler().add_handler("/index.html",
      [](const http::server::request& req, http::server::reply& rep)
      {
        rep.content_type = "text/html";

        std::ostringstream os;
        os
          << "<html>\n"
          <<  "<head><title>Server</title></head>\n"
          <<  "<body>\n"
          <<  "<h1>Hello World!</h1>\n"
          <<  "<p>There have been "
          <<  "XXX"
          <<  " requests so far.</p>\n"
          <<  "</body>\n"
          <<  "</html>\n";

        rep.content = os.str();
      });


    s.request_handler().add_handler("/test",
      [](const http::server::request& req, http::server::reply& rep)
      {
        rep.content_type = "text/plain";
        rep.content.append("Test: Hello, I'm alive!");
      });


    s.request_handler().add_handler("/test2",
      [](const http::server::request& req, http::server::reply& rep)
      {
        rep.content_type = "text/plain";
        rep.content.append("test2");
      });


    s.request_handler().add_handler("/test3",
      [](const http::server::request& req, http::server::reply& rep)
      {
        rep.content_type = "text/plain";
        rep.content.append("test3");
      });

    // Run the server until stopped.
    s.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "exception: " << e.what() << "\n";
  }

  return 0;
}
