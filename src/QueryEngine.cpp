#include "QueryEngine.h"
#include <QApplication>
#include <QDebug>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <openssl/md5.h>  // For hash calculation

QueryEngine::QueryEngine(DatabaseManager* dbManager, AlertSystem* alertSystem, QObject *parent)
    : QObject(parent)
    , databaseManager_(dbManager)
    , alertSystem_(alertSystem)
    , timer_(new QTimer(this))
    , isMonitoring_(false)
    , interval_(1000)  // 1 second default
    , maxConcurrentQueries_(5)
    , totalExecutions_(0)
    , totalFailures_(0)
    , totalExecutionTime_(0)
{
    connect(timer_, &QTimer::timeout, this, &QueryEngine::onTimerTimeout);

    if (databaseManager_) {
        connect(databaseManager_, &DatabaseManager::connectionStatusChanged,
                this, &QueryEngine::onDatabaseConnectionChanged);
    }
}

QueryEngine::~QueryEngine() {
    stopMonitoring();
}

bool QueryEngine::loadQueriesFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        qWarning() << "Failed to open query configuration file:" << filePath.c_str();
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    return loadQueriesFromString(content);
}

bool QueryEngine::loadQueriesFromString(const std::string& configData) {
    QMutexLocker locker(&queriesMutex_);
    queries_.clear();

    return parseConfigFile(configData);
}

void QueryEngine::addQuery(const QueryConfig& query) {
    QMutexLocker locker(&queriesMutex_);
    queries_[query.id] = query;

    qDebug() << "Added query:" << query.id.c_str() << query.name.c_str();
}

void QueryEngine::removeQuery(const std::string& queryId) {
    QMutexLocker locker(&queriesMutex_);
    queries_.erase(queryId);

    qDebug() << "Removed query:" << queryId.c_str();
}

void QueryEngine::updateQuery(const QueryConfig& query) {
    QMutexLocker locker(&queriesMutex_);
    queries_[query.id] = query;
}

void QueryEngine::enableQuery(const std::string& queryId, bool enabled) {
    QMutexLocker locker(&queriesMutex_);
    auto it = queries_.find(queryId);
    if (it != queries_.end()) {
        it->second.enabled = enabled;
    }
}

QueryConfig* QueryEngine::getQuery(const std::string& queryId) {
    QMutexLocker locker(&queriesMutex_);
    auto it = queries_.find(queryId);
    return (it != queries_.end()) ? &(it->second) : nullptr;
}

std::vector<QueryConfig> QueryEngine::getAllQueries() const {
    QMutexLocker locker(&queriesMutex_);
    std::vector<QueryConfig> result;
    for (const auto& pair : queries_) {
        result.push_back(pair.second);
    }
    return result;
}

void QueryEngine::startMonitoring() {
    if (isMonitoring_) {
        qWarning() << "Monitoring is already started";
        return;
    }

    if (!databaseManager_ || !databaseManager_->isConnected()) {
        qWarning() << "Cannot start monitoring: database not connected";
        emit queryError("", "Database not connected");
        return;
    }

    QMutexLocker locker(&queriesMutex_);
    if (queries_.empty()) {
        qWarning() << "Cannot start monitoring: no queries configured";
        emit queryError("", "No queries configured");
        return;
    }

    isMonitoring_ = true;
    timer_->start(interval_);

    qDebug() << "Started monitoring with" << queries_.size() << "queries, interval" << interval_ << "ms";
    emit monitoringStarted();
}

void QueryEngine::stopMonitoring() {
    if (!isMonitoring_) {
        return;
    }

    isMonitoring_ = false;
    timer_->stop();

    qDebug() << "Stopped monitoring";
    emit monitoringStopped();
}

bool QueryEngine::isMonitoring() const {
    return isMonitoring_;
}

void QueryEngine::setInterval(int milliseconds) {
    interval_ = milliseconds;
    if (timer_->isActive()) {
        timer_->setInterval(interval_);
    }
}

int QueryEngine::getInterval() const {
    return interval_;
}

void QueryEngine::setMaxConcurrentQueries(int maxQueries) {
    maxConcurrentQueries_ = maxQueries;
}

