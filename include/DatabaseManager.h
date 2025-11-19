#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <string>
#include <memory>
#include <mutex>
#include <pqxx/pqxx>

class DatabaseManager {
public:
    struct ConnectionConfig {
        std::string host = "localhost";
        int port = 5432;
        std::string database;
        std::string username;
        std::string password;
        int connectTimeout = 10;
    };

    explicit DatabaseManager();
    ~DatabaseManager();

    // Connection management
    bool connect(const ConnectionConfig& config);
    bool isConnected() const;
    void disconnect();
    bool reconnect();

    // Query execution
    pqxx::result executeQuery(const std::string& query);
    pqxx::result executeQuery(const std::string& query, const std::vector<std::string>& params);

    // Connection health
    bool pingConnection();
    std::string getLastError() const;

    // Configuration
    void setConnectionConfig(const ConnectionConfig& config);
    ConnectionConfig getConnectionConfig() const;

private:
    std::unique_ptr<pqxx::connection> connection_;
    ConnectionConfig config_;
    mutable std::mutex connectionMutex_;
    std::string lastError_;
    bool isConnected_;

    bool createConnection();
    void setError(const std::string& error);
    std::string buildConnectionString() const;
};

#endif // DATABASEMANAGER_H