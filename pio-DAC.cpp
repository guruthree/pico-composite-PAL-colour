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
}

void core1_entry();

#define YRESOLUTION 250
#define YDATA_START 44
#define YDATA_END (YDATA_START+YRESOLUTION)
#define YDATA_NEXTFIELD 312



int dma_chan, dma_chan32;

float DACfreq;
uint32_t samplesLine;
float syncVolts, blankVolts, blackVolts, whiteVolts;
uint8_t levelSync, levelBlank, levelBlack, levelWhite;
float levelBlankU, levelWhiteU, levelColorU;

int main() {
//    stdio_init_all();
    set_sys_clock_khz(250000, true);
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

    DACfreq = clock_get_hz(clk_sys) / (frequency_divider + frequency_divider_frac/256); // keep a nice ratio of system clock?
    samplesLine = 64 * DACfreq / 1000000; // 64 microseconds
    float divpervolt = 70 / 1.02; // ADC scaling
    syncVolts = -0.3;
    blankVolts = 0.0; 
    blackVolts =  0.0;
    whiteVolts = 0.4; // anyhigher and integer wrapping?
    levelSync = 0;
    levelBlank = levelConversion(uint8_t((blankVolts - syncVolts) * divpervolt + 0.5));
    levelBlack = levelConversion(uint8_t((blackVolts - syncVolts) * divpervolt + 0.5));
    levelWhite = levelConversion(uint8_t((whiteVolts - syncVolts) * divpervolt + 0.5));
    levelBlankU = (blankVolts - syncVolts) * divpervolt + 0.5;
    levelWhiteU = (whiteVolts - syncVolts) * divpervolt + 0.5;
    levelColorU = 0.15 * divpervolt + 0.5; // scaled and used to add on top of other signal

    PIO pio = pio0;
    int sm = 0;
    int offset = pio_add_program(pio, &dac_program);
    dac_program_init(pio, sm, offset, 0, frequency_divider, frequency_divider_frac);

    dma_chan = dma_claim_unused_channel(true);
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

    dma_chan32 = dma_claim_unused_channel(true);
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


    multicore_launch_core1(core1_entry);
//    while (1) { sleep_us(1); }

//    core1_entry();
}

void resetLines(uint8_t line1[], uint8_t line3[], uint8_t line4[], uint8_t line6[], uint8_t line313[], uint8_t aline[]) {

    // sync is lower, blank is in the middle
    for (uint32_t i = 0; i < samplesLine; i++) {
        line1[i] = levelSync;
        line3[i] = levelSync;
        line4[i] = levelSync;
        line6[i] = levelBlank;
        line313[i] = levelSync;
        aline[i] = levelBlank; // just blank
    }

}

uint32_t samplesGap, samplesShortPulse, samplesHsync, samplesBackPorch, samplesFrontPorch, samplesUntilBurst, samplesBurst;
uint32_t halfLine;

void populateLines(uint8_t line1[], uint8_t line3[], uint8_t line4[], uint8_t line6[], uint8_t line313[], uint8_t aline[], 
        uint8_t line6odd[], uint8_t line6even[], uint8_t alineOdd[], uint8_t alineEven[]) {
    uint32_t i;
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
        line313[i-halfLine] = levelBlank;;//
    }
    for (i = samplesHsync; i < samplesHsync+samplesBackPorch; i++) { // back porch
        aline[i] = levelBlank;
    }
    for (i = samplesLine-samplesFrontPorch; i < samplesLine; i++) { // front porch
        aline[i] = levelBlank;
    }
    for (i = samplesHsync+samplesBackPorch; i < samplesLine-samplesFrontPorch; i++) {
//        aline[i] = levelWhite;
    }

    memcpy(line6odd, line6, samplesLine);
    memcpy(line6even, line6, samplesLine);

    memcpy(alineOdd, aline, samplesLine);
    memcpy(alineEven, aline, samplesLine);
}

void calculateCarrier(float colourCarrier, float COS[], float SIN[]) {
    for (uint32_t i = 0; i < samplesLine; i++) {
        float x = i/DACfreq*2.0*M_PI*colourCarrier+135.0/180.0*M_PI;
        COS[i] = cos(x); // odd lines
        SIN[i] = sin(x); // even lines
/*        if (COS[i] > 0) // square wave approximation
            COS[i] = 1;
        else
            COS[i] = -1;
        if (SIN[i] > 0)
            SIN[i] = 1;
        else
            SIN[i] = -1;*/
    }
}

