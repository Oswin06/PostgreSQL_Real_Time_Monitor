#include "DatabaseManager.h"
#include <iostream>
#include <chrono>

DatabaseManager::DatabaseManager() : isConnected_(false) {
}

DatabaseManager::~DatabaseManager() {
    disconnect();
}

bool DatabaseManager::connect(const ConnectionConfig& config) {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    config_ = config;
    return createConnection();
}

bool DatabaseManager::isConnected() const {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    return isConnected_ && connection_ && connection_->is_open();
}

void DatabaseManager::disconnect() {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (connection_) {
        connection_.reset();
        isConnected_ = false;
    }
}

bool DatabaseManager::reconnect() {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    disconnect();
    return createConnection();
}

pqxx::result DatabaseManager::executeQuery(const std::string& query) {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (!isConnected()) {
        throw std::runtime_error("Not connected to database");
    }

    try {
        pqxx::work transaction(*connection_);
        pqxx::result result = transaction.exec(query);
        transaction.commit();
        return result;
    } catch (const std::exception& e) {
        setError("Query execution failed: " + std::string(e.what()));
        isConnected_ = false;
        throw;
    }
}

pqxx::result DatabaseManager::executeQuery(const std::string& query, const std::vector<std::string>& params) {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (!isConnected()) {
        throw std::runtime_error("Not connected to database");
    }

    try {
        pqxx::work transaction(*connection_);

        // Prepare statement with parameters
        std::string preparedQuery = query;
        for (size_t i = 0; i < params.size(); ++i) {
            size_t pos = preparedQuery.find("$" + std::to_string(i + 1));
            if (pos != std::string::npos) {
                preparedQuery.replace(pos, 2, transaction.quote(params[i]));
            }
        }

        pqxx::result result = transaction.exec(preparedQuery);
        transaction.commit();
        return result;
    } catch (const std::exception& e) {
        setError("Parameterized query execution failed: " + std::string(e.what()));
        isConnected_ = false;
        throw;
    }
}

bool DatabaseManager::pingConnection() {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (!isConnected() || !connection_) {
        return false;
    }

    try {
        pqxx::work transaction(*connection_);
        transaction.exec("SELECT 1");
        transaction.commit();
        return true;
    } catch (const std::exception&) {
        isConnected_ = false;
        return false;
    }
}

std::string DatabaseManager::getLastError() const {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    return lastError_;
}

void DatabaseManager::setConnectionConfig(const ConnectionConfig& config) {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    config_ = config;
}

DatabaseManager::ConnectionConfig DatabaseManager::getConnectionConfig() const {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    return config_;
}

bool DatabaseManager::createConnection() {
    try {
        std::string connectionString = buildConnectionString();
        connection_ = std::make_unique<pqxx::connection>(connectionString);

        if (connection_->is_open()) {
            isConnected_ = true;
            lastError_.clear();

            // Test the connection
            pqxx::work testTransaction(*connection_);
            testTransaction.exec("SELECT 1");
            testTransaction.commit();

            return true;
        } else {
            setError("Failed to open database connection");
            return false;
        }
    } catch (const std::exception& e) {
        setError("Connection failed: " + std::string(e.what()));
        isConnected_ = false;
        return false;
    }
}

void DatabaseManager::setError(const std::string& error) {
    lastError_ = error;
    std::cerr << "Database Error: " << error << std::endl;
}

std::string DatabaseManager::buildConnectionString() const {
    return "postgresql://" + config_.username + ":" + config_.password +
           "@" + config_.host + ":" + std::to_string(config_.port) +
           "/" + config_.database +
           " connect_timeout=" + std::to_string(config_.connectTimeout);
}