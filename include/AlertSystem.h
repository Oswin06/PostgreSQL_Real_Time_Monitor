#ifndef ALERTSYSTEM_H
#define ALERTSYSTEM_H

#include <string>
#include <vector>
#include <deque>
#include <chrono>
#include <memory>
#include <mutex>
#include <QColor>
#include <QDateTime>

enum class AlertType {
    CRITICAL,
    WARNING,
    INFO
};

struct Alert {
    int id;
    AlertType type;
    std::string title;
    std::string message;
    std::string querySource;
    QDateTime timestamp;
    std::string rawResult;

    Alert() : id(0), type(AlertType::INFO) {}

    Alert(int id, AlertType type, const std::string& title, const std::string& message,
          const std::string& querySource, const std::string& rawResult = "")
        : id(id), type(type), title(title), message(message),
          querySource(querySource), timestamp(QDateTime::currentDateTime()),
          rawResult(rawResult) {}

    QColor getColor() const {
        switch (type) {
            case AlertType::CRITICAL:
                return QColor("#d32f2f");  // Red
            case AlertType::WARNING:
                return QColor("#f57c00");  // Orange/Yellow
            case AlertType::INFO:
            default:
                return QColor("#388e3c");  // Green
        }
    }

    std::string getTypeString() const {
        switch (type) {
            case AlertType::CRITICAL:
                return "CRITICAL";
            case AlertType::WARNING:
                return "WARNING";
            case AlertType::INFO:
            default:
                return "INFO";
        }
    }

    QString getFormattedTimestamp() const {
        QDateTime now = QDateTime::currentDateTime();
        qint64 secondsDiff = timestamp.secsTo(now);

        if (secondsDiff < 5) {
            return "Just now";
        } else if (secondsDiff < 60) {
            return QString("%1 seconds ago").arg(secondsDiff);
        } else if (secondsDiff < 3600) {
            return QString("%1 minutes ago").arg(secondsDiff / 60);
        } else if (secondsDiff < 86400) {
            return QString("%1 hours ago").arg(secondsDiff / 3600);
        } else {
            return timestamp.toString("MMM dd, yyyy hh:mm:ss");
        }
    }
};

class AlertSystem {
public:
    explicit AlertSystem();
    ~AlertSystem() = default;

    // Alert management
    int addAlert(const Alert& alert);
    int addAlert(AlertType type, const std::string& title, const std::string& message,
                 const std::string& querySource, const std::string& rawResult = "");

    // Alert classification
    AlertType classifyAlert(const std::string& alertTypeStr, const pqxx::result& result);
    AlertType classifyFromThreshold(const pqxx::result& result, int threshold, AlertType defaultType);

    // Duplicate detection
    bool isDuplicate(const Alert& alert, int timeWindowSeconds = 30);
    bool isSimilar(const Alert& alert, int timeWindowSeconds = 60);

    // Alert retrieval
    std::vector<Alert> getRecentAlerts(int maxCount = 100);
    std::vector<Alert> getAlertsByType(AlertType type, int maxCount = 100);
    std::vector<Alert> getAlertsSince(const QDateTime& since);

    // Alert cleanup
    void cleanupOldAlerts(int maxAgeSeconds = 86400);  // 24 hours default
    void enforceMaxAlerts(int maxAlerts = 1000);

    // Statistics
    int getAlertCount() const;
    int getAlertCountByType(AlertType type) const;
    QDateTime getLastAlertTime() const;

    // Configuration
    void setDuplicateDetectionEnabled(bool enabled);
    void setDuplicateTimeWindow(int seconds);
    void setMaxAlerts(int maxAlerts);

private:
    std::deque<Alert> alerts_;
    mutable std::mutex alertsMutex_;
    int nextAlertId_;
    bool duplicateDetectionEnabled_;
    int duplicateTimeWindow_;
    int maxAlerts_;

    bool isDuplicateInternal(const Alert& alert);
    bool isSimilarInternal(const Alert& alert);
    void removeOldestAlerts(int removeCount);
};

#endif // ALERTSYSTEM_H