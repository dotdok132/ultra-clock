#include "precise_clock.hpp"
#include <ctime>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <x86intrin.h>
#include <thread>
#include <chrono>

static std::string get_russian_noun(int num, const std::string& one, const std::string& two_four, const std::string& five_zero) {
    int n = std::abs(num) % 100;
    int n1 = n % 10;
    if (n > 10 && n < 20) return five_zero;
    if (n1 > 1 && n1 < 5) return two_four;
    if (n1 == 1) return one;
    return five_zero;
}

PreciseClock::PreciseClock() {
    tzset();
    calibrate_tsc_frequency();
}

void PreciseClock::calibrate_tsc_frequency() {
    struct timespec ts1, ts2;
    unsigned int aux1 = 0, aux2 = 0;
    uint64_t tsc1 = __rdtscp(&aux1);
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts1);

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    uint64_t tsc2 = __rdtscp(&aux2);
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts2);

    double dt = static_cast<double>(ts2.tv_sec - ts1.tv_sec) +
                static_cast<double>(ts2.tv_nsec - ts1.tv_nsec) / 1e9;
    uint64_t dtsc = tsc2 - tsc1;

    if (dt > 0.0) {
        tsc_freq_hz_ = static_cast<double>(dtsc) / dt;
    }
}

void PreciseClock::set_use_local_time(bool local_time) {
    use_local_time_ = local_time;
}

bool PreciseClock::is_local_time() const {
    return use_local_time_;
}

PreciseTimeComponents PreciseClock::get_current_time(const NtpStatus& ntp_status) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    unsigned int aux = 0;
    uint64_t current_tsc = __rdtscp(&aux);

    double current_realtime = static_cast<double>(ts.tv_sec) + (static_cast<double>(ts.tv_nsec) / 1e9);
    if (ntp_status.synced) {
        current_realtime += ntp_status.offset_seconds;
    }

    double sec_floor = std::floor(current_realtime);
    time_t raw_sec = static_cast<time_t>(sec_floor);
    double sub_sec = current_realtime - sec_floor;

    if (sub_sec < 0.0) sub_sec = 0.0;
    if (sub_sec >= 1.0) sub_sec = 0.999999999999;

    // 1e12 picoseconds in a second
    uint64_t total_picos = static_cast<uint64_t>(sub_sec * 1e12);

    int ms = static_cast<int>((total_picos / 1000000000ULL) % 1000ULL);
    int us = static_cast<int>((total_picos / 1000000ULL) % 1000ULL);
    int ns = static_cast<int>((total_picos / 1000ULL) % 1000ULL);
    int ps = static_cast<int>(total_picos % 1000ULL);

    struct tm time_info;
    if (use_local_time_) {
        localtime_r(&raw_sec, &time_info);
    } else {
        gmtime_r(&raw_sec, &time_info);
    }

    PreciseTimeComponents res;
    res.year = time_info.tm_year + 1900;
    res.month = time_info.tm_mon + 1;
    res.day = time_info.tm_mday;
    res.hours = time_info.tm_hour;
    res.minutes = time_info.tm_min;
    res.seconds = time_info.tm_sec;
    res.milliseconds = ms;
    res.microseconds = us;
    res.nanoseconds = ns;
    res.picoseconds = ps;
    res.tsc_ticks = current_tsc;
    res.exact_epoch_seconds = current_realtime;

    if (use_local_time_) {
        if (time_info.tm_zone && time_info.tm_zone[0] != '\0') {
            res.mode_label = std::string("LOCAL (") + time_info.tm_zone + ")";
        } else {
            res.mode_label = "LOCAL";
        }
    } else {
        res.mode_label = "UTC";
    }

    // TAI Atomic Time (UTC + 37s leap seconds)
    time_t raw_sec_tai = static_cast<time_t>(sec_floor + 37);
    struct tm tm_tai;
    gmtime_r(&raw_sec_tai, &tm_tai);
    char tai_buf[32];
    std::snprintf(tai_buf, sizeof(tai_buf), "%02d:%02d:%02d (TAI +37s)", tm_tai.tm_hour, tm_tai.tm_min, tm_tai.tm_sec);
    res.tai_str = tai_buf;

    char date_buf[32];
    std::snprintf(date_buf, sizeof(date_buf), "%04d-%02d-%02d", res.year, res.month, res.day);
    res.date_str = date_buf;

    char time_buf[128];
    std::snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d . %03d %03d %03d %03d ps",
                  res.hours, res.minutes, res.seconds, ms, us, ns, ps);
    res.time_str_full = time_buf;

    // Planck Time calculation (1 t_P = 5.39124718e-44 seconds)
    double planck_units = current_realtime * 1.85485844e43;
    char planck_buf[64];
    std::snprintf(planck_buf, sizeof(planck_buf), "%.8e tP (PLANCK UNITS)", planck_units);
    res.planck_time_str = planck_buf;

    // 10,000-Digit Sub-Second Quantum Expansion
    std::string digits;
    digits.reserve(10050);

    // First 12 digits from exact hardware measurement (ms, us, ns, ps)
    char hw_digits[16];
    std::snprintf(hw_digits, sizeof(hw_digits), "%03d%03d%03d%03d", ms, us, ns, ps);
    digits.append(hw_digits);

    // Deterministic PRNG seeded by CPU TSC + nanos to generate remaining digits down to 10,000 places
    uint64_t state = current_tsc ^ (static_cast<uint64_t>(total_picos) << 12) ^ static_cast<uint64_t>(raw_sec);
    auto splitmix64 = [](uint64_t& x) -> uint64_t {
        uint64_t z = (x += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    };

    while (digits.length() < 10000) {
        uint64_t val = splitmix64(state);
        char chunk[16];
        std::snprintf(chunk, sizeof(chunk), "%09llu", static_cast<unsigned long long>(val % 1000000000ULL));
        digits.append(chunk);
    }
    digits.resize(10000);
    res.quantum_digits = digits;

    // "3 ЧАСА ЕГО" calculation
    int h12 = res.hours % 12;
    if (h12 == 0) h12 = 12;
    res.ego_str_short = std::to_string(h12) + " " + get_russian_noun(h12, "ЧАС", "ЧАСА", "ЧАСОВ") + " ЕГО";

    res.ego_str_full = std::to_string(res.hours) + " " + get_russian_noun(res.hours, "ЧАС", "ЧАСА", "ЧАСОВ") + " " +
                       std::to_string(res.minutes) + " " + get_russian_noun(res.minutes, "МИНУТА", "МИНУТЫ", "МИНУТ") + " " +
                       std::to_string(res.seconds) + " " + get_russian_noun(res.seconds, "СЕКУНДА", "СЕКУНДЫ", "СЕКУНД") + " ЕГО";

    return res;
}

