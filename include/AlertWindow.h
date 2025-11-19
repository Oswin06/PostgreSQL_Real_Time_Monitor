#ifndef ALERTWINDOW_H
#define ALERTWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QListWidgetItem>
#include <QTimer>
#include <QLabel>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QProgressBar>
#include <QGroupBox>
#include <QPushButton>
#include <QTextEdit>
#include <QSplitter>
#include <QHeaderView>
#include <QTableWidget>
#include <QContextMenuEvent>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>

#include "AlertSystem.h"
#include "DatabaseManager.h"

// Forward declaration
class ConfigManager;

class AlertWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit AlertWindow(QWidget *parent = nullptr);
    ~AlertWindow();

    // Database management
    bool connectToDatabase(const DatabaseManager::ConnectionConfig& config);
    void disconnectFromDatabase();
    bool isDatabaseConnected() const;

    // Alert display
    void updateAlertDisplay();
    void addAlertToUI(const Alert& alert);
    void clearAlerts();

    // Status management
    void updateConnectionStatus(bool connected);
    void updateLastUpdateTime();

    // Configuration management
    void setConfigManager(ConfigManager* configManager);
    ConfigManager* getConfigManager() const;

public slots:
    void onNewAlertAdded(const Alert& alert);
    void onConnectionStatusChanged(bool connected);
    void onDatabaseError(const QString& error);
    void onConfigChanged();
    void onConfigLoaded();
    void onMonitoringStarted();
    void onMonitoringStopped();

private slots:
    void startMonitoring();
    void stopMonitoring();
    void showSettings();
    void showAbout();
    void exportAlerts();
    void clearAllAlerts();
    void refreshConnection();
    void onAlertItemDoubleClicked(QListWidgetItem* item);
    void onFilterChanged();
    void showAlertDetails(QListWidgetItem* item);

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUI();
    void setupMenuBar();
    void setupStatusBar();
    void setupCentralWidget();
    void setupAlertList();
    void setupFilterPanel();

    void createActions();
    void connectSignals();

    // UI Components
    QWidget* centralWidget_;
    QSplitter* mainSplitter_;
    QWidget* leftPanel_;
    QWidget* rightPanel_;

    // Alert display
    QListWidget* alertList_;
    QTextEdit* alertDetails_;

    // Filter panel
    QGroupBox* filterGroup_;
    QCheckBox* showCritical_;
    QCheckBox* showWarning_;
    QCheckBox* showInfo_;
    QLineEdit* searchBox_;

    // Status bar
    QLabel* connectionStatusLabel_;
    QLabel* lastUpdateLabel_;
    QLabel* alertCountLabel_;
    QProgressBar* connectionProgressBar_;

    // Menus and actions
    QMenu* fileMenu_;
    QMenu* viewMenu_;
    QMenu* toolsMenu_;
    QMenu* helpMenu_;

    QAction* startAction_;
    QAction* stopAction_;
    QAction* settingsAction_;
    QAction* exportAction_;
    QAction* clearAction_;
    QAction* refreshAction_;
    QAction* aboutAction_;
    QAction* exitAction_;

    // System components
    std::unique_ptr<AlertSystem> alertSystem_;
    std::unique_ptr<DatabaseManager> databaseManager_;
    ConfigManager* configManager_;

    // State
    bool isMonitoring_;
    bool isConnected_;

    // UI Helpers
    QListWidgetItem* createAlertItem(const Alert& alert);
    void updateAlertItem(QListWidgetItem* item, const Alert& alert);
    void updateFiltering();
    void updateStatusBar();
    QString getConnectionStatusText() const;
    QColor getAlertColor(AlertType type) const;
    QString getAlertIcon(AlertType type) const;

    // Settings dialog
    void showSettingsDialog();
    bool validateDatabaseConfig(const DatabaseManager::ConnectionConfig& config) const;

    // Alert filtering
    bool shouldShowAlert(const Alert& alert) const;
    void applyFilters();
};

// Settings Dialog
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

    DatabaseManager::ConnectionConfig getDatabaseConfig() const;
    void setDatabaseConfig(const DatabaseManager::ConnectionConfig& config);

    int getDuplicateTimeWindow() const;
    void setDuplicateTimeWindow(int seconds);

    int getMaxAlerts() const;
    void setMaxAlerts(int maxAlerts);

    bool isDuplicateDetectionEnabled() const;
    void setDuplicateDetectionEnabled(bool enabled);

private slots:
    void testConnection();
    void accept() override;

private:
    void setupUI();

    // Database settings
    QLineEdit* hostEdit_;
    QSpinBox* portSpinBox_;
    QLineEdit* databaseEdit_;
    QLineEdit* usernameEdit_;
    QLineEdit* passwordEdit_;
    QSpinBox* timeoutSpinBox_;
    QPushButton* testConnectionButton_;

    // Alert settings
    QCheckBox* duplicateDetectionCheck_;
    QSpinBox* duplicateTimeWindowSpin_;
    QSpinBox* maxAlertsSpin_;

    // Status
    QLabel* testConnectionStatus_;

    DatabaseManager::ConnectionConfig config_;
};

// Alert Details Dialog
class AlertDetailsDialog : public QDialog {
    Q_OBJECT

public:
    explicit AlertDetailsDialog(const Alert& alert, QWidget *parent = nullptr);
    ~AlertDetailsDialog();

private:
    void setupUI(const Alert& alert);

    QLabel* titleLabel_;
    QLabel* typeLabel_;
    QLabel* timestampLabel_;
    QLabel* querySourceLabel_;
    QTextEdit* messageEdit_;
    QTextEdit* rawResultEdit_;
};

#endif // ALERTWINDOW_H