#pragma once
#include <windows.h>
#include <sql.h>
#include <sqlext.h>

// RAII-транзакция: без commit() при выходе из области видимости делает откат
class Transaction
{
public:
    explicit Transaction(SQLHDBC dbc) : dbc(dbc)
    {
        SQLSetConnectAttr(dbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER)SQL_AUTOCOMMIT_OFF, 0);
    }

    bool commit()
    {
        committed = SQL_SUCCEEDED(SQLEndTran(SQL_HANDLE_DBC, dbc, SQL_COMMIT));
        return committed;
    }

    ~Transaction()
    {
        if (!committed) SQLEndTran(SQL_HANDLE_DBC, dbc, SQL_ROLLBACK);
        SQLSetConnectAttr(dbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER)SQL_AUTOCOMMIT_ON, 0);
    }

private:
    SQLHDBC dbc;
    bool committed = false;
};