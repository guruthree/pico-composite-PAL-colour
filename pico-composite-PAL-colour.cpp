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
#include "dac.pio.h"

// replace with const?
#define CLOCK_SPEED 266e6
#define CLOCK_DIV 3.99974249778092305618 // clock divided by carrier divided by 15
#define DAC_FREQ (CLOCK_SPEED / CLOCK_DIV) // this should be

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
#include "time.h"
#include "font.h"
#include "testcardf.h"
#include "raspberrypi.h"
#include "lbm.h"
#include "cube.h"
#include "flames.h"

ColourPal cp;
LBM lbm; // high memory requirements, so global
Flames fire;

void core1_entry();

int main() {
//    stdio_init_all();
    set_sys_clock_khz(CLOCK_SPEED/1e3, true);

//    sleep_ms(1500);
//    printf("hello world\n");

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
    lbm.cylinder(14);

    TriangleRenderer tr;

    #define NUM_CUBES 10
    Cube* cubes[NUM_CUBES];
    for (uint8_t i = 0; i < NUM_CUBES; i++) {
        cubes[i] = new Cube(20);
        cubes[i]->randomise();
    }

    fire.init();
    Flames::COLOUR_SCHEME firecmap = Flames::RED;
    fire.setColormap(firecmap);

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

//        at = 4; // set at here to show one specific demo

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
            writeStr(tbuf, 0, 0, "Hello World!", 100, 100, 100);
        }
        else if (at == 1) {
            memcpy(tbuf, testcardfpng, BUF_SIZE);
        }
        else if (at == 2) {
            memcpy(tbuf, raspberrypipng, BUF_SIZE);
        }
        else if (at == 3) {
            // show the LBM simulation
            memset(tbuf, 0, BUF_SIZE);

            // if floating point was faster, we'd calculate multiple frames between renders
            lbm.timestep(true);

            drawlbm(lbm, tbuf);
            writeStr(tbuf, 1, 1, "Lattice", 0, 0, 0);
            writeStr(tbuf, 1, 7, "Boltzmann", 0, 0, 0);
            writeStr(tbuf, 1, 13, "CFD Simulation", 0, 0, 0);
        }
        else if (at == 4) {
            // show a bouncing cube
            memset(tbuf, 0, BUF_SIZE);
            drawLineRGB(tbuf, 0, 0, XRESOLUTION-1, 0, 35, 35, 35);
            drawLineRGB(tbuf, 0, YRESOLUTION-1, XRESOLUTION-1, YRESOLUTION-1, 35, 35, 35);
            drawLineRGB(tbuf, 0, 1, 0, YRESOLUTION-2, 35, 35, 35);
            // - 2 because some of the buffer is actualyl chopped off...
            drawLineRGB(tbuf, XRESOLUTION-2, 1, XRESOLUTION-2, YRESOLUTION-2, 35, 35, 35);

            cubes[0]->step();

            tr.reset();
            tr.addObject(*cubes[0]);
            tr.render(tbuf);
            writeStr(tbuf, 1, 1, " One Cube", 100, 100, 100);
        }
        else if (at == 5) {
            // show many bouncing cube
            memset(tbuf, 0, BUF_SIZE);
            drawLineRGB(tbuf, 0, 0, XRESOLUTION-1, 0, 35, 35, 35);
            drawLineRGB(tbuf, 0, YRESOLUTION-1, XRESOLUTION-1, YRESOLUTION-1, 35, 35, 35);
            drawLineRGB(tbuf, 0, 1, 0, YRESOLUTION-2, 35, 35, 35);
            // - 2 because some of the buffer is actualyl chopped off...
            drawLineRGB(tbuf, XRESOLUTION-2, 1, XRESOLUTION-2, YRESOLUTION-2, 35, 35, 35);

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
            writeStr(tbuf, 1, 1, "Many Cubes", 100, 100, 100);
        }
        else if (at == 6) {
            // animated fire
            memset(tbuf, 0, BUF_SIZE);

            fire.step();
            fire.draw(tbuf);
            if (firecmap == Flames::RED)
                writeStr(tbuf, 1, 1, "Red Flames", 100, 0, 0);
            else if (firecmap == Flames::PURPLE)
                writeStr(tbuf, 1, 1, "Purple Flames", 100, 0, 100);
            else if (firecmap == Flames::BLUE)
                writeStr(tbuf, 1, 1, "Blue Flames", 0, 0, 100);
        }

        if (at > 2) {
            char buf[20];
            memset(buf, 0, sizeof(buf));
            sprintf(buf, "%4.1f ms", ((time() - frame_start_time)/1e3));
            writeStr(tbuf, 35, 119, buf, 20, 20, 120);
        }

        cp.setBuf(tbuf);
        
        if (time() - demo_start_time > 5*1e6) {
            at++;
            demo_start_time = time();
            if (at == 7) {
                if (firecmap == Flames::RED)
                    firecmap = Flames::PURPLE;
                else if (firecmap == Flames::PURPLE)
                    firecmap = Flames::BLUE;
                else if (firecmap == Flames::BLUE)
                    firecmap = Flames::RED;
                fire.setColormap(firecmap);
                at = 1;
            }
        }
        // 50 Hz?
        to_sleep = 20e3 - (time() - frame_start_time);
        if (to_sleep > 0) {
            sleep_ms(to_sleep/1000); 
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
