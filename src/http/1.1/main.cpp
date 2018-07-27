//
// main.cpp
// ~~~~~~~~

#include "server.hpp"
#include <iostream>

int main(int argc, char* argv[])
{
    try {
        // Check command line arguments.
        if (argc != 5) {
            std::cerr << "Usage: http_server <address> <port> <doc_root> <threads>\n";
            std::cerr << "  For IPv4, try:\n";
            std::cerr << "    receiver 0.0.0.0 80 . 1\n";
            std::cerr << "  For IPv6, try:\n";
            std::cerr << "    receiver 0::0 80 . 1\n";
            return 1;
        }

        auto const threads = std::max<int>(1, std::atoi(argv[4]));

        // Initialise the server.
        http_server::server s(argv[1], argv[2], argv[3], threads);

        s.request_handler().add("/index.html",
            [](const http_server::request_t& req, http_server::response_t& res) {
                res.set(http::field::content_type, "text/html");

                std::ostringstream os;
                os
                    << "<html>\n"
                    << "<head><title>Server</title></head>\n"
                    << "<body>\n"
                    << "<h1>Hello World!</h1>\n"
                    << "<p>There have been "
                    << "XXX"
                    << " requests so far.</p>\n"
                    << "</body>\n"
                    << "</html>\n";

                res.body() = os.str();
            });

        s.request_handler().add("/test",
            [](const http_server::request_t& req, http_server::response_t& res) {
                res.set(http::field::content_type, "text/plain");
                res.body().append("Test: Hello, I'm alive!");
            });

        s.request_handler().add("/test2",
            [](const http_server::request_t& req, http_server::response_t& res) {
                res.set(http::field::content_type, "text/plain");
                res.body().append("test2");
            });

        s.request_handler().add("/test3",
            [](const http_server::request_t& req, http_server::response_t& res) {
                res.set(http::field::content_type, "text/plain");
                res.body().append("test3");
            });

        // Run the server until stopped.
        s.run();
    } catch (std::exception& e) {
        std::cerr << "exception: " << e.what() << "\n";
    }

    return 0;
}
