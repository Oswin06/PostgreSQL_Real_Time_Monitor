#include "AlertWindow.h"
#include <QApplication>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QTextStream>
#include <QFile>
#include <QCloseEvent>
#include <QHeaderView>
#include <QClipboard>
#include <QGuiApplication>

AlertWindow::AlertWindow(QWidget *parent)
    : QMainWindow(parent)
    , centralWidget_(nullptr)
    , mainSplitter_(nullptr)
    , leftPanel_(nullptr)
    , rightPanel_(nullptr)
    , alertList_(nullptr)
    , alertDetails_(nullptr)
    , filterGroup_(nullptr)
    , showCritical_(nullptr)
    , showWarning_(nullptr)
    , showInfo_(nullptr)
    , searchBox_(nullptr)
    , connectionStatusLabel_(nullptr)
    , lastUpdateLabel_(nullptr)
    , alertCountLabel_(nullptr)
    , connectionProgressBar_(nullptr)
    , fileMenu_(nullptr)
    , viewMenu_(nullptr)
    , toolsMenu_(nullptr)
    , helpMenu_(nullptr)
    , startAction_(nullptr)
    , stopAction_(nullptr)
    , settingsAction_(nullptr)
    , exportAction_(nullptr)
    , clearAction_(nullptr)
    , refreshAction_(nullptr)
    , aboutAction_(nullptr)
    , exitAction_(nullptr)
    , isMonitoring_(false)
    , isConnected_(false)
{
    alertSystem_ = std::make_unique<AlertSystem>();
    databaseManager_ = std::make_unique<DatabaseManager>();

    setupUI();
    createActions();
    connectSignals();

    setWindowTitle("PostgreSQL Monitor - Alert Dashboard");
    setMinimumSize(800, 600);
    resize(1200, 800);

    updateConnectionStatus(false);
    updateStatusBar();
}

AlertWindow::~AlertWindow() = default;

void AlertWindow::setupUI() {
    setupMenuBar();
    setupCentralWidget();
    setupStatusBar();
}

void AlertWindow::setupMenuBar() {
    fileMenu_ = menuBar()->addMenu("&File");
    viewMenu_ = menuBar()->addMenu("&View");
    toolsMenu_ = menuBar()->addMenu("&Tools");
    helpMenu_ = menuBar()->addMenu("&Help");
}

void AlertWindow::setupCentralWidget() {
    centralWidget_ = new QWidget(this);
    setCentralWidget(centralWidget_);

    mainSplitter_ = new QSplitter(Qt::Horizontal, centralWidget_);

    setupLeftPanel();
    setupRightPanel();

    mainSplitter_->addWidget(leftPanel_);
    mainSplitter_->addWidget(rightPanel_);
    mainSplitter_->setSizes({800, 400});

    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget_);
    mainLayout->addWidget(mainSplitter_);
    mainLayout->setContentsMargins(5, 5, 5, 5);
}

void AlertWindow::setupLeftPanel() {
    leftPanel_ = new QWidget();

    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel_);

    setupFilterPanel();
    setupAlertList();

    leftLayout->addWidget(filterGroup_);
    leftLayout->addWidget(alertList_, 1);
}

void AlertWindow::setupRightPanel() {
    rightPanel_ = new QWidget();

    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel_);

    QLabel* detailsLabel = new QLabel("Alert Details:");
    detailsLabel->setStyleSheet("font-weight: bold; font-size: 12px;");

    alertDetails_ = new QTextEdit();
    alertDetails_->setReadOnly(true);
    alertDetails_->setMaximumWidth(400);

    rightLayout->addWidget(detailsLabel);
    rightLayout->addWidget(alertDetails_, 1);
}

