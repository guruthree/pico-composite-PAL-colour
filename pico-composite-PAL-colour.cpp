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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <math.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "hardware/regs/rosc.h"
#include "hardware/vreg.h"
#include "dac.pio.h"

// find a CLOCKS_SPEED close to a multiple of the PAL carrier frequency
// using pico-sdk/src/rp2_common/hardware_clocks/scripts/vcocalc.py
// then tweak it and CLOCK_DIV until a colour picture comes through

#define CLOCK_SPEED 321e6
// the divider adjusted by 1.0040, 41, 42, and 43 all works
//#define CLOCK_DIV (9*1.0041)
#define CLOCK_DIV (12*1.004)


#define DAC_FREQ float(CLOCK_SPEED / CLOCK_DIV) // this should be

//#define VREG_VSEL VREG_VOLTAGE_1_15
#define VREG_VSEL VREG_VOLTAGE_1_20
//#define VREG_VSEL VREG_VOLTAGE_1_25
//#define VREG_VSEL VREG_VOLTAGE_1_30

// fast copies of uint8_t arrays, array length needs to be multiple of 4
int dma_chan32;
inline void dmacpy(uint8_t *dst, uint8_t *src, uint16_t size) {
    dma_channel_set_trans_count(dma_chan32, size / 4, false);
    dma_channel_set_read_addr(dma_chan32, src, false);
    dma_channel_set_write_addr(dma_chan32, dst, true);
    dma_channel_wait_for_finish_blocking(dma_chan32);
    // if the write addr isn't NULL, it causes some sort of memory contention slowing down the rest of the loop
    dma_channel_set_write_addr(dma_chan32, NULL, false);
}

#include "colourpal.h"

// one byte y, one byte u, one byte v, repeating for 125x64, line by line
#define BUF_SIZE (XRESOLUTION*YRESOLUTION*3)
int8_t buf0[BUF_SIZE];
int8_t buf1[BUF_SIZE];

#include "random.h"
#include "time_utils.h"
#include "font.h"
#include "testcardf.h"
#include "raspberrypi.h"
#include "lbm.h"
#include "cube.h"
#include "flames.h"
#include "cliffs.h"

#if HORIZONTAL_DOUBLING == 1
    #include "badger.h"
    #include "mushroom1.h"
    #include "mushroom2.h"

    #include "badapple.h"
    #include "sine.h"
    #include "hsv.h"
#endif

ColourPal cp;
#define NUM_DEMOS 10
#define DEMO_DURATION 5 // seconds to show each demo for

// largeish memory requirements (big arrays), so global
LBM lbm;
Flames fire;
Cliffs cliffs;
#define NUM_CUBES 8
Cube* cubes[NUM_CUBES] = {new Cube(20), new Cube(20), new Cube(20), new Cube(20),
                    new Cube(20), new Cube(20), new Cube(20), new Cube(20)};
TriangleRenderer tr;


void core1_entry();

