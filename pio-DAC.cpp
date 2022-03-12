#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "hardware/xosc.h"
#include "dac.pio.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <math.h>

uint8_t ADC_TABLE[70] = { 0, 3, 6, 11, 14, 18, 21, 25, 28, 31, 32, 35, 39, 43, 46, 50, 53, 57, 61, 70, 74, 77, 81, 84, 88, 92, 95, 96, 100, 105, 108, 112, 115,119,123, 126, 139, 142, 146, 149, 154, 156, 159, 161, 164, 167, 171, 174, 178, 181, 185, 189, 198, 202, 205, 209, 212, 216, 220, 223 ,224, 227, 230, 234, 237, 241, 244, 248, 251, 255 };

uint8_t levelConversion(uint8_t in) {
    return ADC_TABLE[in];
//    return in;
}

void core1_entry();

#define YRESOLUTION 250
#define YDATA_START 44
#define YDATA_END (YDATA_START+YRESOLUTION)
#define YDATA_NEXTFIELD 312

int main() {
//    stdio_init_all();
//    set_sys_clock_khz(160000, true); // 160 MHz
//    set_sys_clock_khz(284000, true); // top speed: 284 MHz (282?)
    set_sys_clock_khz(280000, true);
//    set_sys_clock_khz(142000, true); // best multiple to sub-fraction signal?
//    set_sys_clock_khz(133000, true);

    xosc_init(); // hardware oscillator for more stable clocks?

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

    uint8_t frequency_divider = 2;
    uint8_t frequency_divider_frac = 0;
    float DACfreq = clock_get_hz(clk_sys) / (frequency_divider + frequency_divider_frac/256); // keep a nice ratio of system clock?
    uint16_t samplesLine = 64 * DACfreq / 1000000; // 64 microseconds
//    float divpervolt = 255 / 1.02; // no ADC scaling!
    float divpervolt = 70 / 1.02; // ADC scaling
    float syncVolts = -0.3;
    float blankVolts = 0.0; 
    float blackVolts =  0.0;
    float whiteVolts = 0.4; // anyhigher and integer wrapping?
    uint8_t levelSync = 0;
//    uint8_t levelBlank = ADC_TABLE[uint8_t((blankVolts - syncVolts) * divpervolt + 0.5)];
//    uint8_t levelBlack = ADC_TABLE[uint8_t((blackVolts - syncVolts) * divpervolt + 0.5)];
//    uint8_t levelWhite = ADC_TABLE[uint8_t((whiteVolts - syncVolts) * divpervolt + 0.5)];
    uint8_t levelBlank = levelConversion(uint8_t((blankVolts - syncVolts) * divpervolt + 0.5));
    uint8_t levelBlack = levelConversion(uint8_t((blackVolts - syncVolts) * divpervolt + 0.5));
    uint8_t levelWhite = levelConversion(uint8_t((whiteVolts - syncVolts) * divpervolt + 0.5));
    float colourCarrier = 4433618.75; // the exact
    // works better with 266/30 or 133/15
//    float colourCarrier = (368/83)*1e6;
//    float colourCarrier = (266/60)*1e6; // 266 MHz & a divider of 15
//    float colourCarrier = 4.4336e6; // 200 MHz & a divider of 11 + 136/256
    float levelBlankU = (blankVolts - syncVolts) * divpervolt + 0.5;
    float levelWhiteU = (whiteVolts - syncVolts) * divpervolt + 0.5;
    float levelColorU = 0.15 * divpervolt + 0.5; // scaled and used to add on top of other signal
    // U suffix indicates it hasn't been ADC converted




    PIO pio = pio0;
    int sm = 0;
    int offset = pio_add_program(pio, &dac_program);
    dac_program_init(pio, sm, offset, 0, frequency_divider, frequency_divider_frac);

    int dma_chan = dma_claim_unused_channel(true);
    dma_channel_config channel_config = dma_channel_get_default_config(dma_chan);

    channel_config_set_transfer_data_size(&channel_config, DMA_SIZE_8); // transfer 8 bits at a time
    channel_config_set_read_increment(&channel_config, true); // go down the buffer as it's read
    channel_config_set_write_increment(&channel_config, false); // always write the data to the same place
    channel_config_set_dreq(&channel_config, pio_get_dreq(pio, sm, true));

    dma_channel_configure(dma_chan,
                          &channel_config,
                          &pio->txf[sm], // write address
                          NULL, // read address
                          samplesLine, // number of data transfers to 
                          false // start immediately
    );


    int dma_chan32 = dma_claim_unused_channel(true);
    dma_channel_config channel_config32 = dma_channel_get_default_config(dma_chan32);

    channel_config_set_transfer_data_size(&channel_config32, DMA_SIZE_32); // transfer 32 bits at a time
    channel_config_set_read_increment(&channel_config32, true); 
    channel_config_set_write_increment(&channel_config32, true);

    dma_channel_configure(dma_chan32,
                          &channel_config32,
                          NULL, // write address
                          NULL, // read address
                          samplesLine/4, // number of data transfers to 
                          false // start immediately
    );





    // http://www.batsocks.co.uk/readme/video_timing.htm
    uint8_t line1[samplesLine];
    uint8_t line3[samplesLine];
    uint8_t line4[samplesLine];
    uint8_t line6[samplesLine];
    uint8_t line6odd[samplesLine];
    uint8_t line6even[samplesLine];
    uint8_t line313[samplesLine];
    uint8_t line318[samplesLine];
    uint8_t line623[samplesLine];
    uint8_t aline[samplesLine];
    uint8_t alineOdd[samplesLine];
    uint8_t alineEven[samplesLine];

    uint32_t samplesGap = 4.7 * DACfreq / 1000000;
    uint32_t samplesShortPulse = 2.35 * DACfreq / 1000000;
    uint32_t samplesHsync = 4.7 * DACfreq / 1000000;
    uint32_t samplesBackPorch = 5.7 * DACfreq / 1000000;
    uint32_t samplesFrontPorch = 2 * DACfreq / 1000000;
    uint32_t samplesUntilBurst = 5.6 * DACfreq / 1000000; // burst starts at this time
    uint32_t samplesBurst = 2.7 * DACfreq / 1000000;

    uint32_t halfLine = samplesLine/2;

    // sync is lower, blank is in the middle
    uint32_t i;
    for (i = 0; i < samplesLine; i++) {
        line1[i] = levelSync;
        line3[i] = levelSync;
        line4[i] = levelSync;
        line6[i] = levelBlank;
        line313[i] = levelSync;
        line318[i] = levelBlank;
        line623[i] = levelBlank;
//        aline[i] = levelWhite; // white except for horizontal sync
        aline[i] = levelBlank; // just blank
//        aline[i] = syncVolts;
    }

    float COS[samplesLine];
    float SIN[samplesLine];
    for (i = 0; i < samplesLine; i++) {
        float x = i/DACfreq*2.0*M_PI*colourCarrier+135.0/180.0*M_PI;
        COS[i] = cos(x); // odd lines
        SIN[i] = sin(x); // even lines
        if (COS[i] > 0) // square wave approximation
            COS[i] = 1;
        else
            COS[i] = -1;
        if (SIN[i] > 0)
            SIN[i] = 1;
        else
            SIN[i] = -1;
    }

    uint8_t burstOdd[samplesBurst]; // for odd lines
    uint8_t burstEven[samplesBurst]; // for even lines
    for (i = 0; i < samplesBurst; i++) {
        burstOdd[i] = levelConversion(levelColorU*COS[i] + levelBlankU); // with out addition it would try and be negative
        burstEven[i] = levelConversion(levelColorU*SIN[i] + levelBlankU);
//        burstOdd[i] = levelConversion(60);
//        burstEven[i] = levelConversion(60);
    }


    // over drive on the way up
/*    for (i = samplesBurst-1; i > 1; i--) {
        if (burstOdd[i-1] < burstEven[i]) {
            burstOdd[i-1]++;
        }
        if (burstEven[i-1] < burstEven[i]) {
            burstEven[i-1]++;
        }
    }*/

    for (i = halfLine-samplesGap-1; i < halfLine; i++) { // broad sync x2
        line1[i] = levelBlank;
        line1[i+halfLine] = levelBlank;
    }
    for (i = halfLine-samplesGap-1; i < halfLine; i++) { // first half broad sync
        line3[i] = levelBlank;
    }
    for (i = halfLine+samplesShortPulse; i < samplesLine; i++) { // second half short pulse
        line3[i] = levelBlank;
    }
    for (i = halfLine+samplesShortPulse; i < samplesLine; i++) { // short pulse x2
        line4[i-halfLine] = levelBlank;
        line4[i] = levelBlank;
    }
    for (i = 0; i < samplesHsync; i++) { // horizontal sync
        line6[i] = levelSync;
        aline[i] = levelSync;
    }
    for (i = halfLine-samplesGap-1; i < halfLine; i++) { // first half short pulse
        line313[i+halfLine] = levelBlank;
    }
    for (i = halfLine+samplesShortPulse; i < samplesLine; i++) { // second half broad sync
        line313[i-halfLine] = levelBlank;
    }
    for (i = 0; i < samplesShortPulse; i++) { // first half short pulse
        line318[i] = levelSync;
    }
    for (i = 0; i < samplesHsync; i++) { // first half horizontal sync
        line623[i] = levelSync;
    }
    for (i = halfLine; i < samplesShortPulse; i++) { // second half short pulse
        line623[i] = levelSync;
    }
    for (i = samplesHsync; i < samplesHsync+samplesBackPorch; i++) { // back porch
        aline[i] = levelBlank;
    }
    for (i = samplesLine-samplesFrontPorch; i < samplesLine; i++) { // front porch
        aline[i] = levelBlank;
    }
//    for (i = samplesHsync+samplesBackPorch; i < samplesLine-samplesFrontPorch; i++) {
//        aline[i] = levelBlank+i/8;
//    }
    for (i = samplesHsync+samplesBackPorch; i < samplesLine-samplesFrontPorch; i++) {
//        aline[i] = levelWhite;
    }

    // effective 'resolution' 999x260, but vertical distinction, it's more like 333x250
/*    for (i = 230; i < 230+999; i+=9) {
//        aline[i] = levelBlank;
        aline[i] = levelWhite;
        aline[i+1] = levelWhite;
        aline[i+2] = levelWhite;
    }*/

    memcpy(line6odd, line6, samplesLine);
    memcpy(line6even, line6, samplesLine);

    memcpy(line6odd+samplesUntilBurst, burstOdd, samplesBurst);
    memcpy(line6even+samplesUntilBurst, burstEven, samplesBurst);

    memcpy(alineOdd, aline, samplesLine);
    memcpy(alineEven, aline, samplesLine);

    memcpy(alineOdd+samplesUntilBurst, burstOdd, samplesBurst);
    memcpy(alineEven+samplesUntilBurst, burstEven, samplesBurst);


    float r = 0.5;
    float g = 0;
    float b = 0;
    float y = 0.299 * r + 0.587 * g + 0.114 * b; 
    float u = 0.493 * (b - y);
    float v = 0.877 * (r - y);
    for (i = samplesHsync+samplesBackPorch+(0.75*(DACfreq / 1000000)); i < samplesLine-samplesFrontPorch-(0.75*(DACfreq / 1000000)); i++) {
        // odd lines of fields 1 & 2 and even lines of fields 3 & 4?
        alineOdd[i]  = levelConversion(levelBlankU + levelWhiteU * (y + u * SIN[i] + v * COS[i]));
        // even lines of fields 1 & 2 and odd lines of fields 3 & 4?
        alineEven[i] = levelConversion(levelBlankU + levelWhiteU * (y + u * SIN[i] - v * COS[i]));

//        alineOdd[i]  = levelConversion(levelBlankU + levelWhiteU);
//        alineEven[i] = levelConversion(levelBlankU + levelWhiteU);
    }





    uint8_t* alllines[625+1];
    alllines[1] = line1;
    alllines[2] = line1;
    alllines[3] = line3;
    alllines[4] = line4;
    alllines[5] = line4;
    for (i = 6; i < 23; i++) {
        alllines[i] = line6;
    }
    for (i = 23; i < 310; i++) { // data
//        alllines[i] = line6;
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
    alllines[314] = line1;
    alllines[315] = line1;
    alllines[316] = line4;
    alllines[317] = line4;
    alllines[318] = line318;
    for (i = 319; i < 336; i++) {
        alllines[i] = line6;
    }
    for (i = 336; i < 623; i++) { // data
        alllines[i] = line6;
    }
    alllines[623] = line623;
    alllines[624] = line4;
    alllines[625] = line4;

    for (i = YDATA_START; i < YDATA_END; i+=1) {
//    for (i = YDATA_START+50; i < YDATA_START+100; i+=1) {
        alllines[i] = aline; // both fields
        alllines[i+YDATA_NEXTFIELD] = aline;
    }









//    multicore_launch_core1(core1_entry);

//    uint64_t a, b;
    bool led = false;
    uint8_t bufferline[2][samplesLine];
    uint8_t bat = 0;
    while (1) {

//        a = to_us_since_boot(get_absolute_time());

        for (uint16_t at = 1; at < 313; at++) { // would be 626, but 313 is a psuedo progressive

            dma_channel_wait_for_finish_blocking(dma_chan);
            dma_channel_set_read_addr(dma_chan, bufferline[bat++], true);
            // post increment: return the current value then increment
            // so if bat = 0 then the next bit of code operates on bat = 1
            if (bat == 2) {
                bat = 0;
            }

//            if (at > 50 && at < 311) {
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

//        b = to_us_since_boot(get_absolute_time());
//        printf("on time: %lld\n", b - a);

        gpio_put(18, led = !led);
    }
}


void core1_entry() {
}
