#define STB_RECT_PACK_IMPLEMENTATION
#include "stb/stb_rect_pack.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"

#include "renderer.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdint>

constexpr int ATLAS_W = 1024;
constexpr int ATLAS_H = 1024;

static std::vector<uint32_t> utf8_to_utf32(const std::string& str) {
    std::vector<uint32_t> result;
    size_t i = 0;
    while (i < str.length()) {
        uint32_t codepoint = 0;
        unsigned char c = str[i];
        if (c < 0x80) {
            codepoint = c;
            i += 1;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < str.length()) {
            codepoint = ((c & 0x1F) << 6) | (str[i+1] & 0x3F);
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < str.length()) {
            codepoint = ((c & 0x0F) << 12) | ((str[i+1] & 0x3F) << 6) | (str[i+2] & 0x3F);
            i += 3;
        } else {
            i += 1;
        }
        result.push_back(codepoint);
    }
    return result;
}

ClockRenderer::ClockRenderer() {
    cdata_big_ = new stbtt_packedchar[96];
    cdata_cyr_big_ = new stbtt_packedchar[64];
    cdata_small_ = new stbtt_packedchar[96];
    cdata_cyr_small_ = new stbtt_packedchar[64];
}

ClockRenderer::~ClockRenderer() {
    cleanup();
    delete[] static_cast<stbtt_packedchar*>(cdata_big_);
    delete[] static_cast<stbtt_packedchar*>(cdata_cyr_big_);
    delete[] static_cast<stbtt_packedchar*>(cdata_small_);
    delete[] static_cast<stbtt_packedchar*>(cdata_cyr_small_);
}

bool ClockRenderer::init(int width, int height, const std::string& font_path) {
    window_width_ = width;
    window_height_ = height;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return false;
    }

    window_ = SDL_CreateWindow(
        "ULTRA PRECISE CLOCK",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        window_width_, window_height_,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!window_) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        return false;
    }

    renderer_ = SDL_CreateRenderer(
        window_, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!renderer_) {
        renderer_ = SDL_CreateRenderer(window_, -1, 0);
    }

    if (!renderer_) {
        std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
        return false;
    }

    if (!init_font(font_path)) {
        std::cerr << "Warning: Could not load requested TTF font, trying fallbacks..." << std::endl;
        std::vector<std::string> fallbacks = {
            "/usr/share/fonts/TTF/CaskaydiaCoveNerdFontMono-Bold.ttf",
            "/usr/share/fonts/TTF/CaskaydiaCoveNerdFont-Regular.ttf",
            "/usr/share/fonts/TTF/Rubik[wght].ttf",
            "/usr/share/fonts/dejavu/DejaVuSansMono.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
        };
        bool loaded = false;
        for (const auto& path : fallbacks) {
            if (init_font(path)) {
                loaded = true;
                break;
            }
        }
        if (!loaded) {
            std::cerr << "Error: Failed to load any font!" << std::endl;
            return false;
        }
    }

    return true;
}

bool ClockRenderer::init_font(const std::string& font_path) {
    std::ifstream file(font_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<unsigned char> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return false;
    }

    auto bake_atlas = [&](float font_size, void* ascii_data, void* cyr_data) -> SDL_Texture* {
        stbtt_pack_context pack_ctx;
        std::vector<unsigned char> alpha_atlas(ATLAS_W * ATLAS_H);

        stbtt_pack_range ranges[2]{};
        ranges[0].font_size = font_size;
        ranges[0].first_unicode_codepoint_in_range = 32;
        ranges[0].num_chars = 96;
        ranges[0].chardata_for_range = static_cast<stbtt_packedchar*>(ascii_data);

        ranges[1].font_size = font_size;
        ranges[1].first_unicode_codepoint_in_range = 0x0410; // Cyrillic А
        ranges[1].num_chars = 64; // А..я
        ranges[1].chardata_for_range = static_cast<stbtt_packedchar*>(cyr_data);

        if (!stbtt_PackBegin(&pack_ctx, alpha_atlas.data(), ATLAS_W, ATLAS_H, 0, 1, nullptr)) {
            return nullptr;
        }

        if (!stbtt_PackFontRanges(&pack_ctx, buffer.data(), 0, ranges, 2)) {
            stbtt_PackEnd(&pack_ctx);
            return nullptr;
        }
        stbtt_PackEnd(&pack_ctx);

        std::vector<uint32_t> rgba(ATLAS_W * ATLAS_H);
        for (int i = 0; i < ATLAS_W * ATLAS_H; ++i) {
            uint8_t a = alpha_atlas[i];
            rgba[i] = (static_cast<uint32_t>(a) << 24) | 0x00FFFFFF;
        }

        SDL_Surface* surf = SDL_CreateRGBSurfaceFrom(
            rgba.data(), ATLAS_W, ATLAS_H, 32, ATLAS_W * 4,
            0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000
        );

        if (!surf) return nullptr;

        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
        SDL_FreeSurface(surf);
        if (tex) {
            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        }
        return tex;
    };

    if (font_texture_big_) SDL_DestroyTexture(font_texture_big_);
    if (font_texture_small_) SDL_DestroyTexture(font_texture_small_);

    font_texture_big_ = bake_atlas(font_size_big_, cdata_big_, cdata_cyr_big_);
    font_texture_small_ = bake_atlas(font_size_small_, cdata_small_, cdata_cyr_small_);

    return (font_texture_big_ != nullptr && font_texture_small_ != nullptr);
}

