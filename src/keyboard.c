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

#include "ogc_keyboard.h"

#include "config.h"

#include <SDL.h>
#include <malloc.h>
#include <ogc/cache.h>
#include <ogc/gx.h>
#include <wiiuse/wpad.h>

#define ANIMATION_TIME_ENTER 1000
#define ANIMATION_TIME_EXIT 500
#define ROW_HEIGHT 40
#define ROW_SPACING 12
#define KEYBOARD_HEIGHT (NUM_ROWS * (ROW_HEIGHT + ROW_SPACING))
#define FOCUS_BORDER 4
/* For wide fonts this might need to be increased. With our font the max width
 * we use if 205 */
#define LAYOUT_TEXTURE_WIDTH 256
#define TEX_FORMAT_VERSION 1
#define INPUTBOX_SIDE_MARGIN 50
#define INPUTBOX_HEIGHT ROW_HEIGHT
#define INPUTBOX_SIDE_PADDING 2
#define INPUT_CURSOR_WIDTH 4
#define INPUT_CURSOR_BLINK_MS 800
#define MAX_INPUT_LEN 128

#define PIPELINE_UNTEXTURED 0
#define PIPELINE_TEXTURED   1

typedef struct Rect {
    int16_t x, y, w, h;
} Rect;

typedef struct TextureData {
    int16_t width;
    int16_t height;
    uint8_t key_widths[NUM_ROWS][MAX_BUTTONS_PER_ROW];
    uint8_t key_height;
    void *texels;
} TextureData;

typedef uint8_t KeyID;

struct SDL_OGC_DriverData {
    int16_t screen_width;
    int16_t screen_height;
    int16_t start_pan_y;
    int16_t target_pan_y;
    int16_t input_panel_visible_height;
    int16_t input_panel_start_visible_height;
    int16_t input_panel_target_visible_height;
    int16_t input_cursor_x;
    int16_t input_scroll_x;
    int8_t focus_row;
    int8_t focus_col;
    int8_t highlight_row;
    int8_t highlight_col;
    int8_t active_layout;
    uint8_t text_len;
    bool should_stop_text_input;
    int visible_height;
    uint32_t input_cursor_start_ticks;
    int start_ticks;
    int start_visible_height;
    int target_visible_height;
    int animation_time;
    uint32_t key_color;
    /* Not characters, but key IDs */
    KeyID text[MAX_INPUT_LEN];
    SDL_Cursor *app_cursor;
    SDL_Cursor *default_cursor;
    TextureData layout_textures[NUM_LAYOUTS];
};

static const uint32_t ColorKeyboardBg = 0x0e0e12ff;
static const uint32_t ColorKeyBgLetter = 0x5a606aff;
static const uint32_t ColorKeyBgLetterHigh = 0x2d3035ff;
static const uint32_t ColorKeyBgEnter = 0x003c00ff;
static const uint32_t ColorKeyBgEnterHigh = 0x32783eff;
static const uint32_t ColorKeyBgSpecial = 0x32363eff;
static const uint32_t ColorKeyBgSpecialHigh = 0x191b1fff;
static const uint32_t ColorFocus = 0xe0f010ff;
static const uint32_t ColorInputPanelBg = 0x1c1c24ff;
static const uint32_t ColorInputCursor = ColorKeyBgLetter;

static void HideScreenKeyboard(SDL_OGC_VkContext *context);

static inline KeyID key_id_from_pos(int layout_index, int row, int col)
{
    return layout_index * (NUM_ROWS * MAX_BUTTONS_PER_ROW) +
        row * MAX_BUTTONS_PER_ROW + col;
}

static inline void key_id_to_pos(KeyID key_id,
                                 int *layout_index, int *row, int *col)
{
    *col = key_id % MAX_BUTTONS_PER_ROW;
    key_id /= MAX_BUTTONS_PER_ROW;
    *row = key_id % NUM_ROWS;
    key_id /= NUM_ROWS;
    *layout_index = key_id;
}

