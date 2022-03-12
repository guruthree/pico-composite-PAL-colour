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
#define CLOCK_DIV 3.99974249778092305618
//#define CLOCK_SPEED 250e6
//#define CLOCK_DIV 2.2555
//#define CLOCK_SPEED 278e6
//#define CLOCK_DIV 2.09009100447950446622
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

    multicore_launch_core1(core1_entry);

    cp.init();
    cp.start();

    while (1) { tight_loop_contents(); } // need this for USB!
}

uint64_t tmp;
void core1_entry() {
    while (1) {
        tight_loop_contents();
        tmp++;
    }
    
}

