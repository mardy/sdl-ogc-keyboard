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

#ifndef OGC_KEYBOARD_CONFIG_H
#define OGC_KEYBOARD_CONFIG_H

#include <stdint.h>

#define NUM_ROWS 5
#define NUM_LAYOUTS 4
#define MAX_BUTTONS_PER_ROW 10

typedef struct RowLayout {
    const char **symbols;
} RowLayout;

typedef struct ButtonRow {
    int8_t start_x;
    int8_t spacing;
    int8_t num_keys;
    uint16_t special_keys_bitmask;
    uint16_t enter_key_bitmask;
    /* button widths, in units of 2 pixels */
    const uint8_t *widths;
    RowLayout layouts[NUM_LAYOUTS];
} ButtonRow;

extern const char KEYCAP_BACKSPACE[];
extern const char KEYCAP_SHIFT[];
extern const char KEYCAP_SYM1[];
extern const char KEYCAP_SYM2[];
extern const char KEYCAP_SYMBOLS[];
extern const char KEYCAP_ABC[];
extern const char KEYCAP_RETURN[];

extern const ButtonRow *rows[];

#endif // OGC_KEYBOARD_CONFIG_H
