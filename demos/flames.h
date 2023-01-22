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

class Flames {

    public:

        enum COLOUR_SCHEME { RED, BLUE, PURPLE };

    private:

        static const uint8_t ACROSS = 26;
        static const uint8_t DOWN = 20;

        // the output frame is the mean of these frames
        static const uint8_t HISTORY = 8;

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
        int8_t findfirstx(uint8_t (&im)[ACROSS], uint8_t val) {
            int8_t x = 0;
            while (im[x] != val) {
                x++;
                if (x == ACROSS) {
                    return -1;
                }
            }
            return x;
        }

        // index of the last location with a value in a row
        int8_t findlastx(uint8_t (&im)[ACROSS], uint8_t val) {
            int8_t x = ACROSS-1;
            while (im[x] != val) {
                x--;
                if (x == -1) {
                    x = -1;
                    break;
                }
            }
            return x;
        }

        void generateFlames(uint8_t (&im)[DOWN][ACROSS]) {
            memset(im, 0, ACROSS*DOWN*sizeof(uint8_t));
            uint8_t y = 0, p;

            // the fire starts from the bottom
            uint8_t oldx = 2 + randi(4, 6);
            uint8_t x = randi(2, 3);
            fillim(im[y], oldx, oldx+x, 1);
        
            oldx = oldx + x + 1;
            x = randi(0, 2);
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

            // flame outlines
            uint64_t probs[3];
            for (uint8_t zz = 1; zz <= 3; zz++) {
                for (y = 1; y < DOWN; y++) {
                    if (zz == 1) {
                        if (y <= 5) {
                            probs[0] = 0.30f*RAND_MAX;
                            probs[1] = 0.70f*RAND_MAX;
                            probs[2] = 1.00f*RAND_MAX;
                        }
                        else if (y < 12) {
                            probs[0] = 0.10f*RAND_MAX;
                            probs[1] = 0.30f*RAND_MAX;
                            probs[2] = 1.00f*RAND_MAX;
                        }
                        else {
                            probs[0] = 0.00f*RAND_MAX;
                            probs[1] = 0.02f*RAND_MAX;
                            probs[2] = 1.00f*RAND_MAX;
                        }
                    }
                    else if (zz == 2) {
                        if (y <= 5) {
                            probs[0] = 0.50f*RAND_MAX;
                            probs[1] = 0.60f*RAND_MAX;
                            probs[2] = 1.00f*RAND_MAX;
                        }
                        else if (y < 12) {
                            probs[0] = 0.00f*RAND_MAX;
                            probs[1] = 0.30f*RAND_MAX;
                            probs[2] = 1.00f*RAND_MAX;
                        }
                        else {
                            probs[0] = 0.00f*RAND_MAX;
                            probs[1] = 0.02f*RAND_MAX;
                            probs[2] = 1.00f*RAND_MAX;
                        }
                    }
                    else if (zz == 3) {
                        if (y <= 5) {
                            probs[0] = 0.530f*RAND_MAX;
                            probs[1] = 0.40f*RAND_MAX;
                            probs[2] = 1.00f*RAND_MAX;
                        }
                        else if (y < 12) {
                            probs[0] = 0.00f*RAND_MAX;
                            probs[1] = 0.20f*RAND_MAX;
                            probs[2] = 1.00f*RAND_MAX;
                        }
                        else {
                            probs[0] = 0.00f*RAND_MAX;
                            probs[1] = 0.02f*RAND_MAX;
                            probs[2] = 1.00f*RAND_MAX;
                        }
                    }
        
                    x = findfirstx(im[y-1], zz);
        
                    uint64_t r = rand();
                    if (r < probs[0] && im[y][x-1] == 0 && im[y][x] == 0) // expand
                        im[y][x-1] = zz;
                    else if (r < probs[1] && im[y][x] == 0) // unchange
                        im[y][x] = zz;
                    else {//if (r < probs[2] // contract
                        r = rand();
                        if (r < 1/3*RAND_MAX || y < 8)
                            p = 1;
                        else
                            p = 2;

                        if (im[y][x+p] == 0)
                            im[y][x+p] = zz;
                        else
                            break;

                    }
        
                    x = findlastx(im[y-1], zz);
                    if (im[y][x] == zz || im[y][x-1] == zz) // shape is closed
                        break;
                    r = rand();
                    if (r < probs[0] && im[y][x+1] == 0 && im[y][x] == 0) // expand
                        im[y][x+1] = zz;
                    else if (r < probs[1] && im[y][x] == 0) // unchange
                        im[y][x] = zz;
                    else {//if (r < probs[2] // contract
                        r = rand();
                        if (r < 1/3*RAND_MAX || y < 8)
                            p = 1;
                        else
                            p = 2;

                        if (im[y][x-p] == 0)
                            im[y][x-p] = zz;
                        else
                            break;
                    }
                } // yy
            } // flame outlines

            // fill in flame insides
            for (y = 1; y < DOWN; y++) {
                int8_t xgoal = findlastx(im[y], 1);
                if (xgoal > 0) {
                    for (x = 1; x <= xgoal; x++) {
                        if (im[y][x] > 0 && im[y][x+1] == 0)
                            im[y][x+1] = im[y][x];
                    }
                }
            }
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
            uint16_t xat, yat = YRESOLUTION-10; // it's 250 lines, but 125 in the matrix...
            for (uint8_t y = 0; y < DOWN; y++) {
                xat = XRESOLUTION/2 - (ACROSS/2)*4/HORIZONTAL_DOUBLING;
                for (uint8_t x = 0; x < ACROSS; x++) {
                    uint8_t val = 0;
                    // sum up the history of fire
                    for (uint8_t i = 0; i < HISTORY; i++) {
                        val += allim[i][y][x];
                    }

                    // simple nearest neighbour interpolation?
                    setPixelYUV(tbuf, xat, yat, colourmap[val][0], colourmap[val][1], colourmap[val][2]);
                    setPixelYUV(tbuf, xat, yat-1, colourmap[val][0], colourmap[val][1], colourmap[val][2]);
                    setPixelYUV(tbuf, xat, yat-2, colourmap[val][0], colourmap[val][1], colourmap[val][2]);
                    setPixelYUV(tbuf, xat, yat-3, colourmap[val][0], colourmap[val][1], colourmap[val][2]);

                    setPixelYUV(tbuf, xat+1, yat, colourmap[val][0], colourmap[val][1], colourmap[val][2]);
                    setPixelYUV(tbuf, xat+1, yat-1, colourmap[val][0], colourmap[val][1], colourmap[val][2]);
                    setPixelYUV(tbuf, xat+1, yat-2, colourmap[val][0], colourmap[val][1], colourmap[val][2]);
                    setPixelYUV(tbuf, xat+1, yat-3, colourmap[val][0], colourmap[val][1], colourmap[val][2]);

#if HORIZONTAL_DOUBLING == 2
                    xat += 2;
#else
                    setPixelYUV(tbuf, xat+2, yat, colourmap[val][0], colourmap[val][1], colourmap[val][2]);
                    setPixelYUV(tbuf, xat+2, yat-1, colourmap[val][0], colourmap[val][1], colourmap[val][2]);
                    setPixelYUV(tbuf, xat+2, yat-2, colourmap[val][0], colourmap[val][1], colourmap[val][2]);
                    setPixelYUV(tbuf, xat+2, yat-3, colourmap[val][0], colourmap[val][1], colourmap[val][2]);

                    setPixelYUV(tbuf, xat+3, yat, colourmap[val][0], colourmap[val][1], colourmap[val][2]);
                    setPixelYUV(tbuf, xat+3, yat-1, colourmap[val][0], colourmap[val][1], colourmap[val][2]);
                    setPixelYUV(tbuf, xat+3, yat-2, colourmap[val][0], colourmap[val][1], colourmap[val][2]);
                    setPixelYUV(tbuf, xat+3, yat-3, colourmap[val][0], colourmap[val][1], colourmap[val][2]);

                    xat += 4;
#endif
                }
                yat -= 4;
            }
        }

