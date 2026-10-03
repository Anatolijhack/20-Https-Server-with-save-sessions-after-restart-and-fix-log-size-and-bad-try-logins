# 🔧 Http Server for Order

Асинхронный HTTPS-сервер на C++ для интернет-магазина автозапчастей. REST API с ролями, каталогом, корзиной, заказами и интеграцией с платёжным шлюзом LiqPay.

## Возможности

- ⚡ Асинхронный I/O на Boost.Asio, тяжёлая работа вынесена в `ThreadPool`
- 🧵 Work-stealing пул потоков с приоритетными очередями
- 🔒 HTTPS/TLS с graceful shutdown (`close_notify`)
- 🔑 Авторизация по токенам с ролями (`admin` / `employee` / `seller` / `customer`)
- 🔐 Пароли хешируются (PBKDF2-HMAC-SHA256 + соль)
- 🛡️ Блокировка по неудачным попыткам входа (хранится в БД, переживает перезапуск)
- 🗄️ Пул соединений к SQL Server (ODBC)
- 🛍️ Каталог: поиск, пагинация, сортировка, артикул, совместимость с марками/моделями авто
- 🛒 Серверная корзина, привязанная к токену
- 📦 Заказы из нескольких позиций, атомарное создание, автоотмена неоплаченных по таймеру
- 🚚 Данные доставки в заказе (получатель, телефон, город, адрес)
- 💳 LiqPay: создание платежа, проверка подписи и суммы, защита от повторной обработки
- 👤 Профиль, смена пароля, выход из одной или всех сессий
- 🧑‍💼 Админка: пользователи и роли, заказы и статусы, отчёт по марже
- 🌐 CORS настроен для работы с сайтом на другом домене
- 📋 Валидация входных данных везде, где принимаются данные от пользователя
- ⚙️ Конфигурация через `config.json`
- 📝 Логирование с ротацией по размеру

## Архитектура

```
Клиент (сайт / curl / Postman)
        │
        ▼
Server (Boost.Asio acceptor, TLS handshake)
        │
        ▼
Session (парсинг HTTP, keep-alive, graceful shutdown)
        │
        ▼
ThreadPool ──▶ Router (METHOD + PATH + query-параметры)
                  │
                  ▼
        Controller (валидация, авторизация, JSON, статусы)
                  │
                  ▼
        Repository (ODBC, параметризованные SQL)
                  │
                  ▼
        ConnectionPool ──▶ SQL Server (ProductDb)
```

Контроллеры: `AuthController`, `ProductController`, `CartController`, `PaymentController`.
Репозитории: `UserRepository`, `ProductRepository`, `CartRepository`, `OrderRepository`, `PaymentRepository`, `SessionRepository`, `LoginThrottleRepository`.
Фоновые задачи: `OrderCleaner` (автоотмена неоплаченных заказов), `SessionCleaner` (удаление просроченных сессий).

## Схема БД

```
Users ─┬─▶ Orders ─┬─▶ OrderItems ──▶ Products ──▶ ProductCompatibility
       │           └─▶ Payments
       ├─▶ CartItems
       └─▶ Sessions

LoginAttempts (по username, не связана с Users напрямую)
```

| Таблица | Назначение |
|---|---|
| **Products** | id, type, product_name, cost, price, stock_quantity, sku |
| **ProductCompatibility** | product_id, make, model, year_from, year_to |
| **Users** | id, login, hash_password, role |
| **Orders** | order_id, user_id, status, created_at, данные доставки |
| **OrderItems** | order_id, product_id, quantity, price (цена на момент заказа) |
| **Payments** | id, order_id, payment_ref, amount, status |
| **CartItems** | user_id, product_id, quantity |
| **Sessions** | token, user_id, username, role, created_at, last_used_at |
| **LoginAttempts** | username, failures, blocked_until |

`cost` (закупочная цена) наружу через API не отдаётся — виден только в отчёте по марже для `admin`.

