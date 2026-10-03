#include "CartRepository.h"
#include <iostream>

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

// Добавить товар в корзину. Если товар уже есть — увеличить количество.
std::string CartRepository::add_item(int user_id, int product_id, int quantity)
{
    auto leased = pool.acquire();
    SQLHDBC dbc = leased.handle();

    // Проверяем, есть ли уже такая позиция
    SQLHSTMT check_stmt = nullptr;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &check_stmt);

    const char* check_query = "SELECT quantity FROM CartItems WHERE user_id = ? AND product_id = ?";
    SQLPrepareA(check_stmt, (SQLCHAR*)check_query, SQL_NTS);

    SQLLEN uid_ind = 0, pid_ind = 0;
    SQLBindParameter(check_stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &user_id, 0, &uid_ind);
    SQLBindParameter(check_stmt, 2, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &product_id, 0, &pid_ind);

    SQLExecute(check_stmt);

    SQLINTEGER existing_qty = 0;
    SQLLEN qty_ind = 0;
    SQLBindCol(check_stmt, 1, SQL_C_LONG, &existing_qty, 0, &qty_ind);

    bool exists = (SQLFetch(check_stmt) == SQL_SUCCESS);
    SQLFreeHandle(SQL_HANDLE_STMT, check_stmt);

    SQLHSTMT stmt = nullptr;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    SQLRETURN result;

    if (exists)
    {
        // Обновляем количество (прибавляем)
        int new_qty = existing_qty + quantity;

        const char* update_query = "UPDATE CartItems SET quantity = ? WHERE user_id = ? AND product_id = ?";
        SQLPrepareA(stmt, (SQLCHAR*)update_query, SQL_NTS);

        SQLLEN qty_ind2 = 0, uid_ind2 = 0, pid_ind2 = 0;
        SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &new_qty, 0, &qty_ind2);
        SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &user_id, 0, &uid_ind2);
        SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &product_id, 0, &pid_ind2);

        result = SQLExecute(stmt);
    }
    else
    {
        // Вставляем новую позицию
        const char* insert_query = "INSERT INTO CartItems (user_id, product_id, quantity) VALUES (?, ?, ?)";
        SQLPrepareA(stmt, (SQLCHAR*)insert_query, SQL_NTS);

        SQLLEN uid_ind2 = 0, pid_ind2 = 0, qty_ind2 = 0;
        SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &user_id, 0, &uid_ind2);
        SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &product_id, 0, &pid_ind2);
        SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &quantity, 0, &qty_ind2);

        result = SQLExecute(stmt);
    }

    if (!SQL_SUCCEEDED(result))
    {
        print_odbc_error(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return R"({"error":"Failed to add item to cart"})";
    }

    SQLEndTran(SQL_HANDLE_DBC, dbc, SQL_COMMIT);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    return R"({"status":"item added to cart"})";
}

// Получить корзину с деталями товаров (название, цена, сумма по позиции)
std::string CartRepository::get_cart(int user_id)
{
    auto leased = pool.acquire();
    SQLHDBC dbc = leased.handle();

    SQLHSTMT stmt = nullptr;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    const char* query =
        "SELECT c.product_id, p.product_name, p.price, c.quantity, p.stock_quantity "
        "FROM CartItems c "
        "JOIN Products p ON c.product_id = p.id "
        "WHERE c.user_id = ?";

    SQLPrepareA(stmt, (SQLCHAR*)query, SQL_NTS);

    SQLLEN uid_ind = 0;
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &user_id, 0, &uid_ind);
    SQLExecute(stmt);

    SQLINTEGER product_id = 0, quantity = 0, stock = 0;
    SQLCHAR name[128] = {};
    SQLDOUBLE price = 0.0;
    SQLLEN pid_ind, name_ind, price_ind, qty_ind, stock_ind;

    SQLBindCol(stmt, 1, SQL_C_LONG, &product_id, 0, &pid_ind);
    SQLBindCol(stmt, 2, SQL_C_CHAR, name, sizeof(name), &name_ind);
    SQLBindCol(stmt, 3, SQL_C_DOUBLE, &price, 0, &price_ind);
    SQLBindCol(stmt, 4, SQL_C_LONG, &quantity, 0, &qty_ind);
    SQLBindCol(stmt, 5, SQL_C_LONG, &stock, 0, &stock_ind);

    json items = json::array();
    double total = 0.0;

    while (SQLFetch(stmt) == SQL_SUCCESS)
    {
        json item;
        item["product_id"] = product_id;
        item["product_name"] = std::string((char*)name);
        item["price"] = price;
        item["quantity"] = quantity;
        item["stock_available"] = stock;
        item["subtotal"] = price * quantity;
        items.push_back(item);
        total += price * quantity;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    json response;
    response["items"] = items;
    response["total"] = total;

    return response.dump();
}

