#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <string>
#include <memory>
#include <mutex>
#include <pqxx/pqxx>
#include <QObject>
#include <QTimer>
#include <QDateTime>

// Forward declaration
class ConfigManager;
struct DatabaseConfig;

class DatabaseManager : public QObject {
    Q_OBJECT

public:
    explicit DatabaseManager(ConfigManager* configManager = nullptr, QObject *parent = nullptr);
    ~DatabaseManager();

    // Connection management
    bool connect(const DatabaseConfig& config);
    bool connect();  // Uses config from ConfigManager
    bool connect(const std::string& configFilePath);  // Load config from file
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
    void setConnectionConfig(const DatabaseConfig& config);
    DatabaseConfig getConnectionConfig() const;
    void setConfigManager(ConfigManager* configManager);
    ConfigManager* getConfigManager() const;

    // Configuration file management
    bool loadConfigFromFile(const std::string& configFilePath);
    bool loadConfigFromDefaultLocation();
    bool saveConfigToFile(const std::string& configFilePath) const;
    bool saveConfigToDefaultLocation() const;
    std::string getConfigFilePath() const;

    // Auto-reconnection
    void enableAutoReconnect(bool enabled, int intervalMs = 5000);
    bool isAutoReconnectEnabled() const;

    // Connection status tracking
    QDateTime getConnectionEstablishedTime() const;
    QDateTime getLastConnectionAttemptTime() const;
    int getConnectionAttemptCount() const;

public slots:
    void onConfigChanged();
    void attemptReconnect();

signals:
    void connectionStatusChanged(bool connected);
    void connectionError(const std::string& error);
    void reconnectionAttempt(int attemptCount);
    void configLoaded();

private:
    // Core connection management
    std::unique_ptr<pqxx::connection> connection_;
    DatabaseConfig config_;
    mutable std::mutex connectionMutex_;
    std::string lastError_;
    bool isConnected_;

    // Configuration management
    ConfigManager* configManager_;
    std::string currentConfigFilePath_;

    // Auto-reconnection
    QTimer* reconnectTimer_;
    bool autoReconnectEnabled_;
    int reconnectInterval_;
    int connectionAttemptCount_;

    // Connection status tracking
    QDateTime connectionEstablishedTime_;
    QDateTime lastConnectionAttemptTime_;

    // Core methods
    bool createConnection();
    void setError(const std::string& error);
    std::string buildConnectionString() const;
    bool validateDatabaseConfig() const;
    void updateConnectionStatus(bool connected);
};

// Compatibility alias for old ConnectionConfig struct
using ConnectionConfig = DatabaseConfig;

#endif // DATABASEMANAGER_H