void AlertWindow::setupFilterPanel() {
    filterGroup_ = new QGroupBox("Filters");

    QVBoxLayout* filterLayout = new QVBoxLayout(filterGroup_);

    // Alert type filters
    QWidget* typeFilters = new QWidget();
    QHBoxLayout* typeLayout = new QHBoxLayout(typeFilters);

    showCritical_ = new QCheckBox("Critical");
    showWarning_ = new QCheckBox("Warning");
    showInfo_ = new QCheckBox("Info");

    showCritical_->setChecked(true);
    showWarning_->setChecked(true);
    showInfo_->setChecked(true);

    typeLayout->addWidget(showCritical_);
    typeLayout->addWidget(showWarning_);
    typeLayout->addWidget(showInfo_);
    typeLayout->addStretch();

    // Search
    searchBox_ = new QLineEdit();
    searchBox_->setPlaceholderText("Search alerts...");

    filterLayout->addWidget(typeFilters);
    filterLayout->addWidget(new QLabel("Search:"));
    filterLayout->addWidget(searchBox_);

    connect(showCritical_, &QCheckBox::toggled, this, &AlertWindow::onFilterChanged);
    connect(showWarning_, &QCheckBox::toggled, this, &AlertWindow::onFilterChanged);
    connect(showInfo_, &QCheckBox::toggled, this, &AlertWindow::onFilterChanged);
    connect(searchBox_, &QLineEdit::textChanged, this, &AlertWindow::onFilterChanged);
}

void AlertWindow::setupAlertList() {
    alertList_ = new QListWidget();
    alertList_->setAlternatingRowColors(true);
    alertList_->setSelectionMode(QAbstractItemView::SingleSelection);
    alertList_->setContextMenuPolicy(Qt::CustomContextMenu);

    QFont font = alertList_->font();
    font.setPointSize(10);
    alertList_->setFont(font);

    connect(alertList_, &QListWidget::itemDoubleClicked, this, &AlertWindow::onAlertItemDoubleClicked);
    connect(alertList_, &QListWidget::itemSelectionChanged, this, [this]() {
        auto items = alertList_->selectedItems();
        if (!items.empty()) {
            showAlertDetails(items.first());
        }
    });
}

void AlertWindow::setupStatusBar() {
    connectionStatusLabel_ = new QLabel("Disconnected");
    lastUpdateLabel_ = new QLabel("Last update: Never");
    alertCountLabel_ = new QLabel("Alerts: 0");
    connectionProgressBar_ = new QProgressBar();
    connectionProgressBar_->setVisible(false);
    connectionProgressBar_->setMaximumWidth(200);

    statusBar()->addWidget(connectionStatusLabel_);
    statusBar()->addWidget(connectionProgressBar_);
    statusBar()->addPermanentWidget(lastUpdateLabel_);
    statusBar()->addPermanentWidget(alertCountLabel_);
}

void AlertWindow::createActions() {
    // File menu
    exportAction_ = new QAction("&Export Alerts...", this);
    exportAction_->setShortcut(QKeySequence("Ctrl+E"));
    exportAction_->setStatusTip("Export alerts to file");
    fileMenu_->addAction(exportAction_);

    clearAction_ = new QAction("&Clear All Alerts", this);
    clearAction_->setShortcut(QKeySequence("Ctrl+Del"));
    clearAction_->setStatusTip("Clear all alerts from the list");
    fileMenu_->addAction(clearAction_);

    fileMenu_->addSeparator();
    exitAction_ = new QAction("E&xit", this);
    exitAction_->setShortcut(QKeySequence("Ctrl+Q"));
    exitAction_->setStatusTip("Exit the application");
    fileMenu_->addAction(exitAction_);

    // View menu
    QAction* showDetailsAction = new QAction("Show Details Panel", this);
    showDetailsAction->setCheckable(true);
    showDetailsAction->setChecked(true);
    viewMenu_->addAction(showDetailsAction);
    connect(showDetailsAction, &QAction::toggled, rightPanel_, &QWidget::setVisible);

    // Tools menu
    startAction_ = new QAction("&Start Monitoring", this);
    startAction_->setShortcut(QKeySequence("F5"));
    startAction_->setStatusTip("Start database monitoring");
    toolsMenu_->addAction(startAction_);

    stopAction_ = new QAction("St&op Monitoring", this);
    stopAction_->setShortcut(QKeySequence("F6"));
    stopAction_->setStatusTip("Stop database monitoring");
    stopAction_->setEnabled(false);
    toolsMenu_->addAction(stopAction_);

    refreshAction_ = new QAction("&Refresh Connection", this);
    refreshAction_->setShortcut(QKeySequence("F9"));
    refreshAction_->setStatusTip("Refresh database connection");
    toolsMenu_->addAction(refreshAction_);

    toolsMenu_->addSeparator();
    settingsAction_ = new Action("&Settings...", this);
    settingsAction_->setShortcut(QKeySequence("Ctrl+,"));
    settingsAction_->setStatusTip("Configure application settings");
    toolsMenu_->addAction(settingsAction_);

    // Help menu
    aboutAction_ = new QAction("&About", this);
    aboutAction_->setStatusTip("Show application information");
    helpMenu_->addAction(aboutAction_);
}