int QueryEngine::getMaxConcurrentQueries() const {
    return maxConcurrentQueries_;
}

int QueryEngine::getExecutedQueriesCount() const {
    QMutexLocker locker(&statsMutex_);
    return totalExecutions_;
}

int QueryEngine::getFailedQueriesCount() const {
    QMutexLocker locker(&statsMutex_);
    return totalFailures_;
}

QDateTime QueryEngine::getLastExecutionTime() const {
    QMutexLocker locker(&statsMutex_);
    return lastExecutionTime_;
}

std::chrono::milliseconds QueryEngine::getAverageExecutionTime() const {
    QMutexLocker locker(&statsMutex_);
    if (totalExecutions_ == 0) {
        return std::chrono::milliseconds(0);
    }
    return std::chrono::milliseconds(totalExecutionTime_.count() / totalExecutions_);
}

std::map<std::string, int> QueryEngine::getQueryExecutionCounts() const {
    QMutexLocker locker(&statsMutex_);
    return queryExecutionCounts_;
}

void QueryEngine::executeAllQueries() {
    if (!databaseManager_ || !databaseManager_->isConnected()) {
        qWarning() << "Cannot execute queries: database not connected";
        return;
    }

    std::vector<QueryConfig> enabledQueries;
    {
        QMutexLocker locker(&queriesMutex_);
        for (const auto& pair : queries_) {
            if (pair.second.enabled) {
                enabledQueries.push_back(pair.second);
            }
        }
    }

    if (enabledQueries.empty()) {
        return;
    }

    qDebug() << "Executing" << enabledQueries.size() << "queries";

    for (const auto& query : enabledQueries) {
        executeQuery(query.id);
    }
}

void QueryEngine::executeQuery(const std::string& queryId) {
    QueryConfig* query = getQuery(queryId);
    if (!query) {
        qWarning() << "Query not found:" << queryId.c_str();
        return;
    }

    if (!query->enabled) {
        return;
    }

    // Execute in a separate thread to avoid blocking
    QThread* workerThread = new QThread();
    QueryWorker* worker = new QueryWorker(databaseManager_);
    worker->setQuery(*query);

    worker->moveToThread(workerThread);

    connect(workerThread, &QThread::started, worker, &QueryWorker::execute);
    connect(worker, &QueryWorker::completed, this, &QueryEngine::onQueryCompleted);
    connect(worker, &QueryWorker::completed, workerThread, &QThread::quit);
    connect(worker, &QueryWorker::completed, worker, &QueryWorker::deleteLater);
    connect(workerThread, &QThread::finished, workerThread, &QThread::deleteLater);

    workerThread->start();
}

void QueryEngine::onDatabaseConnectionChanged(bool connected) {
    if (!connected && isMonitoring_) {
        qWarning() << "Database connection lost, stopping monitoring";
        stopMonitoring();
        emit queryError("", "Database connection lost");
    }
}

void QueryEngine::onTimerTimeout() {
    executeAllQueries();
}

void QueryEngine::onQueryCompleted(const QueryResult& result) {
    updateStatistics(result);
    processQueryResult(result);
    cleanupQueryHistory();

    emit queryExecuted(result);
}

bool QueryEngine::parseConfigFile(const std::string& content) {
    std::istringstream stream(content);
    std::string line;
    QueryConfig currentQuery;

    while (std::getline(stream, line)) {
        line = trimString(line);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        // Check for section header
        if (line[0] == '[' && line.back() == ']') {
            // Save previous query if exists
            if (!currentQuery.id.empty()) {
                queries_[currentQuery.id] = currentQuery;
            }

            // Start new query
            currentQuery = QueryConfig();
            currentQuery.id = line.substr(1, line.length() - 2);
            continue;
        }

        // Parse key=value pairs
        size_t separatorPos = line.find('=');
        if (separatorPos != std::string::npos) {
            std::string key = trimString(line.substr(0, separatorPos));
            std::string value = trimString(line.substr(separatorPos + 1));

            if (key == "name") {
                currentQuery.name = value;
            } else if (key == "sql") {
                currentQuery.sql = value;
            } else if (key == "alert_type") {
                currentQuery.alertType = parseAlertType(value);
            } else if (key == "threshold") {
                try {
                    currentQuery.threshold = std::stoi(value);
                } catch (const std::exception&) {
                    currentQuery.threshold = 0;
                }
            } else if (key == "enabled") {
                currentQuery.enabled = (value == "true" || value == "1" || value == "yes");
            } else if (key == "timeout") {
                try {
                    currentQuery.timeoutSeconds = std::stoi(value);
                } catch (const std::exception&) {
                    currentQuery.timeoutSeconds = 5;
                }
            }
        }
    }

    // Save last query
    if (!currentQuery.id.empty()) {
        queries_[currentQuery.id] = currentQuery;
    }

    qDebug() << "Loaded" << queries_.size() << "queries from configuration";
    return !queries_.empty();
}

