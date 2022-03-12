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

#define CLOCK_SPEED 266e6
#define CLOCK_DIV 3.99974249778092305618
//#define CLOCK_DIV 7.49951718333923089688
//#define CLOCK_SPEED 250e6
//#define CLOCK_DIV 2.2555
//#define CLOCK_SPEED 200e6
//#define CLOCK_DIV 7.51831296575361474055
#define DAC_FREQ (CLOCK_SPEED / CLOCK_DIV) // this should be


int dma_chan32;
inline void dmacpy(uint8_t *dst, uint8_t *src, uint16_t size) {
    dma_channel_set_trans_count(dma_chan32, size / 4, false);
    dma_channel_set_read_addr(dma_chan32, src, false);
    dma_channel_set_write_addr(dma_chan32, dst, true);
    dma_channel_wait_for_finish_blocking(dma_chan32);
    // if the write addr isn't NULL, it causes some sort of memory contention slowing down the rest of the loop
    dma_channel_set_write_addr(dma_chan32, NULL, false);
}

//mutex_t mx1;

#include "colourpal.h"

//    ColourPal cp;

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


    gpio_init(26); // A0
    gpio_set_dir(26, GPIO_OUT);
    gpio_put(26, 0);

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

//   mutex_init(&mx1);


    multicore_launch_core1(core1_entry);

//    cp.init();
//    cp.start();

//    while (1) { tight_loop_contents(); } // need this for USB!

uint32_t i = 0;
    while (1) {
/*       if (mutex_enter_timeout_us(&mx1, 2)) {
           gpio_put(26,1);
           cp.writepixels(90,90+1);
           gpio_put(26,0);
           mutex_exit(&mx1);
       } */

    test_card_f[i] = 0;
    i++;
    sleep_ms(10);


    }
}

void core1_entry() {

    ColourPal cp;
    cp.init();
    cp.start();

    while (1) {
        tight_loop_contents();
    }
}
