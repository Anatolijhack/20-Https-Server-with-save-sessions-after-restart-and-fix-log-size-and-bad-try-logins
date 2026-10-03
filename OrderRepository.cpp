#include "OrderRepository.h"
#include "OrderStatus.h"
#include <iostream>
#include "Logger.h"
#include "Transaction.h"

static void print_odbc_error(SQLSMALLINT handle_type, SQLHANDLE handle)
{
    SQLCHAR sqlstate[6];
    SQLCHAR message[SQL_MAX_MESSAGE_LENGTH];
    SQLINTEGER native_error;
    SQLSMALLINT message_len;
    SQLSMALLINT i = 1;

    while (SQLGetDiagRecA(handle_type, handle, i, sqlstate, &native_error,
        message, sizeof(message), &message_len) == SQL_SUCCESS)
    {
        std::cerr << "ODBC Error [" << sqlstate << "] " << message << std::endl;
        i++;
    }
}

// Получить актуальную цену товара и остаток напрямую из БД (никогда не доверяем цене от клиента!)
struct ProductPriceInfo
{
    double price;
    std::string name;
    int stock;
};

static std::optional<ProductPriceInfo> get_product_price(SQLHDBC dbc, int product_id)
{
    SQLHSTMT stmt = nullptr;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    const char* query = "SELECT product_name, price, stock_quantity FROM Products WHERE id = ?";
    SQLPrepareA(stmt, (SQLCHAR*)query, SQL_NTS);

    SQLLEN id_ind = 0;
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &product_id, 0, &id_ind);

    SQLExecute(stmt);

    SQLCHAR name[128] = {};
    SQLDOUBLE price = 0.0;
    SQLINTEGER stock = 0;
    SQLLEN name_ind = 0, price_ind = 0, stock_ind = 0;

    SQLBindCol(stmt, 1, SQL_C_CHAR, name, sizeof(name), &name_ind);
    SQLBindCol(stmt, 2, SQL_C_DOUBLE, &price, 0, &price_ind);
    SQLBindCol(stmt, 3, SQL_C_LONG, &stock, 0, &stock_ind);

    std::optional<ProductPriceInfo> result;

    if (SQLFetch(stmt) == SQL_SUCCESS)
    {
        result = ProductPriceInfo{ price, std::string((char*)name), (int)stock };
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return result;
}

