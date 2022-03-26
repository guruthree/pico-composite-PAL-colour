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

class Flames {

    public:

        enum COLOUR_SCHEME { RED, GREEN, BLUE };

    private:

        static const uint8_t ACROSS = 32;
        static const uint8_t DOWN = 20;

        // the output frame is the mean of these frames
        static const uint8_t HISTORY = 1;

        uint8_t allim[HISTORY][DOWN][ACROSS];
        uint8_t imat = 0;

        static const uint8_t NUM_COLOURS = HISTORY*3+1;
        // yuv colour
        int8_t colourmap[NUM_COLOURS][3];

        // set the points in a row to val between x1 and x2 inclusive
        void fillim(uint8_t (&im)[ACROSS], uint8_t x1, uint8_t x2, uint8_t val) {
            for (uint8_t ii = x1; ii <= x2; ii++) {
                im[ii] = val;
            }
        }

        // index of the first location with a value in a row
        int8_t findfirstx(uint8_t (&im)[DOWN][ACROSS], uint8_t y, uint8_t val) {
            int8_t x = 0;
            while (im[y][x] != val) {
                x++;
                if (x == ACROSS) {
                    return -1;
                }
            }
            return x;
        }

        // index of the last location with a value in a row
        int8_t findlastx(uint8_t (&im)[DOWN][ACROSS], uint8_t y, uint8_t val) {
            int8_t x = ACROSS-1;
            while (im[y][x] != val) {
                x--;
                if (x == -1) {
                    x = -1;
                }
            }
            return x;
        }

        void generateFlames(uint8_t (&im)[DOWN][ACROSS]) {
            memset(im, 0, ACROSS*DOWN*sizeof(uint8_t));
            uint8_t y = 1;

            // the fire starts from the bottom
            uint8_t oldx = 2 + randi(5, 7);
            uint8_t x = randi(2, 3);
            fillim(im[y], oldx, oldx+x, 1);
        
            oldx = oldx + x + 1;
            x = randi(1, 2);
            fillim(im[y], oldx, oldx+x, 2);
        
            oldx = oldx + x + 1;
            x = randi(2, 4);
            fillim(im[y], oldx, oldx+x, 3);
        
            oldx = oldx + x + 1;
            x = randi(1, 2);
            fillim(im[y], oldx, oldx+x, 2);
        
            oldx = oldx + x + 1;
            x = randi(2, 3);
            fillim(im[y], oldx, oldx+x, 1);
        }

    public:

        Flames() {}

        void init() {

            memset(allim, 0, sizeof(allim));

            for (uint8_t i = 0; i < HISTORY; i++) {
                generateFlames(allim[i]);
            }
        }

        void step() {
            imat++;
            if (imat == HISTORY) {
                imat = 0;
            }
            generateFlames(allim[imat]);
        }

        void draw(int8_t *tbuf) {
            for (uint8_t y = 0; y < DOWN; y++) {
                for (uint8_t x = 0; x < ACROSS; x++) {
                    uint8_t val = 0;
                    // sum up the history of fire
                    for (uint8_t i = 0; i < HISTORY; i++) {
                        val += allim[i][y][x];
                    }
                    setPixelYUV(tbuf, x, y, colourmap[val][0], colourmap[val][1], colourmap[val][2]);
                }
            }
        }

        void setColormap(COLOUR_SCHEME scheme) {
            memset(colourmap, 0, sizeof(colourmap));
            // calculate in RGB, then convert to YUV

            switch (scheme) {
                case BLUE:
                    // to be written...
                case GREEN:
                    // to be written...
                default:
                case RED:
                    for (uint8_t i = 0; i < NUM_COLOURS; i++) {
                        colourmap[i][0] = 127;
                    }
                    for (uint8_t i = 0; i < NUM_COLOURS; i++) {
                        colourmap[i][1] = (127*i)/NUM_COLOURS;
                    }
                    colourmap[1][0] = 63;
                    colourmap[3][0] = 89;
                    break;
            }
            colourmap[0][0] = 0;
            colourmap[0][1] = 0;
            colourmap[0][2] = 0;

            int32_t y, u, v;
            for (uint8_t i = 0; i < NUM_COLOURS; i++) {
                rgb2yuv(colourmap[i][0], colourmap[i][1], colourmap[i][2], y, u, v);
                colourmap[i][0] = y;
                colourmap[i][1] = u;
                colourmap[i][2] = v;
            }
        }

};
