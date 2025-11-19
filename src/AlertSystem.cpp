#include "AlertSystem.h"
#include <algorithm>
#include <cctype>
#include <QDebug>

AlertSystem::AlertSystem()
    : nextAlertId_(1)
    , duplicateDetectionEnabled_(true)
    , duplicateTimeWindow_(30)
    , maxAlerts_(1000) {
}

int AlertSystem::addAlert(const Alert& alert) {
    std::lock_guard<std::mutex> lock(alertsMutex_);

    if (duplicateDetectionEnabled_ && isDuplicateInternal(alert)) {
        return -1;  // Duplicate alert not added
    }

    Alert newAlert = alert;
    newAlert.id = nextAlertId_++;
    newAlert.timestamp = QDateTime::currentDateTime();

    alerts_.push_back(newAlert);

    // Enforce maximum alert limit
    if (alerts_.size() > static_cast<size_t>(maxAlerts_)) {
        removeOldestAlerts(alerts_.size() - maxAlerts_);
    }

    qDebug() << "Added alert:" << newAlert.title.c_str()
             << "Type:" << newAlert.getTypeString().c_str()
             << "Total alerts:" << alerts_.size();

    return newAlert.id;
}

int AlertSystem::addAlert(AlertType type, const std::string& title, const std::string& message,
                         const std::string& querySource, const std::string& rawResult) {
    Alert alert;
    alert.type = type;
    alert.title = title;
    alert.message = message;
    alert.querySource = querySource;
    alert.rawResult = rawResult;

    return addAlert(alert);
}

AlertType AlertSystem::classifyAlert(const std::string& alertTypeStr, const pqxx::result& result) {
    std::string lowerType = alertTypeStr;
    std::transform(lowerType.begin(), lowerType.end(), lowerType.begin(), ::tolower);

    if (lowerType == "critical" || lowerType == "error" || lowerType == "fatal") {
        return AlertType::CRITICAL;
    } else if (lowerType == "warning" || lowerType == "warn" || lowerType == "alert") {
        return AlertType::WARNING;
    } else if (lowerType == "info" || lowerType == "information" || lowerType == "notice") {
        return AlertType::INFO;
    }

    // Default classification based on result size and content
    if (result.empty()) {
        return AlertType::INFO;
    }

    // Check for keywords in result content
    bool hasCriticalKeywords = false;
    bool hasWarningKeywords = false;

    for (const auto& row : result) {
        for (size_t i = 0; i < row.size(); ++i) {
            std::string value = row[i].as<std::string>();
            std::transform(value.begin(), value.end(), value.begin(), ::tolower);

            if (value.find("error") != std::string::npos ||
                value.find("fail") != std::string::npos ||
                value.find("critical") != std::string::npos ||
                value.find("breach") != std::string::npos) {
                hasCriticalKeywords = true;
            }

            if (value.find("warning") != std::string::npos ||
                value.find("alert") != std::string::npos ||
                value.find("unusual") != std::string::npos) {
                hasWarningKeywords = true;
            }
        }
    }

    if (hasCriticalKeywords) {
        return AlertType::CRITICAL;
    } else if (hasWarningKeywords) {
        return AlertType::WARNING;
    }

    return AlertType::INFO;
}

AlertType AlertSystem::classifyFromThreshold(const pqxx::result& result, int threshold, AlertType defaultType) {
    if (result.empty()) {
        return AlertType::INFO;
    }

    if (result.size() >= 2) {
        // If result has multiple rows, try to get a count
        try {
            const auto& firstRow = result[0];
            if (firstRow.size() > 0) {
                std::string firstValue = firstRow[0].as<std::string>();
                // Try to parse as integer
                int count = std::stoi(firstValue);
                if (count >= threshold) {
                    return (count >= threshold * 2) ? AlertType::CRITICAL : AlertType::WARNING;
                }
            }
        } catch (const std::exception&) {
            // Not a number, use default type
        }
    }

    return defaultType;
}

bool AlertSystem::isDuplicate(const Alert& alert, int timeWindowSeconds) {
    std::lock_guard<std::mutex> lock(alertsMutex_);

    int originalWindow = duplicateTimeWindow_;
    duplicateTimeWindow_ = timeWindowSeconds;
    bool result = isDuplicateInternal(alert);
    duplicateTimeWindow_ = originalWindow;

    return result;
}

bool AlertSystem::isSimilar(const Alert& alert, int timeWindowSeconds) {
    std::lock_guard<std::mutex> lock(alertsMutex_);

    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-timeWindowSeconds);

    for (const auto& existingAlert : alerts_) {
        if (existingAlert.timestamp < cutoff) {
            continue;
        }

        if (existingAlert.type == alert.type &&
            existingAlert.querySource == alert.querySource &&
            existingAlert.title == alert.title) {
            return true;
        }
    }

    return false;
}