void populateBurst(float COS[], float SIN[], uint8_t burstOdd[], uint8_t burstEven[], 
        uint8_t line6odd[], uint8_t line6even[], uint8_t alineOdd[], uint8_t alineEven[]) {
    for (uint32_t i = 0; i < samplesBurst; i++) {
        burstOdd[i] = levelConversion(levelColorU*COS[i] + levelBlankU); // with out addition it would try and be negative
        burstEven[i] = levelConversion(levelColorU*SIN[i] + levelBlankU);
    }

    memcpy(line6odd+samplesUntilBurst, burstOdd, samplesBurst);
    memcpy(line6even+samplesUntilBurst, burstEven, samplesBurst);

    memcpy(alineOdd+samplesUntilBurst, burstOdd, samplesBurst);
    memcpy(alineEven+samplesUntilBurst, burstEven, samplesBurst);
}

bool active = false;

void core1_entry() {

//    float colourCarrier = 4433618.75;//*1.1692; // the exact 1.1692?
//    float colourCarrier = 4.88e6;
// 4.88e6 on the inside, 5.15e6 on the outside ( while 1us delay, 200 mhz sysclock)
// 5 to 5.39 - 5.09 to 5.10 or 5.17 to 5.18
    float colourCarrier = 5.18e6;

    float r = 0.5;
    float g = 0;
    float b = 0;
    float y = 0.299 * r + 0.587 * g + 0.114 * b; 
    float u = 0.493 * (b - y);
    float v = 0.877 * (r - y);

    uint8_t bufferline[2][samplesLine];

    uint8_t line1[samplesLine];
    uint8_t line3[samplesLine];
    uint8_t line4[samplesLine];
    uint8_t line6[samplesLine];
    uint8_t line6odd[samplesLine];
    uint8_t line6even[samplesLine];
    uint8_t line313[samplesLine];
    uint8_t aline[samplesLine];
    uint8_t alineOdd[samplesLine];
    uint8_t alineEven[samplesLine];

    float COS[samplesLine];
    float SIN[samplesLine];

    samplesGap = 4.7 * DACfreq / 1000000;
    samplesShortPulse = 2.35 * DACfreq / 1000000;
    samplesHsync = 4.7 * DACfreq / 1000000;
    samplesBackPorch = 5.7 * DACfreq / 1000000;
    samplesFrontPorch = 2 * DACfreq / 1000000;
    samplesUntilBurst = 5.6 * DACfreq / 1000000; // burst starts at this time
    samplesBurst = 2.7 * DACfreq / 1000000;

    halfLine = samplesLine / 2;

    uint8_t burstOdd[samplesBurst]; // for odd lines
    uint8_t burstEven[samplesBurst]; // for even lines

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

colourCarrier += 1e3;

            resetLines(line1, line3, line4, line6, line313, aline); // initialise arrays
            populateLines(line1, line3, line4, line6, line313, aline, line6odd, line6even, alineOdd, alineEven); // basic black and white PAL stuff

            calculateCarrier(colourCarrier, COS, SIN); // pre-calculate sine and cosine
            populateBurst(COS, SIN, burstOdd, burstEven, line6odd, line6even, alineOdd, alineEven); // put colour burst in arrays

            for (i = samplesHsync+samplesBackPorch+(0.75*(DACfreq / 1000000)); i < halfLine; i++) { //samplesLine-samplesFrontPorch-(0.75*(DACfreq / 1000000)); i++) {
                // odd lines of fields 1 & 2 and even lines of fields 3 & 4?
                alineOdd[i]  = levelConversion(levelBlankU + levelWhiteU * (y + u * SIN[i] + v * COS[i]));
                // even lines of fields 1 & 2 and odd lines of fields 3 & 4?
                alineEven[i] = levelConversion(levelBlankU + levelWhiteU * (y + u * SIN[i] - v * COS[i]));
            }

            active = true;
            numframes = 0;

//printf("%0.2f\n", colourCarrier);

        }


        if (numframes++ == 150) { active = false; }
    
    }
}
