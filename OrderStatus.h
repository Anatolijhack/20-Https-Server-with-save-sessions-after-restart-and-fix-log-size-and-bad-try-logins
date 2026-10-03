#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

namespace order_status
{
    inline const std::unordered_map<std::string, std::vector<std::string>>& allowed_transitions()
    {
        static const std::unordered_map<std::string, std::vector<std::string>> transitions = {
            { "New",        { "Processing", "Cancelled" } },
            { "Processing", { "Shipped", "Cancelled" } },
            { "Shipped",    { "Completed" } },
            { "Completed",  {} },   // конечный статус, дальше переходов нет
            { "Cancelled",  {} }    // конечный статус, дальше переходов нет
        };

        return transitions;
    }

    inline bool is_valid_status(const std::string& status)
    {
        return allowed_transitions().count(status) > 0;
    }

    inline bool can_transition(const std::string& from, const std::string& to)
    {
        auto it = allowed_transitions().find(from);

        if (it == allowed_transitions().end())
        {
            return false;
        }

        return std::find(it->second.begin(), it->second.end(), to) != it->second.end();
    }
}