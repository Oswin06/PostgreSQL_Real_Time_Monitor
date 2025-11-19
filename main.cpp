#include <QApplication>
#include <QStyleFactory>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <iostream>

#include "include/AlertWindow.h"
#include "include/DatabaseManager.h"
#include "include/QueryEngine.h"
#include "include/AlertSystem.h"

void setupApplicationStyle() {
    QApplication::setApplicationName("PostgreSQL Monitor");
    QApplication::setApplicationVersion("1.0");
    QApplication::setOrganizationName("Database Monitoring Systems");
    QApplication::setOrganizationDomain("dbmonitor.local");

    // Set a modern, clean style
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    // Dark theme palette (optional, can be toggled)
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
    darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);

    // Uncomment to apply dark theme:
    // QApplication::setPalette(darkPalette);
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

[DiskSpace]
name=Low Disk Space
sql=SELECT CONCAT('Low disk space on ', pg_tablespace.spcname, ': ', ROUND(100 * (pg_database_size(pg_database.datname) / 1024.0 / 1024.0 / 1024.0), 2), ' GB used') as alert_message FROM pg_database, pg_tablespace WHERE pg_database.dattablespace = pg_tablespace.oid AND pg_database_size(pg_database.datname) / 1024.0 / 1024.0 / 1024.0 > 10
alert_type=warning

[NewUsers]
name=New User Registrations
sql=SELECT CONCAT('New user registered: ', username) as alert_message FROM user_logins WHERE login_time > NOW() - INTERVAL '1 second' AND is_new_user = true
alert_type=info
)";

    return queryEngine->loadQueriesFromString(defaultQueries);
}

bool showConnectionDialog(DatabaseManager::ConnectionConfig& config) {
    // For now, use default values. In a real application, you'd show a proper dialog
    config.host = "localhost";
    config.port = 5432;
    config.database = "postgres";
    config.username = "postgres";
    config.password = "password";
    config.connectTimeout = 10;

    std::cout << "\n=== PostgreSQL Monitor Setup ===\n";
    std::cout << "Using default connection settings:\n";
    std::cout << "  Host: " << config.host << "\n";
    std::cout << "  Port: " << config.port << "\n";
    std::cout << "  Database: " << config.database << "\n";
    std::cout << "  Username: " << config.username << "\n";
    std::cout << "\nNote: You can change these in Settings after startup.\n";

    return true;
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    setupApplicationStyle();

    // Create core components
    auto alertSystem = std::make_unique<AlertSystem>();
    auto databaseManager = std::make_unique<DatabaseManager>();

    std::cout << "Starting PostgreSQL Real-Time Monitor...\n";

    // Try to connect to database
    DatabaseManager::ConnectionConfig config;
    bool connected = false;

    // Try to connect with default settings first
    if (showConnectionDialog(config)) {
        connected = databaseManager->connect(config);
        if (!connected) {
            std::cout << "Warning: Could not connect to database with default settings.\n";
            std::cout << "Application will start but monitoring will be disabled.\n";
            std::cout << "Use Settings to configure database connection and try again.\n\n";
        }
    }

    // Create query engine
    auto queryEngine = std::make_unique<QueryEngine>(databaseManager.get(), alertSystem.get());

    // Load default queries
    if (!loadDefaultQueries(queryEngine.get())) {
        std::cout << "Warning: Failed to load default queries.\n";
    } else {
        std::cout << "Loaded default monitoring queries.\n";
    }

    // Create and setup main window
    AlertWindow window;
    window.show();

    // Connect database manager to main window
    QObject::connect(databaseManager.get(), &DatabaseManager::connectionStatusChanged,
                     &window, &AlertWindow::onConnectionStatusChanged);

    // Connect query engine to main window
    QObject::connect(queryEngine.get(), &QueryEngine::alertGenerated,
                     &window, &AlertWindow::onNewAlertAdded);
    QObject::connect(queryEngine.get(), &QueryEngine::queryError,
                     &window, &AlertWindow::onDatabaseError);

    // Connect alert system to main window (if signals available)
    // This would require adding appropriate signals to AlertSystem

    // Setup database connection in the window
    if (connected) {
        window.connectToDatabase(config);
    }

    // Show startup message
    if (connected) {
        std::cout << "Successfully connected to database.\n";
        std::cout << "You can start monitoring from the Tools menu.\n";
    } else {
        std::cout << "Application started without database connection.\n";
        std::cout << "Configure database connection in Settings to enable monitoring.\n";
    }

    std::cout << "\nApplication ready. Use the interface to:\n";
    std::cout << "  1. Configure database connection (Settings)\n";
    std::cout << "  2. Start/stop monitoring (Tools menu)\n";
    std::cout << "  3. View real-time alerts in the main window\n";
    std::cout << "  4. Configure custom queries in config/queries.conf\n\n";

    int result = app.exec();

    std::cout << "Application shutdown.\n";
    return result;
}