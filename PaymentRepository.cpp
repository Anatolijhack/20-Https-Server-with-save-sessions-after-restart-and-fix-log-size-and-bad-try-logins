#include "PaymentRepository.h"

std::string PaymentRepository::create_payment(int order_id, const std::string& payment_ref, double amount)
{
    auto leased = pool.acquire();
    SQLHDBC dbc = leased.handle();

    SQLHSTMT stmt = nullptr;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    const char* query = "INSERT INTO Payments (order_id, payment_ref, amount, status) VALUES (?, ?, ?, 'Pending')";
    SQLPrepareA(stmt, (SQLCHAR*)query, SQL_NTS);

    SQLLEN oid_ind = 0;
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &order_id, 0, &oid_ind);

    SQLLEN ref_len = SQL_NTS;
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
        payment_ref.size(), 0, (SQLPOINTER)payment_ref.c_str(), 0, &ref_len);

    SQLLEN amount_ind = 0;
    SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_DOUBLE, SQL_DECIMAL, 10, 2, &amount, 0, &amount_ind);

    SQLRETURN result = SQLExecute(stmt);

    if (!SQL_SUCCEEDED(result))
    {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return R"({"error":"Failed to create payment"})";
    }

    SQLEndTran(SQL_HANDLE_DBC, dbc, SQL_COMMIT);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    return R"({"status":"payment created"})";
}

bool PaymentRepository::update_payment_status(const std::string& payment_ref, const std::string& status)
{
    auto leased = pool.acquire();
    SQLHDBC dbc = leased.handle();

    SQLHSTMT stmt = nullptr;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    const char* query = "UPDATE Payments SET status = ? WHERE payment_ref = ?";
    SQLPrepareA(stmt, (SQLCHAR*)query, SQL_NTS);

    SQLLEN status_len = SQL_NTS;
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
        status.size(), 0, (SQLPOINTER)status.c_str(), 0, &status_len);

    SQLLEN ref_len = SQL_NTS;
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
        payment_ref.size(), 0, (SQLPOINTER)payment_ref.c_str(), 0, &ref_len);

    SQLRETURN result = SQLExecute(stmt);
    bool success = SQL_SUCCEEDED(result);

    if (success)
    {
        SQLEndTran(SQL_HANDLE_DBC, dbc, SQL_COMMIT);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return success;
}

std::optional<PaymentRecord> PaymentRepository::find_by_ref(const std::string& payment_ref)
{
    auto leased = pool.acquire();
    SQLHDBC dbc = leased.handle();

    SQLHSTMT stmt = nullptr;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    const char* query = "SELECT order_id, payment_ref, amount, status FROM Payments WHERE payment_ref = ?";
    SQLPrepareA(stmt, (SQLCHAR*)query, SQL_NTS);

    SQLLEN ref_len = SQL_NTS;
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
        payment_ref.size(), 0, (SQLPOINTER)payment_ref.c_str(), 0, &ref_len);

    SQLExecute(stmt);

    SQLINTEGER order_id = 0;
    SQLCHAR ref[64] = {}, status[32] = {};
    SQLDOUBLE amount = 0.0;
    SQLLEN oid_ind, ref_ind, amount_ind, status_ind;

    SQLBindCol(stmt, 1, SQL_C_LONG, &order_id, 0, &oid_ind);
    SQLBindCol(stmt, 2, SQL_C_CHAR, ref, sizeof(ref), &ref_ind);
    SQLBindCol(stmt, 3, SQL_C_DOUBLE, &amount, 0, &amount_ind);
    SQLBindCol(stmt, 4, SQL_C_CHAR, status, sizeof(status), &status_ind);

    std::optional<PaymentRecord> result;

    if (SQLFetch(stmt) == SQL_SUCCESS)
    {
        result = PaymentRecord{ order_id, std::string((char*)ref), amount, std::string((char*)status) };
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return result;
}