static void free_layout_textures(SDL_OGC_DriverData *data)
{
    for (int i = 0; i < NUM_LAYOUTS; i++) {
        void *texels = data->layout_textures[i].texels;
        if (texels) {
            free(texels);
        }
    }
    memset(data->layout_textures, 0, sizeof(data->layout_textures));
}

static inline const char *text_by_pos_and_layout(int row, int col,
                                                 int layout_index)
{
    const ButtonRow *br = rows[row];
    const RowLayout *layout = &br->layouts[layout_index];

    return layout->symbols ? layout->symbols[col] : NULL;
}

static inline const char *text_by_pos(SDL_OGC_DriverData *data, int row, int col)
{
    return text_by_pos_and_layout(row, col, data->active_layout);
}

static int load_texture(TextureData *texture, int layout_index)
{
    char filename[64];
    FILE *file;
    int16_t version;

    sprintf(filename, "osk%d.tex", layout_index);
    file = fopen(filename, "rb");
    if (!file) {
        return 0;
    }
    fread(&version, sizeof(version), 1, file);
    if (version != TEX_FORMAT_VERSION) {
        printf("Unsupported texture version %d", version);
        return 0;
    }

    fread(&texture->width, sizeof(texture->width), 1, file);
    fread(&texture->height, sizeof(texture->height), 1, file);
    fread(&texture->key_widths[0][0], 1, NUM_ROWS * MAX_BUTTONS_PER_ROW, file);
    fread(&texture->key_height, 1, 1, file);
    int texture_size = GX_GetTexBufferSize(texture->width, texture->height,
                                           GX_TF_I4, GX_FALSE, 0);
    texture->texels = memalign(32, texture_size);
    if (!texture->texels) {
        printf("Failed to allocate %d bytes (%dx%d)\n", texture_size, texture->width, texture->height);
        return 0;
    }

    int rc = fread(texture->texels, 1, texture_size, file);
    printf("Read %d, expected %d\n", rc, texture_size);
    fclose(file);
    DCStoreRange(texture->texels, texture_size);
    GX_InvalidateTexAll();
    return rc == texture_size;
}

static TextureData *lookup_layout_texture(SDL_OGC_DriverData *data,
                                          int layout_index)
{
    TextureData *texture = &data->layout_textures[layout_index];
    if (texture->texels == NULL) {
        if (!load_texture(texture, layout_index)) {
            fprintf(stderr, "Failed to load textures\n");
            return NULL;
        }
    }

    return texture;
}

static void setup_pipeline(int type)
{
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_S16, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    if (type & PIPELINE_TEXTURED) {
        GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_U16, 0);
        GX_SetNumTexGens(1);
        GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
        GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
        GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
        /* This custom processing is like GX_MODULATE, except that instead of
         * picking the color from the texture (GX_CC_TEXC) we take full intensity
         * (GX_CC_ONE).
         */
        GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ONE, GX_CC_RASC, GX_CC_ZERO);
        GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
        GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_TEXA, GX_CA_RASA, GX_CA_ZERO);
        GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

        GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, 1, 1);
    } else {
        GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    }
}

static void activate_layout_texture(const TextureData *texture)
{
    GXTexObj texobj;

    GX_InitTexObj(&texobj, texture->texels, texture->width, texture->height,
                  GX_TF_I4, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GX_InitTexObjLOD(&texobj, GX_NEAR, GX_NEAR,
                     0.0f, 0.0f, 0.0f, 0, 0, GX_ANISO_1);
    GX_LoadTexObj(&texobj, GX_TEXMAP0);
}

static void draw_font_texture(const TextureData *texture, int row, int col,
                              int dest_x, int dest_y, uint32_t color)
{
    int16_t x, y, w, h;

    x = 0;
    for (int i = 0; i < col; i++) {
        x += texture->key_widths[row][i];
    }
    y = texture->key_height * row;
    w = texture->key_widths[row][col];
    h = texture->key_height;

    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);

    GX_Position2s16(dest_x, dest_y);
    GX_Color1u32(color);
    GX_TexCoord2u16(x, y);

    GX_Position2s16(dest_x + w, dest_y);
    GX_Color1u32(color);
    GX_TexCoord2u16(x + w, y);

    GX_Position2s16(dest_x + w, dest_y + h);
    GX_Color1u32(color);
    GX_TexCoord2u16(x + w, y + h);

    GX_Position2s16(dest_x, dest_y + h);
    GX_Color1u32(color);
    GX_TexCoord2u16(x, y + h);

    GX_End();
}

