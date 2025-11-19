#include "ConfigManager.h"
#include <QDir>
#include <QStandardPaths>
#include <QTextStream>
#include <QRegularExpression>
#include <QSettings>
#include <QFileInfo>
#include <QCoreApplication>
#include <QDebug>

// Static constants
const QString ConfigManager::CONFIG_FILE_NAME = "config.txt";
const QString ConfigManager::CONFIG_SECTION_MARKER = "[";
const QString ConfigManager::CONFIG_COMMENT_PREFIX = "#";
const QString ConfigManager::CONFIG_KEY_VALUE_SEPARATOR = "=";

ConfigManager::ConfigManager()
    : useEnvironmentVariables_(false)
    , configChanged_(false)
{
    // Initialize with default values
    databaseConfig_ = getDefaultDatabaseConfig();
    alertConfig_ = getDefaultAlertConfig();
    queryConfig_ = getDefaultQueryConfig();
    uiConfig_ = getDefaultUIConfig();

    // Try to load from default location
    loadFromDefaultLocation();
}

ConfigManager::~ConfigManager() = default;

bool ConfigManager::loadFromFile(const QString& filePath) {
    if (!QFileInfo::exists(filePath)) {
        qWarning() << "Config file does not exist:" << filePath;
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot open config file for reading:" << filePath;
        return false;
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    bool success = parseConfigFile(content);
    if (success) {
        currentConfigPath_ = filePath;
        qInfo() << "Successfully loaded config from:" << filePath;

        // Add to recent files
        addToRecentConfigFiles(filePath);
    }

    return success;
}

bool ConfigManager::saveToFile(const QString& filePath) {
    QString directory = QFileInfo(filePath).absolutePath();
    QDir dir;
    if (!dir.mkpath(directory)) {
        qWarning() << "Cannot create config directory:" << directory;
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Cannot open config file for writing:" << filePath;
        return false;
    }

    QTextStream out(&file);
    QString content = formatConfigFile();
    out << content;
    file.close();

    currentConfigPath_ = filePath;
    configChanged_ = false;

    qInfo() << "Successfully saved config to:" << filePath;

    // Add to recent files
    addToRecentConfigFiles(filePath);

    return true;
}

bool ConfigManager::loadFromDefaultLocation() {
    QString defaultPath = getDefaultConfigPath();

    // Try to load from default path
    if (loadFromFile(defaultPath)) {
        return true;
    }

    // If default doesn't exist, try environment config
    loadFromEnvironmentVariables();

    // Create default config file if it doesn't exist
    if (!QFileInfo::exists(defaultPath)) {
        qInfo() << "Creating default config file:" << defaultPath;
        return saveToDefaultLocation();
    }

    return false;
}

bool ConfigManager::saveToDefaultLocation() {
    QString defaultPath = getDefaultConfigPath();
    return saveToFile(defaultPath);
}

QString ConfigManager::getDefaultConfigPath() const {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir dir;
    dir.mkpath(configDir);
    return configDir + "/" + CONFIG_FILE_NAME;
}

DatabaseConfig ConfigManager::getDatabaseConfig() const {
    if (useEnvironmentVariables_) {
        DatabaseConfig config = databaseConfig_;

        // Override with environment variables if they exist
        if (qgetenv("PGHOST").isEmpty() == false) {
            config.host = qgetenv("PGHOST").toStdString();
        }
        if (qgetenv("PGPORT").isEmpty() == false) {
            config.port = qgetenv("PGPORT").toInt();
        }
        if (qgetenv("PGDATABASE").isEmpty() == false) {
            config.database = qgetenv("PGDATABASE").toStdString();
        }
        if (qgetenv("PGUSER").isEmpty() == false) {
            config.username = qgetenv("PGUSER").toStdString();
        }
        if (qgetenv("PGPASSWORD").isEmpty() == false) {
            config.password = qgetenv("PGPASSWORD").toStdString();
        }

        return config;
    }

    return databaseConfig_;
}

void ConfigManager::setDatabaseConfig(const DatabaseConfig& config) {
    databaseConfig_ = config;
    configChanged_ = true;
    notifyConfigChanged();
}

void ConfigManager::updateDatabaseConfig(const std::string& host, int port, const std::string& database,
                                         const std::string& username, const std::string& password,
                                         int timeout, const std::string& sslMode) {
    databaseConfig_.host = host;
    databaseConfig_.port = port;
    databaseConfig_.database = database;
    databaseConfig_.username = username;
    databaseConfig_.password = password;
    databaseConfig_.connectTimeout = timeout;
    databaseConfig_.sslMode = sslMode;

    configChanged_ = true;
    notifyConfigChanged();
}

AlertConfig ConfigManager::getAlertConfig() const {
    return alertConfig_;
}

void ConfigManager::setAlertConfig(const AlertConfig& config) {
    alertConfig_ = config;
    configChanged_ = true;
    notifyConfigChanged();
}

QueryConfig ConfigManager::getQueryConfig() const {
    return queryConfig_;
}

void ConfigManager::setQueryConfig(const QueryConfig& config) {
    queryConfig_ = config;
    configChanged_ = true;
    notifyConfigChanged();
}

UIConfig ConfigManager::getUIConfig() const {
    return uiConfig_;
}

void ConfigManager::setUIConfig(const UIConfig& config) {
    uiConfig_ = config;
    configChanged_ = true;
    notifyConfigChanged();
}

void ConfigManager::loadFromEnvironmentVariables() {
    useEnvironmentVariables_ = true;
    qInfo() << "Using environment variables for database configuration";
}

bool ConfigManager::useEnvironmentVariables() const {
    return useEnvironmentVariables_;
}

void ConfigManager::setUseEnvironmentVariables(bool use) {
    useEnvironmentVariables_ = use;
    configChanged_ = true;
    notifyConfigChanged();
}

bool ConfigManager::parseConfigFile(const QString& content) {
    QStringList lines = content.split('\n');
    int lineNumber = 0;

    while (lineNumber < lines.size()) {
        QString line = lines[lineNumber].trimmed();

        // Skip empty lines and comments
        if (line.isEmpty() || line.startsWith(CONFIG_COMMENT_PREFIX)) {
            lineNumber++;
            continue;
        }

        // Check for section headers
        if (line.startsWith(CONFIG_SECTION_MARKER) && line.endsWith("]")) {
            QString sectionName = line.mid(1, line.length() - 2).trimmed();
            lineNumber++;

            if (sectionName == "Database") {
                if (!parseDatabaseSection(lines, lineNumber)) {
                    return false;
                }
            } else if (sectionName == "Alerts") {
                if (!parseAlertSection(lines, lineNumber)) {
                    return false;
                }
            } else if (sectionName == "Queries") {
                if (!parseQuerySection(lines, lineNumber)) {
                    return false;
                }
            } else if (sectionName == "UI") {
                if (!parseUISection(lines, lineNumber)) {
                    return false;
                }
            } else if (sectionName == "General") {
                if (!parseGeneralSection(lines, lineNumber)) {
                    return false;
                }
            } else {
                qWarning() << "Unknown config section:" << sectionName;
                // Skip to next section
                while (lineNumber < lines.size()) {
                    QString nextLine = lines[lineNumber].trimmed();
                    if (nextLine.startsWith(CONFIG_SECTION_MARKER)) {
                        break;
                    }
                    lineNumber++;
                }
            }
        } else {
            // Try to parse as key=value in General section
            if (!parseGeneralSection(lines, lineNumber)) {
                return false;
            }
        }
    }

    return true;
}

bool ConfigManager::parseDatabaseSection(const QStringList& lines, int& lineNumber) {
    while (lineNumber < lines.size()) {
        QString line = lines[lineNumber].trimmed();

        // Stop at next section or end of file
        if (line.startsWith(CONFIG_SECTION_MARKER) || line.isEmpty()) {
            break;
        }

        if (line.startsWith(CONFIG_COMMENT_PREFIX)) {
            lineNumber++;
            continue;
        }

        int separatorPos = line.indexOf(CONFIG_KEY_VALUE_SEPARATOR);
        if (separatorPos > 0) {
            QString key = line.left(separatorPos).trimmed();
            QString value = line.mid(separatorPos + 1).trimmed();

            // Remove quotes if present
            if (value.startsWith("\"") && value.endsWith("\"")) {
                value = value.mid(1, value.length() - 2);
            }

            if (key == "host") {
                databaseConfig_.host = value.toStdString();
            } else if (key == "port") {
                databaseConfig_.port = value.toInt();
            } else if (key == "database") {
                databaseConfig_.database = value.toStdString();
            } else if (key == "username") {
                databaseConfig_.username = value.toStdString();
            } else if (key == "password") {
                databaseConfig_.password = value.toStdString();
            } else if (key == "connect_timeout") {
                databaseConfig_.connectTimeout = value.toInt();
            } else if (key == "sslmode") {
                databaseConfig_.sslMode = value.toStdString();
            } else if (key == "application_name") {
                databaseConfig_.applicationName = value.toStdString();
            } else if (key == "use_environment_variables") {
                useEnvironmentVariables_ = (value.toLower() == "true" || value == "1");
            } else {
                qWarning() << "Unknown database config key:" << key;
            }
        }

        lineNumber++;
    }

    return true;
}

bool ConfigManager::parseAlertSection(const QStringList& lines, int& lineNumber) {
    while (lineNumber < lines.size()) {
        QString line = lines[lineNumber].trimmed();

        if (line.startsWith(CONFIG_SECTION_MARKER) || line.isEmpty()) {
            break;
        }

        if (line.startsWith(CONFIG_COMMENT_PREFIX)) {
            lineNumber++;
            continue;
        }

        int separatorPos = line.indexOf(CONFIG_KEY_VALUE_SEPARATOR);
        if (separatorPos > 0) {
            QString key = line.left(separatorPos).trimmed();
            QString value = line.mid(separatorPos + 1).trimmed();

            if (value.startsWith("\"") && value.endsWith("\"")) {
                value = value.mid(1, value.length() - 2);
            }

            if (key == "duplicate_detection_enabled") {
                alertConfig_.duplicateDetectionEnabled = (value.toLower() == "true" || value == "1");
            } else if (key == "duplicate_time_window") {
                alertConfig_.duplicateTimeWindow = value.toInt();
            } else if (key == "max_alerts") {
                alertConfig_.maxAlerts = value.toInt();
            } else if (key == "show_timestamps") {
                alertConfig_.showTimestamps = (value.toLower() == "true" || value == "1");
            } else if (key == "auto_scroll") {
                alertConfig_.autoScroll = (value.toLower() == "true" || value == "1");
            } else if (key == "date_format") {
                alertConfig_.dateFormat = value;
            } else if (key == "time_format") {
                alertConfig_.timeFormat = value;
            } else {
                qWarning() << "Unknown alert config key:" << key;
            }
        }

        lineNumber++;
    }

    return true;
}

bool ConfigManager::parseQuerySection(const QStringList& lines, int& lineNumber) {
    while (lineNumber < lines.size()) {
        QString line = lines[lineNumber].trimmed();

        if (line.startsWith(CONFIG_SECTION_MARKER) || line.isEmpty()) {
            break;
        }

        if (line.startsWith(CONFIG_COMMENT_PREFIX)) {
            lineNumber++;
            continue;
        }

        int separatorPos = line.indexOf(CONFIG_KEY_VALUE_SEPARATOR);
        if (separatorPos > 0) {
            QString key = line.left(separatorPos).trimmed();
            QString value = line.mid(separatorPos + 1).trimmed();

            if (value.startsWith("\"") && value.endsWith("\"")) {
                value = value.mid(1, value.length() - 2);
            }

            if (key == "queries_file_path") {
                queryConfig_.queriesFilePath = value.toStdString();
            } else if (key == "execution_interval") {
                queryConfig_.executionInterval = value.toInt();
            } else if (key == "max_concurrent_queries") {
                queryConfig_.maxConcurrentQueries = value.toInt();
            } else if (key == "start_monitoring_on_startup") {
                queryConfig_.startMonitoringOnStartup = (value.toLower() == "true" || value == "1");
            } else if (key == "enable_query_logging") {
                queryConfig_.enableQueryLogging = (value.toLower() == "true" || value == "1");
            } else {
                qWarning() << "Unknown query config key:" << key;
            }
        }

        lineNumber++;
    }

    return true;
}

bool ConfigManager::parseUISection(const QStringList& lines, int& lineNumber) {
    while (lineNumber < lines.size()) {
        QString line = lines[lineNumber].trimmed();

        if (line.startsWith(CONFIG_SECTION_MARKER) || line.isEmpty()) {
            break;
        }

        if (line.startsWith(CONFIG_COMMENT_PREFIX)) {
            lineNumber++;
            continue;
        }

        int separatorPos = line.indexOf(CONFIG_KEY_VALUE_SEPARATOR);
        if (separatorPos > 0) {
            QString key = line.left(separatorPos).trimmed();
            QString value = line.mid(separatorPos + 1).trimmed();

            if (value.startsWith("\"") && value.endsWith("\"")) {
                value = value.mid(1, value.length() - 2);
            }

            if (key == "window_title") {
                uiConfig_.windowTitle = value;
            } else if (key == "window_size") {
                QStringList sizeParts = value.split('x');
                if (sizeParts.size() == 2) {
                    uiConfig_.windowSize = QSize(sizeParts[0].toInt(), sizeParts[1].toInt());
                }
            } else if (key == "window_position") {
                QStringList posParts = value.split(',');
                if (posParts.size() == 2) {
                    uiConfig_.windowPosition = QPoint(posParts[0].toInt(), posParts[1].toInt());
                }
            } else if (key == "show_filter_panel") {
                uiConfig_.showFilterPanel = (value.toLower() == "true" || value == "1");
            } else if (key == "show_details_panel") {
                uiConfig_.showDetailsPanel = (value.toLower() == "true" || value == "1");
            } else if (key == "alert_color_critical") {
                uiConfig_.alertColorCritical = value;
            } else if (key == "alert_color_warning") {
                uiConfig_.alertColorWarning = value;
            } else if (key == "alert_color_info") {
                uiConfig_.alertColorInfo = value;
            } else if (key == "alert_font_family") {
                uiConfig_.alertFontFamily = value;
            } else if (key == "alert_font_size") {
                uiConfig_.alertFontSize = value.toInt();
            } else if (key == "dark_theme") {
                uiConfig_.darkTheme = (value.toLower() == "true" || value == "1");
            } else {
                qWarning() << "Unknown UI config key:" << key;
            }
        }

        lineNumber++;
    }

    return true;
}

bool ConfigManager::parseGeneralSection(const QStringList& lines, int& lineNumber) {
    while (lineNumber < lines.size()) {
        QString line = lines[lineNumber].trimmed();

        if (line.startsWith(CONFIG_SECTION_MARKER) || line.isEmpty()) {
            break;
        }

        if (line.startsWith(CONFIG_COMMENT_PREFIX)) {
            lineNumber++;
            continue;
        }

        int separatorPos = line.indexOf(CONFIG_KEY_VALUE_SEPARATOR);
        if (separatorPos > 0) {
            QString key = line.left(separatorPos).trimmed();
            QString value = line.mid(separatorPos + 1).trimmed();

            if (value.startsWith("\"") && value.endsWith("\"")) {
                value = value.mid(1, value.length() - 2);
            }

            if (key == "use_environment_variables") {
                useEnvironmentVariables_ = (value.toLower() == "true" || value == "1");
            } else {
                qWarning() << "Unknown general config key:" << key;
            }
        }

        lineNumber++;
    }

    return true;
}

QString ConfigManager::formatConfigFile() const {
    QStringList lines;

    lines.append("# PostgreSQL Monitor Configuration File");
    lines.append("# Generated automatically - modify with care");
    lines.append("");

    // Database section
    lines.append(formatDatabaseSection());

    // Alerts section
    lines.append(formatAlertSection());

    // Queries section
    lines.append(formatQuerySection());

    // UI section
    lines.append(formatUISection());

    // General section
    lines.append(formatGeneralSection());

    return lines.join('\n');
}

QStringList ConfigManager::formatDatabaseSection() const {
    QStringList lines;
    lines.append("[Database]");
    lines.append("# Database connection settings");
    lines.append("host=" + QString::fromStdString(databaseConfig_.host));
    lines.append("port=" + QString::number(databaseConfig_.port));
    lines.append("database=" + QString::fromStdString(databaseConfig_.database));
    lines.append("username=" + QString::fromStdString(databaseConfig_.username));
    lines.append("password=" + QString::fromStdString(databaseConfig_.password));
    lines.append("connect_timeout=" + QString::number(databaseConfig_.connectTimeout));
    lines.append("sslmode=" + QString::fromStdString(databaseConfig_.sslMode));
    lines.append("application_name=" + QString::fromStdString(databaseConfig_.applicationName));
    lines.append("use_environment_variables=" + (useEnvironmentVariables_ ? "true" : "false"));
    lines.append("");
    return lines;
}

QStringList ConfigManager::formatAlertSection() const {
    QStringList lines;
    lines.append("[Alerts]");
    lines.append("# Alert system settings");
    lines.append("duplicate_detection_enabled=" + (alertConfig_.duplicateDetectionEnabled ? "true" : "false"));
    lines.append("duplicate_time_window=" + QString::number(alertConfig_.duplicateTimeWindow));
    lines.append("max_alerts=" + QString::number(alertConfig_.maxAlerts));
    lines.append("show_timestamps=" + (alertConfig_.showTimestamps ? "true" : "false"));
    lines.append("auto_scroll=" + (alertConfig_.autoScroll ? "true" : "false"));
    lines.append("date_format=" + alertConfig_.dateFormat);
    lines.append("time_format=" + alertConfig_.timeFormat);
    lines.append("");
    return lines;
}

QStringList ConfigManager::formatQuerySection() const {
    QStringList lines;
    lines.append("[Queries]");
    lines.append("# Query execution settings");
    lines.append("queries_file_path=" + QString::fromStdString(queryConfig_.queriesFilePath));
    lines.append("execution_interval=" + QString::number(queryConfig_.executionInterval));
    lines.append("max_concurrent_queries=" + QString::number(queryConfig_.maxConcurrentQueries));
    lines.append("start_monitoring_on_startup=" + (queryConfig_.startMonitoringOnStartup ? "true" : "false"));
    lines.append("enable_query_logging=" + (queryConfig_.enableQueryLogging ? "true" : "false"));
    lines.append("");
    return lines;
}

QStringList ConfigManager::formatUISection() const {
    QStringList lines;
    lines.append("[UI]");
    lines.append("# User interface settings");
    lines.append("window_title=" + uiConfig_.windowTitle);
    lines.append("window_size=" + QString::number(uiConfig_.windowSize.width()) + "x" + QString::number(uiConfig_.windowSize.height()));
    lines.append("window_position=" + QString::number(uiConfig_.windowPosition.x()) + "," + QString::number(uiConfig_.windowPosition.y()));
    lines.append("show_filter_panel=" + (uiConfig_.showFilterPanel ? "true" : "false"));
    lines.append("show_details_panel=" + (uiConfig_.showDetailsPanel ? "true" : "false"));
    lines.append("alert_color_critical=" + uiConfig_.alertColorCritical);
    lines.append("alert_color_warning=" + uiConfig_.alertColorWarning);
    lines.append("alert_color_info=" + uiConfig_.alertColorInfo);
    lines.append("alert_font_family=" + uiConfig_.alertFontFamily);
    lines.append("alert_font_size=" + QString::number(uiConfig_.alertFontSize));
    lines.append("dark_theme=" + (uiConfig_.darkTheme ? "true" : "false"));
    lines.append("");
    return lines;
}

QStringList ConfigManager::formatGeneralSection() const {
    QStringList lines;
    lines.append("[General]");
    lines.append("# General application settings");
    lines.append("use_environment_variables=" + (useEnvironmentVariables_ ? "true" : "false"));
    lines.append("");
    return lines;
}

bool ConfigManager::validateDatabaseConfig() const {
    DatabaseConfig config = getDatabaseConfig();

    if (!validatePort(config.port)) {
        qWarning() << "Invalid port:" << config.port;
        return false;
    }

    if (!validateTimeout(config.connectTimeout)) {
        qWarning() << "Invalid timeout:" << config.connectTimeout;
        return false;
    }

    if (!validateSSLMode(config.sslMode)) {
        qWarning() << "Invalid SSL mode:" << config.sslMode.c_str();
        return false;
    }

    return config.isValid();
}

bool ConfigManager::validatePort(int port) const {
    return port > 0 && port <= 65535;
}

bool ConfigManager::validateTimeout(int timeout) const {
    return timeout > 0 && timeout <= 300;  // Max 5 minutes
}

bool ConfigManager::validateSSLMode(const std::string& sslMode) const {
    std::vector<std::string> validModes = {"disable", "allow", "prefer", "require", "verify-ca", "verify-full"};
    return std::find(validModes.begin(), validModes.end(), sslMode) != validModes.end();
}

DatabaseConfig ConfigManager::getDefaultDatabaseConfig() const {
    DatabaseConfig config;
    config.host = "localhost";
    config.port = 5432;
    config.database = "postgres";
    config.username = "postgres";
    config.password = "";
    config.connectTimeout = 10;
    config.sslMode = "prefer";
    config.applicationName = "PostgreSQL-Monitor";
    return config;
}

AlertConfig ConfigManager::getDefaultAlertConfig() const {
    AlertConfig config;
    config.duplicateDetectionEnabled = true;
    config.duplicateTimeWindow = 30;
    config.maxAlerts = 1000;
    config.showTimestamps = true;
    config.autoScroll = true;
    config.dateFormat = "hh:mm:ss";
    return config;
}

QueryConfig ConfigManager::getDefaultQueryConfig() const {
    QueryConfig config;
    config.queriesFilePath = "config/queries.conf";
    config.executionInterval = 1000;
    config.maxConcurrentQueries = 5;
    config.startMonitoringOnStartup = false;
    config.enableQueryLogging = false;
    return config;
}

UIConfig ConfigManager::getDefaultUIConfig() const {
    UIConfig config;
    config.windowTitle = "PostgreSQL Monitor - Alert Dashboard";
    config.windowSize = QSize(1200, 800);
    config.windowPosition = QPoint(100, 100);
    config.showFilterPanel = true;
    config.showDetailsPanel = true;
    config.alertColorCritical = "#d32f2f";
    config.alertColorWarning = "#f57c00";
    config.alertColorInfo = "#388e3c";
    config.alertFontFamily = "Segoe UI, Arial, sans-serif";
    config.alertFontSize = 10;
    config.darkTheme = false;
    return config;
}

void ConfigManager::printConfiguration() const {
    qDebug() << "=== Configuration ===";
    qDebug() << "Database:" << QString::fromStdString(getDatabaseConfig().toConnectionString());
    qDebug() << "Use Environment Variables:" << useEnvironmentVariables_;
    qDebug() << "Alert Max Count:" << alertConfig_.maxAlerts;
    qDebug() << "Query Interval:" << queryConfig_.executionInterval;
    qDebug() << "Config Path:" << currentConfigPath_;
}

QString ConfigManager::getConfigurationSummary() const {
    DatabaseConfig dbConfig = getDatabaseConfig();
    return QString("Database: %1@%2:%3/%4 (SSL: %5) | Alerts: %6 max | Queries: %7ms interval")
           .arg(QString::fromStdString(dbConfig.username))
           .arg(QString::fromStdString(dbConfig.host))
           .arg(dbConfig.port)
           .arg(QString::fromStdString(dbConfig.database))
           .arg(QString::fromStdString(dbConfig.sslMode))
           .arg(alertConfig_.maxAlerts)
           .arg(queryConfig_.executionInterval);
}

void ConfigManager::setConfigChangedCallback(std::function<void()> callback) {
    configChangedCallback_ = callback;
}

void ConfigManager::notifyConfigChanged() {
    if (configChangedCallback_) {
        configChangedCallback_();
    }
}

std::vector<QString> ConfigManager::getRecentConfigFiles() {
    QSettings settings;
    QStringList files = settings.value("RecentConfigFiles").toStringList();
    std::vector<QString> result;

    for (const QString& file : files) {
        if (QFileInfo::exists(file)) {
            result.push_back(file);
        }
    }

    // Limit to MAX_RECENT_CONFIG_FILES
    if (result.size() > MAX_RECENT_CONFIG_FILES) {
        result.resize(MAX_RECENT_CONFIG_FILES);
    }

    return result;
}

void ConfigManager::addToRecentConfigFiles(const QString& filePath) {
    std::vector<QString> recentFiles = getRecentConfigFiles();

    // Remove if already exists
    recentFiles.erase(
        std::remove(recentFiles.begin(), recentFiles.end(), filePath),
        recentFiles.end()
    );

    // Add to beginning
    recentFiles.insert(recentFiles.begin(), filePath);

    // Limit size
    if (recentFiles.size() > MAX_RECENT_CONFIG_FILES) {
        recentFiles.resize(MAX_RECENT_CONFIG_FILES);
    }

    // Save to settings
    QStringList filesList;
    for (const QString& file : recentFiles) {
        filesList.append(file);
    }

    QSettings settings;
    settings.setValue("RecentConfigFiles", filesList);
}

namespace ConfigUtils {
    QString findConfigFile(const QStringList& searchPaths) {
        for (const QString& path : searchPaths) {
            if (QFileInfo::exists(path)) {
                return path;
            }
        }
        return QString();
    }

    bool ensureConfigDirectory(const QString& configPath) {
        QFileInfo info(configPath);
        QDir dir = info.absoluteDir();
        return dir.mkpath(dir.absolutePath());
    }
}