void AlertWindow::connectSignals() {
    connect(startAction_, &QAction::triggered, this, &AlertWindow::startMonitoring);
    connect(stopAction_, &QAction::triggered, this, &AlertWindow::stopMonitoring);
    connect(settingsAction_, &QAction::triggered, this, &AlertWindow::showSettings);
    connect(aboutAction_, &QAction::triggered, this, &AlertWindow::showAbout);
    connect(exportAction_, &QAction::triggered, this, &AlertWindow::exportAlerts);
    connect(clearAction_, &QAction::triggered, this, &AlertWindow::clearAllAlerts);
    connect(refreshAction_, &QAction::triggered, this, &AlertWindow::refreshConnection);
}

bool AlertWindow::connectToDatabase(const DatabaseManager::ConnectionConfig& config) {
    try {
        if (databaseManager_->connect(config)) {
            isConnected_ = true;
            updateConnectionStatus(true);
            return true;
        } else {
            QMessageBox::warning(this, "Connection Error",
                                "Failed to connect to database:\n" +
                                QString::fromStdString(databaseManager_->getLastError()));
            return false;
        }
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Connection Error",
                            "Database connection failed:\n" + QString(e.what()));
        return false;
    }
}

void AlertWindow::disconnectFromDatabase() {
    databaseManager_->disconnect();
    isConnected_ = false;
    updateConnectionStatus(false);
}

bool AlertWindow::isDatabaseConnected() const {
    return isConnected_ && databaseManager_->isConnected();
}

void AlertWindow::updateAlertDisplay() {
    auto alerts = alertSystem_->getRecentAlerts();

    alertList_->clear();

    for (const auto& alert : alerts) {
        if (shouldShowAlert(alert)) {
            QListWidgetItem* item = createAlertItem(alert);
            alertList_->addItem(item);
        }
    }

    updateStatusBar();
}

void AlertWindow::addAlertToUI(const Alert& alert) {
    if (shouldShowAlert(alert)) {
        QListWidgetItem* item = createAlertItem(alert);
        alertList_->insertItem(0, item);
        alertList_->scrollToTop();
    }
    updateStatusBar();
}

void AlertWindow::clearAlerts() {
    alertList_->clear();
    alertSystem_->enforceMaxAlerts(0);  // Clear all
    updateStatusBar();
}

void AlertWindow::updateConnectionStatus(bool connected) {
    isConnected_ = connected;

    QString statusText = connected ? "Connected" : "Disconnected";
    QColor statusColor = connected ? Qt::green : Qt::red;

    connectionStatusLabel_->setText(statusText);
    connectionStatusLabel_->setStyleSheet(
        QString("QLabel { color: %1; font-weight: bold; }").arg(statusColor.name()));

    startAction_->setEnabled(!connected && !isMonitoring_);
    stopAction_->setEnabled(connected && isMonitoring_);
}

void AlertWindow::updateLastUpdateTime() {
    lastUpdateLabel_->setText("Last update: " + QDateTime::currentDateTime().toString("hh:mm:ss"));
}

void AlertWindow::onNewAlertAdded(const Alert& alert) {
    addAlertToUI(alert);
}

void AlertWindow::onConnectionStatusChanged(bool connected) {
    updateConnectionStatus(connected);
}

void AlertWindow::onDatabaseError(const QString& error) {
    QMessageBox::warning(this, "Database Error", error);
    updateConnectionStatus(false);
}

void AlertWindow::startMonitoring() {
    // This will be connected to QueryEngine signals
    isMonitoring_ = true;
    startAction_->setEnabled(false);
    stopAction_->setEnabled(true);

    statusBar()->showMessage("Monitoring started", 3000);
}

