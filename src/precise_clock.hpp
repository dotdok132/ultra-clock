#ifndef PRECISE_CLOCK_HPP
#define PRECISE_CLOCK_HPP

#include <string>
#include <cstdint>
#include "ntp_client.hpp"

struct PreciseTimeComponents {
    int year;
    int month;
    int day;
    int hours;
    int minutes;
    int seconds;
    int milliseconds;
    int microseconds;
    int nanoseconds;
    int picoseconds;
    uint64_t tsc_ticks;
    double exact_epoch_seconds;
    std::string time_str_full;   // HH:MM:SS.mmm uuu nnn ppp ps
    std::string date_str;        // YYYY-MM-DD
    std::string mode_label;      // "LOCAL (MSK)" or "UTC"
    std::string tai_str;         // TAI Atomic Time (UTC + 37s)
    std::string quantum_digits;  // 1000+ decimal digits of sub-second time
    std::string planck_time_str; // Time in Planck Time units (1 t_P = 5.39e-44 s)
    std::string ego_str_short;   // "3 ЧАСА ЕГО"
    std::string ego_str_full;    // "21 ЧАС 42 МИНУТЫ 40 СЕКУНД ЕГО"
};

class PreciseClock {
public:
    PreciseClock();

    void set_use_local_time(bool local_time);
    bool is_local_time() const;

    PreciseTimeComponents get_current_time(const NtpStatus& ntp_status);
    std::string generate_quantum_digits(const PreciseTimeComponents& time_comp, size_t num_digits) const;
    bool save_snapshot_to_file(const PreciseTimeComponents& time_comp, const NtpStatus& ntp_status, const std::string& filename = "timestamp_log.txt", size_t num_digits = 100000);

private:
    void calibrate_tsc_frequency();

    bool use_local_time_{true};
    double tsc_freq_hz_{2.5e9}; // Calibrated TSC Frequency
    uint64_t last_tsc_{0};
    struct timespec last_ts_{0, 0};
};

#endif // PRECISE_CLOCK_HPP
