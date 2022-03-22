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
#define BUF_SIZE (XRESOLUTION*YRESOLUTION/2*3)
int8_t buf0[BUF_SIZE];
int8_t buf1[BUF_SIZE];

#include "random.h"
//#include "testcardf.h"
//#include "raspberrypi.h"
#include "renderlbm.h"
#include "discountadafruitgfx.h"
#include "vectormath.h"

ColourPal cp;
LBM lbm;

void core1_entry();

int main() {
//    stdio_init_all();
    set_sys_clock_khz(CLOCK_SPEED/1e3, true);

//sleep_ms(1500);
//printf("hello world\n");

    gpio_init(18);
    gpio_init(19);
    gpio_init(20);
    gpio_set_dir(18, GPIO_OUT);
    gpio_set_dir(19, GPIO_OUT);
    gpio_set_dir(20, GPIO_OUT);
    gpio_put(18, 1); // R
    gpio_put(19, 1); // G
    gpio_put(20, 1); // B

    gpio_put(20, 0); // B
    sleep_ms(1000);
    gpio_put(20, 1); // B

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


    multicore_launch_core1(core1_entry);

    lbm.init();
    lbm.cylinder(14);

    memset(buf0, 0, BUF_SIZE);
    memset(buf1, 0, BUF_SIZE);
    bool buf = false; // buf0
    int8_t *tbuf;


seed_random_from_rosc();

bool led = true;
uint8_t at = 0;

const uint8_t NUM_CUBES = 1;
const uint8_t NUM_VERTEX = 8*NUM_CUBES;

Vector3 v[NUM_VERTEX];
for (uint8_t i = 0; i < NUM_VERTEX; i+=8) {
    v[i+0] = {-20, -20,  20}; // front bottom left
    v[i+1] = { 20, -20,  20}; // front bottom right
    v[i+2] = { 20,  20,  20}; // front top right
    v[i+3] = {-20,  20,  20}; // front top left
    v[i+4] = {-20, -20, -20}; // back bottom left
    v[i+5] = { 20, -20, -20}; // back bottom right
    v[i+6] = { 20,  20, -20}; // back top right 
    v[i+7] = {-20,  20, -20}; // back top left
}


Vector3 vt[NUM_VERTEX];

const uint8_t NUM_TRIS = 12;
uint8_t triangles[NUM_TRIS*3] = {0, 1, 2, 
                                 2, 3, 0,
                                 4, 5, 6, 
                                 6, 7, 4,
                                 0, 3, 4,
                                 4, 7, 3,
                                 1, 2, 5,
                                 5, 6, 2,
                                 2, 3, 6,
                                 6, 3, 7,
                                 1, 0, 5,
                                 5, 0, 4
                                 };
uint8_t color[NUM_TRIS*3] = {100,   0,   0, 
                             100,   0,   0, 
                               0,   0, 100, 
                               0,   0, 100,
                               0, 100,   0,
                               0, 100,   0,
                             100, 100,   0,
                             100, 100,   0,
                             0, 100,  100,
                             0, 100,  100,
                             100, 0,  100,
                             100, 0,  100
                               };

TriangleDepth tridepths[NUM_TRIS];

Vector3 centres[NUM_CUBES];
//for (uint8_t i = 0; i < NUM_CUBES; i++ ) {
//    centres[i] = { float(rand() % 20) - 10.0f,
//                   float(rand() % 40) - 20.0f,
//                   -float(rand() % 80) };
centres[0] = {0, 0, 0};
//}

float xangle = 0, yangle = 0, zangle = 0;
float dx = 0.10f;
float dy = 0.50f;
float dz = -0.25f;
//float xangle2 = 0, yangle2 = 0, zangle2 = 0;

    while (1) {
//        gpio_put(19, led = !led);


            if (buf) {
                tbuf = buf1;
            }
            else {
                tbuf = buf0;
            }
            buf = !buf;
            memset(tbuf, 20, BUF_SIZE); // back to 0 for black

yangle += 0.02f;
xangle += 0.05f;
zangle += 0.001f;

if (xangle >= 360) xangle -= 360;
if (yangle >= 360) yangle -= 360;
if (zangle >= 360) zangle -= 360;

centres[0].x += dx;
centres[0].y += dy;
centres[0].z += dz;

//if (abs(centres[0].x) > 15) {
//    dx = -dx;
//}
//if (abs(centres[0].y) > 18) {
//    dy = -dy;
//}
if (centres[0].z < -100 || centres[0].z >= 0) {
    dz = -dz;
}

//yangle2 += 0.01f;

//Matrix3 rot = Matrix3::getPerspMatrix({1,1,0.1}).multiply(Matrix3::getRotationMatrix(xangle, yangle, zangle));
Matrix3 rot = Matrix3::getRotationMatrix(xangle, yangle, zangle);
//Matrix3 orbit = Matrix3::getRotationMatrix(xangle2, yangle2, zangle2);

for (uint8_t i = 0; i < NUM_VERTEX; i++) {
//    xt[i] =  (x[i] * cosf(angle) + y[i] * sinf(angle))/2 + 30;
//    yt[i] = -x[i] * sinf(angle) + y[i] * cosf(angle) + 60;
    vt[i] = rot.preMultiply(v[i]);

//vt[i].x -= 10;
//vt[i].y += 30;
vt[i] = vt[i].add(centres[i/8]);
//vt[i] = vt[i].add(orbit.preMultiply(centres[i/8]));
//vt[i].z = vt[i].z - 40;

    vt[i] = vt[i].scale(40.0f / (-vt[i].z/2.0 + 40.0f));

    vt[i].x = (vt[i].x/2) + 32;
    vt[i].y += 62;
}

for (uint8_t i = 0; i < NUM_VERTEX; i++) {
    if (vt[i].x > 60) {
        dx = -abs(dx);
        break;
    }
    else if (vt[i].x < 2) {
        dx = abs(dx);
        break;
    }
}
for (uint8_t i = 0; i < NUM_VERTEX; i++) {
    if (vt[i].y > 120) {
        dy = -abs(dy);
        break;
    }
    else if (vt[i].y < 2) {
        dy = abs(dy);
        break;
    }
}

//fillTriangle(tbuf, xt[0], yt[0], xt[1], yt[1], xt[2], yt[2], 0, 100, 0);

// calculate distance away
for (uint8_t i = 0; i < NUM_TRIS*3; i+=3) {
//    tridepths[i/3].depth = (vt[triangles[i]].z + vt[triangles[i+1]].z + vt[triangles[i+2]].z) / 3;
//    tridepths[i/3].depth = MIN(MIN(vt[triangles[i]].z, vt[triangles[i+1]].z), vt[triangles[i+2]].z);
//    tridepths[i/3].depth = (MIN(MIN(vt[triangles[i]].z, vt[triangles[i+1]].z), vt[triangles[i+2]].z) +
//        MAX(MAX(vt[triangles[i]].z, vt[triangles[i+1]].z), vt[triangles[i+2]].z)) / 2;


if (i % 6 == 0) {
    tridepths[i/3].depth = (vt[triangles[i]].z + vt[triangles[i+1]].z + vt[triangles[i+2]].z + 
        vt[triangles[i+3]].z + vt[triangles[i+4]].z + vt[triangles[i+5]].z) / 6;
}
else {
    tridepths[i/3].depth = (vt[triangles[i]].z + vt[triangles[i+1]].z + vt[triangles[i+2]].z + 
        vt[triangles[i-3]].z + vt[triangles[i-2]].z + vt[triangles[i-1]].z) / 6;
}

    tridepths[i/3].index = i/3;
}

qsort(tridepths, NUM_TRIS, sizeof(TriangleDepth), compare);


/*for (uint8_t i = 0; i < NUM_TRIS*3; i+=3) {
    fillTriangle(tbuf, 
        vt[triangles[i]].x, vt[triangles[i]].y, 
        vt[triangles[i+1]].x, vt[triangles[i+1]].y, 
        vt[triangles[i+2]].x, vt[triangles[i+2]].y, 
        color[i], color[i+1], color[i+2]);
}*/

for (uint8_t i = 0; i < NUM_TRIS; i++) {

    uint8_t idx = tridepths[i].index;
//    uint8_t idx = i;   
    idx = idx*3;

    fillTriangle(tbuf, 
        vt[triangles[idx]].x, vt[triangles[idx]].y, 
        vt[triangles[idx+1]].x, vt[triangles[idx+1]].y, 
        vt[triangles[idx+2]].x, vt[triangles[idx+2]].y, 
        color[idx], color[idx+1], color[idx+2]);
}


//for (uint8_t i = 0; i < NUM_VERTEX; i+=3) {
//    fillTriangle(tbuf, xt[i], yt[i], xt[i+1], yt[i+1], xt[i+2], yt[i+2], 0, 100, 0);
//}


sleep_ms(20);





            cp.setBuf(tbuf);
//            sleep_ms(20); // 50 Hz?

//at += 1;
//if (at >= 127) at = 0;

//        } // time-step
//        tight_loop_contents();

        // do something cool here
/*        if (at == 0) {
            cp.setBuf(NULL);
        }
        else if (at == 1) {
            cp.setBuf(testcardfpng);
        }
        else if (at == 2) {
            cp.setBuf(mycardpng);
        }
        if (++at == 3) at = 0; */
//        sleep_ms(500);


        tight_loop_contents();
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
