/*
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2022-2023 guruthree
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
        uint32_t sat = 0; // where we are in the undulating landscape

        float basecliff[ACROSS] = {80, 79, 80, 5, 1, 0, 1, -2, 1, 0, 1, 5, 80, 79, 80};

        // generate an interesting across
        void generateAcross(uint8_t y) {
            if (y == 0 || y > DOWN-1) {
                return;
            }

            float d;
            for (uint8_t x = 0; x < ACROSS; x++) {
                d = basecliff[x] - thecliff[y-1][x];
                thecliff[y][x] = thecliff[y-1][x] + randi(-5, 5) + d/4;
            }

            for (uint8_t x = 4; x < ACROSS-3; x++) {
                thecliff[y][x] + sinf(sat / 10.0f)*2;
            }
        }

    public:
        Cliffs() {}

        void init() {
            for (uint8_t x = 0; x < ACROSS; x++) {
                for (uint8_t y = 0; y < DOWN; y++) {
                    // this bugs out when it's 0? maybe it gets optimised out?
                    thecliff[y][x] = basecliff[x];
                }
            }
        }

        void step() {
            zoffset += dzstep;
            if (zoffset >= 20) {
                zoffset -= 20;
                sat++; //

                // the closest points are now off screen
                // so shuffle down and update of the furthest away geometry 
                for (uint8_t y = 0; y < DOWN-1; y++) {
                    for (uint8_t x = 0; x < ACROSS; x++) {
                        thecliff[y][x] = thecliff[y+1][x];
                    }
                }
//                for (uint8_t x = 0; x < ACROSS; x++) {
//                    thecliff[DOWN-1][x] = 0;
//                }
                generateAcross(DOWN-1);

                // add a random hill at the second to last position
                // (2nd last so that it won't mess up the generation of the next across)
                if (rand() > RAND_MAX*0.92f) {
                    thecliff[DOWN-2][randi(4, ACROSS-4)] += 35;
                }
            }

        }

        void render(int8_t *tbuf) {
            // refresh verticies coordinates array to be transformed again
            for (uint8_t x = 0; x < ACROSS; x++) {
                for (uint8_t y = 0; y < DOWN; y++) {
                    vt[y][x] = {(float(x)-ACROSS/2)*60/HORIZONTAL_DOUBLING, -(thecliff[y][x]/2 - 40), -(float(y)-3)*20 + zoffset};
                }
            }

            // the angle looking down into the valley
            Matrix3 rot = Matrix3::getRotationMatrix(-10.0f/180.0f*M_PI, 0, 0);

            for (uint8_t x = 0; x < ACROSS; x++) {
                for (uint8_t y = 0; y < DOWN; y++) {// apply perspective
                    vt[y][x] = rot.preMultiply(vt[y][x]);
                    vt[y][x] = vt[y][x].scale(40.0f / (-vt[y][x].z/2.0f + 40.0f));
                    // scale coordinates to screen coordinates
                    vt[y][x].x = (vt[y][x].x/2) + XRESOLUTION/2.0f;
                    vt[y][x].y += YRESOLUTION/2.0f;
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
            for (uint8_t y = 0; y < DOWN-1; y++) {

                // color ranges from purple 120, 20, 100 to green 50, 100, 100
                float distanceaway = ((float(y)-3)*20 + zoffset) / ((DOWN + 2)*20);
                uint8_t r = 50 + distanceaway*70;
                uint8_t g = 100 - distanceaway*80;

                for (uint8_t x = 0; x < ACROSS; x++) {

                    // don't show any lines that are entirely outside of screen space or start and end outside of screen space
                    if ((vt[y][x].x < 0 && vt[y][x+1].x < 0) || (vt[y][x].x > XRESOLUTION && vt[y][x+1].x > XRESOLUTION) || 
                        (vt[y][x].y < 0 && vt[y][x+1].y < 0) || (vt[y][x].y > YRESOLUTION && vt[y][x+1].y > YRESOLUTION) || 
                        (vt[y][x].x < 0 && vt[y][x+1].x > YRESOLUTION) || (vt[y][x].y < 0 && vt[y][x+1].y > YRESOLUTION)) {
                    }
                    else if (x < ACROSS-1) {
                        drawLineRGB(tbuf, vt[y][x].x, vt[y][x].y, vt[y][x+1].x, vt[y][x+1].y, r, g, 100);
                    }

                    if ((vt[y][x].x < 0 && vt[y+1][x].x < 0) || (vt[y][x].x > XRESOLUTION && vt[y+1][x].x > XRESOLUTION) || 
                        (vt[y][x].y < 0 && vt[y+1][x].y < 0) || (vt[y][x].y > YRESOLUTION && vt[y+1][x].y > YRESOLUTION) || 
                        (vt[y][x].x < 0 && vt[y+1][x].x > YRESOLUTION) || (vt[y][x].y < 0 && vt[y+1][x].y > YRESOLUTION)) {
                    }
                    else {
                        drawLineRGB(tbuf, vt[y][x].x, vt[y][x].y, vt[y+1][x].x, vt[y+1][x].y, r, g, 100);
                    }

                }
            }
        }
};