std::string OrderRepository::create_order(int user_id, const std::vector<OrderItemInput>& items, const DeliveryInfo& delivery)
{
    if (items.empty())
    {
        return R"({"error":"Order must contain at least one item"})";
    }

    auto leased = pool.acquire();
    SQLHDBC dbc = leased.handle();

    // 1. ????????? ??????? ??????? ? ???????? ?? ???????? ??????
    struct ResolvedItem { int product_id; int quantity; double price; };
    std::vector<ResolvedItem> resolved;

    for (const auto& item : items)
    {
        if (item.quantity <= 0)
        {
            return R"({"error":"Quantity must be positive"})";
        }

        auto info = get_product_price(dbc, item.product_id);

        if (!info)
        {
            json err = { {"error", "Product not found"}, {"product_id", item.product_id} };
            return err.dump();
        }

        if (info->stock < item.quantity)
        {
            json err = {
                {"error", "Insufficient stock"},
                {"product_id", item.product_id},
                {"available", info->stock}
            };
            return err.dump();
        }

        resolved.push_back({ item.product_id, item.quantity, info->price });
    }

    Transaction tx(dbc);

    // 2. ??????? ?????? ? Orders
    SQLHSTMT stmt = nullptr;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    const char* insert_order =
        "INSERT INTO Orders (user_id, status, recipient_name, phone, city, address, comment) "
        "OUTPUT INSERTED.order_id "
        "VALUES (?, 'New', ?, ?, ?, ?, ?)";
    SQLPrepareA(stmt, (SQLCHAR*)insert_order, SQL_NTS);

    SQLLEN user_ind = 0;
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &user_id, 0, &user_ind);

    SQLLEN name_ind = SQL_NTS, phone_ind = SQL_NTS, city_ind = SQL_NTS, addr_ind = SQL_NTS, comment_ind = SQL_NTS;
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, delivery.recipient_name.size(), 0, (SQLPOINTER)delivery.recipient_name.c_str(), 0, &name_ind);
    SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, delivery.phone.size(), 0, (SQLPOINTER)delivery.phone.c_str(), 0, &phone_ind);
    SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, delivery.city.size(), 0, (SQLPOINTER)delivery.city.c_str(), 0, &city_ind);
    SQLBindParameter(stmt, 5, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, delivery.address.size(), 0, (SQLPOINTER)delivery.address.c_str(), 0, &addr_ind);
    SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, delivery.comment.size(), 0, (SQLPOINTER)delivery.comment.c_str(), 0, &comment_ind);

    SQLRETURN result = SQLExecute(stmt);

    if (!SQL_SUCCEEDED(result))
    {
        print_odbc_error(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return R"({"error":"Failed to create order"})";
    }

    SQLINTEGER order_id = 0;
    SQLLEN order_id_ind = 0;

    SQLBindCol(
        stmt,
        1,
        SQL_C_LONG,
        &order_id,
        sizeof(order_id),
        &order_id_ind
    );

    SQLRETURN fetch_result = SQLFetch(stmt);

    if (!SQL_SUCCEEDED(fetch_result))
    {
        print_odbc_error(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return R"({"error":"Failed to get order_id"})";
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    // 3. ???????? ??????????????? order_id
    //SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    //SQLExecDirectA(stmt, (SQLCHAR*)"SELECT SCOPE_IDENTITY()", SQL_NTS);

    //SQLDOUBLE order_id_raw = 0.0;
    //SQLLEN oid_ind = 0;
    //SQLBindCol(stmt, 1, SQL_C_DOUBLE, &order_id_raw, 0, &oid_ind);
    //SQLFetch(stmt);
    //SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    //int order_id = static_cast<int>(order_id_raw);

    // 4. ????????? ??????? ?????? ? ????????? ???????
    double total = 0.0;

    for (const auto& item : resolved)
    {
        SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
        const char* insert_item = "INSERT INTO OrderItems (order_id, product_id, quantity, price) VALUES (?, ?, ?, ?)";
        SQLPrepareA(stmt, (SQLCHAR*)insert_item, SQL_NTS);

        SQLLEN oid_ind2 = 0, pid_ind = 0, qty_ind = 0, price_ind = 0;

        SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &order_id, 0, &oid_ind2);
        SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, (void*)&item.product_id, 0, &pid_ind);
        SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, (void*)&item.quantity, 0, &qty_ind);
        SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_DOUBLE, SQL_DECIMAL, 10, 2, (void*)&item.price, 0, &price_ind);

        SQLRETURN item_result = SQLExecute(stmt);

        if (!SQL_SUCCEEDED(item_result))
        {
            print_odbc_error(SQL_HANDLE_STMT, stmt);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);

            return R"({"error":"Failed to add order item"})";
        }

        SQLFreeHandle(SQL_HANDLE_STMT, stmt);

        // ????????? ???????
        SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
        const char* update_stock =
            "UPDATE Products SET stock_quantity = stock_quantity - ? "
            "WHERE id = ? AND stock_quantity >= ?";
        SQLPrepareA(stmt, (SQLCHAR*)update_stock, SQL_NTS);

        SQLLEN q1_ind = 0, p_ind = 0, q2_ind = 0;
        SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, (void*)&item.quantity, 0, &q1_ind);
        SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, (void*)&item.product_id, 0, &p_ind);
        SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, (void*)&item.quantity, 0, &q2_ind);

        SQLRETURN stock_result = SQLExecute(stmt);
        SQLLEN stock_rows = 0;
        if (SQL_SUCCEEDED(stock_result)) SQLRowCount(stmt, &stock_rows);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);

        if (stock_rows == 0)
        {
            json err = { {"error", "Insufficient stock"}, {"product_id", item.product_id} };
            return err.dump();   // заказ и позиции откатятся
        }

        total += item.price * item.quantity;
    }

    if (!tx.commit()) return R"({"error":"Failed to commit order"})";

    json response = {
        {"order_id", order_id},
        {"status", "New"},
        {"total", total}
    };

    return response.dump();
}

