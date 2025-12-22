/**
 * @file dashboard.h
 * @brief Web-based monitoring dashboard for keyhunt
 *
 * Provides a simple embedded HTTP server for real-time
 * monitoring of search progress, performance metrics,
 * and system health.
 */

#ifndef KEYHUNT_CORE_DASHBOARD_H
#define KEYHUNT_CORE_DASHBOARD_H

#include <string>
#include <map>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <sstream>
#include <vector>

#include "bsgs.h"
#include "config.h"

namespace keyhunt {
namespace core {

/**
 * @brief System metrics for dashboard
 */
struct SystemMetrics {
    double cpu_usage_percent = 0.0;
    size_t memory_used_mb = 0;
    size_t memory_total_mb = 0;
    double memory_percent = 0.0;
    size_t gpu_memory_used_mb = 0;
    size_t gpu_memory_total_mb = 0;
    double gpu_utilization = 0.0;
    std::chrono::steady_clock::time_point timestamp;

    std::string to_json() const {
        std::ostringstream oss;
        oss << "{"
            << "\"cpu_usage\":" << cpu_usage_percent << ","
            << "\"memory_used_mb\":" << memory_used_mb << ","
            << "\"memory_total_mb\":" << memory_total_mb << ","
            << "\"memory_percent\":" << memory_percent << ","
            << "\"gpu_memory_used_mb\":" << gpu_memory_used_mb << ","
            << "\"gpu_memory_total_mb\":" << gpu_memory_total_mb << ","
            << "\"gpu_utilization\":" << gpu_utilization
            << "}";
        return oss.str();
    }
};

/**
 * @brief Search status for dashboard
 */
struct SearchStatus {
    bool running = false;
    bool paused = false;
    std::string mode;
    std::string current_range;
    uint64_t keys_checked = 0;
    uint64_t keys_per_second = 0;
    double progress_percent = 0.0;
    size_t results_found = 0;
    std::chrono::seconds elapsed;
    std::chrono::seconds estimated_remaining;
    std::vector<std::string> recent_log;

    std::string to_json() const {
        std::ostringstream oss;
        oss << "{"
            << "\"running\":" << (running ? "true" : "false") << ","
            << "\"paused\":" << (paused ? "true" : "false") << ","
            << "\"mode\":\"" << mode << "\","
            << "\"current_range\":\"" << current_range << "\","
            << "\"keys_checked\":" << keys_checked << ","
            << "\"keys_per_second\":" << keys_per_second << ","
            << "\"progress_percent\":" << progress_percent << ","
            << "\"results_found\":" << results_found << ","
            << "\"elapsed_seconds\":" << elapsed.count() << ","
            << "\"estimated_remaining_seconds\":" << estimated_remaining.count() << ","
            << "\"recent_log\":[";

        for (size_t i = 0; i < recent_log.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "\"" << recent_log[i] << "\"";
        }

        oss << "]}";
        return oss.str();
    }
};

/**
 * @brief Simple embedded HTTP server for dashboard
 */
class DashboardServer {
public:
    explicit DashboardServer(uint16_t port = 8080);
    ~DashboardServer();

    // Non-copyable
    DashboardServer(const DashboardServer&) = delete;
    DashboardServer& operator=(const DashboardServer&) = delete;

    /**
     * @brief Start the server
     */
    void start();

    /**
     * @brief Stop the server
     */
    void stop();

    /**
     * @brief Check if server is running
     */
    bool is_running() const { return running_.load(); }

    /**
     * @brief Update system metrics
     */
    void update_system_metrics(const SystemMetrics& metrics);

    /**
     * @brief Update search status
     */
    void update_search_status(const SearchStatus& status);

    /**
     * @brief Add log message
     */
    void add_log(const std::string& message);

    /**
     * @brief Get server URL
     */
    std::string get_url() const {
        return "http://localhost:" + std::to_string(port_);
    }

private:
    void server_loop();
    std::string handle_request(const std::string& path);
    std::string generate_html_page();
    std::string generate_api_response(const std::string& endpoint);

    uint16_t port_;
    std::atomic<bool> running_{false};
    std::unique_ptr<std::thread> server_thread_;

