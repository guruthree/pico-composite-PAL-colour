/*
 *
 * The MIT License (MIT)
 *
 * Copyright (c) size/222 guruthree
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

#include <vector>
#include <algorithm>

#include "discountadafruitgfx.h"
#include "vectormath.h"

class Cliffs {

    private:
        static const uint8_t ACROSS = 15;
        static const uint8_t DOWN = 23;

        float thecliff[DOWN][ACROSS]; // base heights, to be put into the transformed verticies
        Vector3 vt[DOWN][ACROSS]; // transformed verticies

        float zoffset = 0; // shuffle the grid towards the screen
        float dzstep = 1.2f; // rate at which it approaches the screen

    public:
        Cliffs() {}

        void init() {
            float basecliff[ACROSS] = {80, 80, 80, 5, 1, 0, 1, -2, 1, 0, 1, 5, 80, 80, 80};

            for (uint8_t x = 0; x < ACROSS; x++) {
                for (uint8_t y = 0; y < DOWN; y++) {
                    // this bugs out when it's 0? maybe it gets optimised out?
                    thecliff[y][x] = basecliff[x]/2 - 40;
                }
            }
        }

        void step() {
            zoffset += dzstep;
            if (zoffset >= 20) {
                zoffset -= 20;
            }


            // update h coordinates when hitting draw distance

            // refresh verticies coordinates array to be transformed again
            for (uint8_t x = 0; x < ACROSS; x++) {
                for (uint8_t y = 0; y < DOWN; y++) {
//                    vt[y][x] = {(float(x)-ACROSS/2)*10, -thecliff[y][x], -(float(y)-3*DOWN/4)*10 + zoffset};
                    vt[y][x] = {(float(x)-ACROSS/2)*20, -thecliff[y][x], -(float(y)-2)*20 + zoffset};
                }
            }
        }

        void render(int8_t *tbuf) {
            // reset vt

            Matrix3 rot = Matrix3::getRotationMatrix(-10.0f/180.0f*M_PI, 0, 0);

            for (uint8_t x = 0; x < ACROSS; x++) {
                for (uint8_t y = 0; y < DOWN; y++) {// apply perspective
                    vt[y][x] = rot.preMultiply(vt[y][x]);
                    vt[y][x] = vt[y][x].scale(40.0f / (-vt[y][x].z/2.0f + 40.0f));
                    // scale coordinates to screen coordinates
                    vt[y][x].x = (vt[y][x].x/2) + 32;
                    vt[y][x].y += 62.0f;
                }
            }


            // fill background with colour
            int32_t y = 0, u = 0, v = 0;
            rgb2yuv(50, 5, 40, y, u, v);
            for (uint8_t xcoord = 0; xcoord < XRESOLUTION; xcoord++) {
                for (uint8_t ycoord = 0; ycoord < YRESOLUTION; ycoord++) {
                    setPixelYUV(tbuf, xcoord, ycoord, y, u, v);
                }
            }


            // loop through x and y and draw the lines
            for (uint8_t x = 0; x < ACROSS-1; x++) {
                for (uint8_t y = 0; y < DOWN-1; y++) {
//            for (uint8_t x = 0; x < 12; x++){
//                for (uint8_t y = 0; y < 2; y++){

                    // don't show any lines that are entirely outside of screen space or start and end outside of screen space
                    if ((vt[y][x].x < 0 && vt[y][x+1].x < 0) || (vt[y][x].x > XRESOLUTION && vt[y][x+1].x > XRESOLUTION) || 
                        (vt[y][x].y < 0 && vt[y][x+1].y < 0) || (vt[y][x].y > YRESOLUTION && vt[y][x+1].y > YRESOLUTION) || 
                        (vt[y][x].x < 0 && vt[y][x+1].x > YRESOLUTION) || (vt[y][x].y < 0 && vt[y][x+1].y > YRESOLUTION)) {
                    }
                    else {
                        drawLineRGB(tbuf, vt[y][x].x, vt[y][x].y, vt[y][x+1].x, vt[y][x+1].y, 120, 20, 100);
                    }

                    if ((vt[y][x].x < 0 && vt[y+1][x].x < 0) || (vt[y][x].x > XRESOLUTION && vt[y+1][x].x > XRESOLUTION) || 
                        (vt[y][x].y < 0 && vt[y+1][x].y < 0) || (vt[y][x].y > YRESOLUTION && vt[y+1][x].y > YRESOLUTION) || 
                        (vt[y][x].x < 0 && vt[y+1][x].x > YRESOLUTION) || (vt[y][x].y < 0 && vt[y+1][x].y > YRESOLUTION)) {
                    }
                    else {
                        drawLineRGB(tbuf, vt[y][x].x, vt[y][x].y, vt[y+1][x].x, vt[y+1][x].y, 120, 20, 100);
                    }
                }
            }

/*uint8_t x = 5;
uint8_t y = 0;

            char sbuf[200];
            memset(sbuf, 0, sizeof(sbuf));
            sprintf(sbuf, "%0.2f", vt[y][x].x);
            writeStr(tbuf, 0, 05, sbuf, 100, 0, 0);
            sprintf(sbuf, "%0.2f", vt[y][x].y);
            writeStr(tbuf, 0, 11, sbuf, 100, 0, 0);
//            sprintf(sbuf, "%0.2f", vt[y][x].z);
//            writeStr(tbuf, 0, 17, sbuf, 100, 0, 0);

/*            sprintf(sbuf, "%0.2f", vt[y][x+1].x);
            writeStr(tbuf, 0, 23, sbuf, 100, 0, 0);
            sprintf(sbuf, "%0.2f", vt[y][x+1].y);
            writeStr(tbuf, 0, 29, sbuf, 100, 0, 0);
            sprintf(sbuf, "%0.2f", vt[y][x+1].z);
            writeStr(tbuf, 0, 35, sbuf, 100, 0, 0);

            sprintf(sbuf, "%0.2f", vt[y+1][x].x);
            writeStr(tbuf, 30, 05, sbuf, 100, 0, 0);
            sprintf(sbuf, "%0.2f", vt[y+1][x].y);
            writeStr(tbuf, 30, 11, sbuf, 100, 0, 0);
//            sprintf(sbuf, "%0.2f", vt[y+1][x].z);
//            writeStr(tbuf, 30, 17, sbuf, 100, 0, 0);

/*            sprintf(sbuf, "%0.2f", vt[y+1][x+1].x);
            writeStr(tbuf, 30, 23, sbuf, 100, 0, 0);
            sprintf(sbuf, "%0.2f", vt[y+1][x+1].y);
            writeStr(tbuf, 30, 29, sbuf, 100, 0, 0);
            sprintf(sbuf, "%0.2f", vt[y+1][x+1].z);
            writeStr(tbuf, 30, 35, sbuf, 100, 0, 0);*/

//            drawLineRGB(tbuf, vt[y][x].x, vt[y][x].y, vt[y][x].x, vt[y][x].y+5, 100, 100, 100);

        }
};