> ⚠️ Загрузка фото пользователем с ПК **не реализована**. Колонка `Products.image_url` существует, но заполняется только вручную (например, прямой ссылкой на изображение при создании товара через `POST /products`).

## Требования

- Windows, Visual Studio (MSVC), C++17+
- Microsoft ODBC Driver 18 for SQL Server, SQL Server
- Boost.Asio (+SSL), OpenSSL, nlohmann/json

## Настройка

**config.json** (рядом с `.exe`):

```json
{
    "server": { "port": 8080 },
    "database": {
        "driver": "ODBC Driver 18 for SQL Server",
        "server": "ИМЯ_СЕРВЕРА",
        "database": "ProductDb",
        "trusted_connection": true,
        "trust_server_certificate": true,
        "pool_size": 10
    },
    "liqpay": {
        "public_key": "",
        "private_key": "",
        "result_url": "https://ваш-домен/payment/result",
        "server_url": "https://ваш-домен/payment/callback"
    }
}
```

**SSL-сертификат** (для разработки):
```powershell
openssl req -x509 -newkey rsa:2048 -keyout server.key -out server.crt -days 365 -nodes -subj "/CN=localhost"
```
Для продакшена — Let's Encrypt на реальном домене.

**Первый администратор.** Публичная `/register` всегда создаёт роль `customer`. Первый `admin` создаётся через временный маршрут `POST /setup/create-admin` — **удалите его из кода сразу после использования**, иначе любой сможет создать себе администратора тем же способом.

## Запуск

1. Открыть решение в Visual Studio, подключить зависимости.
2. Выполнить SQL-скрипты создания всех таблиц (см. раздел «Схема БД»).
3. Собрать Release/x64.
4. Убедиться, что `config.json`, `server.crt`, `server.key` лежат рядом с `.exe`.
5. Запустить — сервер стартует по адресу `https://localhost:8080`.

## API

### Авторизация
| Метод | Путь | Auth | Описание |
|---|---|---|---|
| POST | `/register` | — | Регистрация (роль всегда `customer`) |
| POST | `/login` | — | Вход; блокируется после серии неудачных попыток |
| POST | `/logout` | Bearer | Завершить текущую сессию |
| POST | `/logout/all` | Bearer | Завершить все сессии пользователя |
| GET | `/me` | Bearer | Текущий пользователь |
| PUT | `/me/password` | Bearer | Смена пароля, отзывает все токены |

### Товары
| Метод | Путь | Auth | Описание |
|---|---|---|---|
| GET | `/products` | — | Список (без `cost`) |
| GET | `/products/:id` | — | Один товар |
| GET | `/products/type/:type` | — | По категории |
| GET | `/products/search` | — | `?search=&type=&make=&model=&year=&sort_by=&sort_dir=&limit=&offset=` |
| GET | `/products/:id/compatibility` | — | Совместимые марки/модели |
| POST | `/products/:id/compatibility` | Bearer (admin/employee) | Добавить совместимость |
| POST | `/products` | Bearer (admin/employee) | Добавить товар |
| PUT | `/products/:id` | Bearer (admin/employee) | Обновить цену/остаток |
| DELETE | `/products/:id` | Bearer (admin) | Удалить |

### Корзина (привязана к токену)
| Метод | Путь | Описание |
|---|---|---|
| GET | `/cart` | Содержимое и `total` |
| GET | `/cart/preview` | Актуальные цены, проверка остатков |
| POST | `/cart/add` | Добавить (повтор суммирует количество) |
| PUT | `/cart/update` | Задать точное количество |
| DELETE | `/cart/remove/:product_id` | Убрать позицию |

### Заказы и оплата
| Метод | Путь | Auth | Описание |
|---|---|---|---|
| POST | `/checkout` | Bearer | Заказ из `items` или из корзины + данные доставки |
| GET | `/orders/:id` | Bearer | Только свой заказ (или персонал — любой) |
| GET | `/me/orders` | Bearer | Мои заказы |
| PUT | `/orders/:id/cancel` | Bearer | Отмена своего заказа (только New/Processing) |
| GET | `/admin/orders` | Bearer (admin/employee) | Все заказы |
| PUT | `/admin/orders/:id/status` | Bearer (admin/employee) | Смена статуса по правилам переходов |
| POST | `/payment/callback` | подпись LiqPay | Webhook |