void AlertWindow::stopMonitoring() {
    isMonitoring_ = false;
    startAction_->setEnabled(isConnected_);
    stopAction_->setEnabled(false);

    statusBar()->showMessage("Monitoring stopped", 3000);
}

void AlertWindow::showSettings() {
    showSettingsDialog();
}

void AlertWindow::showAbout() {
    QMessageBox::about(this, "About PostgreSQL Monitor",
                       "<h3>PostgreSQL Real-Time Monitor</h3>"
                       "<p>Version 1.0</p>"
                       "<p>A real-time PostgreSQL database monitoring system "
                       "with configurable alerting and color-coded notifications.</p>"
                       "<p>Executes custom SQL queries every second and displays "
                       "results as color-coded alerts in real-time.</p>");
}

void AlertWindow::exportAlerts() {
    QString fileName = QFileDialog::getSaveFileName(this, "Export Alerts",
                                                   "alerts_export.txt",
                                                   "Text Files (*.txt);;All Files (*)");
    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Export Error", "Could not open file for writing");
        return;
    }

    QTextStream out(&file);
    auto alerts = alertSystem_->getRecentAlerts(1000);  // Export up to 1000 alerts

    out << "PostgreSQL Monitor - Alert Export\n";
    out << "Generated: " << QDateTime::currentDateTime().toString() << "\n";
    out << "Total Alerts: " << alerts.size() << "\n\n";

    for (const auto& alert : alerts) {
        out << "ID: " << alert.id << "\n";
        out << "Type: " << alert.getTypeString().c_str() << "\n";
        out << "Title: " << alert.title.c_str() << "\n";
        out << "Message: " << alert.message.c_str() << "\n";
        out << "Query: " << alert.querySource.c_str() << "\n";
        out << "Timestamp: " << alert.timestamp.toString("yyyy-MM-dd hh:mm:ss") << "\n";
        out << "----------------------------------------\n";
    }

    statusBar()->showMessage("Exported " + QString::number(alerts.size()) + " alerts", 3000);
}

void AlertWindow::clearAllAlerts() {
    int ret = QMessageBox::question(this, "Clear All Alerts",
                                   "Are you sure you want to clear all alerts?",
                                   QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes) {
        clearAlerts();
        statusBar()->showMessage("All alerts cleared", 3000);
    }
}

void AlertWindow::refreshConnection() {
    if (databaseManager_) {
        connectionProgressBar_->setVisible(true);
        connectionProgressBar_->setRange(0, 0);  // Indeterminate progress

        QApplication::processEvents();

        if (databaseManager_->reconnect()) {
            updateConnectionStatus(true);
            statusBar()->showMessage("Connection refreshed successfully", 3000);
        } else {
            updateConnectionStatus(false);
            QMessageBox::warning(this, "Connection Error",
                                "Failed to refresh connection:\n" +
                                QString::fromStdString(databaseManager_->getLastError()));
        }

        connectionProgressBar_->setVisible(false);
        connectionProgressBar_->setRange(0, 100);
    }
}

void AlertWindow::onAlertItemDoubleClicked(QListWidgetItem* item) {
    showAlertDetails(item);
}

void AlertWindow::onFilterChanged() {
    updateFiltering();
}

void AlertWindow::showAlertDetails(QListWidgetItem* item) {
    if (!item) {
        alertDetails_->clear();
        return;
    }

    QVariant alertVariant = item->data(Qt::UserRole);
    if (!alertVariant.isValid()) {
        return;
    }

    // Note: In a real implementation, we'd store Alert object in the item
    // For now, show item text as details
    alertDetails_->setPlainText(item->text());
}

