//#pragma once
//#include <string>
//#include <unordered_map>
//#include <mutex>
//#include <openssl/evp.h>
//#include <openssl/rand.h>
//#include <random>
//#include <sstream>
//#include <optional>
//#include <iomanip>
//
//
//struct AuthSession
//{
//    int user_id;
//    std::string username;
//    std::string role;
//};
//
//class SessionStore
//{
//public:
//    static SessionStore& instance()
//    {
//        static SessionStore store;
//        return store;
//    }
//
//    // Вызвать один раз при старте, до приёма запросов
//    void configure(std::chrono::minutes idle, std::chrono::hours absolute)
//    {
//        std::lock_guard<std::mutex> lock(mtx);
//        idle_ttl = idle;
//        absolute_ttl = absolute;
//    }
//
//    long long idle_seconds() const
//    {
//        std::lock_guard<std::mutex> lock(mtx);
//        return std::chrono::duration_cast<std::chrono::seconds>(idle_ttl).count();
//    }
//
//    std::string create_token(int user_id, const std::string& username, const std::string& role)
//    {
//        std::string token = generate_token();
//        const auto now = Clock::now();
//
//        std::lock_guard<std::mutex> lock(mtx);
//        purge_if_due_locked(now);
//        sessions[token] = Entry{ { user_id, username, role }, now, now };
//
//        return token;
//    }
//
//    // Проверяет срок и продлевает "простой" (скользящее окно)
//    std::optional<AuthSession> find(const std::string& token)
//    {
//        const auto now = Clock::now();
//        std::lock_guard<std::mutex> lock(mtx);
//
//        auto it = sessions.find(token);
//        if (it == sessions.end())
//            return std::nullopt;
//
//        if (is_expired(it->second, now))
//        {
//            sessions.erase(it);
//            return std::nullopt;
//        }
//
//        it->second.last_used = now;
//        return it->second.session;
//    }
//
//    bool remove(const std::string& token)
//    {
//        std::lock_guard<std::mutex> lock(mtx);
//        return sessions.erase(token) > 0;
//    }
//
//    // Выход со всех устройств. Вызывайте и при смене пароля или роли пользователя
//    size_t remove_all_for_user(int user_id)
//    {
//        std::lock_guard<std::mutex> lock(mtx);
//        size_t removed = 0;
//
//        for (auto it = sessions.begin(); it != sessions.end();)
//        {
//            if (it->second.session.user_id == user_id)
//            {
//                it = sessions.erase(it);
//                ++removed;
//            }
//            else
//            {
//                ++it;
//            }
//        }
//
//        return removed;
//    }
//
//private:
//    using Clock = std::chrono::steady_clock;
//
//    struct Entry
//    {
//        AuthSession session;
//        Clock::time_point created;
//        Clock::time_point last_used;
//    };
//
//    mutable std::mutex mtx;
//    std::unordered_map<std::string, Entry> sessions;
//    std::chrono::minutes idle_ttl{ 30 };
//    std::chrono::hours absolute_ttl{ 12 };
//    Clock::time_point last_purge = Clock::now();
//
//    bool is_expired(const Entry& e, Clock::time_point now) const
//    {
//        return (now - e.last_used > idle_ttl) || (now - e.created > absolute_ttl);
//    }
//
//    // Чистит просроченные записи не чаще раза в 5 минут, отдельный поток не нужен
//    void purge_if_due_locked(Clock::time_point now)
//    {
//        if (now - last_purge < std::chrono::minutes(5))
//            return;
//
//        last_purge = now;
//
//        for (auto it = sessions.begin(); it != sessions.end();)
//        {
//            if (is_expired(it->second, now)) it = sessions.erase(it);
//            else ++it;
//        }
//    }
//
//    static std::string generate_token()
//    {
//        unsigned char bytes[32];
//
//        if (RAND_bytes(bytes, sizeof(bytes)) != 1)
//            throw std::runtime_error("RAND_bytes failed");
//
//        static const char* hex = "0123456789abcdef";
//        std::string token;
//        token.reserve(64);
//
//        for (unsigned char b : bytes)
//        {
//            token.push_back(hex[b >> 4]);
//            token.push_back(hex[b & 0x0F]);
//        }
//
//        return token;
//    }
//};