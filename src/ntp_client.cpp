#include "ntp_client.hpp"
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#include <chrono>

constexpr uint64_t NTP_TIMESTAMP_DELTA = 2208988800ULL;

struct NtpPacket {
    uint8_t li_vn_mode{0x1b}; // LI=0, VN=3, Mode=3 (client)
    uint8_t stratum{0};
    uint8_t poll{0};
    uint8_t precision{0};
    uint32_t rootDelay{0};
    uint32_t rootDispersion{0};
    uint32_t refId{0};
    uint32_t refTm_s{0};
    uint32_t refTm_f{0};
    uint32_t origTm_s{0};
    uint32_t origTm_f{0};
    uint32_t rxTm_s{0};
    uint32_t rxTm_f{0};
    uint32_t txTm_s{0};
    uint32_t txTm_f{0};
};

static double ntp_time_to_seconds(uint32_t sec_net, uint32_t frac_net) {
    uint32_t sec = ntohl(sec_net);
    uint32_t frac = ntohl(frac_net);
    if (sec == 0) return 0.0;
    double s = static_cast<double>(sec - NTP_TIMESTAMP_DELTA);
    double f = static_cast<double>(frac) / 4294967296.0;
    return s + f;
}

static void get_current_realtime(uint32_t& sec_net, uint32_t& frac_net) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t ntp_sec = static_cast<uint64_t>(ts.tv_sec) + NTP_TIMESTAMP_DELTA;
    uint64_t ntp_frac = (static_cast<uint64_t>(ts.tv_nsec) * 4294967296ULL) / 1000000000ULL;
    sec_net = htonl(static_cast<uint32_t>(ntp_sec));
    frac_net = htonl(static_cast<uint32_t>(ntp_frac));
}

static double get_realtime_seconds() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return static_cast<double>(ts.tv_sec) + (static_cast<double>(ts.tv_nsec) / 1e9);
}

NtpClient::NtpClient() {
    servers_ = {
        "time.nist.gov",        // US NIST Atomic Reference
        "time.google.com",      // Google TrueTime Atomic Pool
        "time.cloudflare.com",  // Cloudflare NTS/NTP Atomic Pool
        "ntp1.vniiftri.ru",     // Russian National Time Standard
        "ptbtime1.ptb.de",      // PTB Atomic Clock Germany
        "pool.ntp.org",         // Global NTP Pool
        "0.pool.ntp.org",
        "1.pool.ntp.org"
    };
}

NtpClient::~NtpClient() {
    stop();
}

void NtpClient::start() {
    if (running_) return;
    running_ = true;
    worker_thread_ = std::thread(&NtpClient::worker_loop, this);
}

void NtpClient::stop() {
    if (!running_) return;
    running_ = false;
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void NtpClient::trigger_sync() {
    force_sync_requested_ = true;
}

NtpStatus NtpClient::get_status() {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return current_status_;
}

bool NtpClient::query_server(const std::string& host, NtpSample& sample) {
    sample.server = host;
    sample.valid = false;

    struct addrinfo hints, *res = nullptr;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    if (getaddrinfo(host.c_str(), "123", &hints, &res) != 0 || !res) {
        return false;
    }

    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        freeaddrinfo(res);
        return false;
    }

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 200000; // 1.2s timeout
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    NtpPacket packet{};
    packet.li_vn_mode = 0x1b; // LI=0, VN=3, Mode=3

    double t1 = get_realtime_seconds();
    get_current_realtime(packet.txTm_s, packet.txTm_f);

    ssize_t sent = sendto(sockfd, &packet, sizeof(packet), 0, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);

    if (sent < (ssize_t)sizeof(packet)) {
        close(sockfd);
        return false;
    }

    NtpPacket response{};
    ssize_t recvd = recvfrom(sockfd, &response, sizeof(response), 0, nullptr, nullptr);
    double t4 = get_realtime_seconds();
    close(sockfd);

    if (recvd < (ssize_t)sizeof(response)) {
        return false;
    }

    double t2 = ntp_time_to_seconds(response.rxTm_s, response.rxTm_f);
    double t3 = ntp_time_to_seconds(response.txTm_s, response.txTm_f);

    if (t2 == 0.0 || t3 == 0.0) {
        return false;
    }

    sample.offset = ((t2 - t1) + (t3 - t4)) / 2.0;
    sample.rtt = (t4 - t1) - (t3 - t2);
    if (sample.rtt < 0.00001) sample.rtt = 0.00001; // floor
    sample.error_bound = sample.rtt / 2.0;
    sample.stratum = static_cast<int>(response.stratum);
    sample.valid = true;

    return true;
}

