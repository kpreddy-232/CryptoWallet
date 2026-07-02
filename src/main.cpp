#include <iostream>
#include <string>
#include "sqlite3.h"

int main() {
    sqlite3* db;
    int exit_code = sqlite3_open("../data/wallet.db", &db);
    
    if (exit_code != SQLITE_OK) {
        std::cerr << "Error opening database: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    } 
    
    std::cout << "Successfully connected to SQLite Database!" << std::endl;

    // Define our Database Schema using a Raw String Literal
    std::string sqlSchema = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS assets (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            coin_id TEXT NOT NULL, 
            symbol TEXT NOT NULL,  
            total_quantity REAL DEFAULT 0.0,
            average_buy_price REAL DEFAULT 0.0,
            FOREIGN KEY(user_id) REFERENCES users(id)
        );

        CREATE TABLE IF NOT EXISTS transactions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            coin_id TEXT NOT NULL,
            type TEXT NOT NULL,
            quantity REAL NOT NULL,
            price_at_transaction REAL NOT NULL,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY(user_id) REFERENCES users(id)
        );

        CREATE TABLE IF NOT EXISTS watchlist (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            coin_id TEXT NOT NULL,
            added_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY(user_id) REFERENCES users(id),
            UNIQUE(user_id, coin_id) 
        );
    )";

    // Execute the SQL schema
    char* errMsg;
    exit_code = sqlite3_exec(db, sqlSchema.c_str(), nullptr, 0, &errMsg);

    if (exit_code != SQLITE_OK) {
        std::cerr << "SQL Error creating tables: " << errMsg << std::endl;
        sqlite3_free(errMsg); // Free the memory allocated for the error message
    } else {
        std::cout << "All database tables successfully created/verified!" << std::endl;
    }

    // Always close the connection
    sqlite3_close(db);
    return 0;
}