void AlertWindow::contextMenuEvent(QContextMenuEvent *event) {
    QListWidgetItem* item = alertList_->itemAt(event->pos());

    QMenu contextMenu(this);

    if (item) {
        QAction* detailsAction = contextMenu.addAction("View Details");
        QAction* copyAction = contextMenu.addAction("Copy Alert");

        connect(detailsAction, &QAction::triggered, [this, item]() {
            showAlertDetails(item);
        });

        connect(copyAction, &QAction::triggered, [item]() {
            QGuiApplication::clipboard()->setText(item->text());
        });

        contextMenu.addSeparator();
    }

    QAction* clearSelectedAction = contextMenu.addAction("Clear Selected");
    QAction* exportAction = contextMenu.addAction("Export All");

    if (item) {
        connect(clearSelectedAction, &QAction::triggered, [this, item]() {
            delete alertList_->takeItem(alertList_->row(item));
        });
    }

    connect(exportAction, &QAction::triggered, this, &AlertWindow::exportAlerts);

    contextMenu.exec(event->globalPos());
}

void AlertWindow::closeEvent(QCloseEvent *event) {
    if (isMonitoring_) {
        int ret = QMessageBox::question(this, "Confirm Exit",
                                       "Monitoring is still active. Stop monitoring and exit?",
                                       QMessageBox::Yes | QMessageBox::No);
        if (ret == QMessageBox::No) {
            event->ignore();
            return;
        }
        stopMonitoring();
    }

    event->accept();
}

QListWidgetItem* AlertWindow::createAlertItem(const Alert& alert) {
    QListWidgetItem* item = new QListWidgetItem();

    QString alertText = QString("[%1] %2\n%3 - %4")
                       .arg(alert.getFormattedTimestamp())
                       .arg(QString::fromStdString(alert.title))
                       .arg(QString::fromStdString(alert.getTypeString()))
                       .arg(QString::fromStdString(alert.querySource));

    item->setText(alertText);
    item->setBackground(alert.getColor());
    item->setForeground(Qt::white);

    QFont font = item->font();
    font.setBold(true);
    item->setFont(font);

    item->setToolTip(QString::fromStdString(alert.message));

    // Store alert data in the item for later retrieval
    // Note: In a real implementation, we'd use a custom Qt type or QVariant
    // For simplicity, we're storing basic info

    return item;
}

void AlertWindow::updateFiltering() {
    updateAlertDisplay();
}

void AlertWindow::updateStatusBar() {
    alertCountLabel_->setText(QString("Alerts: %1").arg(alertList_->count()));
}

QString AlertWindow::getConnectionStatusText() const {
    return isConnected_ ? "Connected" : "Disconnected";
}

QColor AlertWindow::getAlertColor(AlertType type) const {
    switch (type) {
        case AlertType::CRITICAL:
            return QColor("#d32f2f");  // Red
        case AlertType::WARNING:
            return QColor("#f57c00");  // Orange
        case AlertType::INFO:
        default:
            return QColor("#388e3c");  // Green
    }
}

QString AlertWindow::getAlertIcon(AlertType type) const {
    switch (type) {
        case AlertType::CRITICAL:
            return "🔴";
        case AlertType::WARNING:
            return "🟡";
        case AlertType::INFO:
        default:
            return "🟢";
    }
}

bool AlertWindow::shouldShowAlert(const Alert& alert) const {
    bool showByType = false;
    switch (alert.type) {
        case AlertType::CRITICAL:
            showByType = showCritical_->isChecked();
            break;
        case AlertType::WARNING:
            showByType = showWarning_->isChecked();
            break;
        case AlertType::INFO:
            showByType = showInfo_->isChecked();
            break;
    }

    if (!showByType) {
        return false;
    }

    QString searchText = searchBox_->text().toLower();
    if (searchText.isEmpty()) {
        return true;
    }

    QString alertText = QString::fromStdString(alert.title + " " + alert.message + " " + alert.querySource);
    return alertText.toLower().contains(searchText);
}

void AlertWindow::applyFilters() {
    updateFiltering();
}

void AlertWindow::showSettingsDialog() {
    SettingsDialog dialog(this);

    // Load current config if available
    if (databaseManager_) {
        dialog.setDatabaseConfig(databaseManager_->getConnectionConfig());
    }

    // Load alert system settings
    dialog.setDuplicateDetectionEnabled(alertSystem_->getAlertCount() > 0);
    dialog.setDuplicateTimeWindow(30);  // Default
    dialog.setMaxAlerts(1000);

    if (dialog.exec() == QDialog::Accepted) {
        auto newConfig = dialog.getDatabaseConfig();
        if (validateDatabaseConfig(newConfig)) {
            connectToDatabase(newConfig);
        }

        // Apply alert system settings
        alertSystem_->setDuplicateDetectionEnabled(dialog.isDuplicateDetectionEnabled());
        alertSystem_->setDuplicateTimeWindow(dialog.getDuplicateTimeWindow());
        alertSystem_->setMaxAlerts(dialog.getMaxAlerts());
    }
}

