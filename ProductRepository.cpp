#include "ProductRepository.h"
#include <iostream>

using json = nlohmann::json;
static void print_odbc_error(SQLSMALLINT handle_type, SQLHANDLE handle)
{
    SQLCHAR sqlstate[6];
    SQLCHAR message[SQL_MAX_MESSAGE_LENGTH];
    SQLINTEGER native_error;
    SQLSMALLINT message_len;
    SQLSMALLINT i = 1;

    while (SQLGetDiagRecA(
        handle_type,
        handle,
        i,
        sqlstate,
        &native_error,
        message,
        sizeof(message),
        &message_len
    ) == SQL_SUCCESS)
    {
        std::cerr << "ODBC Error [" << sqlstate << "] " << message << std::endl;
        i++;
    }
}
SQLHDBC Sconnect_to_database()
{
    SQLHENV env = nullptr;
    SQLHDBC dbc = nullptr;

    SQLAllocHandle(
        SQL_HANDLE_ENV,
        SQL_NULL_HANDLE,
        &env
    );

    SQLSetEnvAttr(
        env,
        SQL_ATTR_ODBC_VERSION,
        (SQLPOINTER)SQL_OV_ODBC3,
        0
    );

    SQLAllocHandle(
        SQL_HANDLE_DBC,
        env,
        &dbc
    );

    SQLCHAR connection_string[] =
        "Driver={ODBC Driver 18 for SQL Server};"
        "Server=devilkirya;"
        "Database=Studydb;"
        "Trusted_Connection=yes;"
        "TrustServerCertificate=yes;";

    SQLRETURN result = SQLDriverConnectA(
        dbc,
        nullptr,
        connection_string,
        SQL_NTS,
        nullptr,
        0,
        nullptr,
        SQL_DRIVER_NOPROMPT
    );

    if (!SQL_SUCCEEDED(result))
    {
        SQLFreeHandle(SQL_HANDLE_DBC, dbc);
        SQLFreeHandle(SQL_HANDLE_ENV, env);

        return nullptr;
    }

    return dbc;
}
static DatabaseConnection connect_to_database()
{
    DatabaseConnection connection;

    SQLRETURN result;

    result = SQLAllocHandle(
        SQL_HANDLE_ENV,
        SQL_NULL_HANDLE,
        &connection.env
    );

    if (!SQL_SUCCEEDED(result))
        return {};

    result = SQLSetEnvAttr(
        connection.env,
        SQL_ATTR_ODBC_VERSION,
        (SQLPOINTER)SQL_OV_ODBC3,
        0
    );

    if (!SQL_SUCCEEDED(result))
    {
        SQLFreeHandle(
            SQL_HANDLE_ENV,
            connection.env
        );

        return {};
    }

    result = SQLAllocHandle(
        SQL_HANDLE_DBC,
        connection.env,
        &connection.dbc
    );

    if (!SQL_SUCCEEDED(result))
    {
        SQLFreeHandle(
            SQL_HANDLE_ENV,
            connection.env
        );

        return {};
    }

    SQLCHAR connection_string[] =
        "Driver={ODBC Driver 18 for SQL Server};"
        "Server=devilkirya;"
        "Database=Studydb;"
        "Trusted_Connection=yes;"
        "TrustServerCertificate=yes;";
        "Encrypt=no;";

    result = SQLDriverConnectA(
        connection.dbc,
        nullptr,
        connection_string,
        SQL_NTS,
        nullptr,
        0,
        nullptr,
        SQL_DRIVER_NOPROMPT
    );

    SQLHSTMT check_stmt = nullptr;
    SQLAllocHandle(SQL_HANDLE_STMT, connection.dbc, &check_stmt);

    const char* identity_query = "SELECT SUSER_SNAME(), USER_NAME(), SCHEMA_NAME()";
    SQLExecDirectA(check_stmt, (SQLCHAR*)identity_query, SQL_NTS);

    SQLCHAR login[128] = {}, user[128] = {}, schema[128] = {};
    SQLLEN login_ind = 0, user_ind = 0, schema_ind = 0;

    SQLBindCol(check_stmt, 1, SQL_C_CHAR, login, sizeof(login), &login_ind);
    SQLBindCol(check_stmt, 2, SQL_C_CHAR, user, sizeof(user), &user_ind);
    SQLBindCol(check_stmt, 3, SQL_C_CHAR, schema, sizeof(schema), &schema_ind);

    if (SQLFetch(check_stmt) == SQL_SUCCESS)
    {
        std::cerr << "APP LOGIN: [" << login << "] USER: [" << user << "] SCHEMA: [" << schema << "]" << std::endl;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, check_stmt);

    SQLCHAR server_name[128] = {};
    SQLCHAR db_name[128] = {};
    SQLSMALLINT out_len = 0;

    SQLGetInfoA(connection.dbc, SQL_SERVER_NAME, server_name, sizeof(server_name), &out_len);
    SQLGetInfoA(connection.dbc, SQL_DATABASE_NAME, db_name, sizeof(db_name), &out_len);

    std::cerr << "CONNECTED TO SERVER: [" << server_name << "] DATABASE: [" << db_name << "]" << std::endl;

    if (!SQL_SUCCEEDED(result))
    {
        SQLFreeHandle(
            SQL_HANDLE_DBC,
            connection.dbc
        );

        SQLFreeHandle(
            SQL_HANDLE_ENV,
            connection.env
        );

        return {};
    }

    return connection;
}

