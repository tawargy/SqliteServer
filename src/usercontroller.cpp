#include "usercontroller.hpp"
#include <fmt/core.h> // Include fmt library for string formatting
#include <jsoncons/json.hpp> // Include jsoncons library for JSON handling
#include <picosha2.h>
#include <regex>

// Definition of the implementation class
class UserController::Impl {
public:
    Impl(DatabaseController& dbController, RestHelper& rHelper)
        : dbController(dbController)
        , rHelper(rHelper)
    {
    }

    typedef struct {
        std::string username;
        std::string password_hash;
        std::string role;
        std::string user_data;
    } UserRegistrationData;

    void respond_with_error(crow::response& res, json& response_json, const std::string& status_message, const std::string& response, const short status, const short code);
    bool is_request_data_valid(const crow::request& req, crow::response& res, json& response_json);
    bool is_username_pattern_valid(const std::string& username);
    bool is_string_contains_spaces(const std::string& string);
    bool is_password_pattern_valid(const std::string& password);
    bool is_email_pattern_valid(const std::string& email);
    bool extract_and_sanity_check_user_registration_data(UserRegistrationData& userRegistrationData, json& userdata_json, json& response_json, crow::response& res);

    DatabaseController& dbController;
    RestHelper& rHelper;
};

void UserController::Impl::respond_with_error(crow::response& res, json& response_json, const std::string& status_message, const std::string& response, const short status, const short code)
{
    rHelper.format_response(response_json, status, status_message, response);
    rHelper.finish_response(res, code, response_json);
}

bool UserController::Impl::is_request_data_valid(const crow::request& req, crow::response& res, json& response_json)
{
    // Try to parse json and throw error if invalid json
    try {
        auto userdata_json = json::parse(req.body);
        return true;
    } catch (const std::exception& e) {
        respond_with_error(res, response_json, "Failed to create a new user, invalid JSON", fmt::format("Error parsing user data: {}", e.what()), -1, 400);
        return false;
    }
}

bool UserController::Impl::is_username_pattern_valid(const std::string& username)
{
    const std::regex pattern("^[a-z][a-z0-9_]*$");
    return std::regex_match(username, pattern);
}

bool UserController::Impl::is_string_contains_spaces(const std::string& string)
{
    std::regex pattern("\\s");
    return std::regex_search(string, pattern);
}

bool UserController::Impl::is_password_pattern_valid(const std::string& password)
{
    const std::regex pattern("^(?=.*[a-z])(?=.*[A-Z])(?=.*\\d)(?=.*[!@#$%^&*])[A-Za-z\\d!@#$%^&*]{8,}$");
    return std::regex_match(password, pattern);
}

bool UserController::Impl::is_email_pattern_valid(const std::string& email)
{
    const std::regex pattern(R"((\w+)(\.\w+)*@(\w+)(\.\w+)+)");
    return std::regex_match(email, pattern);
}

bool UserController::Impl::extract_and_sanity_check_user_registration_data(UserRegistrationData& userRegistrationData, json& userdata_json, json& response_json, crow::response& res)
{
    // Extract the username from the JSON
    userRegistrationData.username = userdata_json["username"].as<std::string>();

    // Check username pattern
    if (is_username_pattern_valid(userRegistrationData.username)) {
        userdata_json.erase("username");
    } else {
        respond_with_error(res, response_json, "Failed to create a new user, invalid username", "Username should always be in lowercase characters and underscore or numbers only", -1, 400);
        return false;
    }

    // Check password pattern
    std::string password = userdata_json["password"].as<std::string>();

    if (is_password_pattern_valid(password)) {
        userdata_json.erase("password");
    } else {
        respond_with_error(res, response_json, "Failed to create a new user, invalid password", "Password is weak", -1, 400);
        return false;
    }

    // Create the password hash
    userRegistrationData.password_hash = picosha2::hash256_hex_string(password);

    // Extract the role, user_data, and email for further validation
    userRegistrationData.role = userdata_json["role"].as<std::string>();
    userRegistrationData.user_data = userdata_json["user_data"].as<std::string>();

    std::string email = userdata_json["user_data"]["contact"]["email"].as<std::string>();

    // Check if username contains spaces
    if (is_string_contains_spaces(userRegistrationData.username)) {
        respond_with_error(res, response_json, "Failed to create a new user, username contains spaces", "Username contains spaces", -1, 400);
        return false;
    }

    // Check if user exists
    if (dbController.checkItemExists("users", "username", userRegistrationData.username)) {
        respond_with_error(res, response_json, "Failed to create a new user, user exists", "User already exists", -1, 400);
        return false;
    }

    // Check if username, password, or email are empty
    if (userRegistrationData.username.empty() || password.empty() || userRegistrationData.password_hash.empty()) {
        respond_with_error(res, response_json, "Failed to create a new user, invalid data", "Empty username or password", -1, 400);
        return false;
    }

    // Check if the email matches the pattern
    if (!is_email_pattern_valid(email)) {
        respond_with_error(res, response_json, "Failed to create a new user, invalid data", "Invalid email format", -1, 400);
        return false;
    }
    return true;
}

UserController::UserController(DatabaseController& dbController, RestHelper& rHelper)
    : pImpl(std::make_unique<Impl>(dbController, rHelper))
{
}

UserController::UserController()
{
}

UserController::~UserController()
{
}

void UserController::register_user(const crow::request& req, crow::response& res)
{
    json response_json;
    // Check JSON validity
    if (!pImpl->is_request_data_valid(req, res, response_json)) {
        return;
    }

    // Parse the JSON and extract the data
    try {
        // Get the JSON from request body
        auto userdata_json = json::parse(req.body);

        Impl::UserRegistrationData userRegistrationData;

        // Extract and validate user registration data
        if (!pImpl->extract_and_sanity_check_user_registration_data(userRegistrationData, userdata_json, response_json, res)) {
            return;
        }

        // Construct SQL query using {fmt} for parameterized query
        std::string query = fmt::format(
            "INSERT INTO users (username, password_hash, role, user_data) VALUES ('{}','{}','{}','{}')",
            userRegistrationData.username, userRegistrationData.password_hash, userRegistrationData.role, userRegistrationData.user_data);

        // Execute the query using DatabaseController
        json query_results_json = pImpl->dbController.executeQuery(query);

        pImpl->rHelper.evaluate_response(response_json, query_results_json);
        pImpl->rHelper.finish_response(res, 200, response_json);

    } catch (const std::exception& e) {
        // Handle exception (log, etc.)
        pImpl->respond_with_error(res, response_json, "Failure", fmt::format("Failed: {}", e.what()), -2, 500);
    }
}