std::string OrderRepository::get_margin_report(const std::string& date_from, const std::string& date_to)
{
    auto leased = pool.acquire();
    SQLHDBC dbc = leased.handle();

    SQLHSTMT stmt = nullptr;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    // Cancelled исключаем — по отменённым заказам денег не было
    const char* query =
        "SELECT "
        "  p.id, p.product_name, "
        "  SUM(oi.quantity) AS units_sold, "
        "  SUM(oi.quantity * oi.price) AS revenue, "
        "  SUM(oi.quantity * p.cost) AS cost_total, "
        "  SUM(oi.quantity * (oi.price - p.cost)) AS margin "
        "FROM OrderItems oi "
        "JOIN Orders o ON o.order_id = oi.order_id "
        "JOIN Products p ON p.id = oi.product_id "
        "WHERE o.status != 'Cancelled' "
        "  AND o.created_at >= ? AND o.created_at < ? "
        "GROUP BY p.id, p.product_name "
        "ORDER BY margin DESC";

    SQLPrepareA(stmt, (SQLCHAR*)query, SQL_NTS);

    SQLLEN from_ind = SQL_NTS, to_ind = SQL_NTS;
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
        date_from.size(), 0, (SQLPOINTER)date_from.c_str(), 0, &from_ind);
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
        date_to.size(), 0, (SQLPOINTER)date_to.c_str(), 0, &to_ind);

    SQLExecute(stmt);

    SQLINTEGER product_id = 0, units_sold = 0;
    SQLCHAR product_name[128] = {};
    SQLDOUBLE revenue = 0.0, cost_total = 0.0, margin = 0.0;
    SQLLEN pid_ind, name_ind, units_ind, rev_ind, cost_ind, margin_ind;

    SQLBindCol(stmt, 1, SQL_C_LONG, &product_id, 0, &pid_ind);
    SQLBindCol(stmt, 2, SQL_C_CHAR, product_name, sizeof(product_name), &name_ind);
    SQLBindCol(stmt, 3, SQL_C_LONG, &units_sold, 0, &units_ind);
    SQLBindCol(stmt, 4, SQL_C_DOUBLE, &revenue, 0, &rev_ind);
    SQLBindCol(stmt, 5, SQL_C_DOUBLE, &cost_total, 0, &cost_ind);
    SQLBindCol(stmt, 6, SQL_C_DOUBLE, &margin, 0, &margin_ind);

    json rows = json::array();
    double total_revenue = 0.0, total_cost = 0.0, total_margin = 0.0;

    while (SQLFetch(stmt) == SQL_SUCCESS)
    {
        json item;
        item["product_id"] = product_id;
        item["product_name"] = std::string((char*)product_name);
        item["units_sold"] = units_sold;
        item["revenue"] = revenue;
        item["cost"] = cost_total;
        item["margin"] = margin;
        rows.push_back(item);

        total_revenue += revenue;
        total_cost += cost_total;
        total_margin += margin;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    json response;
    response["from"] = date_from;
    response["to"] = date_to;
    response["products"] = rows;
    response["totals"] = {
        {"revenue", total_revenue},
        {"cost", total_cost},
        {"margin", total_margin}
    };

    return response.dump();
}
std::string OrderRepository::get_order(int order_id)
{
    auto leased = pool.acquire();
    SQLHDBC dbc = leased.handle();

    SQLHSTMT stmt = nullptr;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    const char* query =
        "SELECT order_id, user_id, status, created_at, "
        "recipient_name, phone, city, address, comment "
        "FROM Orders WHERE order_id = ?";
    SQLPrepareA(stmt, (SQLCHAR*)query, SQL_NTS);

    SQLLEN id_ind = 0;
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &order_id, 0, &id_ind);

    SQLExecute(stmt);

    SQLINTEGER out_order_id = 0, out_user_id = 0;
    SQLCHAR status[32] = {};
    SQL_TIMESTAMP_STRUCT created_at{};

    SQLCHAR recipient_name[128] = {};
    SQLCHAR phone[32] = {};
    SQLCHAR city[128] = {};
    SQLCHAR address[256] = {};
    SQLCHAR comment[512] = {};

    SQLLEN oid_ind = 0, uid_ind = 0, status_ind = 0, created_ind = 0;
    SQLLEN name_ind = 0, phone_ind = 0, city_ind = 0, addr_ind = 0, comment_ind = 0;

    SQLBindCol(stmt, 1, SQL_C_LONG, &out_order_id, 0, &oid_ind);
    SQLBindCol(stmt, 2, SQL_C_LONG, &out_user_id, 0, &uid_ind);
    SQLBindCol(stmt, 3, SQL_C_CHAR, status, sizeof(status), &status_ind);
    SQLBindCol(stmt, 4, SQL_C_TYPE_TIMESTAMP, &created_at, 0, &created_ind);
    SQLBindCol(stmt, 5, SQL_C_CHAR, recipient_name, sizeof(recipient_name), &name_ind);
    SQLBindCol(stmt, 6, SQL_C_CHAR, phone, sizeof(phone), &phone_ind);
    SQLBindCol(stmt, 7, SQL_C_CHAR, city, sizeof(city), &city_ind);
    SQLBindCol(stmt, 8, SQL_C_CHAR, address, sizeof(address), &addr_ind);
    SQLBindCol(stmt, 9, SQL_C_CHAR, comment, sizeof(comment), &comment_ind);

    if (SQLFetch(stmt) != SQL_SUCCESS)
    {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return R"({"error":"Order not found"})";
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    // Получаем позиции заказа с названиями товаров
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    const char* items_query =
        "SELECT oi.product_id, p.product_name, oi.quantity, oi.price "
        "FROM OrderItems oi "
        "JOIN Products p ON oi.product_id = p.id "
        "WHERE oi.order_id = ?";

    SQLPrepareA(stmt, (SQLCHAR*)items_query, SQL_NTS);
    SQLLEN oid_param_ind = 0;
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &order_id, 0, &oid_param_ind);
    SQLExecute(stmt);

    SQLINTEGER product_id = 0, quantity = 0;
    SQLCHAR product_name[128] = {};
    SQLDOUBLE price = 0.0;
    SQLLEN pid_ind = 0, pname_ind = 0, qty_ind = 0, price_ind = 0;

    SQLBindCol(stmt, 1, SQL_C_LONG, &product_id, 0, &pid_ind);
    SQLBindCol(stmt, 2, SQL_C_CHAR, product_name, sizeof(product_name), &pname_ind);
    SQLBindCol(stmt, 3, SQL_C_LONG, &quantity, 0, &qty_ind);
    SQLBindCol(stmt, 4, SQL_C_DOUBLE, &price, 0, &price_ind);

    json items = json::array();
    double total = 0.0;

    while (SQLFetch(stmt) == SQL_SUCCESS)
    {
        json item;
        item["product_id"] = product_id;
        item["product_name"] = std::string((char*)product_name);
        item["quantity"] = quantity;
        item["price"] = price;
        items.push_back(item);
        total += price * quantity;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    json response;
    response["order_id"] = out_order_id;
    response["user_id"] = out_user_id;
    response["status"] = std::string((char*)status);
    response["items"] = items;
    response["total"] = total;

    response["delivery"] = {
        {"recipient_name", std::string((char*)recipient_name)},
        {"phone", std::string((char*)phone)},
        {"city", std::string((char*)city)},
        {"address", std::string((char*)address)},
        {"comment", std::string((char*)comment)}
    };

    return response.dump();
}