static inline void draw_font_texture_centered(const TextureData *texture,
                                              int row, int col,
                                              int center_x, int center_y,
                                              uint32_t color)
{
    int16_t w, h;

    w = texture->key_widths[row][col];
    h = texture->key_height;

    draw_font_texture(texture, row, col,
                      center_x - w / 2, center_y - h / 2, color);
}

static inline void draw_filled_rect(int16_t x, int16_t y, int16_t w, int16_t h,
                                    uint32_t color)
{
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);

    GX_Position2s16(x, y);
    GX_Color1u32(color);

    GX_Position2s16(x + w, y);
    GX_Color1u32(color);

    GX_Position2s16(x + w, y + h);
    GX_Color1u32(color);

    GX_Position2s16(x, y + h);
    GX_Color1u32(color);

    GX_End();
}

static void draw_filled_rect_p(const Rect *rect, uint32_t color)
{
    return draw_filled_rect(rect->x, rect->y, rect->w, rect->h, color);
}

static inline void draw_key(SDL_OGC_VkContext *context,
                            const TextureData *texture,
                            int row, int col, const Rect *rect)
{
    SDL_OGC_DriverData *data = context->driverdata;
    int x, y;

    x = rect->x + rect->w / 2;
    y = rect->y + rect->h / 2;
    draw_font_texture_centered(texture, row, col, x, y, data->key_color);
}

static inline void draw_key_background(SDL_OGC_VkContext *context,
                                       Rect *rect, int row, int col)
{
    SDL_OGC_DriverData *data = context->driverdata;
    int highlighted;
    uint32_t color;
    const ButtonRow *br = rows[row];
    uint16_t col_mask = 1 << col;

    if (row == data->focus_row && col == data->focus_col) {
        draw_filled_rect(rect->x - FOCUS_BORDER,
                         rect->y - FOCUS_BORDER,
                         rect->w + FOCUS_BORDER * 2,
                         rect->h + FOCUS_BORDER * 2,
                         ColorFocus);
    }

    highlighted = row == data->highlight_row && col == data->highlight_col;
    if (col_mask & br->enter_key_bitmask) {
        color = highlighted ? ColorKeyBgEnterHigh : ColorKeyBgEnter;
    } else if (col_mask & br->special_keys_bitmask) {
        color = highlighted ? ColorKeyBgSpecialHigh : ColorKeyBgSpecial;
    } else {
        color = highlighted ? ColorKeyBgLetterHigh : ColorKeyBgLetter;
    }
    draw_filled_rect_p(rect, color);
}

static void draw_keys(SDL_OGC_VkContext *context, const TextureData *texture)
{
    SDL_OGC_DriverData *data = context->driverdata;
    int start_y = data->screen_height - data->visible_height + 5;

    activate_layout_texture(texture);

    for (int row = 0; row < NUM_ROWS; row++) {
        const ButtonRow *br = rows[row];
        int y = start_y + (ROW_HEIGHT + ROW_SPACING) * row;
        int x = br->start_x;

        for (int col = 0; col < br->num_keys; col++) {
            Rect rect;
            rect.x = x;
            rect.y = y;
            rect.w = br->widths[col] * 2;
            rect.h = ROW_HEIGHT;
            draw_key(context, texture, row, col, &rect);
            x += br->widths[col] * 2 + br->spacing;
        }
    }
}

static inline int16_t input_box_y(SDL_OGC_DriverData *data)
{
    const int height = data->screen_height - KEYBOARD_HEIGHT;
    int start_y = data->input_panel_visible_height - height;
    return start_y + (height - INPUTBOX_HEIGHT) / 2;
}

