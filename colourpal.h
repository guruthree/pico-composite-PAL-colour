#include "testcardf.h"

#define XRESOLUTION (332/2)
#define YRESOLUTION 250
#define YDATA_START 43
#define YDATA_END (YDATA_START + YRESOLUTION)
#define YDATA_NEXTFIELD 312

// timings - these are here as consts because the division needs to process
// a horizontal line is 64 microseconds
// this HAS to be divisible by 4 to use 32-bit DMA transfers
const uint16_t SAMPLES_PER_LINE = 64 * DAC_FREQ / 1e6;
const uint16_t SAMPLES_GAP = 4.7 * DAC_FREQ / 1e6;
const uint16_t SAMPLES_SHORT_PULSE = 2.35 * DAC_FREQ / 1e6;
const uint16_t SAMPLES_HSYNC = 4.7 * DAC_FREQ / 1e6;
const uint16_t SAMPLES_BACK_PORCH = 5.6 * DAC_FREQ / 1e6;
const uint16_t SAMPLES_FRONT_PORCH = 2 * DAC_FREQ / 1e6;
const uint16_t SAMPLES_UNTIL_BURST = 5.55 * DAC_FREQ / 1e6; // burst starts at this time
const uint16_t SAMPLES_BURST = 2.7 * DAC_FREQ / 1e6; // we may want this to be divisble by 4 at some point?
const uint16_t SAMPLES_HALFLINE = SAMPLES_PER_LINE / 2;

// convert to YUV for PAL encoding, RGB should be 0-127
// no these are not the standard equations
// part of that is integer maths, fine, but other wise...
// no, I don't know why
void rgb2yuv(uint8_t r, uint8_t g, uint8_t b, int16_t &y, int16_t &u, int16_t &v) {
//    y = 0.299 * r + 0.587 * g + 0.114 * b; // luminance
//    u = 2 * 0.493 * (r - y);
//    v = 0.877 * (b - y);
    y = 5 * r / 16 + 9 * g / 16 + b / 8;
    u = (r - y);
    v = 7 * (b - y) / 8;
}

void cp_dma_handler();

// note this does psuedo-progressive, which displays the same lines every field
class ColourPal {

    private:
        // voltages
        const float SYNC_VOLTS = -0.3;
        const float BLANK_VOLTS = 0.0; // also black
        const float WHITE_VOLTS = 0.4; // any higher than 0.2 and integer wrapping on green since the DAC really should have been 0 to 1.25 volts
        const float BUSRT_VOLTS = 0.15;
        uint8_t levelSync = 0, levelBlank, levelWhite, levelColor; // converted to DAC values

        // setup the DAC and DMA
        PIO pio;
        int pio_sm = 0;
        int pio_offset;
        uint8_t dma_chan;

        // these lines are the same every frame
        uint8_t line1[SAMPLES_PER_LINE];
        uint8_t line3[SAMPLES_PER_LINE];
        uint8_t line4[SAMPLES_PER_LINE];
        uint8_t line6odd[SAMPLES_PER_LINE];
        uint8_t line6even[SAMPLES_PER_LINE];

        // for colour
        const float COLOUR_CARRIER = 4433618.75; // this needs to divide in some fashion into the DAC_FREQ
        int16_t COS2[SAMPLES_PER_LINE];
        int16_t SIN2[SAMPLES_PER_LINE];
        // the colour burst
        uint8_t burstOdd[SAMPLES_BURST]; // for odd lines
        uint8_t burstEven[SAMPLES_BURST]; // for even lines

        uint8_t colourbarsOdd[SAMPLES_PER_LINE];
        uint8_t colourbarsEven[SAMPLES_PER_LINE];

        // the next line coming up
//        uint8_t bufferOdd[SAMPLES_PER_LINE];
//        uint8_t bufferEven[SAMPLES_PER_LINE];
//        uint8_t *bufferOdd = NULL;
//        uint8_t *bufferEven = NULL;

        uint8_t* buf = NULL; // image data we are displaying
        uint16_t currentline = 1;
        bool led = false;

