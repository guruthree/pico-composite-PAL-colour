/*
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 guruthree
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <string>

// right most bit is the top left corner of the character

uint16_t font[32*3-2] = {
    0b0010000010010010, // ! (ASCII: 33)
    0b0000000000101101, // "
    0b0101111101111101, // #
    0b0111110111011111, // $
    0b0101001010100101, // %
    0b0110101011001010, // &
    0b0000000000001001, // '
    0b0010001001001010, // (
    0b0010100100100010, // )
    0b0000101010101000, // *
    0b0000010111010000, // +
    0b0001010000000000, // ,
    0b0000000111000000, // -
    0b0010000000000000, // .
    0b0001011010110100, // /
    0b0111101101101111, // 0
    0b0010010010010010, // 1
    0b0111001010100011, // 2
    0b0111100111100111, // 3
    0b0100100111101101, // 4
    0b0011100011001111, // 5
    0b0111101111001111, // 6
    0b0100100100100111, // 7
    0b0111101111101111, // 8
    0b0100100111101111, // 9
    0b0000010000010000, // :
    0b0001010000010000, // ;
    0b0100010001010100, // <
    0b0000111000111000, // =
    0b0001010100010001, // >
    0b0010000110100111, // ?
    0b0010101101100111, // @
    0b0101101111101010, // A
    0b0011101011101011, // B
    0b0111001001001111, // C
    0b0011101101101011, // D
    0b0111001111001111, // E
    0b0001001011001111, // F
    0b0111101001001111, // G
    0b0101101111101101, // H
    0b0111010010010111, // I
    0b0111101100100100, // J
    0b0101101011101101, // K
    0b0111001001001001, // L
    0b0101101101111111, // M
    0b0101101101101111, // N
    0b0111101101101111, // O
    0b0001001011101011, // P
    0b0110111101101111, // Q
    0b0101101011101011, // R
    0b0111100111001111, // S
    0b0010010010010111, // T
    0b0111101101101101, // U
    0b0010101101101101, // V
    0b0111111101101101, // W
    0b0101101010101101, // X
    0b0010010101101101, // Y
    0b0111001010100111, // Z
    0b0110010010010110, // [
    0b0100110010011001, // backslash
    0b0011010010010011, // ]
    0b0000000000101010, // ^
    0b0111000000000000, // _
    0b0000000000010001, // `
    0b0110101010000000, // a
    0b0111101111001000, // b
    0b0011001011000000, // c
    0b0111101111100000, // d
    0b0110001111101010, // e
    0b0001011001010000, // f
    0b0110100110101010, // g
    0b0101101011001001, // h
    0b0010010010000010, // i
    0b0010101100000100, // j
    0b0101011101001001, // k
    0b0010010010010010, // l
    0b0101111111000000, // m
    0b0101101111000000, // n
    0b0111101111000000, // o
    0b0001011101010000, // p
    0b0100110101010000, // q
    0b0001001010000000, // r
    0b0001010001010000, // s
    0b0010010111010000, // t
    0b0111101101000000, // u
    0b0011101101000000, // v
    0b0111111101000000, // w
    0b0101010101000000, // x
    0b0011100110101000, // y
    0b0111011110111000, // z
    0b0100010011010100, // {
    0b0010010010010010, // |
    0b0001010110010001, // }
    0b0000001111100000, // ~
};

// 3x5 font is packed into a uint16_t
// 0b0101101111101010 for the letter A (23530)
// it constructs from left (lsb) to right (msb)
// the last bit is a packing bit to be ignored
// 0b0 101 101 111 101 010 split up into groups of three
// 010 p written out row by row
// 101
// 111
// 101
// 101

void writeStrScaled(int8_t *buf, int32_t xcoord, int32_t ycoord, std::string str, uint8_t r, uint8_t g, uint8_t b, bool twoX) {
    uint16_t val;
    for (uint8_t l = 0; l < str.length(); l++) {
        if (str[l] == ' ') { // there's no space so skip it
            continue;
        }
        val = font[str[l] - 33];
        for (uint8_t i = 0; i < 16; i++) {
            if ((val >> i) & 1) { // bit is set, so draw pixel
                uint8_t x = i % 3;
                uint8_t y = (i - x) / 3;
                if (twoX == false) {
                    setPixelRGB(buf, xcoord + l*4 + x, ycoord + y, r, g, b);
                }
                else {
                    setPixelRGBtwoX(buf, xcoord + (l*4 + x)*2, ycoord + y*2, r, g, b);
                }
            }
        }
    }
}

void writeStr(int8_t *buf, int32_t xcoord, int32_t ycoord, std::string str, uint8_t r, uint8_t g, uint8_t b) {
    writeStrScaled(buf, xcoord, ycoord, str, r, g, b, false);
}