static void draw_input_text(SDL_OGC_VkContext *context)
{
    SDL_OGC_DriverData *data = context->driverdata;
    int16_t base_y = input_box_y(data);
    int layout_index, last_layout_index, row, col;
    const TextureData *texture;

    int16_t field_x = INPUTBOX_SIDE_MARGIN + INPUTBOX_SIDE_PADDING;
    int16_t x = field_x - data->input_scroll_x;
    int16_t y;

    GX_SetScissor(field_x, 0,
                  data->screen_width - field_x * 2, data->screen_height);
    last_layout_index = -1;
    for (int i = 0; i < data->text_len; i++) {
        key_id_to_pos(data->text[i], &layout_index, &row, &col);
        if (layout_index != last_layout_index) {
            texture = lookup_layout_texture(data, layout_index);
            if (!texture) continue;

            activate_layout_texture(texture);
            y = base_y + (INPUTBOX_HEIGHT - texture->key_height) / 2;
            last_layout_index = layout_index;
        }
        draw_font_texture(texture, row, col, x, y, data->key_color);
        x += texture->key_widths[row][col];
    }

    /* Reset scissor */
    GX_SetScissor(0, 0, data->screen_width, data->screen_height);
}

static void draw_input_panel(SDL_OGC_VkContext *context)
{
    SDL_OGC_DriverData *data = context->driverdata;
    uint32_t ticks, elapsed;

    int16_t base_y = input_box_y(data);
    Rect input_rect = {
        INPUTBOX_SIDE_MARGIN,
        base_y,
        data->screen_width - INPUTBOX_SIDE_MARGIN * 2,
        INPUTBOX_HEIGHT,
    };

    draw_filled_rect_p(&input_rect, ColorKeyboardBg);

    /* Draw cursor */
    ticks = SDL_GetTicks();
    elapsed = ticks - data->input_cursor_start_ticks;
    bool visible = (elapsed / INPUT_CURSOR_BLINK_MS) % 2 == 0;

    if (visible) {
        Rect cursor_rect = {
            INPUTBOX_SIDE_MARGIN + data->input_cursor_x - data->input_scroll_x,
            base_y + 1,
            INPUT_CURSOR_WIDTH,
            INPUTBOX_HEIGHT - 2,
        };
        draw_filled_rect_p(&cursor_rect, ColorInputCursor);
    }
}

static void draw_keyboard(SDL_OGC_VkContext *context)
{
    SDL_OGC_DriverData *data = context->driverdata;
    int start_y = data->screen_height - data->visible_height + 5;
    const TextureData *texture;

    for (int row = 0; row < NUM_ROWS; row++) {
        const ButtonRow *br = rows[row];
        int y = start_y + (ROW_HEIGHT + ROW_SPACING) * row;
        int x = br->start_x;

        for (int col = 0; col < br->num_keys; col++) {
            Rect rect;
            rect.x = x;
            rect.y = y;
            rect.w = br->widths[col] * 2;
            rect.h = ROW_HEIGHT;
            draw_key_background(context, &rect, row, col);
            x += br->widths[col] * 2 + br->spacing;
        }
    }

    setup_pipeline(PIPELINE_TEXTURED);
    texture = lookup_layout_texture(data, data->active_layout);
    if (texture) {
        draw_keys(context, texture);
    }

    GX_DrawDone();
}

static inline void init_data(SDL_OGC_DriverData *data)
{
    data->active_layout = 0;
    data->highlight_row = -1;
    data->focus_row = -1;
    data->text_len = 0;
    data->input_scroll_x = 0;
    data->input_cursor_x = 0;
    data->should_stop_text_input = false;
}

static void dispose_keyboard(SDL_OGC_VkContext *context)
{
    SDL_OGC_DriverData *data = context->driverdata;

    if (data->should_stop_text_input) {
        SDL_StopTextInput();
    }

    context->is_open = SDL_FALSE;
    free_layout_textures(data);
    init_data(data);

    if (data->app_cursor) {
        SDL_SetCursor(data->app_cursor);
        data->app_cursor = NULL;
    }
}