std::string ProductRepository::get_products()
{
    auto leased = pool.acquire();
    SQLHDBC dbc = leased.handle();

    SQLHSTMT stmt = nullptr;
    SQLRETURN result = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    if (!SQL_SUCCEEDED(result))
    {
        return R"({"error":"Failed to allocate statement handle"})";
    }

    const char* query = "SELECT id, type, product_name, price, stock_quantity, sku, image_url FROM Products";

    result = SQLExecDirectA(stmt, (SQLCHAR*)query, SQL_NTS);

    if (!SQL_SUCCEEDED(result))
    {
        print_odbc_error(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return R"({"error":"Query execution failed"})";
    }

    json products = json::array();

    SQLINTEGER id = 0;
    SQLCHAR type[64] = {};
    SQLCHAR name[128] = {};
    SQLDOUBLE price = 0.0;
    SQLINTEGER stock = 0;

    SQLLEN id_ind = 0, type_ind = 0, name_ind = 0, price_ind = 0, stock_ind = 0;

    SQLBindCol(stmt, 1, SQL_C_LONG, &id, 0, &id_ind);
    SQLBindCol(stmt, 2, SQL_C_CHAR, type, sizeof(type), &type_ind);
    SQLBindCol(stmt, 3, SQL_C_CHAR, name, sizeof(name), &name_ind);
    SQLBindCol(stmt, 4, SQL_C_DOUBLE, &price, 0, &price_ind);
    SQLBindCol(stmt, 5, SQL_C_LONG, &stock, 0, &stock_ind);

    while (SQLFetch(stmt) == SQL_SUCCESS)
    {
        json item;
        item["id"] = id;
        item["type"] = std::string((char*)type);
        item["name"] = std::string((char*)name);
        item["price"] = price;
        item["stock_quantity"] = stock;
        products.push_back(item);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return products.dump();
}
std::string ProductRepository::get_product(int id)
{
    auto leased = pool.acquire();
    SQLHDBC dbc = leased.handle();

    SQLHSTMT stmt = nullptr;
    SQLRETURN result = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    if (!SQL_SUCCEEDED(result))
    {
        return R"({"error":"Failed to allocate statement handle"})";
    }

    const char* query = "SELECT id, type, product_name, price, stock_quantity, sku, image_url FROM Products WHERE id = ?";

    result = SQLPrepareA(stmt, (SQLCHAR*)query, SQL_NTS);

    if (!SQL_SUCCEEDED(result))
    {
        print_odbc_error(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return R"({"error":"Failed to prepare statement"})";
    }

    SQLLEN id_param_ind = 0;
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &id, 0, &id_param_ind);

    result = SQLExecute(stmt);

    if (!SQL_SUCCEEDED(result))
    {
        print_odbc_error(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return R"({"error":"Query execution failed"})";
    }

    SQLINTEGER out_id = 0;
    SQLCHAR type[64] = {};
    SQLCHAR name[128] = {};
    SQLDOUBLE price = 0.0;
    SQLINTEGER stock = 0;

    SQLLEN id_ind = 0, type_ind = 0, name_ind = 0, price_ind = 0, stock_ind = 0;

    SQLBindCol(stmt, 1, SQL_C_LONG, &out_id, 0, &id_ind);
    SQLBindCol(stmt, 2, SQL_C_CHAR, type, sizeof(type), &type_ind);
    SQLBindCol(stmt, 3, SQL_C_CHAR, name, sizeof(name), &name_ind);
    SQLBindCol(stmt, 4, SQL_C_DOUBLE, &price, 0, &price_ind);
    SQLBindCol(stmt, 5, SQL_C_LONG, &stock, 0, &stock_ind);

    json product;

    if (SQLFetch(stmt) == SQL_SUCCESS)
    {
        product["id"] = out_id;
        product["type"] = std::string((char*)type);
        product["name"] = std::string((char*)name);
        product["price"] = price;
        product["stock_quantity"] = stock;
    }
    else
    {
        product = { {"error", "Product not found"} };
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return product.dump();
}

std::string ProductRepository::add_product(const std::string& type, const std::string& name, double cost, double price, int stock, int sku,const std::string& image_url)
{
    auto leased = pool.acquire();
    SQLHDBC dbc = leased.handle();

    SQLHSTMT stmt = nullptr;
    SQLRETURN result = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    if (!SQL_SUCCEEDED(result))
    {
        return R"({"error":"Failed to allocate statement handle"})";
    }

    const char* query = "INSERT INTO Products (type, product_name, cost, price, stock_quantity, sku, image_url) VALUES (?, ?, ?, ?, ?, ?, ?)";

    result = SQLPrepareA(stmt, (SQLCHAR*)query, SQL_NTS);

    if (!SQL_SUCCEEDED(result))
    {
        print_odbc_error(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return R"({"error":"Failed to prepare statement"})";
    }

    SQLLEN type_len = SQL_NTS;
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
        type.size(), 0, (SQLPOINTER)type.c_str(), 0, &type_len);

    SQLLEN name_len = SQL_NTS;
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
        name.size(), 0, (SQLPOINTER)name.c_str(), 0, &name_len);

    SQLLEN cost_ind = 0;
    SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_DOUBLE, SQL_DECIMAL, 10, 2, &cost, 0, &cost_ind);

    SQLLEN price_ind = 0;
    SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_DOUBLE, SQL_DECIMAL, 10, 2, &price, 0, &price_ind);

    SQLLEN stock_ind = 0;
    SQLBindParameter(stmt, 5, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &stock, 0, &stock_ind);

    SQLLEN sku_ind = 0;
    SQLBindParameter(stmt, 6, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &sku, 0, &sku_ind);

    SQLLEN image_url_len = SQL_NTS;
    SQLBindParameter(stmt, 7, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
        image_url.size(), 0, (SQLPOINTER)image_url.c_str(), 0, &image_url_len);



    result = SQLExecute(stmt);

    if (!SQL_SUCCEEDED(result))
    {
        print_odbc_error(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return R"({"error":"Insert failed"})";
    }

    result = SQLEndTran(SQL_HANDLE_DBC, dbc, SQL_COMMIT);

    if (!SQL_SUCCEEDED(result))
    {
        print_odbc_error(SQL_HANDLE_DBC, dbc);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return R"({"error":"Commit failed"})";
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return R"({"status":"product added"})";
}


std::string ProductRepository::delete_product(int id)
{
    auto leased = pool.acquire();
    SQLHDBC dbc = leased.handle();

    SQLHSTMT stmt = nullptr;
    SQLRETURN result = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    if (!SQL_SUCCEEDED(result))
    {
        return R"({"error":"Failed to allocate statement handle"})";
    }

    const char* query = "DELETE FROM Products WHERE id = ?";

    result = SQLPrepareA(stmt, (SQLCHAR*)query, SQL_NTS);

    if (!SQL_SUCCEEDED(result))
    {
        print_odbc_error(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return R"({"error":"Failed to prepare statement"})";
    }

    SQLLEN id_ind = 0;
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &id, 0, &id_ind);

    result = SQLExecute(stmt);

    if (!SQL_SUCCEEDED(result))
    {
        print_odbc_error(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);

        return R"({"error":"Delete failed (product may be referenced in existing orders)";
    }


    SQLLEN rows = 0;
    SQLRowCount(stmt, &rows);

    result = SQLEndTran(SQL_HANDLE_DBC, dbc, SQL_COMMIT);

    if (!SQL_SUCCEEDED(result))
    {
      print_odbc_error(SQL_HANDLE_DBC, dbc);
      SQLFreeHandle(SQL_HANDLE_STMT, stmt);
      return R"({"error":"Commit failed"})";
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    if (rows == 0)
    {
      return R"({"error":"Product not found"})";
    }

    return R"({"status":"product deleted"})";
 }

std::string ProductRepository::update_product(int id, double price, int stock)
{
    auto leased = pool.acquire();
    SQLHDBC dbc = leased.handle();

    SQLHSTMT stmt = nullptr;
    SQLRETURN result = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    if (!SQL_SUCCEEDED(result))
    {
        return R"({"error":"Failed to allocate statement handle"})";
    }

    const char* query = "UPDATE Products SET price = ?, stock_quantity = ? WHERE id = ?";

    result = SQLPrepareA(stmt, (SQLCHAR*)query, SQL_NTS);

    if (!SQL_SUCCEEDED(result))
    {
        print_odbc_error(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return R"({"error":"Failed to prepare statement"})";
    }

    SQLLEN price_ind = 0;
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_DOUBLE, SQL_DECIMAL, 10, 2, &price, 0, &price_ind);

    SQLLEN stock_ind = 0;
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &stock, 0, &stock_ind);

    SQLLEN id_ind = 0;
    SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &id, 0, &id_ind);

    result = SQLExecute(stmt);

    if (!SQL_SUCCEEDED(result))
    {
        print_odbc_error(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return R"({"error":"Update failed"})";
    }

    SQLLEN rows = 0;
    SQLRowCount(stmt, &rows);

    result = SQLEndTran(SQL_HANDLE_DBC, dbc, SQL_COMMIT);

    if (!SQL_SUCCEEDED(result))
    {
        print_odbc_error(SQL_HANDLE_DBC, dbc);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return R"({"error":"Commit failed"})";
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    if (rows == 0)
    {
        return R"({"error":"Product not found"})";
    }

    return R"({"status":"product updated"})";
}


std::string ProductRepository::get_products_by_type(const std::string& type)
{
    auto leased = pool.acquire();
    SQLHDBC dbc = leased.handle();

    SQLHSTMT stmt = nullptr;
    SQLRETURN result = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    if (!SQL_SUCCEEDED(result))
    {
        return R"({"error":"Failed to allocate statement handle"})";
    }

    const char* query = "SELECT id, type, product_name, price, stock_quantity FROM Products WHERE type = ?";

    result = SQLPrepareA(stmt, (SQLCHAR*)query, SQL_NTS);

    if (!SQL_SUCCEEDED(result))
    {
        print_odbc_error(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return R"({"error":"Failed to prepare statement"})";
    }

    SQLLEN type_len = SQL_NTS;
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
        type.size(), 0, (SQLPOINTER)type.c_str(), 0, &type_len);

    result = SQLExecute(stmt);

    if (!SQL_SUCCEEDED(result))
    {
        print_odbc_error(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return R"({"error":"Query execution failed"})";
    }

    json products = json::array();

    SQLINTEGER id = 0;
    SQLCHAR out_type[64] = {};
    SQLCHAR name[128] = {};
    SQLDOUBLE price = 0.0;
    SQLINTEGER stock = 0;

    SQLLEN id_ind = 0, type_ind = 0, name_ind = 0, price_ind = 0, stock_ind = 0;

    SQLBindCol(stmt, 1, SQL_C_LONG, &id, 0, &id_ind);
    SQLBindCol(stmt, 2, SQL_C_CHAR, out_type, sizeof(out_type), &type_ind);
    SQLBindCol(stmt, 3, SQL_C_CHAR, name, sizeof(name), &name_ind);
    SQLBindCol(stmt, 4, SQL_C_DOUBLE, &price, 0, &price_ind);
    SQLBindCol(stmt, 5, SQL_C_LONG, &stock, 0, &stock_ind);

    while (SQLFetch(stmt) == SQL_SUCCESS)
    {
        json item;
        item["id"] = id;
        item["type"] = std::string((char*)out_type);
        item["name"] = std::string((char*)name);
        item["price"] = price;
        item["stock_quantity"] = stock;
        products.push_back(item);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return products.dump();
}

std::string ProductRepository::search_products(const ProductQuery& q)
{
    auto leased = pool.acquire();
    SQLHDBC dbc = leased.handle();

    // белый список, чтобы нельзя было подставить произвольный SQL через sort_by/sort_dir
    std::string sort_column = (q.sort_by == "price") ? "price" : "id";
    std::string sort_dir = (q.sort_dir == "DESC") ? "DESC" : "ASC";



    std::string query =
        "SELECT DISTINCT p.id, p.type, p.product_name, p.price, p.stock_quantity, "
        "p.sku, p.image_url, COUNT(*) OVER() AS total_count "
        "FROM Products p ";

    if (q.make || q.model)
    {
        query += "JOIN ProductCompatibility c ON c.product_id = p.id ";
    }

    query += "WHERE 1=1 ";

    if (q.search) query += "AND p.product_name LIKE ? ";
    if (q.type)   query += "AND p.type = ? ";
    if (q.make)   query += "AND c.make = ? ";
    if (q.model)  query += "AND c.model = ? ";
    if (q.year)
    {
        query += "AND (c.year_from IS NULL OR c.year_from <= ?) "
            "AND (c.year_to IS NULL OR c.year_to >= ?) ";
    }

    query += "ORDER BY " + sort_column + " " + sort_dir + " "
        "OFFSET ? ROWS FETCH NEXT ? ROWS ONLY";

    SQLHSTMT stmt = nullptr;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    SQLPrepareA(stmt, (SQLCHAR*)query.c_str(), SQL_NTS);

    int param_idx = 1;
    std::string search_pattern;
    std::string make_val = q.make.value_or("");
    std::string model_val = q.model.value_or("");
    int year_val = q.year.value_or(0);

    SQLLEN search_ind = SQL_NTS, type_ind = SQL_NTS, offset_ind = 0, limit_ind = 0, make_ind = SQL_NTS, model_ind = SQL_NTS, year_ind = 0;

    if (q.search)
    {
        search_pattern = "%" + *q.search + "%";
        SQLBindParameter(stmt, param_idx++, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
            search_pattern.size(), 0, (SQLPOINTER)search_pattern.c_str(), 0, &search_ind);
    }

    if (q.type)
    {
        SQLBindParameter(stmt, param_idx++, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
            q.type->size(), 0, (SQLPOINTER)q.type->c_str(), 0, &type_ind);
    }
    if (q.make)
        SQLBindParameter(stmt, param_idx++, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
            make_val.size(), 0, (SQLPOINTER)make_val.c_str(), 0, &make_ind);

    if (q.model)
        SQLBindParameter(stmt, param_idx++, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
            model_val.size(), 0, (SQLPOINTER)model_val.c_str(), 0, &model_ind);

    if (q.year)
    {
        SQLBindParameter(stmt, param_idx++, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &year_val, 0, &year_ind);
        SQLBindParameter(stmt, param_idx++, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &year_val, 0, &year_ind);
    }

    int offset = q.offset, limit = q.limit;
    SQLBindParameter(stmt, param_idx++, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &offset, 0, &offset_ind);
    SQLBindParameter(stmt, param_idx++, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &limit, 0, &limit_ind);

    SQLExecute(stmt);

    json products = json::array();
    int total_count = 0;

    SQLINTEGER id = 0, stock = 0, count = 0;
    SQLCHAR type[64] = {}, name[128] = {};
    SQLCHAR sku[64] = {}, image_url[320] = {};
    SQLDOUBLE price = 0.0;
    SQLLEN id_ind, type_ind2, name_ind, price_ind, stock_ind, count_ind, sku_ind = 0, image_ind = 0;

    SQLBindCol(stmt, 1, SQL_C_LONG, &id, 0, &id_ind);
    SQLBindCol(stmt, 2, SQL_C_CHAR, type, sizeof(type), &type_ind2);
    SQLBindCol(stmt, 3, SQL_C_CHAR, name, sizeof(name), &name_ind);
    SQLBindCol(stmt, 4, SQL_C_DOUBLE, &price, 0, &price_ind);
    SQLBindCol(stmt, 5, SQL_C_LONG, &stock, 0, &stock_ind);
    SQLBindCol(stmt, 6, SQL_C_CHAR, sku, sizeof(sku), &sku_ind);
    SQLBindCol(stmt, 7, SQL_C_CHAR, image_url, sizeof(image_url), &image_ind);
    SQLBindCol(stmt, 8, SQL_C_LONG, &count, 0, &count_ind);

    while (SQLFetch(stmt) == SQL_SUCCESS)
    {
        json item;
        item["id"] = id;
        item["type"] = std::string((char*)type);
        item["name"] = std::string((char*)name);
        item["price"] = price;
        item["stock_quantity"] = stock;
        item["sku"] = std::string((char*)sku);
        item["image_url"] = std::string((char*)image_url);
        products.push_back(item);
        total_count = count;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    json response;
    response["items"] = products;
    response["total"] = total_count;
    response["limit"] = q.limit;
    response["offset"] = q.offset;

    return response.dump();
}

std::string ProductRepository::add_compatibility(int product_id, const std::string& make, const std::string& model,
    std::optional<int> year_from, std::optional<int> year_to)
{
    auto leased = pool.acquire();
    SQLHDBC dbc = leased.handle();

    SQLHSTMT stmt = nullptr;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    const char* query =
        "INSERT INTO ProductCompatibility (product_id, make, model, year_from, year_to) "
        "VALUES (?, ?, ?, ?, ?)";
    SQLPrepareA(stmt, (SQLCHAR*)query, SQL_NTS);

    SQLLEN pid_ind = 0;
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &product_id, 0, &pid_ind);

    SQLLEN make_ind = SQL_NTS, model_ind = SQL_NTS;
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, make.size(), 0, (SQLPOINTER)make.c_str(), 0, &make_ind);
    SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, model.size(), 0, (SQLPOINTER)model.c_str(), 0, &model_ind);

    int yf = year_from.value_or(0), yt = year_to.value_or(0);
    SQLLEN yf_ind = year_from ? 0 : SQL_NULL_DATA;
    SQLLEN yt_ind = year_to ? 0 : SQL_NULL_DATA;
    SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &yf, 0, &yf_ind);
    SQLBindParameter(stmt, 5, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &yt, 0, &yt_ind);

    SQLRETURN result = SQLExecute(stmt);

    if (!SQL_SUCCEEDED(result))
    {
        print_odbc_error(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return R"({"error":"Failed to add compatibility"})";
    }

    SQLEndTran(SQL_HANDLE_DBC, dbc, SQL_COMMIT);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    return R"({"status":"compatibility added"})";
}

std::string ProductRepository::get_compatibility(int product_id)
{
    auto leased = pool.acquire();
    SQLHDBC dbc = leased.handle();

    SQLHSTMT stmt = nullptr;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    const char* query = "SELECT make, model, year_from, year_to FROM ProductCompatibility WHERE product_id = ?";
    SQLPrepareA(stmt, (SQLCHAR*)query, SQL_NTS);

    SQLLEN pid_ind = 0;
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &product_id, 0, &pid_ind);
    SQLExecute(stmt);

    SQLCHAR make[64] = {}, model[64] = {};
    SQLINTEGER year_from = 0, year_to = 0;
    SQLLEN make_ind, model_ind, yf_ind, yt_ind;

    SQLBindCol(stmt, 1, SQL_C_CHAR, make, sizeof(make), &make_ind);
    SQLBindCol(stmt, 2, SQL_C_CHAR, model, sizeof(model), &model_ind);
    SQLBindCol(stmt, 3, SQL_C_LONG, &year_from, 0, &yf_ind);
    SQLBindCol(stmt, 4, SQL_C_LONG, &year_to, 0, &yt_ind);

    json result = json::array();

    while (SQLFetch(stmt) == SQL_SUCCESS)
    {
        json item;
        item["make"] = std::string((char*)make);
        item["model"] = std::string((char*)model);
        item["year_from"] = (yf_ind == SQL_NULL_DATA) ? json(nullptr) : json(year_from);
        item["year_to"] = (yt_ind == SQL_NULL_DATA) ? json(nullptr) : json(year_to);
        result.push_back(item);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return result.dump();
}