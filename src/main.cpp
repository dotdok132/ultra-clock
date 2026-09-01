#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <SDL2/SDL.h>
#include "ntp_client.hpp"
#include "precise_clock.hpp"
#include "renderer.hpp"

int main(int argc, char* argv[]) {
    std::string font_path = "/usr/share/fonts/TTF/CaskaydiaCoveNerdFontMono-Bold.ttf";
    std::string log_filename = "timestamp_100k_log.txt";
    size_t num_digits = 100000;
    bool snapshot_only = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--snapshot" || arg == "-s" || arg == "--snapshot-100k") {
            snapshot_only = true;
        } else if (arg == "--log" && i + 1 < argc) {
            log_filename = argv[++i];
            snapshot_only = true;
        } else if (arg == "--digits" && i + 1 < argc) {
            num_digits = std::stoull(argv[++i]);
        } else if (arg[0] != '-') {
            font_path = arg;
        }
    }

    // Command-line non-GUI snapshot mode
    if (snapshot_only) {
        NtpClient ntp_client;
        ntp_client.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        NtpStatus status = ntp_client.get_status();
        PreciseClock clock;
        PreciseTimeComponents time_comp = clock.get_current_time(status);
        clock.save_snapshot_to_file(time_comp, status, log_filename, num_digits);
        std::cout << "Exact timestamp (" << num_digits << " digits) saved to " << log_filename << std::endl;
        std::cout << "[" << time_comp.date_str << " " << time_comp.time_str_full << " | " << time_comp.mode_label << "]" << std::endl;
        ntp_client.stop();
        return 0;
    }

    ClockRenderer renderer;
    if (!renderer.init(1024, 400, font_path)) {
        std::cerr << "Failed to initialize SDL renderer!" << std::endl;
        return 1;
    }

    NtpClient ntp_client;
    ntp_client.start();

    PreciseClock precise_clock;

    bool running = true;
    SDL_Event event;

    while (running) {
        NtpStatus ntp_status = ntp_client.get_status();
        PreciseTimeComponents time_comp = precise_clock.get_current_time(ntp_status);

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    renderer.handle_resize(event.window.data1, event.window.data2);
                }
            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                    case SDLK_q:
                        running = false;
                        break;
                    case SDLK_s:
                        ntp_client.trigger_sync();
                        break;
                    case SDLK_t:
                        precise_clock.set_use_local_time(!precise_clock.is_local_time());
                        break;
                    case SDLK_p:
                        renderer.toggle_quantum_mode();
                        break;
                    case SDLK_l:
                    case SDLK_SPACE:
                    case SDLK_1:
                        precise_clock.save_snapshot_to_file(time_comp, ntp_status, log_filename, num_digits);
                        renderer.show_notification("[100,000-DIGIT SNAPSHOT SAVED TO " + log_filename + "]");
                        break;
                }
            }
        }

        renderer.render(time_comp, ntp_status);
        SDL_Delay(5);
    }

    ntp_client.stop();
    renderer.cleanup();

    return 0;
}
