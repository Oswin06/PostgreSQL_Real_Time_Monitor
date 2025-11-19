#include <QApplication>
#include <QStyleFactory>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <QCommandLineParser>
#include <QFileInfo>
#include <iostream>

#include "include/AlertWindow.h"
#include "include/DatabaseManager.h"
#include "include/QueryEngine.h"
#include "include/AlertSystem.h"
#include "include/ConfigManager.h"

void setupApplicationStyle() {
    QApplication::setApplicationName("PostgreSQL Monitor");
    QApplication::setApplicationVersion("1.0");
    QApplication::setOrganizationName("Database Monitoring Systems");
    QApplication::setOrganizationDomain("dbmonitor.local");

    // Set a modern, clean style
    QApplication::setStyle(QStyleFactory::create("Fusion"));
}

bool loadDefaultQueries(QueryEngine* queryEngine) {
    const std::string defaultQueries = R"(
[SecurityBreach]
name=Security Breach Detection
sql=SELECT 'BREACH DETECTED' as alert_message, severity FROM security_events WHERE created_at > NOW() - INTERVAL '1 second'
alert_type=critical

[FailedLogins]
name=Failed Login Count
sql=SELECT CONCAT('Failed login attempts: ', COUNT(*)) as alert_message FROM login_attempts WHERE success=false AND timestamp > NOW() - INTERVAL '1 second'
alert_type=warning
threshold=3

[HighCPU]
name=High CPU Usage
sql=SELECT CASE WHEN AVG(cpu_usage) > 80 THEN CONCAT('High CPU usage detected: ', ROUND(AVG(cpu_usage), 2), '%') ELSE 'Normal CPU usage' END as alert_message FROM system_metrics WHERE timestamp > NOW() - INTERVAL '1 second' AND metric_type='cpu'
alert_type=warning
threshold=1

[DatabaseConnections]
name=Database Connection Count
sql=SELECT CONCAT('Active database connections: ', COUNT(*)) as alert_message FROM pg_stat_activity WHERE state = 'active'
alert_type=info
threshold=10

[NewUsers]
name=New User Registrations
sql=SELECT CONCAT('New user registered: ', username) as alert_message FROM user_logins WHERE login_time > NOW() - INTERVAL '1 second' AND is_new_user = true
alert_type=info
)";

    return queryEngine->loadQueriesFromString(defaultQueries);
}

void printStartupInfo(const ConfigManager* configManager, const DatabaseManager* dbManager) {
    std::cout << "\n=== PostgreSQL Monitor Startup ===\n";
    std::cout << "Configuration loaded from: " << configManager->getDefaultConfigPath().toStdString() << "\n";

    DatabaseConfig dbConfig = configManager->getDatabaseConfig();
    std::cout << "Database: " << dbConfig.username << "@" << dbConfig.host << ":" << dbConfig.port << "/" << dbConfig.database << "\n";
    std::cout << "SSL Mode: " << dbConfig.sslMode << "\n";

    if (configManager->useEnvironmentVariables()) {
        std::cout << "Using environment variables for database connection\n";
    }

    std::cout << "Max alerts: " << configManager->getAlertConfig().maxAlerts << "\n";
    std::cout << "Query interval: " << configManager->getQueryConfig().executionInterval << "ms\n";
    std::cout << "================================\n\n";
}

void showUsage() {
    std::cout << "PostgreSQL Real-Time Monitor\n\n";
    std::cout << "Usage: PostgreSQLMonitor [OPTIONS]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --config, -c <file>    Path to configuration file (default: config.txt)\n";
    std::cout << "  --debug               Enable debug output\n";
    std::cout << "  --help, -h           Show this help message\n";
    std::cout << "  --version            Show version information\n\n";
    std::cout << "Examples:\n";
    std::cout << "  PostgreSQLMonitor                    # Use default config\n";
    std::cout << "  PostgreSQLMonitor -c custom.txt      # Use custom config file\n";
    std::cout << "  PostgreSQLMonitor --debug            # Enable debug mode\n\n";
}

