# PostgreSQL Real-Time Monitor

A powerful real-time PostgreSQL database monitoring system that executes custom SQL queries every second and displays results in a Qt-based UI with color-coded alerts (red for critical, yellow for warning, green for info).

## Features

- **Real-time Monitoring**: Executes custom SQL queries every 1 second
- **Color-coded Alerts**: Visual notification system (🔴 Critical, 🟡 Warning, 🟢 Info)
- **Qt-based Interface**: Modern, responsive desktop application
- **Customizable Queries**: Configure your own monitoring SQL queries
- **Database Health Monitoring**: Built-in queries for performance, security, and resource monitoring
- **Alert Management**: Duplicate detection, alert history, and filtering
- **Export Functionality**: Export alerts for analysis and reporting
- **Cross-platform**: Works on Linux, Windows, and macOS

## Quick Start

### Prerequisites

- **Qt6** (Core and Widgets modules)
- **PostgreSQL** client libraries (libpq)
- **libpqxx** (C++ PostgreSQL adapter)
- **CMake** (3.16 or later)
- **C++17 compatible compiler**

### Installation

#### Ubuntu/Debian
```bash
sudo apt update
sudo apt install build-essential cmake
sudo apt install qt6-base-dev qt6-tools-dev
sudo apt install libpq-dev libpqxx-dev
```

#### CentOS/RHEL/Fedora
```bash
# Fedora
sudo dnf install cmake qt6-qtbase-devel postgresql-devel libpqxx-devel

# CentOS/RHEL (with EPEL)
sudo yum install epel-release
sudo yum install cmake qt6-qtbase-devel postgresql-devel libpqxx-devel
```

#### macOS (with Homebrew)
```bash
brew install cmake qt6 postgresql libpqxx
```

#### Building from Source

```bash
git clone <repository-url>
cd Ban_Delta_Breach_Notifier
mkdir build
cd build
cmake ..
make
```

### Configuration

1. **Database Connection**: Configure your PostgreSQL connection in `config/database.conf`:
   ```ini
   [Connection]
   host=localhost
   port=5432
   database=your_database
   username=your_username
   password=your_password
   connect_timeout=10
   ```

2. **Monitor Queries**: Configure custom queries in `config/queries.conf` or use the built-in examples.

3. **Set File Permissions** (recommended):
   ```bash
   chmod 600 config/database.conf
   ```

### Running the Application

```bash
# From build directory
./bin/Ban_Delta_Breach_Notifier

# Or use the make target (if available)
make run
```

## Usage Guide

### First Time Setup

1. **Launch the Application**: Run the executable
2. **Configure Database**: Go to `Tools → Settings` to set up your database connection
3. **Test Connection**: Use the "Test Connection" button to verify connectivity
4. **Start Monitoring**: Click `Tools → Start Monitoring` or press F5

### Main Interface

The main window consists of:

- **Alert List**: Real-time display of generated alerts with color coding
- **Filter Panel**: Filter alerts by type and search functionality
- **Details Panel**: View detailed information about selected alerts
- **Status Bar**: Shows connection status, last update time, and alert count

### Alert Types

- **🔴 Critical**: Security breaches, system failures, high-severity events
- **🟡 Warning**: Performance issues, threshold breaches, medium-severity events
- **🟢 Info**: Normal operations, user activities, low-severity notifications

### Menu Options

#### File Menu
- **Export Alerts** (Ctrl+E): Export alerts to text file
- **Clear All Alerts** (Ctrl+Del): Remove all alerts from the list
- **Exit** (Ctrl+Q): Quit the application

#### Tools Menu
- **Start Monitoring** (F5): Begin real-time monitoring
- **Stop Monitoring** (F6): Pause monitoring
- **Refresh Connection** (F9): Reconnect to database
- **Settings** (Ctrl+,): Configure database and alert settings

#### View Menu
- **Show Details Panel**: Toggle the alert details panel visibility

### Custom Queries

Create your own monitoring queries in `config/queries.conf`:

```ini
[CustomQuery]
name=High Failed Login Count
sql=SELECT CONCAT('Failed logins: ', COUNT(*)) FROM login_attempts WHERE success=false AND timestamp > NOW() - INTERVAL '1 second'
alert_type=warning
threshold=5
enabled=true
```

#### Query Configuration Options