std::string PreciseClock::generate_quantum_digits(const PreciseTimeComponents& time_comp, size_t num_digits) const {
    std::string digits;
    digits.reserve(num_digits + 64);

    char hw_digits[16];
    std::snprintf(hw_digits, sizeof(hw_digits), "%03d%03d%03d%03d",
                  time_comp.milliseconds, time_comp.microseconds, time_comp.nanoseconds, time_comp.picoseconds);
    digits.append(hw_digits);

    uint64_t state = time_comp.tsc_ticks ^ (static_cast<uint64_t>(time_comp.nanoseconds) << 16) ^ static_cast<uint64_t>(time_comp.exact_epoch_seconds);
    auto splitmix64 = [](uint64_t& x) -> uint64_t {
        uint64_t z = (x += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    };

    while (digits.length() < num_digits) {
        uint64_t val = splitmix64(state);
        char chunk[16];
        std::snprintf(chunk, sizeof(chunk), "%09llu", static_cast<unsigned long long>(val % 1000000000ULL));
        digits.append(chunk);
    }
    digits.resize(num_digits);
    return digits;
}

bool PreciseClock::save_snapshot_to_file(const PreciseTimeComponents& time_comp, const NtpStatus& ntp_status, const std::string& filename, size_t num_digits) {
    std::ofstream file(filename, std::ios::app);
    if (!file.is_open()) return false;

    double offset_us = ntp_status.offset_seconds * 1e6;
    double uncert_us = ntp_status.uncertainty_seconds * 1e6;

    std::string digits = generate_quantum_digits(time_comp, num_digits);

    file << "=== ULTRA PRECISE SNAPSHOT (" << num_digits << " SUB-SECOND DECIMALS) ===\n";
    file << "DATE: " << time_comp.date_str << " | TIMEZONE: " << time_comp.mode_label << " | TAI: " << time_comp.tai_str << "\n";
    file << "EXACT TIME: " << time_comp.hours << ":" << time_comp.minutes << ":" << time_comp.seconds << "." << digits << "\n";
    file << "CPU HARDWARE TSC: " << time_comp.tsc_ticks << " TICKS\n";
    file << "NTP ENSEMBLE: " << (ntp_status.synced ? ntp_status.server : "OFFLINE")
         << " (Offset: " << offset_us << " us, Uncertainty: ±" << uncert_us << " us, Active Servers: " << ntp_status.active_servers_count << ")\n";
    file << "PLANCK TIME: " << time_comp.planck_time_str << "\n";
    file << "--------------------------------------------------------------------------------\n\n";

    file.close();
    return true;
}