int main() {
	vreg_set_voltage(VREG_VSEL);
    set_sys_clock_khz(CLOCK_SPEED/1000.0f, true);

    gpio_init(18);
    gpio_init(19);
    gpio_init(20);
    gpio_set_dir(18, GPIO_OUT);
    gpio_set_dir(19, GPIO_OUT);
    gpio_set_dir(20, GPIO_OUT);
    gpio_put(18, 1); // R
    gpio_put(19, 1); // G
    gpio_put(20, 1); // B

    gpio_put(19, 0); // G
    sleep_ms(1000);
    gpio_put(19, 1); // G

    // a pin out to use to check timings
//    gpio_init(26); // Tiny2040 A0
//    gpio_set_dir(26, GPIO_OUT);
//    gpio_put(26, 0);

    // setup dma for dmacopy (faster than memcpy)
    dma_chan32 = dma_claim_unused_channel(true);
    dma_channel_config channel_config32 = dma_channel_get_default_config(dma_chan32);

    channel_config_set_transfer_data_size(&channel_config32, DMA_SIZE_32); // transfer 32 bits at a time
    channel_config_set_read_increment(&channel_config32, true); 
    channel_config_set_write_increment(&channel_config32, true);

    dma_channel_configure(dma_chan32,
                          &channel_config32,
                          NULL, // write address
                          NULL, // read address
                          0, // number of data transfers to 
                          false // start immediately
    );

    seed_random_from_rosc();

    multicore_launch_core1(core1_entry);
//    return 0;

    lbm.init();
    lbm.cylinder();

    for (uint8_t i = 0; i < NUM_CUBES; i++) {
        cubes[i]->randomise();
    }

    fire.init();

    cliffs.init();

    #if HORIZONTAL_DOUBLING == 1
        uint32_t currentbadgerframe = 0;
        uint8_t mushroomat = 0;
        uint64_t mushroomtime;

        uint32_t currentbadappleframe = 0;
        float badapplehue = 0;

        int8_t plasmavals[3*3] = {5, 2, 4, 2, 4, 8, 10, 7, 6};
    #endif

    memset(buf0, 0, BUF_SIZE);
    memset(buf1, 0, BUF_SIZE);
    bool buf = false; // buf0
    int8_t *tbuf;

    sleep_ms(1000);

    bool led = true;
    uint8_t at = 1;
    uint64_t numframes = 0;
    uint64_t demo_start_time = time(), frame_start_time;
    int64_t to_sleep;
    while (1) {
        frame_start_time = time();

        // flash LED ahead of calculating frame
        gpio_put(20, led = !led); 
        sleep_us(1000);
        gpio_put(20, led = !led); 

//        at = 7; // set at here to show one specific demo

        // swap between two buffers
        if (buf) {
            tbuf = buf1;
        }
        else {
            tbuf = buf0;
        }
        buf = !buf;

        // loop through some cool demos
        if (at == 0) {
            memset(tbuf, 0, BUF_SIZE);
            writeStr(tbuf, 7, 0, "Hello World!", 100, 100, 100);

            drawLineRGB(tbuf, 0, 20, XRESOLUTION-1, 20, 127, 0, 0);
            drawLineRGB(tbuf, 0, 21, XRESOLUTION-1, 21, 127, 0, 0);
            drawLineRGB(tbuf, 0, 22, XRESOLUTION-1, 22, 127, 0, 0);
            drawLineRGB(tbuf, 0, 40, XRESOLUTION-1, 40, 0, 127, 0);
            drawLineRGB(tbuf, 0, 41, XRESOLUTION-1, 41, 0, 127, 0);
            drawLineRGB(tbuf, 0, 42, XRESOLUTION-1, 42, 0, 127, 0);
            drawLineRGB(tbuf, 0, 60, XRESOLUTION-1, 60, 0, 0, 127);
            drawLineRGB(tbuf, 0, 61, XRESOLUTION-1, 61, 0, 0, 127);
            drawLineRGB(tbuf, 0, 62, XRESOLUTION-1, 62, 0, 0, 127);

            writeStr(tbuf, 2, 20, "the quick brown fox jumps over the lazy dog 1234567890", 120, 120, 120);
            writeStr(tbuf, 2, 30, "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG 1234567890", 120, 120, 120);
            writeStr(tbuf, 2, 40, "the quick brown fox jumps over the lazy dog 1234567890", 120, 0, 0);
            writeStr(tbuf, 2, 50, "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG 1234567890", 120, 0, 0);
            writeStr(tbuf, 2, 60, "the quick brown fox jumps over the lazy dog 1234567890", 0, 120, 0);
            writeStr(tbuf, 2, 70, "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG 1234567890", 0, 120, 0);
            writeStr(tbuf, 2, 80, "the quick brown fox jumps over the lazy dog 1234567890", 0, 0, 120);
            writeStr(tbuf, 2, 90, "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG 1234567890", 0, 0, 120);
        }
        else if (at == 1) {
            memcpy(tbuf, raspberrypipng, BUF_SIZE);
        }
        else if (at == 2) {
            // show BBC2 Test Card F with a tuning in animation
            // current time in seconds
            float now = (time() - demo_start_time) / 1e6;

            // scroll up, start by showing the bottom at the top first, then the roll up
            if (now < 0.3) { // 0.5
                memset(tbuf, 0, BUF_SIZE);
                int8_t yoff = 30 - 25*(cosf(now*M_PI*10/3+M_PI)+1);
                int8_t gap = 20;
                if (yoff > 0) {
                    memcpy(tbuf, testcardfpng+BUF_SIZE-XRESOLUTION*yoff*3, XRESOLUTION*yoff*3);
                }
                memcpy(tbuf+XRESOLUTION*(yoff+gap)*3, testcardfpng, BUF_SIZE-XRESOLUTION*(yoff+gap)*3);
            }
            else if (now < 0.35) { // after the big roll, a little bump
                memset(tbuf, 0, BUF_SIZE);
                int8_t yoff = 3*fabs(sinf((now-0.3)*M_PI*20)); // t - 0.2
                memcpy(tbuf, testcardfpng+XRESOLUTION*yoff*3, BUF_SIZE-XRESOLUTION*yoff*3);
            }
            else {
                // after sync, the image is perfect
                memcpy(tbuf, testcardfpng, BUF_SIZE);
            }

            // flash static for a little bit, in a pulsing pattern
            // also fade the colours in
            if (now < 3) {
                float intensity; // intensity of the static
                if (now < 0.5) {
                    intensity = cosf(2*now*M_PI)*0.4+0.6;
                }
                else if (now < 1.5) {
                    intensity = 0.125*(cosf(2*now*M_PI)+1.125);
                    intensity *= (1.5-now)*0.2;
                }
                else { // after 1.5 seconds, a clear picture
                    intensity = 0;
                }
                int32_t intensityint = intensity * 256; // perform intensity adjustments as integer math
                int32_t t;
                int32_t tint = (now*2.5+0.5)*256; // the colour fades in
                for (int32_t ycoord = 0; ycoord < YRESOLUTION; ycoord++) {
                    for (int32_t xcoord = 12; xcoord < XRESOLUTION-12; xcoord++) {
                        int8_t *idx = tbuf + (ycoord * XRESOLUTION + xcoord) * 3;
                        // the static
                        if (now < 0.5) {
                            t = (*idx - *idx * intensityint / 512) + (randi(-127, 127) * intensityint) / 256;
                        }
                        else if (now < 1.5) {
                            t = *idx + (randi(-127, 127) * intensityint) / 256;
                        }
                        else {
                            t = *idx;
                        }
                        if (t > 127) t = 127;
                        if (t < 0) t = 0;
                        *idx = t;
                        // the colour fade
                        *(idx+1) = (*(idx+1) * tint) / 2048; // divide by 256 * 8
                        *(idx+2) = (*(idx+2) * tint) / 2048;
                    }
                }
            } // now

        }
        else if (at == 3) {
            // show the LBM simulation
            memset(tbuf, 0, BUF_SIZE);

            // if floating point was faster, we'd calculate multiple frames between renders
//            lbm.timestep();
            lbm.timestep(true);
            drawlbm(lbm, tbuf);
            writeStr(tbuf, 12, 1, "Lattice Boltzmann", 100, 100, 100);
            writeStr(tbuf, 12, 7, "CFD Simulation", 100, 100, 100);
            writeStr(tbuf, 12, YRESOLUTION-10, "Eddy Shedding", 100, 100, 100);

            char txtbuf[20];
            memset(txtbuf, 0, sizeof(buf));
            sprintf(txtbuf, "t = %d", lbm.getNumberOfTimeSteps());
            writeStr(tbuf, 12, YRESOLUTION-5, txtbuf, 100, 100, 100);
        }
        else if (at == 4) {
            // show a bouncing cube
            memset(tbuf, 0, BUF_SIZE);
            drawLineRGB(tbuf, 0, 0, XRESOLUTION-1, 0, 50, 50, 50);
            drawLineRGB(tbuf, 0, YRESOLUTION-1, XRESOLUTION-1, YRESOLUTION-1, 50, 50, 50);
            drawLineRGB(tbuf, 0, 1, 0, YRESOLUTION-2, 50, 50, 50);
            // - 2 because some of the buffer is actualyl chopped off...
            drawLineRGB(tbuf, XRESOLUTION-2, 1, XRESOLUTION-2, YRESOLUTION-2, 50, 50, 50);

            cubes[0]->step();

            tr.reset();
            tr.addObject(*cubes[0]);
            tr.render(tbuf);
            writeStr(tbuf, 7, 3, " One Cube", 100, 100, 100);
        }
        else if (at == 5) {
            // show many bouncing cube
            memset(tbuf, 0, BUF_SIZE);
            drawLineRGB(tbuf, 0, 0, XRESOLUTION-2, 0, 50, 50, 50);
            drawLineRGB(tbuf, 0, YRESOLUTION-1, XRESOLUTION-2, YRESOLUTION-1, 50, 50, 50);
            drawLineRGB(tbuf, 0, 1, 0, YRESOLUTION-2, 50, 50, 50);
            // - 2 because some of the buffer is actualyl chopped off...
            drawLineRGB(tbuf, XRESOLUTION-2, 1, XRESOLUTION-2, YRESOLUTION-2, 50, 50, 50);

            for (uint8_t i = 0; i < NUM_CUBES; i++) {
                cubes[i]->step();
            }

            for (uint8_t i = 0; i < NUM_CUBES; i++) {
                for (uint8_t j = i+1; j < NUM_CUBES; j++) {
                    cubes[i]->collide(*cubes[j]);
                }
            }

            tr.reset();
            for (uint8_t i = 0; i < NUM_CUBES; i++) {
                tr.addObject(*cubes[i]);
            }
            tr.render(tbuf);
            writeStr(tbuf, 7, 3, "Many Cubes", 100, 100, 100);
        }
        else if (at == 6) {
            // animated fire
            memset(tbuf, 0, BUF_SIZE);

            fire.step();
            fire.draw(tbuf);
            if (fire.getColormap() == Flames::RED)
                writeStr(tbuf, 7, 3, "Red Flames", 100, 0, 0);
            else if (fire.getColormap() == Flames::PURPLE)
                writeStr(tbuf, 7, 3, "Purple Flames", 100, 0, 100);
            else if (fire.getColormap() == Flames::BLUE)
                writeStr(tbuf, 7, 3, "Blue Flames", 0, 0, 100);
        }
        else if (at == 7) {
            // flying through the cliffs
            memset(tbuf, 0, BUF_SIZE);

            cliffs.step();
            cliffs.render(tbuf);
            writeStr(tbuf, 7, 3, "Synth Cliffs", 0, 0, 0);
        }
#if HORIZONTAL_DOUBLING == 1
        else if (at == 8) {
#else
        // the video won't currently work at low res (need to convert images)
        // so instead just show static
        else if (at == 8 || at == 9 || at == 10) {
#endif
            // static
            memset(tbuf, 0, BUF_SIZE);

            for (int32_t ycoord = 0; ycoord < YRESOLUTION; ycoord++) {
                for (int32_t xcoord = 0; xcoord < XRESOLUTION; xcoord++) {
                    setPixelYUV(tbuf, xcoord, ycoord, rand() & 127, 0, 0);
                }
            }
//            writeStr(tbuf, 7, 3, "Classic Snow", 0, 0, 0);
        }
#if HORIZONTAL_DOUBLING == 1
        else if (at == 9) {
            // run length encoding video
            // badgers & mushrooms
            memset(tbuf, 0, BUF_SIZE);

            if (mushroomat == 0) {
                int8_t* currentframe = badgerframes[currentbadgerframe];
                int8_t *idx = tbuf;

                for (uint32_t j = 0; j < badgerframelengths[currentbadgerframe]; j++) {
                    for (uint8_t i = 0; i < currentframe[j*4 + 3]; i++) {
                        // replicate the specified pixel that number of times
                        *(idx++) = currentframe[j*4 + 0];
                        *(idx++) = currentframe[j*4 + 1];
                        *(idx++) = currentframe[j*4 + 2];
                    }
                }

                if (currentbadgerframe < NUM_BADGERFRAMES-1) { // minus one so it's for the next iteration around
                    if (buf) { // the video is 25 fps, but display is 50, so only update every other frame
                        currentbadgerframe++;
                    }
                }
                else {
                    currentbadgerframe = 0;
                    mushroomat = 1;
                    mushroomtime = time();
                }
            }
            else {
                if (mushroomat == 1) {
                    memcpy(tbuf, mushroom1png, BUF_SIZE);
                }
                else {
                    memcpy(tbuf, mushroom2png, BUF_SIZE);
                }
                if (time() - mushroomtime > 1.65*1e6) {
                    mushroomat = 0;
                }
                else if (time() - mushroomtime > 0.65*1e6) {
                    mushroomat = 2;
                }
            }
//            writeStr(tbuf, 7, 3, "Badger Badger Badger", 127, 127, 127);
        }
#endif
#if HORIZONTAL_DOUBLING == 1
        else if (at == 10) {
            // bad apple demo
            // plasma video effect:
            //     https://rosettacode.org/wiki/Plasma_effect
            //     https://samuel-lereah.com/articles/CS/making-plasma-effects 
            memset(tbuf, 0, BUF_SIZE);

            uint32_t xcoord, ycoord; // screen coordinates
            int8_t y, u, v; // colour at screen coordinate

            // combine three sine waves to make the plasma effect
            // the plasma intensity is mapped to hsv via a colourmap
            uint32_t phase1 = ((time()*64)/1000000) % 128;
            uint32_t phase2 = ((time()*48)/1000000) % 128;
            uint32_t phase3 = ((time()*24)/1000000) % 128;
            int32_t val1, val2, val3;
            uint8_t r; // the final intensity of the plasma

            bool currentcolour = false; // only set colour in place of white (trye)

            uint8_t* currentframe = badappleframes[currentbadappleframe];
            uint32_t pixelnum = 0;

            // run through each frame, decode it, and set the current buffer to either black or white (plasma)
            for (uint32_t j = 0; j < badappleframelengths[currentbadappleframe]; j++) {
                for (uint8_t i = 0; i < currentframe[j]; i++) {

                    if (currentcolour) {
                        xcoord = pixelnum % XRESOLUTION;
                        ycoord = pixelnum / YRESOLUTION;

                        val1 = (plasmavals[0] * xcoord + plasmavals[1] * ycoord ) / plasmavals[2] + phase1;
                        val2 = (plasmavals[3] * xcoord + plasmavals[4] * ycoord ) / plasmavals[5] + phase2;
                        val3 = (plasmavals[6] * xcoord + plasmavals[7] * ycoord ) / plasmavals[8] + phase3;

                        // needs to end up 0-63, sine table ranges 0-128 so mod to get in range
                        // the table returns -127 to 127 so add + numthings*128 to remove negatives
                        //  then divide by numthings*4 to end up offset 0-63
                        r = ((sine_table[val1 % 128] + sine_table[val2 % 128] + sine_table[val3 % 128]) + 3*128) / (3*4);
                        rgb2yuv(hsv[r][0], hsv[r][1], hsv[r][2], y, u, v);
                    }
                    else {
                        y = 0;
                        u = 0;
                        v = 0;
                    }

                    tbuf[pixelnum * 3 + 0] = currentcolour ? y : 0; // 127 : 0
                    tbuf[pixelnum * 3 + 1] = currentcolour ? u : 0;
                    tbuf[pixelnum * 3 + 2] = currentcolour ? v : 0;

                    pixelnum++;
                }
                // the current colour is either black or white, we swap because black & white video
                // is encoded where each byte is the number of black, then number of white, etc
                currentcolour = !currentcolour;
            }

            if (buf) { // the video is 25 fps, but display is 50, so only update every other frame
                currentbadappleframe++;
                if (currentbadappleframe == NUM_BADAPPLEFRAMES) {
                    currentbadappleframe = 0;
                }

                if (currentbadappleframe >= 126) { // there's a matching value to change above
                    badapplehue += 1.0;
                }

                if (badapplehue >= 360) {
                    badapplehue = 0;
                }

                if (currentbadappleframe % (DEMO_DURATION*25) == 0) {
                    // cycle the plasma each time
                    for (uint8_t i = 0; i < 9; i++) {
                        if (rand() > 0.5) {
                            plasmavals[i] = randi(2, 10);
                        }
                        else {
                            plasmavals[i] = randi(-10, -2);
                        }
                    }
                }
            }
//            writeStr(tbuf, 7, 3, "Bad Apple", 0, 0, 0);
        }
#endif
        // end of demo selection

        if (at > 2) {
            char txtbuf[20];
            memset(txtbuf, 0, sizeof(buf));
            sprintf(txtbuf, "%4.1f ms", ((time() - frame_start_time)/1e3));
            writeStr(tbuf, XRESOLUTION-35, YRESOLUTION-5, txtbuf, 100, 100, 100);
        }

        // show the current "channel" (demo) number for a little bit after changing
        if (time() - demo_start_time < 2*1e6) {
            char txtbuf[20];
            sprintf(txtbuf, "%d", at);
            writeStr(tbuf, XRESOLUTION-21+1, 9+1, txtbuf, 0, 0, 0, true);
            writeStr(tbuf, XRESOLUTION-21, 9, txtbuf, 0, 100, 0, true);
        }

        cp.setBuf(tbuf);
        
        if (time() - demo_start_time > DEMO_DURATION*1e6) {
            at++;
            demo_start_time = time();
            if (at == 7) {
                if (fire.getColormap() == Flames::RED)
                    fire.setColormap(Flames::PURPLE);
                else if (fire.getColormap() == Flames::PURPLE)
                    fire.setColormap(Flames::BLUE);
                else if (fire.getColormap() == Flames::BLUE)
                    fire.setColormap(Flames::RED);
            }
            if (at == NUM_DEMOS+1) {
                at = 1;
            }
        }
        // 50 Hz? sleep up to 20 ms to do the next frame
        to_sleep = 20e3 - (time() - frame_start_time);
        if (to_sleep > 0) {
            sleep_us(to_sleep); 
        }

    }
}

// do all composite processing on the second core
void core1_entry() {

    cp.init();
    cp.start();

    // should never get here, cp.start() should loop
    while (1) {
        tight_loop_contents();
    }
}
