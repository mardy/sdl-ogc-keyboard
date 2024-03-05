#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "SDL.h"
#include "SDL_ttf.h"

#ifdef __wii__
#include "ogc_keyboard.h"
#endif

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *background;
static TTF_Font *font;

static SDL_Texture *btn_input1_tex = NULL;
static SDL_Texture *btn_quit_tex = NULL;
static const SDL_Rect btn_input1_rect = {5, 5, 200, 30};
static const SDL_Rect btn_quit_rect = { 30, 400, 500, 30 };

/* Declare binary resources embedded into executable */
extern char _binary_DejaVuSans_ttf_start[], _binary_DejaVuSans_ttf_end[];

static SDL_Texture *build_text(const char *text)
{
    const SDL_Color white = { 0xff, 0xff, 0xff, 0xff };

    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, white);
    if (!surface) return NULL;

    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return tex;
}

static void draw_texture(SDL_Texture *tex, int x, int y)
{
    SDL_Rect dest = { x, y, 0, 0 };
    SDL_QueryTexture(tex, NULL, NULL, &dest.w, &dest.h);
    SDL_RenderCopy(renderer, tex, NULL, &dest);
}

static void draw_ui()
{

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);


    SDL_SetRenderDrawColor(renderer, 96, 0, 0, 196);
    SDL_RenderFillRect(renderer, &btn_input1_rect);
    draw_texture(btn_input1_tex, 10, 10);

    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 196);
    SDL_RenderFillRect(renderer, &btn_quit_rect);
    draw_texture(btn_quit_tex, btn_quit_rect.x + 200, btn_quit_rect.y + 5);
}

static bool loop()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_TEXTEDITING:
            printf("EDIT %s", event.edit.text);
            break;
        case SDL_TEXTEDITING_EXT:
            printf("EDIT_EXT %s", event.editExt.text);
            SDL_free(event.editExt.text);
            break;
        case SDL_TEXTINPUT:
            printf("INPUT %s", event.text.text);
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button != SDL_BUTTON_LEFT) break;

            SDL_Point pt = { event.button.x, event.button.y };
            if (SDL_PointInRect(&pt, &btn_quit_rect)) {
                return true;
            } else if (SDL_PointInRect(&pt, &btn_input1_rect)) {
                if (SDL_IsTextInputActive()) {
                    SDL_Log("Stopping text input\n");
                    SDL_StopTextInput();
                } else {
                    SDL_Log("Starting text input\n");
                    SDL_StartTextInput();
                }
            }
            break;
        case SDL_QUIT:
            return true;
            break;
        default:
            break;
        }
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, background, NULL, NULL);
    draw_ui();
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderPresent(renderer);
    return false;
}

SDL_Texture *load_background(SDL_Renderer *renderer)
{
    SDL_Surface *surface = SDL_LoadBMP("background.bmp");
    if (!surface) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error loading background");
        return NULL;
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

int main(int argc, char *argv[])
{
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    chdir("/");
#ifdef __wii__
    SDL_OGC_RegisterVkPlugin(ogc_keyboard_get_plugin());
#endif

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    TTF_Init();
    SDL_RWops *pFontMem = SDL_RWFromConstMem(_binary_DejaVuSans_ttf_start,
                                             _binary_DejaVuSans_ttf_end - _binary_DejaVuSans_ttf_start);
    font = TTF_OpenFontRW(pFontMem, 1, 16);


    window = SDL_CreateWindow("OSK example",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              640, 480, 0);
    renderer = SDL_CreateRenderer(window, -1, 0);

    background = load_background(renderer);

    btn_input1_tex = build_text("Input without control:");
    btn_quit_tex = build_text("Quit");

    SDL_PumpEvents();

    bool done = 0;
    while (!done) {
        done = loop();
    }

    SDL_Quit();
    return 0;
}

/* vi: set ts=4 sw=4 expandtab: */