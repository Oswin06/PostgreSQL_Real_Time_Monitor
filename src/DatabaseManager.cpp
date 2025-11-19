#include "DatabaseManager.h"
#include "ConfigManager.h"
#include <iostream>
#include <chrono>

DatabaseManager::DatabaseManager(ConfigManager* configManager, QObject *parent)
    : QObject(parent)
    , isConnected_(false)
    , configManager_(configManager)
    , reconnectTimer_(new QTimer(this))
    , autoReconnectEnabled_(false)
    , reconnectInterval_(5000)
    , connectionAttemptCount_(0)
{
    // Setup auto-reconnection timer
    reconnectTimer_->setSingleShot(true);
    reconnectTimer_->setInterval(reconnectInterval_);
    connect(reconnectTimer_, &QTimer::timeout, this, &DatabaseManager::attemptReconnect);

    // Load initial configuration
    if (configManager_) {
        config_ = configManager_->getDatabaseConfig();

        // Connect to config changes
        connect(configManager_, &ConfigManager::configChanged, this, &DatabaseManager::onConfigChanged);
    }

    // Try to load from default location if no config manager provided
    if (!configManager_) {
        loadConfigFromDefaultLocation();
    }
}

DatabaseManager::~DatabaseManager() {
    disconnect();
}

bool DatabaseManager::connect(const DatabaseConfig& config) {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (!validateDatabaseConfig()) {
        setError("Invalid database configuration");
        return false;
    }

    config_ = config;
    connectionAttemptCount_++;
    lastConnectionAttemptTime_ = QDateTime::currentDateTime();

    bool success = createConnection();
    updateConnectionStatus(success);

    return success;
}

bool DatabaseManager::connect() {
    if (configManager_) {
        DatabaseConfig config = configManager_->getDatabaseConfig();
        return connect(config);
    }

    if (!config_.isValid()) {
        setError("No database configuration available");
        return false;
    }

    return connect(config_);
}

bool DatabaseManager::connect(const std::string& configFilePath) {
    if (loadConfigFromFile(configFilePath)) {
        return connect();
    }
    return false;
}

bool DatabaseManager::isConnected() const {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    return isConnected_ && connection_ && connection_->is_open();
}

void DatabaseManager::disconnect() {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (reconnectTimer_) {
        reconnectTimer_->stop();
    }

    if (connection_) {
        connection_.reset();
        updateConnectionStatus(false);
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
        updateConnectionStatus(false);

        // Trigger auto-reconnect if enabled
        if (autoReconnectEnabled_) {
            QMetaObject::invokeMethod(this, "attemptReconnect", Qt::QueuedConnection);
        }

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
        updateConnectionStatus(false);

        // Trigger auto-reconnect if enabled
        if (autoReconnectEnabled_) {
            QMetaObject::invokeMethod(this, "attemptReconnect", Qt::QueuedConnection);
        }

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
        updateConnectionStatus(false);
        return false;
    }
}

std::string DatabaseManager::getLastError() const {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    return lastError_;
}

void DatabaseManager::setConnectionConfig(const DatabaseConfig& config) {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    config_ = config;

    // Update config manager if available
    if (configManager_) {
        configManager_->setDatabaseConfig(config);
    }
}

DatabaseConfig DatabaseManager::getConnectionConfig() const {
    std::lock_guard<std::mutex> lock(connectionMutex_);

    // Return config from config manager if available and using environment variables
    if (configManager_ && configManager_->useEnvironmentVariables()) {
        return configManager_->getDatabaseConfig();
    }

    return config_;
}

void DatabaseManager::setConfigManager(ConfigManager* configManager) {
    configManager_ = configManager;

    if (configManager_) {
        // Load configuration from config manager
        config_ = configManager_->getDatabaseConfig();

        // Connect to config changes
        connect(configManager_, &ConfigManager::configChanged, this, &DatabaseManager::onConfigChanged);
    }
}

ConfigManager* DatabaseManager::getConfigManager() const {
    return configManager_;
}

bool DatabaseManager::loadConfigFromFile(const std::string& configFilePath) {
    if (!configManager_) {
        qWarning() << "No ConfigManager available to load config file";
        return false;
    }

    QString filePath = QString::fromStdString(configFilePath);
    if (configManager_->loadFromFile(filePath)) {
        currentConfigFilePath_ = configFilePath;
        config_ = configManager_->getDatabaseConfig();

        emit configLoaded();
        qDebug() << "Database configuration loaded from:" << filePath;
        return true;
    }

    qWarning() << "Failed to load database configuration from:" << filePath;
    return false;
}

