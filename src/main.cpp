#include <iostream>

#include <fstream>
#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>

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

int main() {
    /* Initialize SDL */
    SDL_Init(SDL_INIT_EVERYTHING);

    SDL_Window* window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 160 * 4, 144 * 4, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetScale(renderer, 4.0f, 4.0f);

    /* Read whole binary file */
    std::ifstream ifs(std::getenv("BINARY"), std::ios_base::binary | std::ios_base::ate);
    auto size = ifs.tellg();
    std::vector<u8> program(size);
    ifs.seekg(0, std::ios_base::beg);
    ifs.read(reinterpret_cast<char *>(program.data()), size);

    /* Initialize Mcu */
    Mcu mcu;

    mcu.load_program(program);

    mcu.interrupts.enabled = true;

    /* Main loop */
    bool done = false;
    while (!done) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT:
                    done = true;
                    break;

                case SDL_KEYDOWN:
                    if (e.key.keysym.sym == SDLK_UP ||
                        e.key.keysym.sym == SDLK_DOWN ||
                        e.key.keysym.sym == SDLK_LEFT ||
                        e.key.keysym.sym == SDLK_RIGHT) {
                        mcu.interrupts.button = true;
                    }
                    break;
                default:
                    break;
            }
        }

        /* Step */
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

    SDL_Quit();

    return 0;
}