static void send_input_text(SDL_OGC_VkContext *context)
{
    SDL_OGC_DriverData *data = context->driverdata;
    int layout_index, row, col;

    for (int i = 0; i < data->text_len; i++) {
        key_id_to_pos(data->text[i], &layout_index, &row, &col);
        const char *text = text_by_pos_and_layout(row, col, layout_index);
        SDL_OGC_SendKeyboardText(text);
    }
    data->should_stop_text_input = true;
    HideScreenKeyboard(context);
}

static void update_input_cursor(SDL_OGC_DriverData *data)
{
    int layout_index, last_layout_index, row, col;
    const TextureData *texture;

    const int max_x = data->screen_width -
        (INPUTBOX_SIDE_MARGIN + INPUTBOX_SIDE_PADDING) * 2 -
        INPUT_CURSOR_WIDTH;
    int x = 0;

    /* For the time being, the cursor is always at the end of the string */
    last_layout_index = -1;
    for (int i = 0; i < data->text_len; i++) {
        key_id_to_pos(data->text[i], &layout_index, &row, &col);
        if (layout_index != last_layout_index) {
            texture = lookup_layout_texture(data, layout_index);
            if (!texture) continue;

            last_layout_index = layout_index;
        }
        x += texture->key_widths[row][col];
    }

    if (x < data->input_scroll_x) {
        data->input_scroll_x = x;
    } else if (x > max_x) {
        data->input_scroll_x = x - max_x;
    } else {
        data->input_scroll_x = 0;
    }
    data->input_cursor_x = x;
    /* Reset the cursor time so that it's shown */
    data->input_cursor_start_ticks = SDL_GetTicks();
}

static void update_animation(SDL_OGC_VkContext *context)
{
    SDL_OGC_DriverData *data = context->driverdata;
    uint32_t ticks, elapsed;
    int height_diff;

    ticks = SDL_GetTicks();
    elapsed = ticks - data->start_ticks;

    if (elapsed >= data->animation_time) {
        data->visible_height = data->target_visible_height;
        data->input_panel_visible_height = data->input_panel_target_visible_height;
        context->screen_pan_y = data->target_pan_y;
        data->animation_time = 0;
        printf("Desired state reached\n");
        if (data->target_visible_height == 0) {
            dispose_keyboard(context);
        }
    } else {
        height_diff = data->target_visible_height - data->start_visible_height;
        double pos = sin(M_PI_2 * elapsed / data->animation_time);
        data->visible_height = data->start_visible_height +
            height_diff * pos;

        height_diff = data->input_panel_target_visible_height - data->input_panel_start_visible_height;
        data->input_panel_visible_height = data->input_panel_start_visible_height +
            height_diff * pos;

        height_diff = data->target_pan_y - data->start_pan_y;
        context->screen_pan_y = data->start_pan_y + height_diff * pos;
    }
}

static int key_at(SDL_OGC_VkContext *context, int px, int py,
                  int *out_row, int *out_col)
{
    SDL_OGC_DriverData *data = context->driverdata;
    int start_y = data->screen_height - data->visible_height + 5;

    for (int row = 0; row < NUM_ROWS; row++) {
        const ButtonRow *br = rows[row];
        int y = start_y + (ROW_HEIGHT + ROW_SPACING) * row;
        int x;

        if (py < y) break;
        if (py >= y + ROW_HEIGHT) continue;

        x = br->start_x;

        for (int col = 0; col < br->num_keys; col++) {
            if (px > x && px < x + br->widths[col] * 2) {
                *out_row = row;
                *out_col = col;
                return 1;
            }
            x += br->widths[col] * 2 + br->spacing;
        }
    }
    return 0;
}

static void switch_layout(SDL_OGC_VkContext *context, int level)
{
    SDL_OGC_DriverData *data = context->driverdata;

    data->active_layout = level;
}

static void activate_mouse(SDL_OGC_DriverData *data)
{
    data->focus_row = -1;
}

static void activate_joypad(SDL_OGC_DriverData *data)
{
    if (data->focus_row < 0) {
        data->focus_row = 2;
        data->focus_col = rows[data->focus_row]->num_keys / 2;
    }
    data->highlight_row = -1;
}