std::string OrderRepository::get_orders_by_user(int user_id)
{
    auto leased = pool.acquire();
    SQLHDBC dbc = leased.handle();

    SQLHSTMT stmt = nullptr;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    const char* query = "SELECT order_id, status, created_at FROM Orders WHERE user_id = ? ORDER BY created_at DESC";
    SQLPrepareA(stmt, (SQLCHAR*)query, SQL_NTS);

    SQLLEN uid_ind = 0;
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &user_id, 0, &uid_ind);
    SQLExecute(stmt);

    SQLINTEGER order_id = 0;
    SQLCHAR status[32] = {};
    SQL_TIMESTAMP_STRUCT created_at{};
    SQLLEN oid_ind = 0, status_ind = 0, created_ind = 0;

    SQLBindCol(stmt, 1, SQL_C_LONG, &order_id, 0, &oid_ind);
    SQLBindCol(stmt, 2, SQL_C_CHAR, status, sizeof(status), &status_ind);
    SQLBindCol(stmt, 3, SQL_C_TYPE_TIMESTAMP, &created_at, 0, &created_ind);

    json orders = json::array();

    while (SQLFetch(stmt) == SQL_SUCCESS)
    {
        json item;
        item["order_id"] = order_id;
        item["status"] = std::string((char*)status);
        orders.push_back(item);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return orders.dump();
}

