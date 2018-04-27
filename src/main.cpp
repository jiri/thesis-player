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

            return (state[SDL_SCANCODE_UP]    << 0u) |
                   (state[SDL_SCANCODE_DOWN]  << 1u) |
                   (state[SDL_SCANCODE_LEFT]  << 2u) |
                   (state[SDL_SCANCODE_RIGHT] << 3u) |
                   (state[SDL_SCANCODE_Z]     << 4u) |
                   (state[SDL_SCANCODE_X]     << 5u);
        },
    };

    std::queue<u8> serial_buffer {};

    mcu.io_handlers[0x10] = IoHandler {
        .get = [&serial_buffer]() {
            auto tmp = serial_buffer.front();
            serial_buffer.pop();
            return tmp;
        },
        .set = [](u8 chr) {
            putc(chr, stdout);
        },
    };

    u8 keyboard_buffer = 0x00;

    mcu.io_handlers[0x03] = IoHandler {
        .get = [&keyboard_buffer]() {
            return keyboard_buffer;
        },
    };

    /* Main loop */
    bool done = false;
    while (!done) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            bool key_event = false;

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
                            key_event = true;
                            break;
                        default:
                            break;
                    }
                    break;

                default:
                    break;
            }

            if (key_event) {
                mcu.interrupts.button = true;
            }
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
        SDL_SetRenderDrawColor(renderer, 0xFF, 0x00, 0x00, 0xFF);
        SDL_RenderClear(renderer);

        /* Render screen */
        for (u16 y = 0; y < 144; y++) {
            for (u16 x = 0; x < 160; x++) {
                auto byte = mcu.memory[0x8000 + y * 160 + x];
                auto [ r, g, b ] = palette[byte];

                SDL_SetRenderDrawColor(renderer, r, g, b, 0xFF);
                SDL_RenderDrawPoint(renderer, x, y);
            }
        }

        SDL_RenderPresent(renderer);
    }

    putc('\n', stdout);

    SDL_Quit();

    return 0;
}
