// AuthController.h
#pragma once
#include "UserRepository.h"
#include "Router.h"
#include "LoginThrothle.h"
#include "LoginThrottleRepository.h"
#include "SessionRepository.h"


class AuthController
{
public:
    AuthController(ConnectionPool& pool, SessionRepository& sessions)
        : user_repository(pool), throttle_repository(pool), sessions(sessions) {}

    void register_routes(Router& router);

private:
    
    LoginThrottleRepository throttle_repository;
    Response register_user(const Request& req);
    Response login(const Request& req);
    Response create_admin_setup(const Request& req); // временный метод
    Response create_worker_setup(const Request& req);
    UserRepository user_repository;
    Response logout(const Request& req);
    Response logout_all(const Request& req);
    Response change_password(const Request& req);
    Response list_users(const Request& req);
    Response change_user_role(const Request& req);
    SessionRepository& sessions;
};