bool AlertWindow::validateDatabaseConfig(const DatabaseManager::ConnectionConfig& config) const {
    if (config.host.empty() || config.database.empty() ||
        config.username.empty() || config.password.empty()) {
        QMessageBox::warning(this, "Invalid Configuration",
                            "Please fill in all database connection fields");
        return false;
    }

    if (config.port <= 0 || config.port > 65535) {
        QMessageBox::warning(this, "Invalid Configuration",
                            "Port must be between 1 and 65535");
        return false;
    }

    return true;
}

// SettingsDialog implementation
SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , testConnectionStatus_(nullptr)
{
    setupUI();
    setWindowTitle("Settings");
    setModal(true);
    resize(400, 300);
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Database connection group
    QGroupBox* dbGroup = new QGroupBox("Database Connection");
    QFormLayout* dbLayout = new QFormLayout(dbGroup);

    hostEdit_ = new QLineEdit("localhost");
    portSpinBox_ = new QSpinBox();
    portSpinBox_->setRange(1, 65535);
    portSpinBox_->setValue(5432);
    databaseEdit_ = new QLineEdit();
    usernameEdit_ = new QLineEdit();
    passwordEdit_ = new QLineEdit();
    passwordEdit_->setEchoMode(QLineEdit::Password);
    timeoutSpinBox_ = new QSpinBox();
    timeoutSpinBox_->setRange(1, 60);
    timeoutSpinBox_->setValue(10);

    testConnectionButton_ = new QPushButton("Test Connection");
    testConnectionStatus_ = new QLabel();
    testConnectionStatus_->setWordWrap(true);

    dbLayout->addRow("Host:", hostEdit_);
    dbLayout->addRow("Port:", portSpinBox_);
    dbLayout->addRow("Database:", databaseEdit_);
    dbLayout->addRow("Username:", usernameEdit_);
    dbLayout->addRow("Password:", passwordEdit_);
    dbLayout->addRow("Timeout (s):", timeoutSpinBox_);
    dbLayout->addRow(testConnectionButton_);
    dbLayout->addRow(testConnectionStatus_);

    // Alert settings group
    QGroupBox* alertGroup = new QGroupBox("Alert Settings");
    QFormLayout* alertLayout = new QFormLayout(alertGroup);

    duplicateDetectionCheck_ = new QCheckBox("Enable duplicate detection");
    duplicateDetectionCheck_->setChecked(true);
    duplicateTimeWindowSpin_ = new QSpinBox();
    duplicateTimeWindowSpin_->setRange(5, 300);
    duplicateTimeWindowSpin_->setValue(30);
    duplicateTimeWindowSpin_->setSuffix(" seconds");
    maxAlertsSpin_ = new QSpinBox();
    maxAlertsSpin_->setRange(100, 10000);
    maxAlertsSpin_->setValue(1000);

    alertLayout->addRow(duplicateDetectionCheck_);
    alertLayout->addRow("Duplicate time window:", duplicateTimeWindowSpin_);
    alertLayout->addRow("Maximum alerts:", maxAlertsSpin_);

    // Dialog buttons
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(dbGroup);
    mainLayout->addWidget(alertGroup);
    mainLayout->addStretch();
    mainLayout->addWidget(buttonBox);

    connect(testConnectionButton_, &QPushButton::clicked, this, &SettingsDialog::testConnection);
}

DatabaseManager::ConnectionConfig SettingsDialog::getDatabaseConfig() const {
    DatabaseManager::ConnectionConfig config;
    config.host = hostEdit_->text().toStdString();
    config.port = portSpinBox_->value();
    config.database = databaseEdit_->text().toStdString();
    config.username = usernameEdit_->text().toStdString();
    config.password = passwordEdit_->text().toStdString();
    config.connectTimeout = timeoutSpinBox_->value();
    return config;
}

