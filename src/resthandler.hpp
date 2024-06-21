#include "databasehandler.hpp"
#include "threadpool.hpp"
#include <crow.h>

class RestHandler {
public:
    RestHandler(DatabaseHandler& dbHandler, ThreadPool& threadPool);

    void handle_get(const crow::request& req, crow::response& res, int id);
    uint64_t handle_create_client_personal_history(const crow::request& req, crow::response& res);

private:
    DatabaseHandler& dbHandler;
    ThreadPool& threadPool;
};