AlertType QueryEngine::parseAlertType(const std::string& typeStr) const {
    std::string lowerType = typeStr;
    std::transform(lowerType.begin(), lowerType.end(), lowerType.begin(), ::tolower);

    if (lowerType == "critical" || lowerType == "error" || lowerType == "fatal") {
        return AlertType::CRITICAL;
    } else if (lowerType == "warning" || lowerType == "warn" || lowerType == "alert") {
        return AlertType::WARNING;
    } else {
        return AlertType::INFO;
    }
}

std::string QueryEngine::trimString(const std::string& str) const {
    const std::string whitespace = " \t\n\r\f\v";
    size_t start = str.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return "";
    }
    size_t end = str.find_last_not_of(whitespace);
    return str.substr(start, end - start + 1);
}

QueryResult QueryEngine::executeQueryInternal(const QueryConfig& query) {
    QueryResult result(query.id, query.name);
    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        result.data = databaseManager_->executeQuery(query.sql);
        result.success = true;

        // Check for duplicate results
        std::string dataHash = calculateResultHash(result.data);
        if (isRecentDuplicate(query.id, dataHash)) {
            result.success = false;
            result.errorMessage = "Duplicate result ignored";
            return result;
        }

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = e.what();
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    return result;
}

void QueryEngine::processQueryResult(const QueryResult& result) {
    if (result.success && !result.data.empty()) {
        generateDataAlert(result);
    } else if (!result.success) {
        generateErrorAlert(result);
    }
}

void QueryEngine::generateAlerts(const QueryResult& result) {
    // This method can be extended to generate multiple types of alerts
    generateDataAlert(result);
}

void QueryEngine::generateDataAlert(const QueryResult& result) {
    QueryConfig* query = getQuery(result.queryId);
    if (!query) {
        return;
    }

    AlertType alertType = query->alertType;

    // Apply threshold logic
    if (query->threshold > 0) {
        alertType = alertSystem_->classifyFromThreshold(result.data, query->threshold, alertType);
    }

    // Create alert message
    std::string message = formatAlertMessage(*query, result.data);

    // Add alert
    if (alertSystem_) {
        int alertId = alertSystem_->addAlert(alertType, query->name, message,
                                           query->id, "Data returned from query");
        if (alertId > 0) {
            emit alertGenerated(Alert(alertId, alertType, query->name, message,
                                    query->id, "Query executed successfully"));
        }
    }
}

void QueryEngine::generateErrorAlert(const QueryResult& result) {
    QueryConfig* query = getQuery(result.queryId);
    if (!query) {
        return;
    }

    std::string message = "Query execution failed: " + result.errorMessage;

    if (alertSystem_) {
        int alertId = alertSystem_->addAlert(AlertType::WARNING, query->name, message,
                                           query->id, "Query error: " + result.errorMessage);
        if (alertId > 0) {
            emit alertGenerated(Alert(alertId, AlertType::WARNING, query->name, message,
                                    query->id, result.errorMessage));
        }
    }
}

std::string QueryEngine::formatAlertMessage(const QueryConfig& query, const pqxx::result& result) {
    if (result.empty()) {
        return "No results returned from query";
    }

    // Try to format the first column of the first row
    try {
        const auto& firstRow = result[0];
        if (firstRow.size() > 0) {
            std::string firstValue = firstRow[0].as<std::string>();
            if (result.size() == 1) {
                return firstValue;
            } else {
                return firstValue + " (and " + std::to_string(result.size() - 1) + " more rows)";
            }
        }
    } catch (const std::exception&) {
        // Fall back to count
    }

    return "Query returned " + std::to_string(result.size()) + " row(s)";
}

