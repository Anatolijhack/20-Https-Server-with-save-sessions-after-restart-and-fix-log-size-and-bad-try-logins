//#pragma once
//#include <string>
//#include <openssl/sha.h>
//#include <openssl/evp.h>
//#include <nlohmann/json.hpp>
//
//using json = nlohmann::json;
//
//class LiqPayHelper
//{
//public:
//    LiqPayHelper(const std::string& public_key, const std::string& private_key)
//        : public_key(public_key), private_key(private_key) {}
//
//    // Создаёт data + signature для формы оплаты
//    struct PaymentForm
//    {
//        std::string data;
//        std::string signature;
//    };
//
//    PaymentForm create_payment(const std::string& order_ref, double amount, const std::string& description)
//    {
//        json payload = {
//            {"version", 3},
//            {"public_key", public_key},
//            {"action", "pay"},
//            {"amount", amount},
//            {"currency", "UAH"},
//            {"description", description},
//            {"order_id", order_ref},
//            {"result_url", "https://ваш-домен/payment/result"},
//            {"server_url", "https://ваш-домен/payment/callback"}
//        };
//
//        std::string json_str = payload.dump();
//        std::string data_b64 = base64_encode(json_str);
//        std::string signature = make_signature(data_b64);
//
//        return { data_b64, signature };
//    }
//
//    // Проверка подписи входящего callback от LiqPay
//    bool verify_signature(const std::string& data, const std::string& received_signature)
//    {
//        std::string expected = make_signature(data);
//        return expected == received_signature;
//    }
//
//    // Декодирует data обратно в JSON (для чтения статуса платежа)
//    json decode_data(const std::string& data_b64)
//    {
//        std::string decoded = base64_decode(data_b64);
//        return json::parse(decoded);
//    }
//
//private:
//    std::string public_key;
//    std::string private_key;
//
//    std::string make_signature(const std::string& data)
//    {
//        std::string to_sign = private_key + data + private_key;
//
//        unsigned char hash[SHA_DIGEST_LENGTH];
//        SHA1(reinterpret_cast<const unsigned char*>(to_sign.c_str()), to_sign.size(), hash);
//
//        return base64_encode(std::string(reinterpret_cast<char*>(hash), SHA_DIGEST_LENGTH));
//    }
//
//    static std::string base64_encode(const std::string& input)
//    {
//        static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
//        std::string result;
//        int val = 0, valb = -6;
//
//        for (unsigned char c : input)
//        {
//            val = (val << 8) + c;
//            valb += 8;
//            while (valb >= 0)
//            {
//                result.push_back(chars[(val >> valb) & 0x3F]);
//                valb -= 6;
//            }
//        }
//
//        if (valb > -6)
//            result.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
//
//        while (result.size() % 4)
//            result.push_back('=');
//
//        return result;
//    }
//
//    static std::string base64_decode(const std::string& input)
//    {
//        static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
//        std::vector<int> T(256, -1);
//        for (size_t i = 0; i < chars.size(); i++) T[chars[i]] = (int)i;
//
//        std::string result;
//        int val = 0, valb = -8;
//
//        for (unsigned char c : input)
//        {
//            if (c == '=') break;
//            if (T[c] == -1) continue;
//            val = (val << 6) + T[c];
//            valb += 6;
//            if (valb >= 0)
//            {
//                result.push_back(char((val >> valb) & 0xFF));
//                valb -= 8;
//            }
//        }
//
//        return result;
//    }
//};

#pragma once
#include <string>
#include <vector>
#include <openssl/sha.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class LiqPayHelper
{
public:
    LiqPayHelper(const std::string& public_key, const std::string& private_key)
        : public_key(public_key), private_key(private_key) {}

    struct PaymentForm
    {
        std::string data;
        std::string signature;
    };

    PaymentForm create_payment(const std::string& order_ref, double amount, const std::string& description,
        const std::string& result_url, const std::string& server_url)
    {
        json payload = {
            {"version", 3},
            {"public_key", public_key},
            {"action", "pay"},
            {"amount", amount},
            {"currency", "UAH"},
            {"description", description},
            {"order_id", order_ref},
            {"result_url", result_url},
            {"server_url", server_url}
        };

        std::string json_str = payload.dump();
        std::string data_b64 = base64_encode(json_str);
        std::string signature = make_signature(data_b64);

        return { data_b64, signature };
    }

    bool verify_signature(const std::string& data, const std::string& received_signature)
    {
        std::string expected = make_signature(data);
        return expected == received_signature;
    }

    json decode_data(const std::string& data_b64)
    {
        std::string decoded = base64_decode(data_b64);
        return json::parse(decoded);
    }

private:
    std::string public_key;
    std::string private_key;

    std::string make_signature(const std::string& data)
    {
        std::string to_sign = private_key + data + private_key;

        unsigned char hash[SHA_DIGEST_LENGTH];
        SHA1(reinterpret_cast<const unsigned char*>(to_sign.c_str()), to_sign.size(), hash);

        return base64_encode(std::string(reinterpret_cast<char*>(hash), SHA_DIGEST_LENGTH));
    }

    static std::string base64_encode(const std::string& input)
    {
        static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string result;
        int val = 0, valb = -6;

        for (unsigned char c : input)
        {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0)
            {
                result.push_back(chars[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }

        if (valb > -6)
            result.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);

        while (result.size() % 4)
            result.push_back('=');

        return result;
    }

    static std::string base64_decode(const std::string& input)
    {
        static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::vector<int> T(256, -1);
        for (size_t i = 0; i < chars.size(); i++) T[chars[i]] = (int)i;

        std::string result;
        int val = 0, valb = -8;

        for (unsigned char c : input)
        {
            if (c == '=') break;
            if (T[c] == -1) continue;
            val = (val << 6) + T[c];
            valb += 6;
            if (valb >= 0)
            {
                result.push_back(char((val >> valb) & 0xFF));
                valb -= 8;
            }
        }

        return result;
    }
};