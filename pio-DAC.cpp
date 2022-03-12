#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <math.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "dac.pio.h"
#include "testcardf.h"

#define CLOCK_SPEED 266e6
#define CLOCK_DIV 4
#define DAC_FREQ (CLOCK_SPEED / CLOCK_DIV) // this should be 

#include "colourpal.h"

int dma_chan32;

void core1_entry();

int main() {
    set_sys_clock_khz(CLOCK_SPEED/1e3, true);

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

    // second DMA channel for faster copies
/*    dma_chan32 = dma_claim_unused_channel(true);
    dma_channel_config channel_config32 = dma_channel_get_default_config(dma_chan32);

    channel_config_set_transfer_data_size(&channel_config32, DMA_SIZE_32); // transfer 32 bits at a time
    channel_config_set_read_increment(&channel_config32, true); 
    channel_config_set_write_increment(&channel_config32, true);

    dma_channel_configure(dma_chan32,
                          &channel_config32,
                          NULL, // write address
                          NULL, // read address
                          SAMPLES_PER_LINE / 4, // number of data transfers to 
                          false // start immediately
    );*/

//    multicore_launch_core1(core1_entry);

    cp.init();
    cp.start();

//    while (1) { tight_loop_contents(); } // need this for USB!

//    core1_entry();
}




/*

bool active = false;


void core1_entry() {


    uint8_t bufferline[2][SAMPLES_PER_LINE];

    uint32_t i;
    uint8_t* alllines[313+1];
    alllines[1] = line1;
    alllines[2] = line1;
    alllines[3] = line3;
    alllines[4] = line4;
    alllines[5] = line4;
    for (i = 6; i < 23; i++) {
        alllines[i] = line6;
    }
    for (i = 23; i < 310; i++) { // data
        if (i & 1) { // odd
            alllines[i] = line6odd;
        }
        else {
            alllines[i] = line6even;
        }
    }
    alllines[310] = line4; // 304 lines of picture for pal non-interlace
    alllines[311] = line4;
    alllines[312] = line4;
    alllines[313] = line313;

    for (i = YDATA_START; i < YDATA_END; i+=1) {
        alllines[i] = aline; // both fields
    }

    bool led = false;
    uint8_t bat = 0;
    uint8_t numframes = 0;
    while (1) {

        if (active) {

            for (uint16_t at = 1; at < 313; at++) { // would be 626, but 313 is a psuedo progressive

                dma_channel_wait_for_finish_blocking(dma_chan);
                dma_channel_set_read_addr(dma_chan, bufferline[bat++], true);
                // post increment: return the current value then increment
                // so if bat = 0 then the next bit of code operates on bat = 1
                if (bat == 2) {
                    bat = 0;
                }

                if (at >= YDATA_START && at < YDATA_END) {
                    if (at & 1) { // odd
                        dma_channel_set_read_addr(dma_chan32, alineOdd, false);
                    }
                    else { // even
                        dma_channel_set_read_addr(dma_chan32, alineEven, false);
                    }
                }
                else {
                    dma_channel_set_read_addr(dma_chan32, alllines[at], false);
                }

                dma_channel_set_write_addr(dma_chan32, bufferline[bat], true);
                dma_channel_wait_for_finish_blocking(dma_chan32);
                // if the write addr isn't NULL, it causes some sort of memory contention slowing down the rest of the loop
                dma_channel_set_write_addr(dma_chan32, NULL, false);

            }
            gpio_put(18, led = !led);

        }
        else {

//colourCarrier += 2e5;

            resetLines(line1, line3, line4, line6, line313, aline); // initialise arrays
            populateLines(line1, line3, line4, line6, line313, aline, line6odd, line6even, alineOdd, alineEven); // basic black and white PAL stuff

            calculateCarrier(colourCarrier, COS, SIN); // pre-calculate sine and cosine
            populateBurst(COS, SIN, burstOdd, burstEven, line6odd, line6even, alineOdd, alineEven); // put colour burst in arrays

//            for (i = samplesHsync+samplesBackPorch+(0.75*(DAC_FREQ / 1000000)); i < halfLine; i++)
            uint32_t ioff = samplesHsync+samplesBackPorch+(1*(DAC_FREQ / 1000000));
            uint32_t ioff2 = ioff - 5;
            uint32_t irange = SAMPLES_PER_LINE-samplesFrontPorch-(1*(DAC_FREQ / 1000000)) - ioff;
            for (i = ioff; i < ioff + irange; i++) {


                double y, u, v;
                if ((i - ioff) < (irange / 8))
                    rgb2yuv(1, 1, 1, y, u, v);
                else if ((i - ioff) < (2 * irange / 8))
                    rgb2yuv(0.75, 0.75, 0, y, u, v);
                else if ((i - ioff) < (3 * irange / 8))
                    rgb2yuv(0, 0.75, 0.75, y, u, v);
                else if ((i - ioff) < (4 * irange / 8))
                    rgb2yuv(0, 0.75, 0, y, u, v);
                else if ((i - ioff) < (5 * irange / 8))
                    rgb2yuv(0.75, 0, 0.75, y, u, v);
                else if ((i - ioff) < (6 * irange / 8))
                    rgb2yuv(0.75, 0, 0, y, u, v);
                else if ((i - ioff) < (7 * irange / 8))
                    rgb2yuv(0, 0, 0.75, y, u, v);
                else
                    rgb2yuv(0, 0, 0, y, u, v);

                // odd lines of fields 1 & 2 and even lines of fields 3 & 4?
                alineOdd[i]  = levelConversion(levelBlankU + levelWhiteU * (y + u * SIN[i-ioff2] + v * COS[i-ioff2]));
                // even lines of fields 1 & 2 and odd lines of fields 3 & 4?
                alineEven[i] = levelConversion(levelBlankU + levelWhiteU * (y + u * SIN[i-ioff2] - v * COS[i-ioff2]));




//                alineOdd[i]  = levelConversion(levelBlankU + levelWhiteU * (y + v * SIN[i] + u * COS[i]));
//                alineEven[i] = levelConversion(levelBlankU + levelWhiteU * (y + v * SIN[i] - u * COS[i]));

//        alineOdd[i]  = levelConversion(levelBlankU + levelWhiteU);*2
//        alineEven[i] = levelConversion(levelBlankU + levelWhiteU);

//printf("%i, %0.8f, %0.8f\n", i, levelBlankU + levelWhiteU * (y + u * SIN[i] + v * COS[i]), levelBlankU + levelWhiteU * (y + u * SIN[i] - v * COS[i]));
//printf("%i, %i, %i\n", i, alineOdd[i+300], alineEven[i+300]);
//sleep_ms(1);
            }

            active = true;
            numframes = 0;

//printf("%0.2f\n", colourCarrier);

        }


//        if (numframes++ == 150) { active = false; }
    
    }
}*/