void ClockRenderer::cleanup() {
    if (font_texture_big_) { SDL_DestroyTexture(font_texture_big_); font_texture_big_ = nullptr; }
    if (font_texture_small_) { SDL_DestroyTexture(font_texture_small_); font_texture_small_ = nullptr; }
    if (renderer_) { SDL_DestroyRenderer(renderer_); renderer_ = nullptr; }
    if (window_) { SDL_DestroyWindow(window_); window_ = nullptr; }
    SDL_Quit();
}

void ClockRenderer::handle_resize(int new_w, int new_h) {
    window_width_ = new_w;
    window_height_ = new_h;
}

static void get_char_info(uint32_t cp, stbtt_packedchar* ascii_data, stbtt_packedchar* cyr_data, stbtt_packedchar** out_data, int* out_idx) {
    if (cp >= 32 && cp < 128) {
        *out_data = ascii_data;
        *out_idx = static_cast<int>(cp - 32);
    } else if (cp >= 0x0410 && cp <= 0x044F) {
        *out_data = cyr_data;
        *out_idx = static_cast<int>(cp - 0x0410);
    } else if (cp == 0x0401) { // Ё
        *out_data = cyr_data;
        *out_idx = 5; // Е fallback
    } else if (cp == 0x0451) { // ё
        *out_data = cyr_data;
        *out_idx = 37; // е fallback
    } else {
        *out_data = ascii_data;
        *out_idx = '?' - 32;
    }
}

static float measure_text_utf8(const std::string& text, stbtt_packedchar* ascii_data, stbtt_packedchar* cyr_data) {
    auto cps = utf8_to_utf32(text);
    float x = 0.0f, y = 0.0f;
    for (uint32_t cp : cps) {
        stbtt_packedchar* data = nullptr;
        int idx = 0;
        get_char_info(cp, ascii_data, cyr_data, &data, &idx);
        stbtt_aligned_quad q;
        stbtt_GetPackedQuad(data, ATLAS_W, ATLAS_H, idx, &x, &y, &q, 0);
    }
    return x;
}