    mutable std::mutex data_mutex_;
    SystemMetrics system_metrics_;
    SearchStatus search_status_;
    std::vector<std::string> log_messages_;
    static constexpr size_t MAX_LOG_MESSAGES = 100;
};

/**
 * @brief Dashboard HTML template
 */
inline std::string get_dashboard_html() {
    return R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Keyhunt Dashboard</title>
    <style>
        :root {
            --bg-primary: #0d1117;
            --bg-secondary: #161b22;
            --bg-tertiary: #21262d;
            --text-primary: #c9d1d9;
            --text-secondary: #8b949e;
            --accent-green: #3fb950;
            --accent-blue: #58a6ff;
            --accent-yellow: #d29922;
            --accent-red: #f85149;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, sans-serif;
            background: var(--bg-primary);
            color: var(--text-primary);
            min-height: 100vh;
        }
        .container { max-width: 1400px; margin: 0 auto; padding: 20px; }
        .header {
            display: flex; justify-content: space-between; align-items: center;
            padding: 20px 0; border-bottom: 1px solid var(--bg-tertiary);
        }
        .header h1 { font-size: 24px; font-weight: 600; }
        .status-badge {
            padding: 6px 12px; border-radius: 20px; font-size: 12px; font-weight: 600;
        }
        .status-running { background: rgba(63, 185, 80, 0.2); color: var(--accent-green); }
        .status-paused { background: rgba(210, 153, 34, 0.2); color: var(--accent-yellow); }
        .status-stopped { background: rgba(248, 81, 73, 0.2); color: var(--accent-red); }
        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; margin-top: 20px; }
        .card {
            background: var(--bg-secondary); border-radius: 12px; padding: 20px;
            border: 1px solid var(--bg-tertiary);
        }
        .card h2 { font-size: 14px; color: var(--text-secondary); margin-bottom: 16px; text-transform: uppercase; }
        .metric { margin-bottom: 16px; }
        .metric-label { font-size: 12px; color: var(--text-secondary); }
        .metric-value { font-size: 28px; font-weight: 700; margin-top: 4px; }
        .metric-small { font-size: 18px; }
        .progress-bar {
            height: 8px; background: var(--bg-tertiary); border-radius: 4px; overflow: hidden; margin-top: 8px;
        }
        .progress-fill { height: 100%; background: var(--accent-green); transition: width 0.3s; }
        .log-container {
            background: var(--bg-tertiary); border-radius: 8px; padding: 12px;
            max-height: 300px; overflow-y: auto; font-family: monospace; font-size: 12px;
        }
        .log-entry { padding: 4px 0; border-bottom: 1px solid var(--bg-secondary); }
        .controls { display: flex; gap: 10px; margin-top: 20px; }
        .btn {
            padding: 10px 20px; border: none; border-radius: 8px; cursor: pointer;
            font-weight: 600; transition: all 0.2s;
        }
        .btn-primary { background: var(--accent-blue); color: white; }
        .btn-danger { background: var(--accent-red); color: white; }
        .btn:hover { opacity: 0.8; transform: translateY(-1px); }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🔑 Keyhunt Dashboard</h1>
            <span id="status-badge" class="status-badge status-stopped">Stopped</span>
        </div>

        <div class="grid">
            <div class="card">
                <h2>Search Progress</h2>
                <div class="metric">
                    <div class="metric-label">Keys Checked</div>
                    <div class="metric-value" id="keys-checked">0</div>
                </div>
                <div class="metric">
                    <div class="metric-label">Speed</div>
                    <div class="metric-value metric-small" id="speed">0 keys/s</div>
                </div>
                <div class="metric">
                    <div class="metric-label">Progress</div>
                    <div class="progress-bar">
                        <div class="progress-fill" id="progress-fill" style="width: 0%"></div>
                    </div>
                    <div class="metric-value metric-small" id="progress">0%</div>
                </div>
            </div>

            <div class="card">
                <h2>Results</h2>
                <div class="metric">
                    <div class="metric-label">Keys Found</div>
                    <div class="metric-value" id="keys-found" style="color: var(--accent-green)">0</div>
                </div>
                <div class="metric">
                    <div class="metric-label">Current Range</div>
                    <div class="metric-value metric-small" id="current-range" style="font-family: monospace;">-</div>
                </div>
            </div>