std::vector<Alert> AlertSystem::getRecentAlerts(int maxCount) {
    std::lock_guard<std::mutex> lock(alertsMutex_);

    std::vector<Alert> recentAlerts;
    int count = 0;

    // Iterate from newest to oldest
    auto it = alerts_.rbegin();
    while (it != alerts_.rend() && count < maxCount) {
        recentAlerts.insert(recentAlerts.begin(), *it);
        ++it;
        ++count;
    }

    return recentAlerts;
}

std::vector<Alert> AlertSystem::getAlertsByType(AlertType type, int maxCount) {
    std::lock_guard<std::mutex> lock(alertsMutex_);

    std::vector<Alert> filteredAlerts;
    int count = 0;

    auto it = alerts_.rbegin();
    while (it != alerts_.rend() && count < maxCount) {
        if (it->type == type) {
            filteredAlerts.insert(filteredAlerts.begin(), *it);
            ++count;
        }
        ++it;
    }

    return filteredAlerts;
}

std::vector<Alert> AlertSystem::getAlertsSince(const QDateTime& since) {
    std::lock_guard<std::mutex> lock(alertsMutex_);

    std::vector<Alert> recentAlerts;

    for (const auto& alert : alerts_) {
        if (alert.timestamp >= since) {
            recentAlerts.insert(recentAlerts.begin(), alert);
        }
    }

    return recentAlerts;
}

void AlertSystem::cleanupOldAlerts(int maxAgeSeconds) {
    std::lock_guard<std::mutex> lock(alertsMutex_);

    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-maxAgeSeconds);

    alerts_.erase(
        std::remove_if(alerts_.begin(), alerts_.end(),
                      [cutoff](const Alert& alert) {
                          return alert.timestamp < cutoff;
                      }),
        alerts_.end()
    );

    qDebug() << "Cleaned up old alerts, remaining:" << alerts_.size();
}

void AlertSystem::enforceMaxAlerts(int maxAlerts) {
    std::lock_guard<std::mutex> lock(alertsMutex_);

    if (alerts_.size() > static_cast<size_t>(maxAlerts)) {
        removeOldestAlerts(alerts_.size() - maxAlerts);
    }
}

int AlertSystem::getAlertCount() const {
    std::lock_guard<std::mutex> lock(alertsMutex_);
    return static_cast<int>(alerts_.size());
}

int AlertSystem::getAlertCountByType(AlertType type) const {
    std::lock_guard<std::mutex> lock(alertsMutex_);

    return std::count_if(alerts_.begin(), alerts_.end(),
                        [type](const Alert& alert) {
                            return alert.type == type;
                        });
}

QDateTime AlertSystem::getLastAlertTime() const {
    std::lock_guard<std::mutex> lock(alertsMutex_);

    if (alerts_.empty()) {
        return QDateTime();
    }

    return alerts_.back().timestamp;
}

void AlertSystem::setDuplicateDetectionEnabled(bool enabled) {
    duplicateDetectionEnabled_ = enabled;
}

void AlertSystem::setDuplicateTimeWindow(int seconds) {
    duplicateTimeWindow_ = seconds;
}

void AlertSystem::setMaxAlerts(int maxAlerts) {
    maxAlerts_ = maxAlerts;
}

bool AlertSystem::isDuplicateInternal(const Alert& alert) {
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-duplicateTimeWindow_);

    for (const auto& existingAlert : alerts_) {
        if (existingAlert.timestamp < cutoff) {
            continue;
        }

        if (existingAlert.type == alert.type &&
            existingAlert.querySource == alert.querySource &&
            existingAlert.title == alert.title &&
            existingAlert.message == alert.message) {
            return true;
        }
    }

    return false;
}

bool AlertSystem::isSimilarInternal(const Alert& alert) {
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-duplicateTimeWindow_);

    for (const auto& existingAlert : alerts_) {
        if (existingAlert.timestamp < cutoff) {
            continue;
        }

        if (existingAlert.type == alert.type &&
            existingAlert.querySource == alert.querySource) {
            return true;
        }
    }

    return false;
}

void AlertSystem::removeOldestAlerts(int removeCount) {
    if (removeCount <= 0) {
        return;
    }

    if (removeCount >= static_cast<int>(alerts_.size())) {
        alerts_.clear();
        return;
    }

    alerts_.erase(alerts_.begin(), alerts_.begin() + removeCount);
}