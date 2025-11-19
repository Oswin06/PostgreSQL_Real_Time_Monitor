#ifndef QUERYENGINE_H
#define QUERYENGINE_H

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <chrono>
#include <QTimer>
#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QDateTime>

#include "DatabaseManager.h"
#include "AlertSystem.h"

struct QueryConfig {
    std::string id;
    std::string name;
    std::string sql;
    AlertType alertType;
    int threshold;
    bool enabled;
    int timeoutSeconds;

    QueryConfig() : alertType(AlertType::INFO), threshold(0), enabled(true), timeoutSeconds(5) {}

    QueryConfig(const std::string& id, const std::string& name, const std::string& sql,
                AlertType type = AlertType::INFO, int threshold = 0)
        : id(id), name(name), sql(sql), alertType(type), threshold(threshold),
          enabled(true), timeoutSeconds(5) {}
};

struct QueryResult {
    std::string queryId;
    std::string queryName;
    bool success;
    std::string errorMessage;
    pqxx::result data;
    QDateTime timestamp;
    std::chrono::milliseconds executionTime;

    QueryResult() : success(false), executionTime(0) {}

    QueryResult(const std::string& id, const std::string& name)
        : queryId(id), queryName(name), success(false), executionTime(0),
          timestamp(QDateTime::currentDateTime()) {}
};

class QueryEngine : public QObject {
    Q_OBJECT

public:
    explicit QueryEngine(DatabaseManager* dbManager, AlertSystem* alertSystem, QObject *parent = nullptr);
    ~QueryEngine();

    // Query configuration
    bool loadQueriesFromFile(const std::string& filePath);
    bool loadQueriesFromString(const std::string& configData);
    void addQuery(const QueryConfig& query);
    void removeQuery(const std::string& queryId);
    void updateQuery(const QueryConfig& query);

    // Query management
    void enableQuery(const std::string& queryId, bool enabled);
    QueryConfig* getQuery(const std::string& queryId);
    std::vector<QueryConfig> getAllQueries() const;

    // Monitoring control
    void startMonitoring();
    void stopMonitoring();
    bool isMonitoring() const;

    // Configuration
    void setInterval(int milliseconds);
    int getInterval() const;

    void setMaxConcurrentQueries(int maxQueries);
    int getMaxConcurrentQueries() const;

    // Statistics
    int getExecutedQueriesCount() const;
    int getFailedQueriesCount() const;
    QDateTime getLastExecutionTime() const;
    std::chrono::milliseconds getAverageExecutionTime() const;
    std::map<std::string, int> getQueryExecutionCounts() const;

public slots:
    void executeAllQueries();
    void executeQuery(const std::string& queryId);
    void onDatabaseConnectionChanged(bool connected);

signals:
    void queryExecuted(const QueryResult& result);
    void alertGenerated(const Alert& alert);
    void monitoringStarted();
    void monitoringStopped();
    void queryError(const std::string& queryId, const std::string& error);

private slots:
    void onTimerTimeout();
    void onQueryCompleted(const QueryResult& result);

private:
    // Configuration parsing
    bool parseConfigFile(const std::string& content);
    AlertType parseAlertType(const std::string& typeStr) const;
    std::string trimString(const std::string& str) const;

    // Query execution
    QueryResult executeQueryInternal(const QueryConfig& query);
    void processQueryResult(const QueryResult& result);
    void generateAlerts(const QueryResult& result);

    // Alert generation
    void generateDataAlert(const QueryResult& result);
    void generateErrorAlert(const QueryResult& result);
    std::string formatAlertMessage(const QueryConfig& query, const pqxx::result& result);

    // Thread safety
    void lockQueries();
    void unlockQueries();

    // Members
    DatabaseManager* databaseManager_;
    AlertSystem* alertSystem_;
    std::map<std::string, QueryConfig> queries_;
    mutable QMutex queriesMutex_;

    QTimer* timer_;
    bool isMonitoring_;
    int interval_;
    int maxConcurrentQueries_;

    // Statistics
    int totalExecutions_;
    int totalFailures_;
    QDateTime lastExecutionTime_;
    std::chrono::milliseconds totalExecutionTime_;
    std::map<std::string, int> queryExecutionCounts_;
    mutable QMutex statsMutex_;

    // Duplicate detection cache
    struct QueryHash {
        std::string queryId;
        std::string dataHash;
        QDateTime timestamp;

        QueryHash(const std::string& id, const std::string& hash)
            : queryId(id), dataHash(hash), timestamp(QDateTime::currentDateTime()) {}
    };

    std::vector<QueryHash> queryHistory_;
    mutable QMutex historyMutex_;
    static const int MAX_QUERY_HISTORY = 100;

    // Helper methods
    std::string calculateResultHash(const pqxx::result& result) const;
    bool isRecentDuplicate(const std::string& queryId, const std::string& dataHash, int timeWindowSeconds = 5) const;
    void cleanupQueryHistory();
    void updateStatistics(const QueryResult& result);
};

// Query worker for async execution
class QueryWorker : public QObject {
    Q_OBJECT

public:
    explicit QueryWorker(DatabaseManager* dbManager, QObject *parent = nullptr);
    ~QueryWorker();

    void setQuery(const QueryConfig& query);
    void stop();

public slots:
    void execute();

signals:
    void completed(const QueryResult& result);

private:
    DatabaseManager* databaseManager_;
    QueryConfig query_;
    bool shouldStop_;
    QMutex queryMutex_;
};

#endif // QUERYENGINE_H