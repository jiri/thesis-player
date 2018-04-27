#include <iostream>

#include <fstream>
#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <queue>
#include <poll.h>

#include <SDL2/SDL.h>

#include <Mcu.hpp>

static std::unordered_map<u8, std::tuple<u8, u8, u8>> palette {
        { 0x00, { 0x00, 0x00, 0x00 } },
        { 0x01, { 0x00, 0x00, 0xAA } },
        { 0x02, { 0x00, 0xAA, 0x00 } },
        { 0x03, { 0x00, 0xAA, 0xAA } },
        { 0x04, { 0xAA, 0x00, 0x00 } },
        { 0x05, { 0xAA, 0x00, 0xAA } },
        { 0x06, { 0xAA, 0x55, 0x00 } },
        { 0x07, { 0xAA, 0xAA, 0xAA } },
        { 0x08, { 0x55, 0x55, 0x55 } },
        { 0x09, { 0x55, 0x55, 0xFF } },
        { 0x0A, { 0x55, 0xFF, 0x55 } },
        { 0x0B, { 0x55, 0xFF, 0xFF } },
        { 0x0C, { 0xFF, 0x55, 0x55 } },
        { 0x0D, { 0xFF, 0x55, 0x55 } },
        { 0x0E, { 0xFF, 0x55, 0xFF } },
        { 0x0F, { 0xFF, 0xFF, 0xFF } },
};

int main(int argc, const char* argv[]) {
    if (argc != 2) {
        printf("Please provide a path to binary");
    }

    /* Initialize SDL */
    SDL_Init(SDL_INIT_EVERYTHING);

    SDL_Window* window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 160 * 4, 144 * 4, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetScale(renderer, 4.0f, 4.0f);

    SDL_Surface* surface = SDL_LoadBMP("res/font_8x8.bmp");
    SDL_SetColorKey(surface, SDL_TRUE, SDL_MapRGB(surface->format, 0xFF, 0x00, 0xFF));
    SDL_Texture* font = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    /* Read whole binary file */
    std::ifstream ifs(argv[1], std::ios_base::binary | std::ios_base::ate);
    auto size = ifs.tellg();
    std::vector<u8> program(size);
    ifs.seekg(0, std::ios_base::beg);
    ifs.read(reinterpret_cast<char *>(program.data()), size);

    /* Initialize Mcu */
    Mcu mcu;
    mcu.load_program(program);

    mcu.io_handlers[0x02] = IoHandler {
        .get = []() {
            const u8* state = SDL_GetKeyboardState(nullptr);

            return (state[SDL_SCANCODE_UP]     << 0u) |
                   (state[SDL_SCANCODE_DOWN]   << 1u) |
                   (state[SDL_SCANCODE_LEFT]   << 2u) |
                   (state[SDL_SCANCODE_RIGHT]  << 3u) |
                   (state[SDL_SCANCODE_Z]      << 4u) |
                   (state[SDL_SCANCODE_X]      << 5u) |
                   (state[SDL_SCANCODE_RETURN] << 6u);
        },
    };

    u8 keyboard_buffer = 0x00;

    mcu.io_handlers[0x03] = IoHandler {
        .get = [&keyboard_buffer]() {
            return keyboard_buffer;
        },
    };

    std::queue<u8> serial_buffer {};

    mcu.io_handlers[0x10] = IoHandler {
            .get = [&serial_buffer]() -> u8 {
                if (serial_buffer.empty()) {
                    return 0x00;
                }

                auto tmp = serial_buffer.front();
                serial_buffer.pop();
                return tmp;
            },
            .set = [](u8 chr) {
                putc(chr, stdout);
            },
    };

    enum class DisplayMode {
        None,
        Graphical,
        Text,
    };

    DisplayMode display_mode = DisplayMode::None;

    mcu.io_handlers[0x05] = IoHandler {
        .set = [&display_mode](u8 mode) {
            switch (mode) {
                case 0x10:
                    display_mode = DisplayMode::Graphical;
                    break;
                case 0x13:
                    display_mode = DisplayMode::Text;
                    break;
                default:
                    display_mode = DisplayMode::None;
            }
        },
    };

    /* Main loop */
    bool done = false;
    while (!done) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            bool button_event = false;

            switch (e.type) {
                case SDL_QUIT:
                    done = true;
                    break;

                case SDL_KEYDOWN:
                    if (e.key.keysym.sym >= SDLK_a && e.key.keysym.sym <= SDLK_z) {
                        mcu.interrupts.keyboard = true;
                        keyboard_buffer = static_cast<u8>(e.key.keysym.sym);
                    }
                case SDL_KEYUP:
                    switch (e.key.keysym.sym) {
                        case SDLK_UP:
                        case SDLK_DOWN:
                        case SDLK_LEFT:
                        case SDLK_RIGHT:
                        case SDLK_z:
                        case SDLK_x:
                        case SDLK_RETURN:
                            button_event = true;
                            break;
                        default:
                            break;
                    }
                    break;

                default:
                    break;
            }

            if (button_event) {
                mcu.interrupts.button = true;
            }
        }

        if (done) {
            break;
        }

        /* Read from stdin */
        pollfd fds[1];

        fds[0].fd = fileno(stdin);
        fds[0].events = POLLIN;

        if (poll(fds, 1, 0) == 1) {
            std::string line;
            getline(std::cin, line);

            for (auto c : line) {
                serial_buffer.push(c);
            }

            mcu.interrupts.serial = true;
        }

        /* Step */
        mcu.interrupts.vblank = true;

        for (int i = 0; i < 266667; i++) {
            mcu.step();
        }

        /* Render */
        SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
        SDL_RenderClear(renderer);

        /* Render screen */
        switch (display_mode) {
            case DisplayMode::None:
                break;

            case DisplayMode::Graphical:
                for (u16 y = 0; y < 144; y++) {
                    for (u16 x = 0; x < 160; x++) {
                        auto byte = mcu.memory[0x8000 + y * 160 + x];
                        auto [ r, g, b ] = palette[byte];

                        SDL_SetRenderDrawColor(renderer, r, g, b, 0xFF);
                        SDL_RenderDrawPoint(renderer, x, y);
                    }
                }
                break;

            case DisplayMode::Text:
                for (u16 y = 0; y < 18; y++) {
                    for (u16 x = 0; x < 20; x++) {
                        auto clr = mcu.memory[0x8000 + y * 20 * 2 + x * 2 + 0];
                        auto chr = mcu.memory[0x8000 + y * 20 * 2 + x * 2 + 1];

                        auto [ br, bg, bb ] = palette[(clr & 0x0F) >> 0];
                        auto [ fr, fg, fb ] = palette[(clr & 0xF0) >> 4];

                        SDL_Rect src { (chr % 16) * 8, (chr / 16) * 8, 8, 8 };
                        SDL_Rect dst { x * 8, y * 8, 8, 8 };

                        SDL_SetRenderDrawColor(renderer, br, bg, bb, 0xFF);
                        SDL_RenderFillRect(renderer, &dst);

                        SDL_SetTextureColorMod(font, fr, fg, fb);
                        SDL_RenderCopy(renderer, font, &src, &dst);
                    }
                }
                break;
        }

        SDL_RenderPresent(renderer);
    }

    putc('\n', stdout);
    SDL_Quit();

    return 0;
}
