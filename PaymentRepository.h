#pragma once
#include "ConnectionPool.h"
#include <string>
#include <optional>

struct PaymentRecord
{
    int order_id;
    std::string payment_ref;
    double amount;
    std::string status;
};

class PaymentRepository
{
public:
    PaymentRepository(ConnectionPool& pool) : pool(pool) {}

    std::string create_payment(int order_id, const std::string& payment_ref, double amount);
    bool update_payment_status(const std::string& payment_ref, const std::string& status);
    std::optional<PaymentRecord> find_by_ref(const std::string& payment_ref);

private:
    ConnectionPool& pool;
};