        // where we are copying data to in the scanline
        uint16_t ioff = SAMPLES_HSYNC + SAMPLES_BACK_PORCH + (1*(DAC_FREQ / 1000000));
        uint16_t irange = SAMPLES_PER_LINE - SAMPLES_FRONT_PORCH-(1*(DAC_FREQ / 1000000)) - ioff;
        uint16_t SAMPLES_PER_PIXEL = irange / XRESOLUTION;

    public:
        ColourPal() {}

        void init() {

            // pre-calculate voltage levels
            levelBlank = (BLANK_VOLTS - SYNC_VOLTS) * DIVISIONS_PER_VOLT + 0.5;
            levelWhite = (WHITE_VOLTS - SYNC_VOLTS) * DIVISIONS_PER_VOLT + 0.5;
            levelColor = BUSRT_VOLTS * DIVISIONS_PER_VOLT + 0.5; // scaled and used to add on top of other signal

            // setup pio & dma copies
            pio = pio0;
            pio_offset = pio_add_program(pio, &dac_program);

            dac_program_init(pio, pio_sm, pio_offset, 0, CLOCK_DIV);
            dma_chan = dma_claim_unused_channel(true);
            dma_channel_config channel_config = dma_channel_get_default_config(dma_chan);

            channel_config_set_transfer_data_size(&channel_config, DMA_SIZE_32); // transfer 8 bits at a time
            channel_config_set_read_increment(&channel_config, true); // go down the buffer as it's read
            channel_config_set_write_increment(&channel_config, false); // always write the data to the same place
            channel_config_set_dreq(&channel_config, pio_get_dreq(pio, pio_sm, true));

            dma_channel_configure(dma_chan,
                                  &channel_config,
                                  &pio->txf[pio_sm], // write address
                                  NULL, // read address
                                  SAMPLES_PER_LINE / 4, // number of data transfers to ( / 4 because 32-bit copies are faster)
                                  false // start immediately
            );

//            dma_channel_set_irq0_enabled(dma_chan, true);
//            irq_set_exclusive_handler(DMA_IRQ_0, cp_dma_handler);
//            irq_set_enabled(DMA_IRQ_0, true);

            // pre-calculate all the lines that don't change depending on what's being shown
            resetLines();
            // pre-calculate the colour carrier that the colour data is embeeded in
            calculateCarrier();
            // pre-calculate colour burst (includes convserion to DAC voltages)
            populateBurst();
            // default simple test pattern
            createColourBars();


            // display a test card by default
            this->setBuf(test_card_f);
//            setBuf((uint8_t*)1);
        }

        void resetLines() {

            // sync is lower, blank is in the middle
            memset(line1, levelSync, SAMPLES_PER_LINE);
            memset(line3, levelSync, SAMPLES_PER_LINE);
            memset(line4, levelSync, SAMPLES_PER_LINE);

            uint8_t line6[SAMPLES_PER_LINE];
            memset(line6, levelBlank, SAMPLES_PER_LINE);

            // TODO: convert to memset
            uint16_t i;
            for (i = SAMPLES_HALFLINE-SAMPLES_GAP-1; i < SAMPLES_HALFLINE; i++) { // broad sync x2
                line1[i] = levelBlank;
                line1[i+SAMPLES_HALFLINE] = levelBlank;
            }
            for (i = SAMPLES_HALFLINE-SAMPLES_GAP-1; i < SAMPLES_HALFLINE; i++) { // first half broad sync
                line3[i] = levelBlank;
            }
            for (i = SAMPLES_HALFLINE+SAMPLES_SHORT_PULSE; i < SAMPLES_PER_LINE; i++) { // second half short pulse
                line3[i] = levelBlank;
            }
            for (i = SAMPLES_HALFLINE+SAMPLES_SHORT_PULSE; i < SAMPLES_PER_LINE; i++) { // short pulse x2
                line4[i-SAMPLES_HALFLINE] = levelBlank;
                line4[i] = levelBlank;
            }
            for (i = 0; i < SAMPLES_HSYNC; i++) { // horizontal sync
                line6[i] = levelSync;
            }

            memcpy(line6odd, line6, SAMPLES_PER_LINE);
            memcpy(line6even, line6, SAMPLES_PER_LINE);
        }