// Marzullo's Intersection Algorithm for Atomic Clock Ensembles
void NtpClient::apply_marzullo_algorithm(const std::vector<NtpSample>& samples, NtpStatus& status) {
    struct Endpoint {
        double pos;
        int type; // -1 for start, +1 for end
    };

    std::vector<Endpoint> endpoints;
    std::vector<NtpSample> valid_samples;

    for (const auto& s : samples) {
        if (s.valid) {
            valid_samples.push_back(s);
            endpoints.push_back({s.offset - s.error_bound, -1});
            endpoints.push_back({s.offset + s.error_bound, +1});
        }
    }

    if (endpoints.empty()) {
        return;
    }

    std::sort(endpoints.begin(), endpoints.end(), [](const Endpoint& a, const Endpoint& b) {
        if (a.pos != b.pos) return a.pos < b.pos;
        return a.type < b.type;
    });

    int count = 0;
    int max_overlap = 0;
    double best_l = endpoints.front().pos;
    double best_r = endpoints.back().pos;

    for (size_t i = 0; i < endpoints.size(); ++i) {
        count -= endpoints[i].type; // -(-1) = +1 at start, -(+1) = -1 at end
        if (count > max_overlap) {
            max_overlap = count;
            best_l = endpoints[i].pos;
            if (i + 1 < endpoints.size()) {
                best_r = endpoints[i + 1].pos;
            }
        }
    }

    double optimal_offset = (best_l + best_r) / 2.0;
    double uncertainty = (best_r - best_l) / 2.0;
    if (uncertainty < 0.000001) uncertainty = 0.000001;

    double now_sec = get_realtime_seconds();
    if (last_kalman_time_sec_ == 0.0) {
        last_kalman_time_sec_ = now_sec - 1.0;
        kalman_state_.offset = optimal_offset;
    }

    double dt = now_sec - last_kalman_time_sec_;
    if (dt > 0.001) {
        // --- 2-STATE DISCRETE KALMAN FILTER ---
        // 1. Predict state
        double pred_offset = kalman_state_.offset + kalman_state_.drift_rate * dt;
        double pred_drift = kalman_state_.drift_rate;

        // 2. Predict covariance P' = F*P*F^T + Q
        double q00 = 1e-10; // Process noise
        double q11 = 1e-14;
        double p00_p = kalman_state_.p00 + dt * (kalman_state_.p10 + kalman_state_.p01) + dt * dt * kalman_state_.p11 + q00;
        double p01_p = kalman_state_.p01 + dt * kalman_state_.p11;
        double p10_p = kalman_state_.p10 + dt * kalman_state_.p11;
        double p11_p = kalman_state_.p11 + q11;

        // 3. Innovation y = z - H*x
        double z = optimal_offset;
        double r = uncertainty * uncertainty; // Measurement variance R
        double y = z - pred_offset;
        double s = p00_p + r;

        // 4. Kalman Gain K = P'*H^T / S
        double k0 = p00_p / s;
        double k1 = p10_p / s;

        // 5. Update state
        kalman_state_.offset = pred_offset + k0 * y;
        kalman_state_.drift_rate = pred_drift + k1 * y;

        // 6. Update covariance P = (I - K*H)*P'
        kalman_state_.p00 = (1.0 - k0) * p00_p;
        kalman_state_.p01 = (1.0 - k0) * p01_p;
        kalman_state_.p10 = p10_p - k1 * p00_p;
        kalman_state_.p11 = p11_p - k1 * p01_p;

        last_kalman_time_sec_ = now_sec;
    }

    double avg_rtt = 0.0;
    int min_stratum = 99;
    std::string primary_server;

    for (const auto& s : valid_samples) {
        avg_rtt += s.rtt;
        if (s.stratum < min_stratum) {
            min_stratum = s.stratum;
            primary_server = s.server;
        }
    }
    avg_rtt /= valid_samples.size();

    auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    status.offset_seconds = kalman_state_.offset;
    status.drift_rate_s_per_s = kalman_state_.drift_rate;
    status.rtt_seconds = avg_rtt;
    status.uncertainty_seconds = std::sqrt(kalman_state_.p00);
    status.synced = true;
    status.stratum = min_stratum;
    status.active_servers_count = static_cast<int>(valid_samples.size());
    status.server = primary_server;
    status.status_msg = "AEROSPACE KALMAN ATOMIC FILTER (MMSE Optimal)";
    status.last_sync_timestamp_ns = now_ns;
}

void NtpClient::worker_loop() {
    while (running_) {
        {
            std::lock_guard<std::mutex> lock(status_mutex_);
            current_status_.status_msg = "Querying Global Atomic Reference Servers...";
        }

        std::vector<NtpSample> samples;
        for (const auto& server : servers_) {
            if (!running_) break;
            NtpSample sample;
            if (query_server(server, sample)) {
                samples.push_back(sample);
            }
        }

        {
            std::lock_guard<std::mutex> lock(status_mutex_);
            if (!samples.empty()) {
                apply_marzullo_algorithm(samples, current_status_);
            } else {
                if (!current_status_.synced) {
                    current_status_.status_msg = "OFFLINE (Hardware TSC Monotonic)";
                } else {
                    current_status_.status_msg = "SYNC DRIFT (Re-querying...)";
                }
            }
        }

        // Wait 10 seconds or until forced sync
        force_sync_requested_ = false;
        for (int i = 0; i < 100 && running_ && !force_sync_requested_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}