- **name**: Human-readable name for the query
- **sql**: SQL query to execute every second
- **alert_type**: `critical`, `warning`, or `info`
- **threshold**: Minimum numeric value to trigger alert (optional)
- **enabled**: `true` or `false` (optional, defaults to `true`)
- **timeout**: Query timeout in seconds (optional, defaults to 5)

### Built-in Monitoring Queries

The application includes pre-configured queries for:

- **Security Monitoring**: Failed logins, suspicious activity, breach detection
- **Performance Monitoring**: CPU usage, long queries, connection counts
- **Resource Monitoring**: Disk space, memory usage, database size
- **Database Health**: Table locks, replication lag, maintenance needs

## Database Setup

### Creating a Monitoring User

For security, create a dedicated monitoring user with minimal privileges:

```sql
-- Create the monitoring user
CREATE USER monitor_user WITH PASSWORD 'secure_password';

-- Grant necessary privileges
GRANT CONNECT ON DATABASE your_database TO monitor_user;
GRANT USAGE ON SCHEMA public TO monitor_user;
GRANT SELECT ON pg_stat_activity TO monitor_user;
GRANT SELECT ON pg_stat_database TO monitor_user;
GRANT SELECT ON pg_locks TO monitor_user;

-- Grant select on specific tables you need to monitor
GRANT SELECT ON your_table TO monitor_user;
```

### Sample Tables for Demo Queries

```sql
-- Create sample tables for testing
CREATE TABLE security_events (
    id SERIAL PRIMARY KEY,
    severity VARCHAR(20) NOT NULL,
    description TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE login_attempts (
    id SERIAL PRIMARY KEY,
    username VARCHAR(100),
    success BOOLEAN,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    ip_address INET
);

CREATE TABLE system_metrics (
    id SERIAL PRIMARY KEY,
    metric_type VARCHAR(50),
    usage DECIMAL(10,2),
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Insert sample data for testing
INSERT INTO security_events (severity, description) VALUES
('HIGH', 'Unauthorized access attempt detected'),
('MEDIUM', 'Multiple failed login attempts'),
('HIGH', 'Potential data breach detected');

INSERT INTO login_attempts (username, success, ip_address) VALUES
('user1', false, '192.168.1.100'),
('admin', true, '192.168.1.101'),
('user1', false, '192.168.1.100');
```

## Troubleshooting

### Connection Issues

1. **"Connection refused"**: Check if PostgreSQL is running and accepting connections
2. **"Authentication failed"**: Verify username and password in database.conf
3. **"Timeout":** Increase connect_timeout or check network connectivity

### Performance Issues

1. **Slow UI**: Reduce the number of active queries
2. **Database load**: Optimize your SQL queries with proper indexing
3. **Memory usage**: Set appropriate maximum alert limits

### Query Problems

1. **Invalid SQL**: Test your queries in psql first
2. **Permission denied**: Grant SELECT privileges to monitoring user
3. **Timeout errors**: Increase query timeout or optimize queries

## Advanced Configuration

### Environment Variables

You can override configuration with environment variables:

```bash
export PGHOST=localhost
export PGPORT=5432
export PGDATABASE=postgres
export PGUSER=postgres
export PGPASSWORD=password
```

### SSL Connections

For secure connections to remote databases:

```ini
[Connection]
host=secure-db.example.com
port=5432
sslmode=require
sslrootcert=/path/to/ca-certificates.crt
```

### Connection Pooling

Configure for use with PgBouncer or similar connection poolers:

```ini
[Connection]
host=localhost
port=6432  # PgBouncer default port
database=your_database
username=monitor_user
```

## Development

### Building in Debug Mode

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

### Running Tests

```bash
cd build
make run          # Run the application
make debug-run    # Run with GDB debugger
```

### Code Structure

- `include/`: C++ header files
- `src/`: C++ source implementation files
- `config/`: Configuration files
- `main.cpp`: Application entry point

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Support

For issues, questions, or contributions:

1. Check the troubleshooting section above
2. Review the configuration examples
3. Test with the sample database setup
4. Create an issue with detailed information about your environment

## Changelog

### Version 1.0.0
- Initial release
- Real-time PostgreSQL monitoring
- Qt-based UI with color-coded alerts
- Custom query configuration
- Built-in security and performance monitoring queries
- Alert filtering and export functionality
- Cross-platform support