//std::string OrderRepository::update_order_status(int order_id, const std::string& status)
//{
//    auto leased = pool.acquire();
//    SQLHDBC dbc = leased.handle();
//
//    Transaction tx(dbc);
//
//    SQLHSTMT stmt = nullptr;
//    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
//
//    const char* query = "UPDATE Orders SET status = ? WHERE order_id = ?";
//    SQLPrepareA(stmt, (SQLCHAR*)query, SQL_NTS);
//
//    SQLLEN status_len = SQL_NTS;
//    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
//        status.size(), 0, (SQLPOINTER)status.c_str(), 0, &status_len);
//
//    SQLLEN id_ind = 0;
//    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &order_id, 0, &id_ind);
//
//    SQLRETURN result = SQLExecute(stmt);
//
//    // SQL_NO_DATA — это не ошибка, а "ни одна строка не затронута"
//    if (!SQL_SUCCEEDED(result) && result != SQL_NO_DATA)
//    {
//        print_odbc_error(SQL_HANDLE_STMT, stmt);
//        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
//        return R"({"error":"Update failed"})";
//    }
//
//    SQLLEN rows = 0;
//    if (result != SQL_NO_DATA)
//    {
//        SQLRowCount(stmt, &rows);
//    }
//
//    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
//
//    if (rows == 0)
//    {
//        return R"({"error":"Order not found"})";
//    }
//
//    if (!tx.commit())
//    {
//        return R"({"error":"Update failed"})";
//    }
//
//    return R"({"status":"order updated"})";
//}
std::string OrderRepository::update_order_status(int order_id, const std::string& new_status)
{
    auto leased = pool.acquire();
    SQLHDBC dbc = leased.handle();

    Transaction tx(dbc);

    // Сначала читаем текущий статус
    SQLHSTMT check_stmt = nullptr;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &check_stmt);

    const char* check_query = "SELECT status FROM Orders WHERE order_id = ?";
    SQLPrepareA(check_stmt, (SQLCHAR*)check_query, SQL_NTS);

    SQLLEN id_ind = 0;
    SQLBindParameter(check_stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &order_id, 0, &id_ind);

    SQLRETURN check_result = SQLExecute(check_stmt);

    if (!SQL_SUCCEEDED(check_result) && check_result != SQL_NO_DATA)
    {
        print_odbc_error(SQL_HANDLE_STMT, check_stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, check_stmt);
        return R"({"error":"Update failed"})";
    }

    SQLCHAR current_status[32] = {};
    SQLLEN status_ind = 0;
    SQLBindCol(check_stmt, 1, SQL_C_CHAR, current_status, sizeof(current_status), &status_ind);

    bool found = SQL_SUCCEEDED(SQLFetch(check_stmt));
    SQLFreeHandle(SQL_HANDLE_STMT, check_stmt);

    if (!found)
    {
        return R"({"error":"Order not found"})";
    }

    std::string from_status((char*)current_status);

    // Проверяем, разрешён ли такой переход
    if (!order_status::can_transition(from_status, new_status))
    {
        json err = {
            {"error", "Invalid status transition"},
            {"from", from_status},
            {"to", new_status}
        };
        return err.dump();
    }

    // Переход разрешён — выполняем UPDATE
    SQLHSTMT stmt = nullptr;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    const char* query = "UPDATE Orders SET status = ? WHERE order_id = ?";
    SQLPrepareA(stmt, (SQLCHAR*)query, SQL_NTS);

    SQLLEN status_len = SQL_NTS;
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
        new_status.size(), 0, (SQLPOINTER)new_status.c_str(), 0, &status_len);

    SQLLEN id_ind2 = 0;
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &order_id, 0, &id_ind2);

    SQLRETURN result = SQLExecute(stmt);

    if (!SQL_SUCCEEDED(result) && result != SQL_NO_DATA)
    {
        print_odbc_error(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return R"({"error":"Update failed"})";
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    if (!tx.commit())
    {
        return R"({"error":"Update failed"})";
    }

    return R"({"status":"order updated"})";
}

    std::string OrderRepository::get_orders_by_status(const std::string & status)
    {
        auto leased = pool.acquire();
        SQLHDBC dbc = leased.handle();

        SQLHSTMT stmt = nullptr;
        SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

        const char* query = "SELECT order_id, user_id, status, created_at FROM Orders WHERE status = ? ORDER BY created_at DESC";
        SQLPrepareA(stmt, (SQLCHAR*)query, SQL_NTS);

        SQLLEN status_len = SQL_NTS;
        SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
            status.size(), 0, (SQLPOINTER)status.c_str(), 0, &status_len);

        SQLExecute(stmt);

        json orders = json::array();

        SQLINTEGER order_id = 0, user_id = 0;
        SQLCHAR out_status[32] = {};
        SQLLEN oid_ind, uid_ind, status_ind;

        SQLBindCol(stmt, 1, SQL_C_LONG, &order_id, 0, &oid_ind);
        SQLBindCol(stmt, 2, SQL_C_LONG, &user_id, 0, &uid_ind);
        SQLBindCol(stmt, 3, SQL_C_CHAR, out_status, sizeof(out_status), &status_ind);

        while (SQLFetch(stmt) == SQL_SUCCESS)
        {
            json item;
            item["order_id"] = order_id;
            item["user_id"] = user_id;
            item["status"] = std::string((char*)out_status);
            orders.push_back(item);
        }

        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return orders.dump();
    }

    std::string OrderRepository::get_all_orders()
    {
        auto leased = pool.acquire();
        SQLHDBC dbc = leased.handle();

        SQLHSTMT stmt = nullptr;
        SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

        const char* query =
            "SELECT order_id, user_id, status, created_at "
            "FROM Orders "
            "ORDER BY created_at DESC";

        SQLExecDirectA(stmt, (SQLCHAR*)query, SQL_NTS);

        json orders = json::array();

        SQLINTEGER order_id = 0, user_id = 0;
        SQLCHAR status[32] = {};
        SQL_TIMESTAMP_STRUCT created_at{};
        SQLLEN oid_ind = 0, uid_ind = 0, status_ind = 0, created_ind = 0;

        SQLBindCol(stmt, 1, SQL_C_LONG, &order_id, 0, &oid_ind);
        SQLBindCol(stmt, 2, SQL_C_LONG, &user_id, 0, &uid_ind);
        SQLBindCol(stmt, 3, SQL_C_CHAR, status, sizeof(status), &status_ind);
        SQLBindCol(stmt, 4, SQL_C_TYPE_TIMESTAMP, &created_at, 0, &created_ind);

        while (SQLFetch(stmt) == SQL_SUCCESS)
        {
            json item;
            item["order_id"] = order_id;
            item["user_id"] = user_id;
            item["status"] = std::string((char*)status);
            orders.push_back(item);
        }

        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return orders.dump();
    }

    std::optional<ResolvedOrderItem> OrderRepository::resolve_item(int product_id, int quantity)
    {
        auto leased = pool.acquire();
        SQLHDBC dbc = leased.handle();

        SQLHSTMT stmt = nullptr;
        SQLRETURN result = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

        if (!SQL_SUCCEEDED(result))
        {
            return std::nullopt;
        }

        const char* query = "SELECT product_name, price, stock_quantity FROM Products WHERE id = ?";

        result = SQLPrepareA(stmt, (SQLCHAR*)query, SQL_NTS);

        if (!SQL_SUCCEEDED(result))
        {
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            return std::nullopt;
        }

        SQLLEN id_ind = 0;
        SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &product_id, 0, &id_ind);

        result = SQLExecute(stmt);

        if (!SQL_SUCCEEDED(result))
        {
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            return std::nullopt;
        }

        SQLCHAR name[128] = {};
        SQLDOUBLE price = 0.0;
        SQLINTEGER stock = 0;

        SQLLEN name_ind = 0, price_ind = 0, stock_ind = 0;

        SQLBindCol(stmt, 1, SQL_C_CHAR, name, sizeof(name), &name_ind);
        SQLBindCol(stmt, 2, SQL_C_DOUBLE, &price, 0, &price_ind);
        SQLBindCol(stmt, 3, SQL_C_LONG, &stock, 0, &stock_ind);

        std::optional<ResolvedOrderItem> resolved;

        if (SQLFetch(stmt) == SQL_SUCCESS)
        {
            resolved = ResolvedOrderItem{
                product_id,
                std::string((char*)name),
                quantity,
                price,
                static_cast<int>(stock)
            };
        }

        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return resolved;
    }

    static SQLLEN exec_by_int(SQLHDBC dbc, const char* sql, int value)
    {
        SQLHSTMT stmt = nullptr;
        SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
        SQLPrepareA(stmt, (SQLCHAR*)sql, SQL_NTS);

        SQLLEN ind = 0;
        SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &value, 0, &ind);

        SQLRETURN r = SQLExecute(stmt);
        SQLLEN rows = -1;

        if (r == SQL_NO_DATA) rows = 0;
        else if (SQL_SUCCEEDED(r)) SQLRowCount(stmt, &rows);
        else print_odbc_error(SQL_HANDLE_STMT, stmt);

        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return rows;
    }

    //std::string OrderRepository::cancel_order(int order_id, bool only_unpaid)
    //{
    //    auto leased = pool.acquire();
    //    SQLHDBC dbc = leased.handle();
    //    Transaction tx(dbc);

    //    // Статус меняется ровно один раз, поэтому и склад вернётся ровно один раз
    //    const char* set_cancelled = only_unpaid
    //        ? "UPDATE Orders SET status = 'Cancelled' WHERE order_id = ? AND status = 'New'"
    //        : "UPDATE Orders SET status = 'Cancelled' WHERE order_id = ? AND status IN ('New','Processing')";

    //    SQLLEN changed = exec_by_int(dbc, set_cancelled, order_id);
    //    if (changed < 0)  return R"({"error":"Cancel failed"})";
    //    if (changed == 0) return R"({"error":"Order not found or cannot be cancelled"})";

    //    if (exec_by_int(dbc,
    //        "UPDATE p SET p.stock_quantity = p.stock_quantity + oi.quantity "
    //        "FROM Products p JOIN OrderItems oi ON oi.product_id = p.id "
    //        "WHERE oi.order_id = ?", order_id) < 0)
    //        return R"({"error":"Cancel failed"})";

    //    if (exec_by_int(dbc,
    //        "UPDATE Payments SET status = 'Failed' WHERE order_id = ? AND status = 'Pending'", order_id) < 0)
    //        return R"({"error":"Cancel failed"})";

    //    if (!tx.commit()) return R"({"error":"Cancel failed"})";

    //    return R"({"status":"order cancelled"})";
    //}
    std::string OrderRepository::cancel_order(int order_id, bool only_unpaid)
    {
        auto leased = pool.acquire();
        SQLHDBC dbc = leased.handle();
        Transaction tx(dbc);

        // 1. Читаем текущий статус
        SQLHSTMT check_stmt = nullptr;
        SQLAllocHandle(SQL_HANDLE_STMT, dbc, &check_stmt);

        const char* check_query = "SELECT status FROM Orders WHERE order_id = ?";
        SQLPrepareA(check_stmt, (SQLCHAR*)check_query, SQL_NTS);

        SQLLEN id_ind = 0;
        SQLBindParameter(check_stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &order_id, 0, &id_ind);
        SQLExecute(check_stmt);

        SQLCHAR current_status[32] = {};
        SQLLEN status_ind = 0;
        SQLBindCol(check_stmt, 1, SQL_C_CHAR, current_status, sizeof(current_status), &status_ind);

        bool found = SQL_SUCCEEDED(SQLFetch(check_stmt));
        SQLFreeHandle(SQL_HANDLE_STMT, check_stmt);

        if (!found)
        {
            return R"({"error":"Order not found"})";
        }

        std::string from_status((char*)current_status);

        // 2. Проверяем переход через ту же таблицу правил, что и обычная смена статуса
        if (!order_status::can_transition(from_status, "Cancelled"))
        {
            json err = { {"error", "Order cannot be cancelled"}, {"current_status", from_status} };
            return err.dump();
        }

        // 3. only_unpaid — для автоотмены по таймеру: трогаем только совсем свежие заказы,
        //    чтобы не отменить то, что уже взяли в обработку
        if (only_unpaid && from_status != "New")
        {
            json err = { {"error", "Order already in progress, skip auto-cancel"}, {"current_status", from_status} };
            return err.dump();
        }

        // 4. Меняем статус — WHERE по order_id И status защищает от гонки:
        //    если статус кто-то успел поменять между шагом 1 и этим UPDATE, строка не обновится
        const char* set_cancelled = "UPDATE Orders SET status = 'Cancelled' WHERE order_id = ? AND status = ?";

        SQLHSTMT stmt = nullptr;
        SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
        SQLPrepareA(stmt, (SQLCHAR*)set_cancelled, SQL_NTS);

        SQLLEN oid_ind = 0, status_param_ind = SQL_NTS;
        SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &order_id, 0, &oid_ind);
        SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
            from_status.size(), 0, (SQLPOINTER)from_status.c_str(), 0, &status_param_ind);

        SQLRETURN result = SQLExecute(stmt);
        SQLLEN changed = 0;
        if (SQL_SUCCEEDED(result)) SQLRowCount(stmt, &changed);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);

        if (changed == 0)
        {
            // кто-то параллельно уже поменял статус — не наша транзакция выиграла гонку
            return R"({"error":"Order status changed concurrently, retry"})";
        }

        // 5. Возвращаем остаток на склад
        if (exec_by_int(dbc,
            "UPDATE p SET p.stock_quantity = p.stock_quantity + oi.quantity "
            "FROM Products p JOIN OrderItems oi ON oi.product_id = p.id "
            "WHERE oi.order_id = ?", order_id) < 0)
            return R"({"error":"Cancel failed"})";

        // 6. Помечаем платёж как неудавшийся, если он ещё висел в ожидании
        if (exec_by_int(dbc,
            "UPDATE Payments SET status = 'Failed' WHERE order_id = ? AND status = 'Pending'", order_id) < 0)
            return R"({"error":"Cancel failed"})";

        if (!tx.commit()) return R"({"error":"Cancel failed"})";

        return R"({"status":"order cancelled"})";
    }

    // OrderRepository.cpp
    std::string OrderRepository::cancel_order_by_customer(int order_id, int user_id)
    {
        auto leased = pool.acquire();
        SQLHDBC dbc = leased.handle();

        SQLHSTMT stmt = nullptr;
        SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

        const char* query = "SELECT user_id FROM Orders WHERE order_id = ?";
        SQLPrepareA(stmt, (SQLCHAR*)query, SQL_NTS);

        SQLLEN id_ind = 0;
        SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &order_id, 0, &id_ind);
        SQLExecute(stmt);

        SQLINTEGER owner_id = 0;
        SQLLEN owner_ind = 0;
        SQLBindCol(stmt, 1, SQL_C_LONG, &owner_id, 0, &owner_ind);

        bool found = SQL_SUCCEEDED(SQLFetch(stmt));
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);

        if (!found)
        {
            // не палим, что заказ вообще существует, если он чужой
            return R"({"error":"Order not found"})";
        }

        if (owner_id != user_id)
        {
            return R"({"error":"Order not found"})";
        }

        return cancel_order(order_id, false);
    }

    std::vector<int> OrderRepository::find_expired_order_ids(int minutes)
    {
        auto leased = pool.acquire();
        SQLHDBC dbc = leased.handle();

        SQLHSTMT stmt = nullptr;
        SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

        const char* query =
            "SELECT order_id FROM Orders "
            "WHERE status = 'New' AND created_at < DATEADD(MINUTE, ?, GETDATE())";
        SQLPrepareA(stmt, (SQLCHAR*)query, SQL_NTS);

        int offset = -minutes;
        SQLLEN off_ind = 0;
        SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &offset, 0, &off_ind);
        SQLExecute(stmt);

        SQLINTEGER id = 0;
        SQLLEN id_ind = 0;
        SQLBindCol(stmt, 1, SQL_C_LONG, &id, 0, &id_ind);

        std::vector<int> ids;
        while (SQLFetch(stmt) == SQL_SUCCESS) ids.push_back(id);

        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return ids;
    }

    int OrderRepository::cancel_expired_orders(int minutes)
    {
        int cancelled = 0;

        for (int id : find_expired_order_ids(minutes))
        {
            // only_unpaid = true: если оплата успела прийти, заказ уже Processing и не тронется
            if (json::parse(cancel_order(id, true)).contains("status")) cancelled++;
        }

        return cancelled;
    }