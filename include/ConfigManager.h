#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <string>
#include <map>
#include <vector>
#include <QVariant>
#include <QString>
#include <QSettings>
#include <QDir>
#include <QStandardPaths>
#include <QTextStream>
#include <QFile>
#include <QDebug>

struct DatabaseConfig {
    std::string host = "localhost";
    int port = 5432;
    std::string database = "postgres";
    std::string username = "postgres";
    std::string password = "";
    int connectTimeout = 10;
    std::string sslMode = "prefer";
    std::string applicationName = "PostgreSQL-Monitor";
    bool useEnvironmentVariables = false;
    std::string configFilePath = "";

    bool isValid() const {
        return !host.empty() && !database.empty() && !username.empty() && port > 0;
    }

    std::string toConnectionString() const {
        return "postgresql://" + username + ":" + password +
               "@" + host + ":" + std::to_string(port) +
               "/" + database +
               " connect_timeout=" + std::to_string(connectTimeout) +
               " sslmode=" + sslMode +
               " application_name=" + applicationName;
    }
};

struct AlertConfig {
    bool duplicateDetectionEnabled = true;
    int duplicateTimeWindow = 30;
    int maxAlerts = 1000;
    bool showTimestamps = true;
    bool autoScroll = true;
    QString dateFormat = "hh:mm:ss";
    QString timeFormat = "Just now;X seconds ago;X minutes ago;X hours ago;MMM dd, yyyy hh:mm:ss";
};

struct QueryConfig {
    std::string queriesFilePath = "config/queries.conf";
    int executionInterval = 1000;  // milliseconds
    int maxConcurrentQueries = 5;
    bool startMonitoringOnStartup = false;
    bool enableQueryLogging = false;
};

struct UIConfig {
    QString windowTitle = "PostgreSQL Monitor - Alert Dashboard";
    QSize windowSize = QSize(1200, 800);
    QPoint windowPosition = QPoint(100, 100);
    bool showFilterPanel = true;
    bool showDetailsPanel = true;
    QString alertColorCritical = "#d32f2f";
    QString alertColorWarning = "#f57c00";
    QString alertColorInfo = "#388e3c";
    QString alertFontFamily = "Segoe UI, Arial, sans-serif";
    int alertFontSize = 10;
    bool darkTheme = false;
};

class ConfigManager {
public:
    explicit ConfigManager();
    ~ConfigManager();

    // Configuration file management
    bool loadFromFile(const QString& filePath);
    bool saveToFile(const QString& filePath);
    bool loadFromDefaultLocation();
    bool saveToDefaultLocation();
    QString getDefaultConfigPath() const;

    // Database configuration
    DatabaseConfig getDatabaseConfig() const;
    void setDatabaseConfig(const DatabaseConfig& config);
    void updateDatabaseConfig(const std::string& host, int port, const std::string& database,
                             const std::string& username, const std::string& password,
                             int timeout = 10, const std::string& sslMode = "prefer");

    // Alert configuration
    AlertConfig getAlertConfig() const;
    void setAlertConfig(const AlertConfig& config);

    // Query configuration
    QueryConfig getQueryConfig() const;
    void setQueryConfig(const QueryConfig& config);

    // UI configuration
    UIConfig getUIConfig() const;
    void setUIConfig(const UIConfig& config);

    // Environment variables support
    void loadFromEnvironmentVariables();
    bool useEnvironmentVariables() const;
    void setUseEnvironmentVariables(bool use);

    // Configuration validation
    bool validateDatabaseConfig() const;
    bool validateConfigFile(const QString& filePath) const;

    // Configuration reset
    void resetToDefaults();
    void resetDatabaseConfigToDefaults();
    void resetAlertConfigToDefaults();
    void resetQueryConfigToDefaults();
    void resetUIConfigToDefaults();

    // Configuration templates
    DatabaseConfig getDatabaseConfigTemplate() const;
    void applyDatabaseTemplate(const std::string& templateName);

    // Configuration migration
    bool migrateFromOldFormat(const QString& oldConfigPath);
    bool upgradeConfigFormat();

    // Configuration backup/restore
    bool backupConfig(const QString& backupPath) const;
    bool restoreConfig(const QString& backupPath);

    // Configuration debugging
    void printConfiguration() const;
    QString getConfigurationSummary() const;

    // Configuration watchers
    void setConfigChangedCallback(std::function<void()> callback);
    void notifyConfigChanged();

    // Configuration utilities
    static QString escapeConfigValue(const QString& value);
    static QString unescapeConfigValue(const QString& value);
    static std::vector<QString> getRecentConfigFiles();
    static void addToRecentConfigFiles(const QString& filePath);

private:
    // Configuration parsing
    bool parseConfigFile(const QString& content);
    QString formatConfigFile() const;
    QString escapeValue(const QString& value) const;
    QString unescapeValue(const QString& value) const;

    // Section parsers
    bool parseDatabaseSection(const QStringList& lines, int& lineNumber);
    bool parseAlertSection(const QStringList& lines, int& lineNumber);
    bool parseQuerySection(const QStringList& lines, int& lineNumber);
    bool parseUISection(const QStringList& lines, int& lineNumber);
    bool parseGeneralSection(const QStringList& lines, int& lineNumber);

    // Section formatters
    QStringList formatDatabaseSection() const;
    QStringList formatAlertSection() const;
    QStringList formatQuerySection() const;
    QStringList formatUISection() const;
    QStringList formatGeneralSection() const;

    // Configuration validation helpers
    bool validatePort(int port) const;
    bool validateTimeout(int timeout) const;
    bool validateSSLMode(const std::string& sslMode) const;
    bool validateColor(const QString& color) const;

    // Configuration defaults
    DatabaseConfig getDefaultDatabaseConfig() const;
    AlertConfig getDefaultAlertConfig() const;
    QueryConfig getDefaultQueryConfig() const;
    UIConfig getDefaultUIConfig() const;

    // Configuration templates
    std::map<std::string, DatabaseConfig> getDatabaseTemplates() const;

    // Member variables
    DatabaseConfig databaseConfig_;
    AlertConfig alertConfig_;
    QueryConfig queryConfig_;
    UIConfig uiConfig_;

    QString currentConfigPath_;
    bool useEnvironmentVariables_;
    bool configChanged_;

    std::function<void()> configChangedCallback_;

    // Constants
    static const QString CONFIG_FILE_NAME;
    static const QString CONFIG_SECTION_MARKER;
    static const QString CONFIG_COMMENT_PREFIX;
    static const QString CONFIG_KEY_VALUE_SEPARATOR;
    static const int MAX_RECENT_CONFIG_FILES = 5;
};

// Configuration utility functions
namespace ConfigUtils {
    QString findConfigFile(const QStringList& searchPaths);
    bool ensureConfigDirectory(const QString& configPath);
    QString getConfigDirectory();
    QString getHomeConfigPath();
    QString getApplicationDataPath();
    bool validateConfigSyntax(const QString& content);
    std::vector<QString> parseMultiLineValue(const QString& content, int& lineIndex);
}

#endif // CONFIGMANAGER_H