void showVersion() {
    std::cout << "PostgreSQL Monitor v1.0\n";
    std::cout << "Real-time PostgreSQL database monitoring system\n";
    std::cout << "Built with Qt6 and libpqxx\n\n";
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    setupApplicationStyle();

    // Parse command line arguments
    QCommandLineParser parser;
    parser.setApplicationDescription("PostgreSQL Real-Time Monitor");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption configOption(QStringList() << "c" << "config",
                                   "Path to configuration file", "file", "config.txt");
    parser.addOption(configOption);

    QCommandLineOption debugOption(QStringList() << "debug",
                                  "Enable debug output");
    parser.addOption(debugOption);

    parser.process(app);

    QString configFilePath = parser.value(configOption);
    bool debugMode = parser.isSet(debugOption);

    if (debugMode) {
        QLoggingCategory::setFilterRules("*.debug=true");
        qDebug() << "Debug mode enabled";
    }

    std::cout << "Starting PostgreSQL Real-Time Monitor...\n";

    // Create configuration manager
    auto configManager = std::make_unique<ConfigManager>();

    // Load configuration
    bool configLoaded = false;
    if (QFileInfo::exists(configFilePath)) {
        configLoaded = configManager->loadFromFile(configFilePath);
        if (configLoaded) {
            std::cout << "Loaded configuration from: " << configFilePath.toStdString() << "\n";
        } else {
            std::cout << "Warning: Failed to load config from " << configFilePath.toStdString() << "\n";
        }
    } else {
        // Try to find config file in common locations
        QStringList searchPaths = {
            "config.txt",
            "config/config.txt",
            "../config.txt",
            "../../config.txt"
        };

        for (const QString& path : searchPaths) {
            if (QFileInfo::exists(path)) {
                if (configManager->loadFromFile(path)) {
                    configFilePath = path;
                    configLoaded = true;
                    std::cout << "Loaded configuration from: " << path.toStdString() << "\n";
                    break;
                }
            }
        }

        if (!configLoaded) {
            std::cout << "No configuration file found. Using defaults and creating config.txt\n";
            configManager->saveToDefaultLocation();
        }
    }

    // Create core components with config manager
    auto alertSystem = std::make_unique<AlertSystem>();
    auto databaseManager = std::make_unique<DatabaseManager>(configManager.get());

    // Configure alert system from config
    AlertConfig alertConfig = configManager->getAlertConfig();
    alertSystem->setDuplicateDetectionEnabled(alertConfig.duplicateDetectionEnabled);
    alertSystem->setDuplicateTimeWindow(alertConfig.duplicateTimeWindow);
    alertSystem->setMaxAlerts(alertConfig.maxAlerts);

    // Enable auto-reconnection for database manager
    databaseManager->enableAutoReconnect(true, 5000);

    // Print startup information
    if (debugMode) {
        printStartupInfo(configManager.get(), databaseManager.get());
    }

    // Try to connect to database
    bool connected = false;
    if (configLoaded || configManager->validateDatabaseConfig()) {
        connected = databaseManager->connect();
        if (connected) {
            std::cout << "Successfully connected to database.\n";
        } else {
            std::cout << "Warning: Could not connect to database.\n";
            std::cout << "Error: " << databaseManager->getLastError() << "\n";
            std::cout << "Application will start but monitoring will be disabled.\n";
            std::cout << "Use Settings to configure database connection and try again.\n\n";
        }
    } else {
        std::cout << "Warning: Invalid database configuration.\n";
        std::cout << "Application will start but monitoring will be disabled.\n";
        std::cout << "Use Settings to configure database connection and try again.\n\n";
    }

    // Create query engine
    auto queryEngine = std::make_unique<QueryEngine>(databaseManager.get(), alertSystem.get());

    // Configure query engine from config
    QueryConfig queryConfig = configManager->getQueryConfig();
    queryEngine->setInterval(queryConfig.executionInterval);
    queryEngine->setMaxConcurrentQueries(queryConfig.maxConcurrentQueries);

    // Load queries from configured file or defaults
    QString queriesFile = QString::fromStdString(queryConfig.queriesFilePath);
    if (QFileInfo::exists(queriesFile)) {
        if (queryEngine->loadQueriesFromFile(queriesFile.toStdString())) {
            std::cout << "Loaded queries from: " << queriesFile.toStdString() << "\n";
        } else {
            std::cout << "Warning: Failed to load queries from " << queriesFile.toStdString() << "\n";
            loadDefaultQueries(queryEngine.get());
        }
    } else {
        std::cout << "Queries file not found: " << queriesFile.toStdString() << "\n";
        std::cout << "Loading default queries...\n";
        loadDefaultQueries(queryEngine.get());
    }

    // Create and setup main window with config manager
    AlertWindow window;
    window.setConfigManager(configManager.get());
    window.show();

    // Apply UI configuration
    UIConfig uiConfig = configManager->getUIConfig();
    window.setWindowTitle(uiConfig.windowTitle);
    window.resize(uiConfig.windowSize);
    window.move(uiConfig.windowPosition);

    // Connect signals and slots
    QObject::connect(databaseManager.get(), &DatabaseManager::connectionStatusChanged,
                     &window, &AlertWindow::onConnectionStatusChanged);
    QObject::connect(databaseManager.get(), &DatabaseManager::connectionError,
                     &window, &AlertWindow::onDatabaseError);
    QObject::connect(databaseManager.get(), &DatabaseManager::configLoaded,
                     &window, &AlertWindow::onConfigLoaded);

    QObject::connect(queryEngine.get(), &QueryEngine::alertGenerated,
                     &window, &AlertWindow::onNewAlertAdded);
    QObject::connect(queryEngine.get(), &QueryEngine::queryError,
                     &window, &AlertWindow::onDatabaseError);
    QObject::connect(queryEngine.get(), &QueryEngine::monitoringStarted,
                     &window, &AlertWindow::onMonitoringStarted);
    QObject::connect(queryEngine.get(), &QueryEngine::monitoringStopped,
                     &window, &AlertWindow::onMonitoringStopped);

    // Connect config manager signals
    QObject::connect(configManager.get(), &ConfigManager::configChanged,
                     &window, &AlertWindow::onConfigChanged);

    // Setup database connection status in window
    window.onConnectionStatusChanged(connected);

    // Auto-start monitoring if configured
    if (connected && queryConfig.startMonitoringOnStartup) {
        std::cout << "Auto-starting monitoring as configured...\n";
        queryEngine->startMonitoring();
    }

    // Show ready message
    std::cout << "\nApplication ready. Use the interface to:\n";
    std::cout << "  1. Configure database connection (Settings → Database)\n";
    std::cout << "  2. Start/stop monitoring (Tools menu)\n";
    std::cout << "  3. View real-time alerts in the main window\n";
    std::cout << "  4. Configure custom queries in config/queries.conf\n";
    std::cout << "  5. Adjust application settings (Settings)\n\n";

    if (connected) {
        std::cout << "Status: Connected to database - Ready to monitor\n";
    } else {
        std::cout << "Status: Not connected - Configure database connection in Settings\n";
    }

    std::cout << "Configuration file: " << (configLoaded ? configFilePath.toStdString() : configManager->getDefaultConfigPath().toStdString()) << "\n\n";

    int result = app.exec();

    // Save configuration before exit
    configManager->saveToDefaultLocation();

    std::cout << "Application shutdown.\n";
    return result;
}