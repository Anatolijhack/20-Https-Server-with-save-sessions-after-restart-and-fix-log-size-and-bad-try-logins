#pragma once
#include "OrderRepository.h"
#include "Logger.h"
#include <atomic>
#include <chrono>
#include <thread>

class OrderCleaner
{
public:
    OrderCleaner(OrderRepository& repo, int ttl_minutes)
        : repo(repo), ttl_minutes(ttl_minutes), worker([this] { run(); }) {}

    ~OrderCleaner()
    {
        stop = true;
        if (worker.joinable()) worker.join();
    }

private:
    void run()
    {
        while (!stop)
        {
            try
            {
                int n = repo.cancel_expired_orders(ttl_minutes);
                if (n > 0) LOG_INFO("Auto-cancelled unpaid orders: " + std::to_string(n));
            }
            catch (const std::exception& e)
            {
                LOG_ERROR(std::string("Order cleaner failed: ") + e.what());
            }

            for (int i = 0; i < 60 && !stop; i++)
                std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    OrderRepository& repo;
    int ttl_minutes;
    std::atomic<bool> stop{ false };
    std::thread worker;   // объявлен последним: поток стартует, когда остальные поля готовы
};