void QueryEngine::lockQueries() {
    queriesMutex_.lock();
}

void QueryEngine::unlockQueries() {
    queriesMutex_.unlock();
}

std::string QueryEngine::calculateResultHash(const pqxx::result& result) const {
    if (result.empty()) {
        return "empty";
    }

    std::stringstream hashStream;
    hashStream << result.size() << "_";

    // Hash the first few rows and columns
    int maxRows = std::min(3, static_cast<int>(result.size()));
    for (int i = 0; i < maxRows; ++i) {
        const auto& row = result[i];
        int maxCols = std::min(3, static_cast<int>(row.size()));
        for (int j = 0; j < maxCols; ++j) {
            try {
                hashStream << row[j].as<std::string>() << "|";
            } catch (const std::exception&) {
                hashStream << "NULL|";
            }
        }
        hashStream << ";";
    }

    // Simple hash (in production, use proper cryptographic hash)
    std::string hashInput = hashStream.str();
    std::hash<std::string> hasher;
    return std::to_string(hasher(hashInput));
}

bool QueryEngine::isRecentDuplicate(const std::string& queryId, const std::string& dataHash, int timeWindowSeconds) const {
    QMutexLocker locker(&historyMutex_);
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-timeWindowSeconds);

    for (const auto& entry : queryHistory_) {
        if (entry.queryId == queryId && entry.dataHash == dataHash && entry.timestamp >= cutoff) {
            return true;
        }
    }

    return false;
}

void QueryEngine::cleanupQueryHistory() {
    QMutexLocker locker(&historyMutex_);
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-60);  // Keep 1 minute history

    queryHistory_.erase(
        std::remove_if(queryHistory_.begin(), queryHistory_.end(),
                      [cutoff](const QueryHash& entry) {
                          return entry.timestamp < cutoff;
                      }),
        queryHistory_.end()
    );

    // Keep only the most recent entries
    while (queryHistory_.size() > MAX_QUERY_HISTORY) {
        queryHistory_.erase(queryHistory_.begin());
    }
}

void QueryEngine::updateStatistics(const QueryResult& result) {
    QMutexLocker locker(&statsMutex_);

    totalExecutions_++;
    if (!result.success) {
        totalFailures_++;
    }

    totalExecutionTime_ += result.executionTime;
    lastExecutionTime_ = result.timestamp;

    queryExecutionCounts_[result.queryId]++;

    // Update query history for duplicate detection
    if (result.success && !result.data.empty()) {
        QMutexLocker historyLocker(&historyMutex_);
        std::string dataHash = calculateResultHash(result.data);
        queryHistory_.emplace_back(result.queryId, dataHash);
    }
}

// QueryWorker implementation
QueryWorker::QueryWorker(DatabaseManager* dbManager, QObject *parent)
    : QObject(parent)
    , databaseManager_(dbManager)
    , shouldStop_(false)
{
}

QueryWorker::~QueryWorker() = default;

void QueryWorker::setQuery(const QueryConfig& query) {
    QMutexLocker locker(&queryMutex_);
    query_ = query;
    shouldStop_ = false;
}

void QueryWorker::stop() {
    QMutexLocker locker(&queryMutex_);
    shouldStop_ = true;
}

void QueryWorker::execute() {
    QueryResult result;
    QueryConfig query;

    {
        QMutexLocker locker(&queryMutex_);
        if (shouldStop_) {
            return;
        }
        query = query_;
        result.queryId = query.id;
        result.queryName = query.name;
    }

    result.timestamp = QDateTime::currentDateTime();
    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        if (!databaseManager_ || !databaseManager_->isConnected()) {
            throw std::runtime_error("Database not connected");
        }

        result.data = databaseManager_->executeQuery(query.sql);
        result.success = true;

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = e.what();
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    emit completed(result);
}

#include "QueryEngine.moc"