// Установить точное количество (не прибавить, а именно задать)
std::string CartRepository::update_item(int user_id, int product_id, int quantity)
{
    auto leased = pool.acquire();
    SQLHDBC dbc = leased.handle();

    SQLHSTMT stmt = nullptr;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    const char* query = "UPDATE CartItems SET quantity = ? WHERE user_id = ? AND product_id = ?";
    SQLPrepareA(stmt, (SQLCHAR*)query, SQL_NTS);

    SQLLEN qty_ind = 0, uid_ind = 0, pid_ind = 0;
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &quantity, 0, &qty_ind);
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &user_id, 0, &uid_ind);
    SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &product_id, 0, &pid_ind);

    SQLRETURN result = SQLExecute(stmt);

    if (!SQL_SUCCEEDED(result) && result != SQL_NO_DATA)
    {
        print_odbc_error(SQL_HANDLE_STMT, stmt);  // <-- убедитесь, что это есть
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return R"({"error":"Failed to update cart item"})";
    }

    SQLLEN rows = 0;
    SQLRowCount(stmt, &rows);

    SQLEndTran(SQL_HANDLE_DBC, dbc, SQL_COMMIT);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    if (rows == 0)
    {
        return R"({"error":"Item not found in cart"})";
    }

    return R"({"status":"cart item updated"})";
}

std::string CartRepository::remove_item(int user_id, int product_id)
{
    auto leased = pool.acquire();
    SQLHDBC dbc = leased.handle();

    SQLHSTMT stmt = nullptr;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    const char* query = "DELETE FROM CartItems WHERE user_id = ? AND product_id = ?";
    SQLPrepareA(stmt, (SQLCHAR*)query, SQL_NTS);

    SQLLEN uid_ind = 0, pid_ind = 0;
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &user_id, 0, &uid_ind);
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &product_id, 0, &pid_ind);

    SQLRETURN result = SQLExecute(stmt);

    if (!SQL_SUCCEEDED(result) && result != SQL_NO_DATA)
    {
        print_odbc_error(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return R"({"error":"Failed to remove item"})";
    }

    SQLLEN rows = 0;
    SQLRowCount(stmt, &rows);

    SQLEndTran(SQL_HANDLE_DBC, dbc, SQL_COMMIT);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    if (rows == 0)
    {
        return R"({"error":"Item not found in cart"})";
    }

    return R"({"status":"item removed from cart"})";
}

std::string CartRepository::clear_cart(int user_id)
{
    auto leased = pool.acquire();
    SQLHDBC dbc = leased.handle();

    SQLHSTMT stmt = nullptr;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    const char* query = "DELETE FROM CartItems WHERE user_id = ?";
    SQLPrepareA(stmt, (SQLCHAR*)query, SQL_NTS);

    SQLLEN uid_ind = 0;
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &user_id, 0, &uid_ind);

    SQLExecute(stmt);
    SQLEndTran(SQL_HANDLE_DBC, dbc, SQL_COMMIT);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    return R"({"status":"cart cleared"})";
}


std::vector<OrderItemInput> CartRepository::get_cart_as_order_items(int user_id)
{
    auto leased = pool.acquire();
    SQLHDBC dbc = leased.handle();

    SQLHSTMT stmt = nullptr;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    const char* query = "SELECT product_id, quantity FROM CartItems WHERE user_id = ?";
    SQLPrepareA(stmt, (SQLCHAR*)query, SQL_NTS);

    SQLLEN uid_ind = 0;
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &user_id, 0, &uid_ind);
    SQLExecute(stmt);

    SQLINTEGER product_id = 0, quantity = 0;
    SQLLEN pid_ind = 0, qty_ind = 0;

    SQLBindCol(stmt, 1, SQL_C_LONG, &product_id, 0, &pid_ind);
    SQLBindCol(stmt, 2, SQL_C_LONG, &quantity, 0, &qty_ind);

    std::vector<OrderItemInput> items;

    while (SQLFetch(stmt) == SQL_SUCCESS)
    {
        items.push_back({ product_id, quantity });
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return items;
}