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
//#define CLOCK_SPEED 250e6
//#define CLOCK_DIV 2.2555
//#define CLOCK_SPEED 278e6
//#define CLOCK_DIV 2.09009100447950446622
#define DAC_FREQ (CLOCK_SPEED / CLOCK_DIV) // this should be

#include "colourpal.h"


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

//    multicore_launch_core1(core1_entry);

    cp.init();
    cp.start();

//    while (1) { tight_loop_contents(); } // need this for USB!
}

/*uint64_t tmp;
void core1_entry() {
    while (1) {
        tight_loop_contents();
        tmp++;
    }
}*/
