//#pragma once
//#include <string>
//#include <unordered_map>
//#include <sstream>
//
//inline std::string url_decode(const std::string& value)
//{
//    std::string result;
//    result.reserve(value.size());
//
//    for (size_t i = 0; i < value.size(); i++)
//    {
//        if (value[i] == '%' && i + 2 < value.size())
//        {
//            int hex_val = std::stoi(value.substr(i + 1, 2), nullptr, 16);
//            result += static_cast<char>(hex_val);
//            i += 2;
//        }
//        else if (value[i] == '+')
//        {
//            result += ' ';
//        }
//        else
//        {
//            result += value[i];
//        }
//    }
//
//    return result;
//}
//
//inline std::unordered_map<std::string, std::string> parse_form_urlencoded(const std::string& body)
//{
//    std::unordered_map<std::string, std::string> result;
//    std::istringstream stream(body);
//    std::string pair;
//
//    while (std::getline(stream, pair, '&'))
//    {
//        auto eq = pair.find('=');
//        if (eq != std::string::npos)
//        {
//            std::string key = pair.substr(0, eq);
//            std::string value = url_decode(pair.substr(eq + 1));
//            result[key] = value;
//        }
//    }
//
//    return result;
//}

#pragma once
#include <string>
#include <unordered_map>
#include <sstream>

inline std::string url_decode(const std::string& value)
{
    std::string result;
    result.reserve(value.size());

    for (size_t i = 0; i < value.size(); i++)
    {
        if (value[i] == '%' && i + 2 < value.size())
        {
            int hex_val = std::stoi(value.substr(i + 1, 2), nullptr, 16);
            result += static_cast<char>(hex_val);
            i += 2;
        }
        else if (value[i] == '+')
        {
            result += ' ';
        }
        else
        {
            result += value[i];
        }
    }

    return result;
}

inline std::unordered_map<std::string, std::string> parse_form_urlencoded(const std::string& body)
{
    std::unordered_map<std::string, std::string> result;
    std::istringstream stream(body);
    std::string pair;

    while (std::getline(stream, pair, '&'))
    {
        auto eq = pair.find('=');
        if (eq != std::string::npos)
        {
            std::string key = pair.substr(0, eq);
            std::string value = url_decode(pair.substr(eq + 1));
            result[key] = value;
        }
    }

    return result;
}