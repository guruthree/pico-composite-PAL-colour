#define XRESOLUTION 332
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
// no these are not the standard equations, no I don't know why
void rgb2yuv(uint8_t r, uint8_t g, uint8_t b, int16_t &y, int16_t &u, int16_t &v) {
    y = 0.299 * r + 0.587 * g + 0.114 * b; // luminance
    u = 2 * 0.493 * (r - y);
    v = 0.877 * (b - y);
}

void cp_dma_handler();

// note this does psuedo-progressive, which displays the same lines every field
class ColourPal {

    private:
        // voltages
        const float SYNC_VOLTS = -0.3;
        const float BLANK_VOLTS = 0.0; // also black
        const float WHITE_VOLTS = 0.4; // any higher than 0.2 and integer wrapping on green since the DAC really should have been 0 to 1.25 volts
        uint8_t levelSync = 0, levelBlank, levelWhite; // converted to DAC values
        uint8_t levelBlankU, levelWhiteU, levelColorU; // not yet converted to DAC values

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
        int8_t COS2[SAMPLES_PER_LINE];
        int8_t SIN2[SAMPLES_PER_LINE];
        // the colour burst
        uint8_t burstOdd[SAMPLES_BURST]; // for odd lines
        uint8_t burstEven[SAMPLES_BURST]; // for even lines

        uint8_t colourbarsOdd[SAMPLES_PER_LINE];
        uint8_t colourbarsEven[SAMPLES_PER_LINE];

        // the next line coming up
        uint8_t bufferOdd[SAMPLES_PER_LINE];
        uint8_t bufferEven[SAMPLES_PER_LINE];

        uint8_t* buf = NULL; // image data we are displaying
        uint16_t currentline = 1;
        bool led = false;

        // where we are copying data to in the scanline
        uint16_t ioff = SAMPLES_HSYNC + SAMPLES_BACK_PORCH + (1*(DAC_FREQ / 1000000));
        uint16_t irange = SAMPLES_PER_LINE - SAMPLES_FRONT_PORCH-(1*(DAC_FREQ / 1000000)) - ioff;
        uint8_t SAMPLES_PER_PIXEL = irange / XRESOLUTION;

    public:
        ColourPal() {}