static void activate_key(SDL_OGC_VkContext *context, int row, int col)
{
    SDL_OGC_DriverData *data = context->driverdata;
    const char *text = text_by_pos(data, row, col);

    bool has_input_box = data->input_panel_visible_height > 0;

    /* We can use pointer comparisons here */
    if (text == KEYCAP_BACKSPACE) {
        if (has_input_box) {
            if (data->text_len > 0) data->text_len--;
            update_input_cursor(data);
        } else {
            SDL_OGC_SendVirtualKeyboardKey(SDL_PRESSED, SDL_SCANCODE_BACKSPACE);
        }
    } else if (text == KEYCAP_RETURN) {
        if (has_input_box) {
            send_input_text(context);
        } else {
            SDL_OGC_SendVirtualKeyboardKey(SDL_PRESSED, SDL_SCANCODE_RETURN);
        }
    } else if (text == KEYCAP_ABC) {
        switch_layout(context, 0);
    } else if (text == KEYCAP_SHIFT) {
        switch_layout(context, !data->active_layout);
    } else if (text == KEYCAP_SYMBOLS || text == KEYCAP_SYM2) {
        switch_layout(context, 2);
    } else if (text == KEYCAP_SYM1) {
        switch_layout(context, 3);
    } else {
        if (has_input_box) {
            if (data->text_len < MAX_INPUT_LEN) {
                KeyID key = key_id_from_pos(data->active_layout, row, col);
                data->text[data->text_len++] = key;
                update_input_cursor(data);
            }
        } else {
            SDL_OGC_SendKeyboardText(text);
        }
    }
}

static void handle_click(SDL_OGC_VkContext *context, int px, int py)
{
    SDL_OGC_DriverData *data = context->driverdata;
    int row, col;

    if (data->focus_row >= 0) return;

    bool has_input_box = data->input_panel_visible_height > 0;
    if (!has_input_box && py < data->screen_height - KEYBOARD_HEIGHT) {
        data->should_stop_text_input = true;
        HideScreenKeyboard(context);
        return;
    }

    if (key_at(context, px, py, &row, &col)) {
        activate_key(context, row, col);
    }
}

static void handle_motion(SDL_OGC_VkContext *context, int px, int py)
{
    SDL_OGC_DriverData *data = context->driverdata;
    int row, col;

    activate_mouse(data);

    if (key_at(context, px, py, &row, &col)) {
        if (data->highlight_row != row ||
            data->highlight_col != col) {
            data->highlight_row = row;
            data->highlight_col = col;
            WPAD_Rumble(0, 1);
            WPAD_Rumble(0, 0);
        }
    } else {
        data->highlight_row = -1;
    }
}

static void move_right(SDL_OGC_DriverData *data)
{
    data->focus_col++;
    if (data->focus_col >= rows[data->focus_row]->num_keys) {
        data->focus_col = 0;
    }
}

static void move_left(SDL_OGC_DriverData *data)
{
    data->focus_col--;
    if (data->focus_col < 0) {
        data->focus_col = rows[data->focus_row]->num_keys - 1;
    }
}

static int adjust_column(int row, int oldrow, int oldcol) {
    const ButtonRow *br = rows[oldrow];
    int x, oldx, col;

    x = br->start_x;
    for (col = 0; col < oldcol; col++) {
        x += br->widths[col] * 2 + br->spacing;
    }
    /* Take the center of the button */
    oldx = x + br->widths[oldcol];

    /* Now find a button at about the same x in the new row */
    br = rows[row];
    x = br->start_x;
    for (col = 0; col < br->num_keys; col++) {
        if (x > oldx) {
            return col > 0 ? (col - 1) : col;
        }
        x += br->widths[col] * 2 + br->spacing;
    }
    return col - 1;
}

static void move_up(SDL_OGC_DriverData *data)
{
    int oldrow = data->focus_row;

    data->focus_row--;
    if (data->focus_row < 0) {
        data->focus_row = NUM_ROWS - 1;
    }

    if (oldrow >= 0) {
        data->focus_col = adjust_column(data->focus_row, oldrow, data->focus_col);
    }
}

