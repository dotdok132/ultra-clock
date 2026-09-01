#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <SDL2/SDL.h>
#include <string>
#include "precise_clock.hpp"
#include "ntp_client.hpp"

class ClockRenderer {
public:
    ClockRenderer();
    ~ClockRenderer();

    bool init(int width, int height, const std::string& font_path);
    void cleanup();

    void render(const PreciseTimeComponents& time_comp, const NtpStatus& ntp_status);
    void handle_resize(int new_w, int new_h);
    void toggle_quantum_mode() { quantum_mode_ = !quantum_mode_; }
    void show_notification(const std::string& msg) {
        notification_msg_ = msg;
        notification_ticks_ = SDL_GetTicks();
    }

    SDL_Window* get_window() const { return window_; }

private:
    bool init_font(const std::string& font_path);
    void draw_text(const std::string& text, int x, int y, float scale, SDL_Color color, bool center_h = false);

    SDL_Window* window_{nullptr};
    SDL_Renderer* renderer_{nullptr};
    int window_width_{1024};
    int window_height_{400};

    // STB font data
    unsigned char* font_buffer_{nullptr};
    SDL_Texture* font_texture_big_{nullptr};
    SDL_Texture* font_texture_small_{nullptr};
    void* cdata_big_{nullptr};       // stbtt_packedchar array (ASCII)
    void* cdata_cyr_big_{nullptr};   // stbtt_packedchar array (Cyrillic)
    void* cdata_small_{nullptr};     // stbtt_packedchar array (ASCII)
    void* cdata_cyr_small_{nullptr}; // stbtt_packedchar array (Cyrillic)

    float font_size_big_{64.0f};
    float font_size_small_{22.0f};
    bool quantum_mode_{false};
    std::string notification_msg_{""};
    uint32_t notification_ticks_{0};
};

#endif // RENDERER_HPP