        void init() {

            // pre-calculate voltage levels
            levelBlank = levelConversion(uint8_t((BLANK_VOLTS - SYNC_VOLTS) * DIVISIONS_PER_VOLT + 0.5));
            levelWhite = levelConversion(uint8_t((WHITE_VOLTS - SYNC_VOLTS) * DIVISIONS_PER_VOLT + 0.5));
            levelBlankU = (BLANK_VOLTS - SYNC_VOLTS) * DIVISIONS_PER_VOLT + 0.5;
            levelWhiteU = (WHITE_VOLTS - SYNC_VOLTS) * DIVISIONS_PER_VOLT + 0.5;
            levelColorU = 0.15 * DIVISIONS_PER_VOLT + 0.5; // scaled and used to add on top of other signal

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

            dma_channel_set_irq0_enabled(dma_chan, true);
            irq_set_exclusive_handler(DMA_IRQ_0, cp_dma_handler);
            irq_set_enabled(DMA_IRQ_0, true);

            // pre-calculate all the lines that don't change depending on what's being shown
            resetLines();
            // pre-calculate the colour carrier that the colour data is embeeded in
            calculateCarrier();
            // pre-calculate colour burst (includes convserion to DAC voltages)
            populateBurst();
            // default simple test pattern
            createColourBars();


            // display a test card by default
//            this->setBuf(test_card_f_332x250_R5G6B5_bmp);
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
                COS2[i] = cosf(x)*127.0; // odd lines
                SIN2[i] = sinf(x)*127.0; // even lines (sin vs cos gives the phase shift)
            }
        }

        void populateBurst() {
            for (uint16_t i = 0; i < SAMPLES_BURST; i++) {
                float x = float(i) / DAC_FREQ * 2.0 * M_PI * COLOUR_CARRIER + (135.0 / 180.0) * M_PI;
                burstOdd[i] = levelConversion(levelColorU*cosf(x) + levelBlankU); // with out addition it would try and be negative
                burstEven[i] = levelConversion(levelColorU*sinf(x) + levelBlankU);
            }

            memcpy(line6odd+SAMPLES_UNTIL_BURST, burstOdd, SAMPLES_BURST);
            memcpy(line6even+SAMPLES_UNTIL_BURST, burstEven, SAMPLES_BURST);

            memcpy(colourbarsOdd, line6odd, SAMPLES_PER_LINE);
            memcpy(colourbarsEven, line6even, SAMPLES_PER_LINE);

            memcpy(bufferOdd, line6odd, SAMPLES_PER_LINE);
            memcpy(bufferEven, line6even, SAMPLES_PER_LINE);
        }

        void createColourBars() {

            for (uint16_t i = ioff; i < ioff + irange; i++) {

                int16_t y, u, v;
                if ((i - ioff) < (irange / 8))
                    rgb2yuv(127, 127, 127, y, u, v);
                else if ((i - ioff) < (2 * irange / 8))
                    rgb2yuv(96, 96, 0, y, u, v);
                else if ((i - ioff) < (3 * irange / 8))
                    rgb2yuv(0, 96, 96, y, u, v);
                else if ((i - ioff) < (4 * irange / 8))
                    rgb2yuv(0, 96, 0, y, u, v);
                else if ((i - ioff) < (5 * irange / 8))
                    rgb2yuv(96, 0, 96, y, u, v);
                else if ((i - ioff) < (6 * irange / 8))
                    rgb2yuv(96, 0, 0, y, u, v);
                else if ((i - ioff) < (7 * irange / 8))
                    rgb2yuv(0, 0, 96, y, u, v);
                else
                    rgb2yuv(0, 0, 0, y, u, v);

                // odd lines of fields 1 & 2 and even lines of fields 3 & 4?
                colourbarsOdd[i]  = levelConversion( levelBlankU + (levelWhiteU * (y * 128 + u * SIN2[i-ioff] - v * COS2[i-ioff])) / 16384);
                // even lines of fields 1 & 2 and odd lines of fields 3 & 4?
                colourbarsEven[i] = levelConversion( levelBlankU + (levelWhiteU * (y * 128 + u * SIN2[i-ioff] + v * COS2[i-ioff])) / 16384);
            }

/*for (currentline = YDATA_START+31; currentline < YDATA_START+33; currentline++) {
                uint16_t i = ioff; // where we are in the scanline data
                uint8_t *idx = buf + 0x8a + (currentline - YDATA_START) * 332 * 2; // where we are in the buffer data
//                               for (uint16_t *idx2 = (uint16_t*)idx; idx2 < (uint16_t*)idx + 332 * 2; idx2 += 2) {
uint16_t *idx2 = (uint16_t*)idx;
                    uint16_t r5g6b5 = (*idx2 << 8) | (*idx2 >> 8 );
printf("here2\n\n");
sleep_ms(500);
                    uint8_t r = ((r5g6b5 >> 11) & 0x1F);///32.0;
                    uint8_t g = ((r5g6b5 >> 5) & 0x3F);///64.0;
                    uint8_t b = (r5g6b5 & 0x1F);///32.0;
                    float y, u, v;
       printf("%i, %i, %i, %0.5f, %0.5f, %0.5f, \n", r, g, b);
sleep_ms(500);
                    rgb2yuv(r/32.0, g/64.0, b/32.0, y, u, v);
       printf("%0.5f, %0.5f, %0.5f, \n",y, u, v);
sleep_ms(500);
//                    for (uint16_t i2 = i; i2 < i + SAMPLES_PER_PIXEL; i++) {
                    for (uint16_t i2 = ioff; i2 < ioff + irange; i2++) {

                        if (currentline & 1) {
                            colourbarsEven[i2] = levelConversion(levelBlankU + levelWhiteU * (y + u * SIN2[i2-ioff] + v * COS2[i2-ioff]));
                        }
                        else {
                            colourbarsOdd[i2]  = levelConversion(levelBlankU + levelWhiteU * (y + u * SIN2[i2-ioff] - v * COS2[i2-ioff]));
                        }
//                    }
                }
}*/

        }

        void setBuf(uint8_t *in) {
            buf = in;
        }

        void dmaHandler() {

//while (true) {
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
                    }
                    else {
                        dma_channel_set_read_addr(dma_chan, bufferEven, true);
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


//            if (buf == NULL) {
                if (currentline & 1) { // odd, next line is even
                    memcpy(bufferEven, colourbarsEven, SAMPLES_PER_LINE);
//                    memcpy(bufferEven, line6even, SAMPLES_PER_LINE);
                }
                else {
                    memcpy(bufferOdd, colourbarsOdd, SAMPLES_PER_LINE);
//                    memcpy(bufferOdd, line6odd, SAMPLES_PER_LINE);
                }
//            }



            gpio_put(18, led = !led); // not flashing as it should be?

            currentline++;
            if (currentline == 313) {
                currentline = 1;
            }
//} // while (true)


            // prepare data for next line? raise semaphore for it?
            dma_hw->ints0 = 1u << dma_chan;
        }

        void start() {
            dma_channel_set_read_addr(dma_chan, line1, true); // everything is set, start!
            currentline++; // onto line 2!
//dmaHandler();
        }

};

ColourPal cp;

void cp_dma_handler() {
    cp.dmaHandler();
}
