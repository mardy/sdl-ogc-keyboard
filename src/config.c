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

const char KEYCAP_BACKSPACE[] = "\u2190";
const char KEYCAP_SHIFT[] = "\u2191";
const char KEYCAP_SYM1[] = "1/2";
const char KEYCAP_SYM2[] = "2/2";
const char KEYCAP_SYMBOLS[] = "=\\<";
const char KEYCAP_ABC[] = "abc";
const char KEYCAP_RETURN[] = "\u23CE";

static const uint8_t s_widths_10[] = { 26, 26, 26, 26, 26, 26, 26, 26, 26, 26 };
static const char *row0syms[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "0" };
static const char *row0syms2[] = { "~", "@", "#", "$", "%", "^", "&", "*", "(", ")" };
static const ButtonRow row0 = { 6, 12, 10, 0x0, 0x0, s_widths_10, {
    { row0syms },
    { row0syms },
    { row0syms2 },
    { row0syms2 },
}};

static const char *row1syms0[] = { "q", "w", "e", "r", "t", "y", "u", "i", "o", "p" };
static const char *row1syms1[] = { "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P" };
static const char *row1syms2[] = { "\\", "/", "€", "¢", "=", "-", "_", "+", "[", "]" };
static const char *row1syms3[] = { "©", "®", "£", "µ", "¥", "№", "°", "\u2605", "\u261e", "\u261c" };
static const ButtonRow row1 = { 6, 12, 10, 0x0, 0x0, s_widths_10, {
    { row1syms0 },
    { row1syms1 },
    { row1syms2 },
    { row1syms3 },
}};

static const char *row2syms0[] = { "a", "s", "d", "f", "g", "h", "j", "k", "l" };
static const char *row2syms1[] = { "A", "S", "D", "F", "G", "H", "J", "K", "L" };
static const char *row2syms2[] = { "<", ">", "¿", "¡", "—", "´", "|", "{", "}" };
static const char *row2syms3[] = { "«", "»", "\u263A", "\u2639", "\U0001f600", "\U0001f609", "\U0001f622", "\U0001f607", "\U0001f608" };
static const ButtonRow row2 = { 38, 12, 9, 0x0, 0x0, s_widths_10, {
    { row2syms0 },
    { row2syms1 },
    { row2syms2 },
    { row2syms3 },
}};

static const uint8_t s_widths_7_2[] = { 42, 26, 26, 26, 26, 26, 26, 26, 42 };
static const char *row3syms0[] = { KEYCAP_SHIFT, "z", "x", "c", "v", "b", "n", "m", KEYCAP_BACKSPACE };
static const char *row3syms1[] = { KEYCAP_SHIFT, "Z", "X", "C", "V", "B", "N", "M", KEYCAP_BACKSPACE };
static const char *row3syms2[] = { KEYCAP_SYM1, "`", "\"", "'", ":", ";", "!", "?", KEYCAP_BACKSPACE };
static const char *row3syms3[] = { KEYCAP_SYM2, "\u26a0", "§", "±", "\u2642", "\u2640", "\u2600", "\u263e", KEYCAP_BACKSPACE };
static const ButtonRow row3 = { 6, 12, 9, 0x101, 0x0, s_widths_7_2, {
    { row3syms0 },
    { row3syms1 },
    { row3syms2 },
    { row3syms3 },
}};

static const uint8_t s_widths_bar[] = { 42, 26, 122, 26, 74 };
static const char *row4syms0[] = { KEYCAP_SYMBOLS, ",", " ", ".", KEYCAP_RETURN };
static const char *row4syms2[] = { KEYCAP_ABC, ",", " ", ".", KEYCAP_RETURN };
static const ButtonRow row4 = { 6, 12, 5, 0x1, 0x10, s_widths_bar, {
    { row4syms0 },
    { row4syms0 },
    { row4syms2 },
    { row4syms2 },
}};

const ButtonRow *rows[] = { &row0, &row1, &row2, &row3, &row4 };