            <div class="card">
                <h2>System Resources</h2>
                <div class="metric">
                    <div class="metric-label">CPU Usage</div>
                    <div class="metric-value metric-small" id="cpu-usage">0%</div>
                </div>
                <div class="metric">
                    <div class="metric-label">Memory</div>
                    <div class="metric-value metric-small" id="memory">0 / 0 MB</div>
                </div>
                <div class="metric">
                    <div class="metric-label">GPU Memory</div>
                    <div class="metric-value metric-small" id="gpu-memory">0 / 0 MB</div>
                </div>
            </div>

            <div class="card">
                <h2>Timing</h2>
                <div class="metric">
                    <div class="metric-label">Elapsed</div>
                    <div class="metric-value metric-small" id="elapsed">00:00:00</div>
                </div>
                <div class="metric">
                    <div class="metric-label">Estimated Remaining</div>
                    <div class="metric-value metric-small" id="remaining">-</div>
                </div>
            </div>
        </div>

        <div class="card" style="margin-top: 20px;">
            <h2>Log</h2>
            <div class="log-container" id="log-container">
                <div class="log-entry">Waiting for data...</div>
            </div>
        </div>

        <div class="controls">
            <button class="btn btn-primary" onclick="pauseResume()">Pause/Resume</button>
            <button class="btn btn-danger" onclick="stopSearch()">Stop</button>
        </div>
    </div>

    <script>
        function formatNumber(n) {
            if (n >= 1e15) return (n / 1e15).toFixed(2) + 'P';
            if (n >= 1e12) return (n / 1e12).toFixed(2) + 'T';
            if (n >= 1e9) return (n / 1e9).toFixed(2) + 'G';
            if (n >= 1e6) return (n / 1e6).toFixed(2) + 'M';
            if (n >= 1e3) return (n / 1e3).toFixed(2) + 'K';
            return n.toString();
        }

        function formatTime(seconds) {
            const h = Math.floor(seconds / 3600);
            const m = Math.floor((seconds % 3600) / 60);
            const s = seconds % 60;
            return `${h.toString().padStart(2, '0')}:${m.toString().padStart(2, '0')}:${s.toString().padStart(2, '0')}`;
        }

        async function fetchData() {
            try {
                const [statusRes, metricsRes] = await Promise.all([
                    fetch('/api/status'),
                    fetch('/api/metrics')
                ]);
                const status = await statusRes.json();
                const metrics = await metricsRes.json();

                // Update status badge
                const badge = document.getElementById('status-badge');
                badge.className = 'status-badge ' + (status.running ? (status.paused ? 'status-paused' : 'status-running') : 'status-stopped');
                badge.textContent = status.running ? (status.paused ? 'Paused' : 'Running') : 'Stopped';

                // Update metrics
                document.getElementById('keys-checked').textContent = formatNumber(status.keys_checked);
                document.getElementById('speed').textContent = formatNumber(status.keys_per_second) + ' keys/s';
                document.getElementById('progress').textContent = status.progress_percent.toFixed(4) + '%';
                document.getElementById('progress-fill').style.width = Math.min(status.progress_percent, 100) + '%';
                document.getElementById('keys-found').textContent = status.results_found;
                document.getElementById('current-range').textContent = status.current_range || '-';
                document.getElementById('elapsed').textContent = formatTime(status.elapsed_seconds);
                document.getElementById('remaining').textContent = status.estimated_remaining_seconds > 0 ? formatTime(status.estimated_remaining_seconds) : '-';

                document.getElementById('cpu-usage').textContent = metrics.cpu_usage.toFixed(1) + '%';
                document.getElementById('memory').textContent = `${metrics.memory_used_mb} / ${metrics.memory_total_mb} MB`;
                document.getElementById('gpu-memory').textContent = `${metrics.gpu_memory_used_mb} / ${metrics.gpu_memory_total_mb} MB`;

                // Update log
                if (status.recent_log && status.recent_log.length > 0) {
                    const logContainer = document.getElementById('log-container');
                    logContainer.innerHTML = status.recent_log.map(l => `<div class="log-entry">${l}</div>`).join('');
                    logContainer.scrollTop = logContainer.scrollHeight;
                }
            } catch (e) {
                console.error('Failed to fetch data:', e);
            }
        }

        function pauseResume() { fetch('/api/pause', { method: 'POST' }); }
        function stopSearch() { fetch('/api/stop', { method: 'POST' }); }

        setInterval(fetchData, 1000);
        fetchData();
    </script>
</body>
</html>
)HTML";
}

} // namespace core
} // namespace keyhunt

#endif // KEYHUNT_CORE_DASHBOARD_H
