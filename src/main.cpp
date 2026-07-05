#include <iostream>
#include <string>
#include <unordered_map>
#include <random>
#include <vector>
#include <array>
#include <memory>
#include "crow_all.h"
#include "sqlite3.h"
#include "picosha2.h"

// ----------------------------------------------------
// GLOBAL DATA (In-Memory)
// ----------------------------------------------------
// Maps a session token (e.g., "A1b2...") to a username (e.g., "pranav2")
std::unordered_map<std::string, std::string> active_sessions;

// Helper function to generate a secure 32-character random token
std::string generate_session_token() {
    const std::string chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> distribution(0, chars.size() - 1);
    
    std::string token;
    for (int i = 0; i < 32; ++i) {
        token += chars[distribution(generator)];
    }
    return token;
}

int main() {
    sqlite3* db;
    if (sqlite3_open("../data/wallet.db", &db) != SQLITE_OK) {
        std::cerr << "Failed to open database!" << std::endl;
        return 1;
    }
    std::cout << "Database connected successfully." << std::endl;

    crow::SimpleApp app;

    // ----------------------------------------------------
    // REGISTRATION ROUTE
    // ----------------------------------------------------
    CROW_ROUTE(app, "/api/register").methods(crow::HTTPMethod::Post)([&db](const crow::request& req) {
        crow::json::wvalue response;
        auto body = crow::json::load(req.body);
        if (!body) {
            response["status"] = "error";
            response["message"] = "Invalid JSON format";
            return crow::response(400, response);
        }

        std::string username = body["username"].s();
        std::string password = body["password"].s(); 

        std::string hashed_password;
        picosha2::hash256_hex_string(password, hashed_password);
        
        std::string sql = "INSERT INTO users (username, password_hash) VALUES ('" + username + "', '" + hashed_password + "');";
        char* errMsg;
        if (sqlite3_exec(db, sql.c_str(), nullptr, 0, &errMsg) != SQLITE_OK) {
            response["status"] = "error";
            response["message"] = "Username already exists or database error.";
            sqlite3_free(errMsg);
            return crow::response(409, response);
        }

        response["status"] = "success";
        response["message"] = "User registered securely!";
        return crow::response(201, response);
    });

    // ----------------------------------------------------
    // LOGIN ROUTE
    // ----------------------------------------------------
    CROW_ROUTE(app, "/api/login").methods(crow::HTTPMethod::Post)([&db](const crow::request& req) {
        crow::json::wvalue response;
        auto body = crow::json::load(req.body);
        if (!body) {
            response["status"] = "error";
            response["message"] = "Invalid JSON format";
            return crow::response(400, response);
        }

        std::string username = body["username"].s();
        std::string password = body["password"].s();

        std::string hashed_attempt;
        picosha2::hash256_hex_string(password, hashed_attempt);

        std::string sql = "SELECT password_hash FROM users WHERE username = '" + username + "';";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            response["status"] = "error";
            response["message"] = "Database error";
            return crow::response(500, response);
        }

        int step = sqlite3_step(stmt);
        if (step == SQLITE_ROW) {
            std::string saved_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            
            if (saved_hash == hashed_attempt) {
                // SUCCESS! Generate a token and save the session.
                std::string token = generate_session_token();
                active_sessions[token] = username;

                response["status"] = "success";
                response["message"] = "Login successful!";
                response["token"] = token;
                
                sqlite3_finalize(stmt); 
                return crow::response(200, response); 
            }
        }
        
        sqlite3_finalize(stmt);
        response["status"] = "error";
        response["message"] = "Invalid username or password";
        return crow::response(401, response); 
    });

    // ----------------------------------------------------
    // PORTFOLIO ROUTE (PROTECTED)
    // ----------------------------------------------------
    CROW_ROUTE(app, "/api/portfolio").methods(crow::HTTPMethod::Get)([&db](const crow::request& req) {
        crow::json::wvalue response;

        // 1. Security Check: Grab the token from the "Authorization" header
        std::string auth_header = req.get_header_value("Authorization");
        
        // 2. Validate token against our in-memory map
        if (auth_header.empty() || active_sessions.find(auth_header) == active_sessions.end()) {
            response["status"] = "error";
            response["message"] = "Unauthorized. Please log in first.";
            return crow::response(401, response); 
        }

        // 3. Token is valid! Let's get the username associated with it
        std::string current_username = active_sessions[auth_header];

        // 4. Query the DB to get their unique 'user_id' (we need this to look up their assets)
        std::string user_sql = "SELECT id FROM users WHERE username = '" + current_username + "';";
        sqlite3_stmt* user_stmt;
        int user_id = -1;
        
        if (sqlite3_prepare_v2(db, user_sql.c_str(), -1, &user_stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(user_stmt) == SQLITE_ROW) {
                user_id = sqlite3_column_int(user_stmt, 0);
            }
        }
        sqlite3_finalize(user_stmt);

        // 5. Query the 'assets' table to find all coins owned by this user_id
        std::string assets_sql = "SELECT symbol, total_quantity, average_buy_price FROM assets WHERE user_id = " + std::to_string(user_id) + ";";
        sqlite3_stmt* assets_stmt;
        
        std::vector<crow::json::wvalue> assets_list; // This vector will hold JSON objects for each coin

        if (sqlite3_prepare_v2(db, assets_sql.c_str(), -1, &assets_stmt, nullptr) == SQLITE_OK) {
            // Loop through EVERY row returned by the database
            while (sqlite3_step(assets_stmt) == SQLITE_ROW) {
                crow::json::wvalue asset;
                asset["symbol"] = reinterpret_cast<const char*>(sqlite3_column_text(assets_stmt, 0));
                asset["quantity"] = sqlite3_column_double(assets_stmt, 1);
                asset["avg_price"] = sqlite3_column_double(assets_stmt, 2);
                
                assets_list.push_back(std::move(asset)); // Add it to our JSON array
            }
        }
        sqlite3_finalize(assets_stmt);

        // 6. Return the secure portfolio data to the user!
        response["status"] = "success";
        response["username"] = current_username;
        response["portfolio"] = std::move(assets_list); // Crow automatically converts this vector to a JSON array!
        
        return crow::response(200, response);
    });

    // ----------------------------------------------------
    // ADD TRANSACTION ROUTE (PROTECTED)
    // ----------------------------------------------------
    CROW_ROUTE(app, "/api/transaction").methods(crow::HTTPMethod::Post)([&db](const crow::request& req) {
        crow::json::wvalue response;

        // 1. Security Check
        std::string auth_header = req.get_header_value("Authorization");
        if (auth_header.empty() || active_sessions.find(auth_header) == active_sessions.end()) {
            response["status"] = "error";
            response["message"] = "Unauthorized. Please log in first.";
            return crow::response(401, response); 
        }

        std::string current_username = active_sessions[auth_header];
        
        // 2. Parse JSON Data
        auto body = crow::json::load(req.body);
        if (!body) {
            response["status"] = "error";
            response["message"] = "Invalid JSON format";
            return crow::response(400, response);
        }

        std::string coin_id = body["coin_id"].s();
        std::string symbol = body["symbol"].s();
        std::string type = body["type"].s(); // "BUY" or "SELL"
        double quantity = body["quantity"].d();
        double price = body["price"].d();

        // 3. Get User ID
        std::string user_sql = "SELECT id FROM users WHERE username = '" + current_username + "';";
        sqlite3_stmt* user_stmt;
        int user_id = -1;
        if (sqlite3_prepare_v2(db, user_sql.c_str(), -1, &user_stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(user_stmt) == SQLITE_ROW) {
                user_id = sqlite3_column_int(user_stmt, 0);
            }
        }
        sqlite3_finalize(user_stmt);

        // 4. Insert Transaction Record
        std::string trans_sql = "INSERT INTO transactions (user_id, coin_id, type, quantity, price_at_transaction) VALUES (" 
            + std::to_string(user_id) + ", '" + coin_id + "', '" + type + "', " + std::to_string(quantity) + ", " + std::to_string(price) + ");";
        char* errMsg;
        sqlite3_exec(db, trans_sql.c_str(), nullptr, 0, &errMsg);

        // 5. Update Assets Table (Simple logic for MVP)
        std::string check_asset_sql = "SELECT total_quantity FROM assets WHERE user_id = " + std::to_string(user_id) + " AND coin_id = '" + coin_id + "';";
        sqlite3_stmt* check_stmt;
        bool owns_asset = false;
        double current_qty = 0.0;
        
        if (sqlite3_prepare_v2(db, check_asset_sql.c_str(), -1, &check_stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(check_stmt) == SQLITE_ROW) {
                owns_asset = true;
                current_qty = sqlite3_column_double(check_stmt, 0);
            }
        }
        sqlite3_finalize(check_stmt);

        if (owns_asset) {
            double new_qty = (type == "BUY") ? (current_qty + quantity) : (current_qty - quantity);
            std::string update_sql = "UPDATE assets SET total_quantity = " + std::to_string(new_qty) + " WHERE user_id = " + std::to_string(user_id) + " AND coin_id = '" + coin_id + "';";
            sqlite3_exec(db, update_sql.c_str(), nullptr, 0, &errMsg);
        } else if (type == "BUY") {
            std::string insert_asset = "INSERT INTO assets (user_id, coin_id, symbol, total_quantity, average_buy_price) VALUES (" 
                + std::to_string(user_id) + ", '" + coin_id + "', '" + symbol + "', " + std::to_string(quantity) + ", " + std::to_string(price) + ");";
            sqlite3_exec(db, insert_asset.c_str(), nullptr, 0, &errMsg);
        }

        response["status"] = "success";
        response["message"] = "Transaction recorded successfully!";
        return crow::response(200, response);
    });

    // ----------------------------------------------------
    // TRANSACTION HISTORY ROUTE (PROTECTED)
    // ----------------------------------------------------
    CROW_ROUTE(app, "/api/history").methods(crow::HTTPMethod::Get)([&db](const crow::request& req) {
        crow::json::wvalue response;

        // 1. Security Check
        std::string auth_header = req.get_header_value("Authorization");
        if (auth_header.empty() || active_sessions.find(auth_header) == active_sessions.end()) {
            response["status"] = "error";
            response["message"] = "Unauthorized. Please log in first.";
            return crow::response(401, response); 
        }

        std::string current_username = active_sessions[auth_header];

        // 2. Get User ID
        std::string user_sql = "SELECT id FROM users WHERE username = '" + current_username + "';";
        sqlite3_stmt* user_stmt;
        int user_id = -1;
        if (sqlite3_prepare_v2(db, user_sql.c_str(), -1, &user_stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(user_stmt) == SQLITE_ROW) {
                user_id = sqlite3_column_int(user_stmt, 0);
            }
        }
        sqlite3_finalize(user_stmt);

        // 3. Query the 'transactions' table for this user
        std::string hist_sql = "SELECT coin_id, type, quantity, price_at_transaction, timestamp FROM transactions WHERE user_id = " + std::to_string(user_id) + " ORDER BY timestamp DESC;";
        sqlite3_stmt* hist_stmt;
        std::vector<crow::json::wvalue> history_list;

        if (sqlite3_prepare_v2(db, hist_sql.c_str(), -1, &hist_stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(hist_stmt) == SQLITE_ROW) {
                crow::json::wvalue tx;
                tx["coin_id"] = reinterpret_cast<const char*>(sqlite3_column_text(hist_stmt, 0));
                tx["type"] = reinterpret_cast<const char*>(sqlite3_column_text(hist_stmt, 1));
                tx["quantity"] = sqlite3_column_double(hist_stmt, 2);
                tx["price"] = sqlite3_column_double(hist_stmt, 3);
                tx["timestamp"] = reinterpret_cast<const char*>(sqlite3_column_text(hist_stmt, 4));
                
                history_list.push_back(std::move(tx));
            }
        }
        sqlite3_finalize(hist_stmt);

        // 4. Return history data
        response["status"] = "success";
        response["history"] = std::move(history_list);
        
        return crow::response(200, response);
    });

    // ----------------------------------------------------
    // HELPER: FETCH LIVE PRICES (Using Windows cURL)
    // ----------------------------------------------------
    // This lambda uses _popen to run a terminal command silently and capture the output!
    auto fetch_live_price = [](const std::string& coin_id) -> std::string {
        std::string cmd = "curl -s \"https://api.coingecko.com/api/v3/simple/price?ids=" + coin_id + "&vs_currencies=usd\"";
        std::array<char, 128> buffer;
        std::string result;
        
        std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd.c_str(), "r"), _pclose);
        if (!pipe) return "{}";
        
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        return result;
    };

    // ----------------------------------------------------
    // MARKET DATA ROUTE (Live Pricing)
    // ----------------------------------------------------
    // Notice the <string> in the route? This allows us to pass any coin (e.g., /api/market/ethereum)
    CROW_ROUTE(app, "/api/market/<string>").methods(crow::HTTPMethod::Get)([&fetch_live_price](const std::string& coin_id) {
        crow::json::wvalue response;
        
        // 1. Call our helper function to ping CoinGecko
        std::string raw_json = fetch_live_price(coin_id);
        
        // 2. Parse the raw text string into a Crow JSON object
        auto parsed_price = crow::json::load(raw_json);
        
        // 3. Extract the price if it exists
        if (parsed_price && parsed_price.has(coin_id) && parsed_price[coin_id].has("usd")) {
            double current_price = parsed_price[coin_id]["usd"].d();
            response["status"] = "success";
            response["coin"] = coin_id;
            response["price_usd"] = current_price;
            return crow::response(200, response);
        }
        
        response["status"] = "error";
        response["message"] = "Failed to fetch live price or invalid coin ID.";
        return crow::response(400, response);
    });

    // ----------------------------------------------------
    // WATCHLIST ROUTES
    // ----------------------------------------------------
    
    // ADD TO WATCHLIST
    CROW_ROUTE(app, "/api/watchlist").methods(crow::HTTPMethod::Post)([&db](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body) return crow::response(400);

        std::string auth_header = req.get_header_value("Authorization");
        if (auth_header.empty() || active_sessions.find(auth_header) == active_sessions.end()) {
            return crow::response(401);
        }

        std::string current_username = active_sessions[auth_header];
        std::string coin_id = body["coin_id"].s();

        // Get user_id
        std::string user_sql = "SELECT id FROM users WHERE username = '" + current_username + "';";
        sqlite3_stmt* user_stmt;
        int user_id = -1;
        if (sqlite3_prepare_v2(db, user_sql.c_str(), -1, &user_stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(user_stmt) == SQLITE_ROW) user_id = sqlite3_column_int(user_stmt, 0);
        }
        sqlite3_finalize(user_stmt);

        // Insert into watchlist
        std::string sql = "INSERT OR IGNORE INTO watchlist (user_id, coin_id) VALUES (" + std::to_string(user_id) + ", '" + coin_id + "');";
        char* err_msg = nullptr;
        if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err_msg) == SQLITE_OK) {
            crow::json::wvalue res;
            res["status"] = "success";
            return crow::response(200, res);
        } else {
            return crow::response(500);
        }
    });

    // FETCH WATCHLIST
    CROW_ROUTE(app, "/api/watchlist").methods(crow::HTTPMethod::Get)([&db](const crow::request& req) {
        std::string auth_header = req.get_header_value("Authorization");
        if (auth_header.empty() || active_sessions.find(auth_header) == active_sessions.end()) {
            return crow::response(401);
        }

        std::string current_username = active_sessions[auth_header];
        
        // Get user_id
        std::string user_sql = "SELECT id FROM users WHERE username = '" + current_username + "';";
        sqlite3_stmt* user_stmt;
        int user_id = -1;
        if (sqlite3_prepare_v2(db, user_sql.c_str(), -1, &user_stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(user_stmt) == SQLITE_ROW) user_id = sqlite3_column_int(user_stmt, 0);
        }
        sqlite3_finalize(user_stmt);

        // Get watchlist
        std::string sql = "SELECT coin_id FROM watchlist WHERE user_id = " + std::to_string(user_id) + ";";
        sqlite3_stmt* stmt;
        std::vector<std::string> watchlist;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                watchlist.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
            }
        }
        sqlite3_finalize(stmt);

        crow::json::wvalue res;
        res["watchlist"] = watchlist;
        return crow::response(200, res);
    });

    std::cout << "Starting Server on port 8080..." << std::endl;
    app.port(8080).multithreaded().run();

    sqlite3_close(db);
    return 0;
}