        void calculateCarrier() {
            for (uint16_t i = 0; i < SAMPLES_PER_LINE; i++) {
                float x = float(i) / DAC_FREQ * 2.0 * M_PI * COLOUR_CARRIER + ((130.0 + 135.0) / 180.0) * M_PI;
                COS2[i] = levelWhite*cosf(x); // odd lines
                SIN2[i] = levelWhite*sinf(x); // even lines (sin vs cos gives the phase shift)
            }
        }

        void populateBurst() {
            for (uint16_t i = 0; i < SAMPLES_BURST; i++) {
                float x = float(i) / DAC_FREQ * 2.0 * M_PI * COLOUR_CARRIER + (135.0 / 180.0) * M_PI;
                burstOdd[i] = (levelColor*cosf(x) + levelBlank); // with out addition it would try and be negative
                burstEven[i] = (levelColor*sinf(x) + levelBlank);
            }

            memcpy(line6odd+SAMPLES_UNTIL_BURST, burstOdd, SAMPLES_BURST);
            memcpy(line6even+SAMPLES_UNTIL_BURST, burstEven, SAMPLES_BURST);

            memcpy(colourbarsOdd, line6odd, SAMPLES_PER_LINE);
            memcpy(colourbarsEven, line6even, SAMPLES_PER_LINE);

//            memcpy(bufferOdd, line6odd, SAMPLES_PER_LINE);
//            memcpy(bufferEven, line6even, SAMPLES_PER_LINE);
        }

        void createColourBars() {

            for (uint16_t i = 0; i < irange; i++) {

                int16_t y, u, v;
                if (i < (irange / 8))
                    rgb2yuv(127, 127, 127, y, u, v);
                else if (i < (2 * irange / 8))
                    rgb2yuv(96, 96, 0, y, u, v);
                else if (i < (3 * irange / 8))
                    rgb2yuv(0, 96, 96, y, u, v);
                else if (i < (4 * irange / 8))
                    rgb2yuv(0, 96, 0, y, u, v);
                else if (i < (5 * irange / 8))
                    rgb2yuv(96, 0, 96, y, u, v);
                else if (i < (6 * irange / 8))
                    rgb2yuv(96, 0, 0, y, u, v);
                else if (i < (7 * irange / 8))
                    rgb2yuv(0, 0, 96, y, u, v);
                else
                    rgb2yuv(0, 0, 0, y, u, v);

                // odd lines of fields 1 & 2 and even lines of fields 3 & 4?
                colourbarsOdd[i+ioff]  = levelBlank + (y * levelWhite + u * SIN2[i] - v * COS2[i]) / 128;
                // even lines of fields 1 & 2 and odd lines of fields 3 & 4?
                colourbarsEven[i+ioff] = levelBlank + (y * levelWhite + u * SIN2[i] + v * COS2[i]) / 128;
            }
        }

        void setBuf(uint8_t *in) {
            buf = in;
        }




