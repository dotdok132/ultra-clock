#ifndef NTP_CLIENT_HPP
#define NTP_CLIENT_HPP

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

struct NtpSample {
    std::string server;
    double offset{0.0};
    double rtt{0.0};
    double error_bound{0.0};
    int stratum{0};
    bool valid{false};
};

struct KalmanClockState {
    double offset{0.0};       // State 0: Time offset (s)
    double drift_rate{0.0};   // State 1: Frequency drift rate (s/s)
    double p00{1e-4};         // Covariance P00
    double p01{0.0};          // Covariance P01
    double p10{0.0};          // Covariance P10
    double p11{1e-8};         // Covariance P11
};

struct NtpStatus {
    bool synced{false};
    double offset_seconds{0.0};        // Optimal ensemble offset (Kalman filtered)
    double drift_rate_s_per_s{0.0};    // Estimated oscillator drift rate (s/s)
    double rtt_seconds{0.0};           // Average RTT
    double uncertainty_seconds{0.0};   // Error bound (± seconds)
    int stratum{0};
    int active_servers_count{0};
    std::string server;
    std::string status_msg{"Initializing Aerospace Kalman Atomic Filter..."};
    uint64_t last_sync_timestamp_ns{0};
};

class NtpClient {
public:
    NtpClient();
    ~NtpClient();

    void start();
    void stop();
    void trigger_sync();
    NtpStatus get_status();

private:
    void worker_loop();
    bool query_server(const std::string& host, NtpSample& sample);
    void apply_marzullo_algorithm(const std::vector<NtpSample>& samples, NtpStatus& status);

    std::vector<std::string> servers_;
    std::atomic<bool> running_{false};
    std::atomic<bool> force_sync_requested_{false};
    std::thread worker_thread_;
    std::mutex status_mutex_;
    NtpStatus current_status_;
    KalmanClockState kalman_state_;
    double last_kalman_time_sec_{0.0};
};

#endif // NTP_CLIENT_HPP
