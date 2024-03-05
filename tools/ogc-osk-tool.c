/*
 * sdl-ogc-keyboard: an OSK keyboard for the Wii/GameCube consoles
 * Copyright (C) 2024 Alberto Mardegan
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*/

#include "config.h"

#include <SDL.h>
#include <SDL_ttf.h>
#include <endian.h>
#include <malloc.h>
#include <stdbool.h>

/* For wide fonts this might need to be increased. With our font the max width
 * we use if 205 */
#define LAYOUT_TEXTURE_WIDTH 512
#define TEX_FORMAT_VERSION 1

#define CELL_SIZE 8 /* Texture cell size for IA4 format */
#define NUM_CELLS(s) ((s + CELL_SIZE - 1) / CELL_SIZE)
#define ROUND_TO_CELL_SIZE(s) (NUM_CELLS(s) * CELL_SIZE)

typedef struct TextureData {
    int16_t width;
    int16_t height;
    uint8_t key_widths[NUM_ROWS][MAX_BUTTONS_PER_ROW];
    uint8_t key_height;
    void *texels;
} TextureData;


static inline const char *text_by_pos_and_layout(const ButtonRow *br,
                                                 int col,
                                                 int layout_index)
{
    const RowLayout *layout = &br->layouts[layout_index];

    return layout->symbols ? layout->symbols[col] : NULL;
}

static bool font_surface_to_texture(SDL_Surface *surface, uint8_t *texels,
                                    int start_x, int start_y, int pitch)
{
    uint8_t *pixels;
    int alpha_offset;

    /* Font textures are always in 32-bit ARGB format, but endianness matters */
    switch (surface->format->format) {
        case SDL_PIXELFORMAT_BGRA32: alpha_offset = 3; break;
        case SDL_PIXELFORMAT_ARGB32: alpha_offset = 0; break;
        default:
            fprintf(stderr, "Unexpected pixel format %x",
                    surface->format->format);
            return false;
    }

    for (int y = 0; y < surface->h; y++) {
        uint8_t pixel_pair;

        pixels = (uint8_t*)surface->pixels + y * surface->pitch + alpha_offset;
        for (int x = 0; x < surface->w; x++) {
            int tx = start_x + x;
            int ty = start_y + y;
            int cell = (ty / 8) * ((pitch + 7) / 8) + tx / 8;
            int offset = cell * 32 + (ty % 8) * 4 + (tx / 2) % 4;
            if (tx % 2 == 0) {
                pixel_pair = *pixels & 0xf0;
            } else {
                if (x == 0) {
                    /* Take the value from the texture */
                    pixel_pair = texels[offset];
                }
                pixel_pair |= *pixels >> 4;
            }

            texels[offset] = pixel_pair;
            pixels += 4;
        }
    }

    return true;
}

static inline bool write_word(int16_t word, FILE *file)
{
    int16_t value = htobe16(word);
    return fwrite(&value, sizeof(value), 1, file) == sizeof(value);
}

static bool save_texture(const TextureData *texture, int layout_index)
{
    char filename[64];
    FILE *file;
    int16_t version = TEX_FORMAT_VERSION;
    int16_t max_width = 0, rounded_height, width_cells, height_cells;
    int16_t value;

    sprintf(filename, "osk%d.tex", layout_index);
    file = fopen(filename, "wb");
    write_word(version, file);

    for (int row = 0; row < NUM_ROWS; row++) {
        int row_width = 0;
        for (int col = 0; col < MAX_BUTTONS_PER_ROW; col++) {
            row_width += texture->key_widths[row][col];
        }
        if (row_width > max_width)
            max_width = row_width;
    }
    /* It's easier if the width and height are multiple of 8, so we can save
     * whole cells */
    width_cells = NUM_CELLS(max_width);
    max_width = width_cells * CELL_SIZE;
    height_cells = NUM_CELLS(texture->height);
    rounded_height = height_cells * CELL_SIZE;

    write_word(max_width, file);
    write_word(rounded_height, file);
    fwrite(&texture->key_widths[0][0], 1, NUM_ROWS * MAX_BUTTONS_PER_ROW, file);
    fwrite(&texture->key_height, 1, 1, file);
    for (int y = 0; y < height_cells; y++) {
        fwrite(texture->texels + y * (texture->width / 8) * 32, 32, width_cells, file);
    }
    fclose(file);
    return true;
}

static int compute_row_height(TTF_Font *font)
{
    const SDL_Color white = { 0xff, 0xff, 0xff, 0xff };
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, "|", white);
    if (!surface) return -1;

    int height = surface->h;
    SDL_FreeSurface(surface);
    return height;
}

static TextureData *build_layout_texture(const ButtonRow **rows,
                                         int layout_index,
                                         TTF_Font *font)
{
    const char *text;
    SDL_Surface *surface;
    const SDL_Color white = { 0xff, 0xff, 0xff, 0xff };

    int row_height = compute_row_height(font);
    int tex_w = ROUND_TO_CELL_SIZE(LAYOUT_TEXTURE_WIDTH);
    int tex_h = ROUND_TO_CELL_SIZE(row_height * NUM_ROWS);
    uint8_t *texels = malloc(tex_w * tex_h);
    if (!texels) return NULL;
    memset(texels, 0, tex_w * tex_h);

    TextureData *texture = malloc(sizeof(TextureData));
    if (!texture) goto error_alloc_texture;
    memset(texture, 0, sizeof(TextureData));

    int y = 0;
    for (int row = 0; row < NUM_ROWS; row++) {
        const ButtonRow *br = rows[row];
        int x = 0;

        for (int col = 0; col < br->num_keys; col++) {
            text = text_by_pos_and_layout(br, col, layout_index);
            if (!text) continue;

            surface = TTF_RenderUTF8_Blended(font, text, white);
            if (!surface) goto error_render;

            SDL_LockSurface(surface);
            bool ok = font_surface_to_texture(surface, texels, x, y, tex_w);
            SDL_UnlockSurface(surface);
            SDL_FreeSurface(surface);
            if (!ok) goto error_render;

            x += surface->w;
            texture->key_widths[row][col] = surface->w;
        }
        y += row_height;
    }

    texture->width = tex_w;
    texture->height = tex_h;
    texture->key_height = row_height;
    texture->texels = texels;
    return texture;

error_render:
    free(texture);
error_alloc_texture:
    free(texels);
    return NULL;
}

static bool build_layout_textures(const ButtonRow **rows,
                                  const char *font_file, int font_size)
{
    TTF_Init();
    TTF_Font *font = TTF_OpenFont(font_file, font_size);
    if (!font) {
        fprintf(stderr, "Could not open font %s\n", font_file);
        return false;
    }

    for (int i = 0; i < NUM_LAYOUTS; i++) {
        TextureData *texture = build_layout_texture(rows, i, font);
        if (!texture) return false;

        bool ok = save_texture(texture, i);
        if (!ok) return false;
    }
    return true;
}

void show_help()
{
    fputs("\nUsage:\n\n"
          "\togc-osk-tool <font-file> <font-size>\n\n",
          stderr);
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        show_help();
        return EXIT_FAILURE;
    }

    int font_size = atoi(argv[2]);
    bool ok = build_layout_textures(rows, argv[1], font_size);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
