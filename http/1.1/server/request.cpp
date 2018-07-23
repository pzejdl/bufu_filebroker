#include "request.hpp"

namespace http_server {

bool request::query(const std::string& key, std::string& value) const
{
    // Find if we have a specific handler for the path given
    auto iter = std::find_if(query_params_.cbegin(), query_params_.cend(),
        [&key](const std::pair<std::string, std::string>& keyvalue) { return keyvalue.first == key; });

    if (iter != query_params_.cend()) {
        value = iter->second;
        return true;
    }
    return false;
}

bool request::parse_uri()
{
    string_view uri{ target() };
    string_view path{ target() };
    string_view query;

    // Split uri into path and query
    auto query_pos = uri.find_first_of('?');
    if (query_pos != std::string::npos) {
        path = uri.substr(0, query_pos);
        query = uri.substr(query_pos + 1);
    }

    // Count how many key-value pairs we may have and reserve a storage for them
    auto nb_keyvalues = std::count_if(query.cbegin(), query.cend(), [](char ch) { return ch == '&' || ch == ';'; });
    query_params_.reserve(nb_keyvalues);

    // Parse query attribute-value pairs
    if (!query.empty()) {
        do {
            query_pos = query.find_first_of("&;");

            auto keyvalue = query.substr(0, query_pos);
            if (!keyvalue.empty()) {
                // Split pair into key and value
                auto sep = keyvalue.find_first_of('=');

                std::string key;
                std::string value;

                // Decode key
                if (!url_decode(keyvalue.substr(0, sep), key)) {
                    return false;
                }

                // Decode value
                if (sep != std::string::npos) {
                    if (!url_decode(keyvalue.substr(sep + 1), value)) {
                        return false;
                    }
                } else {
                    value = "";
                }

                query_params_.emplace_back(std::move(key), std::move(value));
            }

            query.remove_prefix(query_pos + 1);
        } while (query_pos != std::string::npos);
    }

    if (!url_decode(path, path_)) {
        return false;
    }

    return true;
}

bool url_decode(const string_view& in, std::string& out)
{
    out.clear();
    out.reserve(in.size());
    for (std::size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%') {
            if (i + 3 <= in.size()) {
                int value = 0;
                char* input = const_cast<char*>(in.data()) + i + 1;
                char* end;
                value = std::strtol(input, &end, 16);

                if (input != end) {
                    out += static_cast<char>(value);
                    i += 2;
                } else {
                    return false;
                }
            } else {
                return false;
            }
        } else if (in[i] == '+') {
            out += ' ';
        } else {
            out += in[i];
        }
    }
    return true;
}

} // namespace http_server