void SettingsDialog::setDatabaseConfig(const DatabaseManager::ConnectionConfig& config) {
    hostEdit_->setText(QString::fromStdString(config.host));
    portSpinBox_->setValue(config.port);
    databaseEdit_->setText(QString::fromStdString(config.database));
    usernameEdit_->setText(QString::fromStdString(config.username));
    passwordEdit_->setText(QString::fromStdString(config.password));
    timeoutSpinBox_->setValue(config.connectTimeout);
}

int SettingsDialog::getDuplicateTimeWindow() const {
    return duplicateTimeWindowSpin_->value();
}

void SettingsDialog::setDuplicateTimeWindow(int seconds) {
    duplicateTimeWindowSpin_->setValue(seconds);
}

int SettingsDialog::getMaxAlerts() const {
    return maxAlertsSpin_->value();
}

void SettingsDialog::setMaxAlerts(int maxAlerts) {
    maxAlertsSpin_->setValue(maxAlerts);
}

bool SettingsDialog::isDuplicateDetectionEnabled() const {
    return duplicateDetectionCheck_->isChecked();
}

void SettingsDialog::setDuplicateDetectionEnabled(bool enabled) {
    duplicateDetectionCheck_->setChecked(enabled);
}

void SettingsDialog::testConnection() {
    testConnectionButton_->setEnabled(false);
    testConnectionStatus_->setText("Testing connection...");
    testConnectionStatus_->setStyleSheet("color: blue;");
    QApplication::processEvents();

    DatabaseManager::ConnectionConfig config = getDatabaseConfig();

    try {
        DatabaseManager testManager;
        if (testManager.connect(config)) {
            testConnectionStatus_->setText("Connection successful!");
            testConnectionStatus_->setStyleSheet("color: green;");
        } else {
            testConnectionStatus_->setText("Connection failed: " +
                                          QString::fromStdString(testManager.getLastError()));
            testConnectionStatus_->setStyleSheet("color: red;");
        }
    } catch (const std::exception& e) {
        testConnectionStatus_->setText("Connection error: " + QString(e.what()));
        testConnectionStatus_->setStyleSheet("color: red;");
    }

    testConnectionButton_->setEnabled(true);
}

// AlertDetailsDialog implementation
AlertDetailsDialog::AlertDetailsDialog(const Alert& alert, QWidget *parent)
    : QDialog(parent)
{
    setupUI(alert);
    setWindowTitle("Alert Details");
    setModal(true);
    resize(500, 400);
}

AlertDetailsDialog::~AlertDetailsDialog() = default;

void AlertDetailsDialog::setupUI(const Alert& alert) {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Create detail fields
    titleLabel_ = new QLabel(QString::fromStdString(alert.title));
    titleLabel_->setStyleSheet("font-size: 16px; font-weight: bold;");

    typeLabel_ = new QLabel("Type: " + QString::fromStdString(alert.getTypeString()));
    typeLabel_->setStyleSheet(QString("color: %1; font-weight: bold;").arg(alert.getColor().name()));

    timestampLabel_ = new QLabel("Timestamp: " + alert.timestamp.toString("yyyy-MM-dd hh:mm:ss"));
    querySourceLabel_ = new QLabel("Query Source: " + QString::fromStdString(alert.querySource));

    messageEdit_ = new QTextEdit();
    messageEdit_->setPlainText(QString::fromStdString(alert.message));
    messageEdit_->setMaximumHeight(100);

    rawResultEdit_ = new QTextEdit();
    rawResultEdit_->setPlainText(QString::fromStdString(alert.rawResult));
    rawResultEdit_->setMaximumHeight(150);

    // Layout
    mainLayout->addWidget(titleLabel_);
    mainLayout->addWidget(typeLabel_);
    mainLayout->addWidget(timestampLabel_);
    mainLayout->addWidget(querySourceLabel_);

    mainLayout->addWidget(new QLabel("Message:"));
    mainLayout->addWidget(messageEdit_);

    mainLayout->addWidget(new QLabel("Raw Result:"));
    mainLayout->addWidget(rawResultEdit_);

    // Close button
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

#include "AlertWindow.moc"