static void move_down(SDL_OGC_DriverData *data)
{
    int oldrow = data->focus_row;

    data->focus_row++;
    if (data->focus_row >= NUM_ROWS) {
        data->focus_row = 0;
    }

    if (oldrow >= 0) {
        data->focus_col = adjust_column(data->focus_row, oldrow, data->focus_col);
    }
}

static void handle_joy_axis(SDL_OGC_VkContext *context,
                            const SDL_JoyAxisEvent *event)
{
    SDL_OGC_DriverData *data = context->driverdata;

    activate_joypad(data);

    if (event->axis == 0) {
        if (event->value > 256) move_right(data);
        else if (event->value < -256) move_left(data);
    } else if (event->axis == 1) {
        if (event->value > 256) move_down(data);
        else if (event->value < -256) move_up(data);
    }
}

static void handle_joy_hat(SDL_OGC_VkContext *context, Uint8 pos)
{
    SDL_OGC_DriverData *data = context->driverdata;

    activate_joypad(data);

    switch (pos) {
    case SDL_HAT_RIGHT: move_right(data); break;
    case SDL_HAT_LEFT: move_left(data); break;
    case SDL_HAT_DOWN: move_down(data); break;
    case SDL_HAT_UP: move_up(data); break;
    }
}

static void handle_joy_button(SDL_OGC_VkContext *context,
                              Uint8 button, Uint8 state)
{
    SDL_OGC_DriverData *data = context->driverdata;

    if (data->focus_row < 0) return;

    printf("Button %d, state %d\n", button, state);
    /* For now, only handle button press */
    if (state != SDL_PRESSED) return;

    switch (button) {
    case 0:
        activate_key(context, data->focus_row, data->focus_col);
        break;
    case 1:
        SDL_OGC_SendVirtualKeyboardKey(SDL_PRESSED, SDL_SCANCODE_BACKSPACE);
        break;
    }
}

static void init_screen(SDL_OGC_DriverData *data)
{
    SDL_Rect screen;
    SDL_GetDisplayBounds(0, &screen);
    data->screen_width = screen.w;
    data->screen_height = screen.h;
    printf("Screen: %d,%d\n", screen.w, screen.h);
}

static void Init(SDL_OGC_VkContext *context)
{
    SDL_OGC_DriverData *data;

    printf("%s called\n", __func__);

    data = SDL_calloc(sizeof(SDL_OGC_DriverData), 1);
    init_data(data);
    data->key_color = 0xffffffff;
    context->driverdata = data;
}

static void RenderKeyboard(SDL_OGC_VkContext *context)
{
    SDL_OGC_DriverData *data = context->driverdata;
    Rect osk_rect;

    //printf("%s called\n", __func__);
    if (data->animation_time > 0) {
        update_animation(context);
        if (!context->is_open) return;
    }

    setup_pipeline(PIPELINE_UNTEXTURED);

    osk_rect.x = 0;
    osk_rect.y = data->screen_height - data->visible_height;
    osk_rect.w = data->screen_width;
    osk_rect.h = KEYBOARD_HEIGHT;
    draw_filled_rect_p(&osk_rect, ColorKeyboardBg);

    if (data->input_panel_visible_height > 0) {
        osk_rect.y = 0;
        osk_rect.h = data->input_panel_visible_height;
        draw_filled_rect_p(&osk_rect, ColorInputPanelBg);
        draw_input_panel(context);
    }

    draw_keyboard(context);

    if (data->input_panel_visible_height > 0) {
        draw_input_text(context);
    }

    GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_FALSE, 0, 0);
    if (data->app_cursor) {
        SDL_SetCursor(data->default_cursor);
    }
}