### Админка
| Метод | Путь | Auth | Описание |
|---|---|---|---|
| GET | `/admin/users` | Bearer (admin) | Список пользователей |
| PUT | `/admin/users/:id/role` | Bearer (admin) | Смена роли, отзывает токены пользователя |
| GET | `/admin/reports/margin` | Bearer (admin) | `?from=&to=` — выручка, себестоимость, маржа по товарам |

## Переходы статусов заказа

```
New ──▶ Processing ──▶ Shipped ──▶ Completed
 │           │
 └───────────┴──▶ Cancelled
```

Отмена возможна только из `New`/`Processing`, при отмене остаток товара возвращается на склад, платёж помечается `Failed`. Заказы без оплаты автоматически отменяются фоновой задачей по истечении таймаута.

## Пример checkout

```json
POST /checkout
Authorization: Bearer <token>

{
  "delivery": {
    "recipient_name": "Ivan Petrov",
    "phone": "+380991234567",
    "city": "Odesa",
    "address": "vul. Pushkinska 10"
  }
}
```
Если `items` не переданы — заказ собирается из текущей корзины пользователя, корзина очищается после успешного оформления. Цена и остаток всегда берутся из БД, не от клиента.

## Тестирование

Полный набор проверок по всем маршрутам — в `TESTS.md`.

```powershell
curl.exe -k -X POST "https://localhost:8080/login" -H "Content-Type: application/json" -d '{\"username\":\"admin\",\"password\":\"admin123\"}'
```

> В PowerShell значения с пробелами передавайте через `--data-binary "@file.json"` или `Invoke-RestMethod`, иначе тело обрезается на первом пробеле.

## Безопасность

- PBKDF2-HMAC-SHA256, 100 000 итераций, уникальная соль
- Параметризованные SQL-запросы везде
- Регистрация не даёт роль `admin`
- Блокировка входа с растущей задержкой после неудачных попыток, хранится в БД
- Сессии и попытки входа переживают перезапуск сервера
- Владение проверяется на уровне данных: чужой заказ или чужая отмена всегда `404`, не `403`
- Цена заказа считается на сервере
- Callback LiqPay: проверка подписи, сверка суммы, идемпотентность

**Перед реальным запуском:**
- [ ] Удалить `/setup/create-admin`
- [ ] Настоящий домен и сертификат Let's Encrypt
- [ ] Реальные ключи LiqPay (сначала sandbox)

## Планы

- [ ] Загрузка фото товара с компьютера пользователя (`multipart/form-data`) — не реализовано
- [ ] Уведомления клиенту/персоналу (email/Telegram)
- [ ] Автотесты вместо ручного прогона TESTS.md
- [ ] Сайт для клиентов поверх готового API

## Технические заметки

- `#include <windows.h>` — раньше `sql.h`/`sqlext.h`/`sqltypes.h` в каждом файле, где они используются
- Кириллица в строковых литералах — файл должен быть сохранён в UTF-8 без BOM
- `UPDATE`/`DELETE` без совпадений возвращают `SQL_NO_DATA`, а не ошибку — проверяйте это отдельно, иначе «не найдено» превращается в 500
- `ssl::stream<tcp::socket>` закрывается через `.next_layer().close()`, не `.close()` напрямую
- `ConnectionPool::LeasedConnection` — RAII, возвращает соединение в пул даже при исключении
- `Transaction` — RAII-обёртка транзакции: без явного `commit()` при выходе из области видимости делает `ROLLBACK`
- Логи ротируются по размеру (по умолчанию 10 МБ), старые архивы с меткой времени, лишние удаляются автоматически
