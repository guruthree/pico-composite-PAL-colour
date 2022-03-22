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

#include "lbm.h"
#include "jet.h"

void drawlbm(LBM &lbm, int8_t *tbuf) {

    memset(tbuf, 0, BUF_SIZE);

    uint8_t xat = 0, yat = 0, speed;
    for (uint8_t y = 2; y < lbm.NY-2; y++) { // don't draw boundaries
        xat = 0;
        for (uint8_t x = 0; x < lbm.NX; x++) {
            if (!lbm.BOUND[x][y]) {
                speed = lbm.getSpeed(x, y) * 63.0 / lbm.maxVal;
                speed > 63 ? speed = 63 : 1;

                // simple nearest neighbour interpolation?
                setPixelRGB(tbuf, xat, yat, jet[speed][0], jet[speed][1], jet[speed][2]);
                setPixelRGB(tbuf, xat, yat+1, jet[speed][0], jet[speed][1], jet[speed][2]);
                setPixelRGB(tbuf, xat, yat+2, jet[speed][0], jet[speed][1], jet[speed][2]);
                setPixelRGB(tbuf, xat, yat+3, jet[speed][0], jet[speed][1], jet[speed][2]);

                setPixelRGB(tbuf, xat+1, yat, jet[speed][0], jet[speed][1], jet[speed][2]);
                setPixelRGB(tbuf, xat+1, yat+1, jet[speed][0], jet[speed][1], jet[speed][2]);
                setPixelRGB(tbuf, xat+1, yat+2, jet[speed][0], jet[speed][1], jet[speed][2]);
                setPixelRGB(tbuf, xat+1, yat+3, jet[speed][0], jet[speed][1], jet[speed][2]);
            }
            xat += 2;
        }
        yat += 4;
    } 

}