        void setColormap(COLOUR_SCHEME scheme) {
            memset(colourmap, 0, sizeof(colourmap));
            // calculate in RGB, then convert to YUV

            switch (scheme) {
                case BLUE:
                    // green inside, blue outside
                    for (uint8_t i = 0; i < NUM_COLOURS; i++) {
                        colourmap[i][2] = (120*(NUM_COLOURS-i))/NUM_COLOURS;
                    }
                    for (uint8_t i = 0; i < NUM_COLOURS; i++) {
                        colourmap[i][1] = (120*i)/NUM_COLOURS;
                    }
                    for (uint8_t i = 1; i < NUM_COLOURS/3; i++) {
                        colourmap[i][2] = (120*i)/(NUM_COLOURS/3);
                    }
                    break;

                case PURPLE:
                    // white inside, purple outside
                    for (uint8_t i = 0; i < NUM_COLOURS; i++) {
                        colourmap[i][0] = 120;
                        colourmap[i][2] = 120;
                    }
                    for (uint8_t i = 0; i < NUM_COLOURS; i++) {
                        colourmap[i][1] = (120*i)/NUM_COLOURS;
                    }
                    for (uint8_t i = 1; i < NUM_COLOURS/2; i++) {
                        colourmap[i][0] = (120*i)/(NUM_COLOURS/2);
                        colourmap[i][2] = (120*i)/(NUM_COLOURS/2);
                    }
                    break;

                default:
                case RED:
                    // yellow inside, red outside
                    for (uint8_t i = 0; i < NUM_COLOURS; i++) {
                        colourmap[i][0] = 120;
                    }
                    for (uint8_t i = 0; i < NUM_COLOURS; i++) {
                        colourmap[i][1] = (120*i)/NUM_COLOURS;
                    }
                    for (uint8_t i = 1; i < NUM_COLOURS/3; i++) {
                        colourmap[i][0] = (120*i)/(NUM_COLOURS/3);
                    }
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