bool DatabaseManager::loadConfigFromDefaultLocation() {
    if (!configManager_) {
        qWarning() << "No ConfigManager available to load default config";
        return false;
    }

    if (configManager_->loadFromDefaultLocation()) {
        config_ = configManager_->getDatabaseConfig();
        currentConfigFilePath_ = configManager_->getDefaultConfigPath().toStdString();

        emit configLoaded();
        qDebug() << "Database configuration loaded from default location";
        return true;
    }

    qWarning() << "Failed to load database configuration from default location";
    return false;
}

bool DatabaseManager::saveConfigToFile(const std::string& configFilePath) const {
    if (!configManager_) {
        qWarning() << "No ConfigManager available to save config file";
        return false;
    }

    QString filePath = QString::fromStdString(configFilePath);
    return configManager_->saveToFile(filePath);
}

bool DatabaseManager::saveConfigToDefaultLocation() const {
    if (!configManager_) {
        qWarning() << "No ConfigManager available to save default config";
        return false;
    }

    return configManager_->saveToDefaultLocation();
}

std::string DatabaseManager::getConfigFilePath() const {
    if (configManager_) {
        return configManager_->getDefaultConfigPath().toStdString();
    }
    return currentConfigFilePath_;
}

void DatabaseManager::enableAutoReconnect(bool enabled, int intervalMs) {
    autoReconnectEnabled_ = enabled;
    reconnectInterval_ = intervalMs;

    if (reconnectTimer_) {
        reconnectTimer_->setInterval(reconnectInterval_);
        if (!enabled) {
            reconnectTimer_->stop();
        }
    }
}

bool DatabaseManager::isAutoReconnectEnabled() const {
    return autoReconnectEnabled_;
}

QDateTime DatabaseManager::getConnectionEstablishedTime() const {
    return connectionEstablishedTime_;
}

QDateTime DatabaseManager::getLastConnectionAttemptTime() const {
    return lastConnectionAttemptTime_;
}

int DatabaseManager::getConnectionAttemptCount() const {
    return connectionAttemptCount_;
}

void DatabaseManager::onConfigChanged() {
    if (configManager_) {
        DatabaseConfig newConfig = configManager_->getDatabaseConfig();

        // Check if database configuration actually changed
        if (newConfig.host != config_.host || newConfig.port != config_.port ||
            newConfig.database != config_.database || newConfig.username != config_.username) {

            qInfo() << "Database configuration changed, reconnecting...";
            config_ = newConfig;

            // Reconnect with new configuration if currently connected
            if (isConnected()) {
                reconnect();
            }
        }
    }
}

void DatabaseManager::attemptReconnect() {
    if (!autoReconnectEnabled_) {
        return;
    }

    qInfo() << "Attempting database reconnection" << connectionAttemptCount_ << "...";

    emit reconnectionAttempt(connectionAttemptCount_);

    if (reconnect()) {
        qInfo() << "Database reconnection successful";
        connectionAttemptCount_ = 0;
        return;
    }

    // Schedule next reconnection attempt
    if (reconnectTimer_ && autoReconnectEnabled_) {
        reconnectTimer_->start();
    }
}

bool DatabaseManager::createConnection() {
    try {
        std::string connectionString = buildConnectionString();
        connection_ = std::make_unique<pqxx::connection>(connectionString);

        if (connection_->is_open()) {
            isConnected_ = true;
            lastError_.clear();
            connectionEstablishedTime_ = QDateTime::currentDateTime();

            // Test the connection
            pqxx::work testTransaction(*connection_);
            testTransaction.exec("SELECT 1");
            testTransaction.commit();

            qInfo() << "Database connection established successfully";
            return true;
        } else {
            setError("Failed to open database connection");
            return false;
        }
    } catch (const std::exception& e) {
        setError("Connection failed: " + std::string(e.what()));
        return false;
    }
}

void DatabaseManager::setError(const std::string& error) {
    lastError_ = error;
    qWarning() << "Database Error:" << QString::fromStdString(error);

    emit connectionError(error);
}

std::string DatabaseManager::buildConnectionString() const {
    DatabaseConfig config = getConnectionConfig();

    return config.toConnectionString();
}

bool DatabaseManager::validateDatabaseConfig() const {
    DatabaseConfig config = getConnectionConfig();
    return config.isValid();
}

void DatabaseManager::updateConnectionStatus(bool connected) {
    bool wasConnected = isConnected_;
    isConnected_ = connected;

    if (wasConnected != connected) {
        emit connectionStatusChanged(connected);

        if (connected) {
            qInfo() << "Database connected";
        } else {
            qWarning() << "Database disconnected";
        }
    }
}

#include "DatabaseManager.moc"