static SDL_bool ProcessEvent(SDL_OGC_VkContext *context, SDL_Event *event)
{
    printf("%s called\n", __func__);
    switch (event->type) {
    case SDL_MOUSEBUTTONDOWN:
        if (event->button.which != 0) break;
        handle_click(context, event->button.x, event->button.y);
        return SDL_TRUE;
    case SDL_MOUSEMOTION:
        if (event->motion.which != 0) break;
        handle_motion(context, event->motion.x, event->motion.y);
        return SDL_TRUE;
    case SDL_JOYAXISMOTION:
        handle_joy_axis(context, &event->jaxis);
        return SDL_TRUE;
    case SDL_JOYHATMOTION:
        handle_joy_hat(context, event->jhat.value);
        return SDL_TRUE;
    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP:
        handle_joy_button(context, event->jbutton.button,
                          event->jbutton.state);
        return SDL_TRUE;
    }

    if (event->type >= SDL_MOUSEMOTION &&
        event->type <= SDL_CONTROLLERSENSORUPDATE) {
        return SDL_TRUE;
    }

    return SDL_FALSE;
}

static void StartTextInput(SDL_OGC_VkContext *context)
{
    printf("%s called\n", __func__);
}

static void StopTextInput(SDL_OGC_VkContext *context)
{
    printf("%s called\n", __func__);
}

static void SetTextInputRect(SDL_OGC_VkContext *context, const SDL_Rect *rect)
{
    SDL_OGC_DriverData *data = context->driverdata;


    if (rect) {
        memcpy(&context->input_rect, rect, sizeof(SDL_Rect));
    } else {
        memset(&context->input_rect, 0, sizeof(SDL_Rect));
    }

    if (context->input_rect.h != 0) {
        init_screen(data);
        /* Pan the input rect so that it remains visible even when the OSK is
         * open */
        int desired_input_rect_y = (data->screen_height - KEYBOARD_HEIGHT - context->input_rect.h) / 2;
        data->target_pan_y = desired_input_rect_y - context->input_rect.y;
    } else {
        data->target_pan_y = 0;
    }
    data->start_pan_y = context->screen_pan_y;
}

static void ShowScreenKeyboard(SDL_OGC_VkContext *context)
{
    SDL_OGC_DriverData *data = context->driverdata;
    SDL_Cursor *cursor, *default_cursor;

    printf("%s called\n", __func__);
    init_screen(data);
    context->is_open = SDL_TRUE;
    data->start_ticks = SDL_GetTicks();
    data->start_visible_height = data->visible_height;
    data->target_visible_height = KEYBOARD_HEIGHT;
    data->animation_time = ANIMATION_TIME_ENTER;

    if (context->input_rect.h == 0) {
        /* If there's no input rect, bring down our own */
        data->input_panel_start_visible_height = data->input_panel_visible_height;
        data->input_panel_target_visible_height =
            data->screen_height - KEYBOARD_HEIGHT;
    }

    cursor = SDL_GetCursor();
    default_cursor = SDL_GetDefaultCursor();
    if (cursor != default_cursor) {
        data->app_cursor = cursor;
        data->default_cursor = default_cursor;
    }
}

static void HideScreenKeyboard(SDL_OGC_VkContext *context)
{
    SDL_OGC_DriverData *data = context->driverdata;

    printf("%s called\n", __func__);
    data->start_ticks = SDL_GetTicks();
    data->start_visible_height = data->visible_height;
    data->target_visible_height = 0;
    data->input_panel_start_visible_height = data->input_panel_visible_height;
    data->input_panel_target_visible_height = 0;
    data->start_pan_y = context->screen_pan_y;
    data->target_pan_y = 0;
    data->animation_time = ANIMATION_TIME_EXIT;
}

static const SDL_OGC_VkPlugin plugin = {
    .struct_size = sizeof(SDL_OGC_VkPlugin),
    .Init = Init,
    .RenderKeyboard = RenderKeyboard,
    .ProcessEvent = ProcessEvent,
    .StartTextInput = StartTextInput,
    .StopTextInput = StopTextInput,
    .SetTextInputRect = SetTextInputRect,
    .ShowScreenKeyboard = ShowScreenKeyboard,
    .HideScreenKeyboard = HideScreenKeyboard,
};

const SDL_OGC_VkPlugin *ogc_keyboard_get_plugin()
{
    printf("%s called\n", __func__);
    return &plugin;
}