void ClockRenderer::draw_text(const std::string& text, int x, int y, float scale, SDL_Color color, bool center_h) {
    bool use_big = (scale > 1.5f);
    SDL_Texture* tex = use_big ? font_texture_big_ : font_texture_small_;
    stbtt_packedchar* ascii_data = static_cast<stbtt_packedchar*>(use_big ? cdata_big_ : cdata_small_);
    stbtt_packedchar* cyr_data = static_cast<stbtt_packedchar*>(use_big ? cdata_cyr_big_ : cdata_cyr_small_);

    if (!tex || !ascii_data || !cyr_data) return;

    auto cps = utf8_to_utf32(text);
    float draw_x = static_cast<float>(x);
    float draw_y = static_cast<float>(y);

    if (center_h) {
        float width = measure_text_utf8(text, ascii_data, cyr_data);
        draw_x = (window_width_ - width) / 2.0f;
    }

    SDL_SetTextureColorMod(tex, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(tex, color.a);

    for (uint32_t cp : cps) {
        stbtt_packedchar* data = nullptr;
        int idx = 0;
        get_char_info(cp, ascii_data, cyr_data, &data, &idx);

        stbtt_aligned_quad q;
        stbtt_GetPackedQuad(data, ATLAS_W, ATLAS_H, idx, &draw_x, &draw_y, &q, 0);

        SDL_Rect src;
        src.x = static_cast<int>(q.s0 * ATLAS_W);
        src.y = static_cast<int>(q.t0 * ATLAS_H);
        src.w = static_cast<int>((q.s1 - q.s0) * ATLAS_W);
        src.h = static_cast<int>((q.t1 - q.t0) * ATLAS_H);

        SDL_Rect dst;
        dst.x = static_cast<int>(q.x0);
        dst.y = static_cast<int>(q.y0);
        dst.w = static_cast<int>(q.x1 - q.x0);
        dst.h = static_cast<int>(q.y1 - q.y0);

        SDL_RenderCopy(renderer_, tex, &src, &dst);
    }
}

void ClockRenderer::render(const PreciseTimeComponents& time_comp, const NtpStatus& ntp_status) {
    // Pure black minimalist background
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);

    SDL_Color white = {255, 255, 255, 255};
    SDL_Color gray = {140, 140, 140, 255};
    SDL_Color dim_gray = {80, 80, 80, 255};

    if (!quantum_mode_) {
        int center_y = window_height_ / 2;

        // Top Bar: Date & Timezone
        std::string top_line = time_comp.date_str + "  |  TIMEZONE: " + time_comp.mode_label + "  |  " + time_comp.tai_str;
        draw_text(top_line, 0, center_y - 100, 1.0f, gray, true);

        // Main Clock Row: HH:MM:SS . mmm uuu nnn ppp ps
        std::string main_clock_str = time_comp.time_str_full;
        draw_text(main_clock_str, 0, center_y - 20, 2.0f, white, true);

        // Sub Row 1: Hardware CPU TSC Ticks
        char tsc_buf[128];
        std::snprintf(tsc_buf, sizeof(tsc_buf), "CPU HARDWARE TSC: %llu TICKS", static_cast<unsigned long long>(time_comp.tsc_ticks));
        draw_text(tsc_buf, 0, center_y + 45, 1.0f, gray, true);

        // Sub Row 2: Atomic Ensemble Telemetry
        char ntp_buf[256];
        if (ntp_status.synced) {
            double offset_us = ntp_status.offset_seconds * 1e6;
            double uncert_us = ntp_status.uncertainty_seconds * 1e6;
            std::snprintf(ntp_buf, sizeof(ntp_buf),
                          "ENSEMBLE: %s (%s)  |  OFFSET: %+.1f us  |  UNCERTAINTY: ±%.1f us  |  SERVERS: %d ACTIVE",
                          ntp_status.status_msg.c_str(),
                          ntp_status.server.c_str(),
                          offset_us,
                          uncert_us,
                          ntp_status.active_servers_count);
            draw_text(ntp_buf, 0, center_y + 85, 1.0f, gray, true);
        } else {
            std::snprintf(ntp_buf, sizeof(ntp_buf), "ENSEMBLE: %s", ntp_status.status_msg.c_str());
            draw_text(ntp_buf, 0, center_y + 85, 1.0f, dim_gray, true);
        }

        // Notification display if active (for 3 seconds)
        if (!notification_msg_.empty() && SDL_GetTicks() - notification_ticks_ < 3000) {
            draw_text(notification_msg_, 0, window_height_ - 55, 1.0f, white, true);
        }

        // Keybindings tip
        draw_text("[SPACE/L] Save Log Snapshot   [P] 10,000-Digit View   [S] Re-sync   [T] UTC/Local   [ESC/Q] Quit", 0, window_height_ - 25, 1.0f, dim_gray, true);
    } else {
        // --- 10,000-DIGIT HYPER-QUANTUM CHRONOMETRY VIEW ---
        std::string top_line = "HYPER-QUANTUM MODE (10,000 SUB-SECOND DECIMALS)  |  " + time_comp.date_str + "  |  " + time_comp.planck_time_str;
        draw_text(top_line, 0, 15, 1.0f, gray, true);

        char main_time[64];
        std::snprintf(main_time, sizeof(main_time), "%02d:%02d:%02d .", time_comp.hours, time_comp.minutes, time_comp.seconds);
        draw_text(main_time, 0, 45, 2.0f, white, true);

        // Render 10,000 sub-second digits in formatted dense matrix across screen
        const std::string& digits = time_comp.quantum_digits;
        int y_offset = 85;
        int digits_per_row = 60;

        for (size_t i = 0; i < digits.length() && y_offset < window_height_ - 35; i += digits_per_row) {
            std::string chunk = digits.substr(i, digits_per_row);
            std::string formatted_row;
            for (size_t j = 0; j < chunk.length(); ++j) {
                formatted_row += chunk[j];
                if ((j + 1) % 10 == 0 && j + 1 < chunk.length()) {
                    formatted_row += " ";
                }
            }
            draw_text(formatted_row, 0, y_offset, 1.0f, (i < 12) ? white : gray, true);
            y_offset += 18;
        }

        draw_text("[P] Return to Standard View    [S] Re-sync    [T] Toggle Local/UTC    [ESC/Q] Quit", 0, window_height_ - 25, 1.0f, dim_gray, true);
    }

    SDL_RenderPresent(renderer_);
}