        void dmaHandler() {
        uint8_t bufferOdd[SAMPLES_PER_LINE];
        uint8_t bufferEven[SAMPLES_PER_LINE];

while (true) {
            switch (currentline) {
                case 1 ... 2:
                    dma_channel_set_read_addr(dma_chan, line1, true);
                    break;
                case 3:
                    dma_channel_set_read_addr(dma_chan, line3, true);
                    break;
                case 4 ... 5:
                    dma_channel_set_read_addr(dma_chan, line4, true);
                    break;
                case 6 ... YDATA_START-1:
                    if (currentline & 1) { // odd
                        dma_channel_set_read_addr(dma_chan, line6odd, true);
                    }
                    else {
                        dma_channel_set_read_addr(dma_chan, line6even, true);
                    }
                    break;
                case YDATA_START ... YDATA_END: // in the absence of anything else, empty lines
                    if (currentline & 1) { // odd
                        dma_channel_set_read_addr(dma_chan, bufferOdd, true);
//                        free(bufferEven);
                    }
                    else {
                        dma_channel_set_read_addr(dma_chan, bufferEven, true);
//                        free(bufferOdd);
                    }
                    break;
                case YDATA_END+1 ...309:
                    if (currentline & 1) { // odd
                        dma_channel_set_read_addr(dma_chan, line6odd, true);
                    }
                    else {
                        dma_channel_set_read_addr(dma_chan, line6even, true);
                    }
                    break;
                case 310 ... 312:
                    dma_channel_set_read_addr(dma_chan, line4, true);
                    break;
                default:
                    break;
            }

if (currentline >= YDATA_START - 1 && currentline < YDATA_END) {

            if (buf == NULL) {
                if (currentline & 1) { // odd, next line is even
//bufferEven = (uint8_t*)malloc(SAMPLES_PER_LINE);
                    memcpy(bufferEven, colourbarsEven, SAMPLES_PER_LINE);
//                    memcpy(bufferEven, line6even, SAMPLES_PER_LINE);
                }
                else {
//bufferOdd = (uint8_t*)malloc(SAMPLES_PER_LINE);
                    memcpy(bufferOdd, colourbarsOdd, SAMPLES_PER_LINE);
//                    memcpy(bufferOdd, line6odd, SAMPLES_PER_LINE);
                }
            }
            else {

                // note the Y resolution stored is 1/2 the YRESOLUTION, so the offset is / 2
                uint8_t *idx = buf + ((currentline - YDATA_START) / 2) * XRESOLUTION;

                int16_t y = 50, u = 0, v = 0;
                int16_t dmavfactor;
                uint8_t *dmatargetbuffer;

                uint8_t tbuf[SAMPLES_PER_LINE];
                if (currentline & 1) {// this is multiplied by v to get even odd lines
                    dmatargetbuffer = bufferEven+ioff;
                    dmavfactor = 1;
                //            memcpy(tbuf, line6even, SAMPLES_PER_LINE);
                }
                else {
                    dmatargetbuffer = bufferOdd+ioff;
                    dmavfactor = -1;
                //            memcpy(tbuf, line6odd, SAMPLES_PER_LINE);
                }
                  

//                for (uint16_t i = ioff; i < ioff + irange; i += SAMPLES_PER_PIXEL) {
//                for (uint16_t i = ioff; i < ioff + 2; i += SAMPLES_PER_PIXEL) {
                    y = *idx >> 4;
                    u = (*idx >> 2) & 3;
                    v = *idx & 3;
                    // make y, u, v out of 127
                    y = y * 8;
                    u = u * 32;
                    v = v * 32; 
//dmay++;
//                    for (uint16_t i2 = i; i2 < i + SAMPLES_PER_PIXEL; i2++) {
                    for (uint16_t dmai2 = 0; dmai2 < irange; dmai2++) {
//                        *(++dmatargetbuffer) =  levelBlank + (y * levelWhite + u * SIN2[dmai2] + dmavfactor * v * COS2[dmai2]) / 128 ;
                        tbuf[dmai2] =  levelBlank + (y * levelWhite + u * SIN2[dmai2] + dmavfactor * v * COS2[dmai2]) / 128 ;
                    }
//                    idx++;
//                }

                if (currentline & 1) {// this is multiplied by v to get even odd lines
//                            memcpy(bufferEven, tbuf, SAMPLES_PER_LINE);
                }
                else {
//                            memcpy(bufferOdd, tbuf, SAMPLES_PER_LINE);
                }

            } // buf != null

} // ydata lines


dma_channel_wait_for_finish_blocking(dma_chan);
            gpio_put(18, led = !led); // not flashing as it should be?

            currentline++;
            if (currentline == 313) {
                currentline = 1;
            }
} // while (true)


            // prepare data for next line? raise semaphore for it?
//            dma_hw->ints0 = 1u << dma_chan;
        }

        void start() {
            dma_channel_set_read_addr(dma_chan, line1, true); // everything is set, start!
            currentline++; // onto line 2!
dmaHandler();
        }

};

ColourPal cp;

void cp_dma_handler() {
    cp.dmaHandler();
}
