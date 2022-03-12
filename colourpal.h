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
const uint16_t SAMPLES_BACK_PORCH = 5.7 * DAC_FREQ / 1e6;
const uint16_t SAMPLES_FRONT_PORCH = 2 * DAC_FREQ / 1e6;
const uint16_t SAMPLES_UNTIL_BURST = 5.6 * DAC_FREQ / 1e6; // burst starts at this time
const uint16_t SAMPLES_BURST = 2.7 * DAC_FREQ / 1e6; // we may want this to be divisble by 4 at some point?
const uint16_t SAMPLES_HALFLINE = SAMPLES_PER_LINE / 2;

// why are g and r flipped? who knows...
void rgb2yuv(double g, double r, double b, double &y, double &u, double &v) {
    y = 0.299 * r + 0.587 * g + 0.114 * b; // luminance
    u = 0.493 * (b - y);
    v = 0.877 * (r - y);
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
        float levelBlankU, levelWhiteU, levelColorU; // not yet converted to DAC values

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
        uint8_t line313[SAMPLES_PER_LINE];

        // for colour
        const float COLOUR_CARRIER = 4433618.75; // this needs to divide in some fashion into the DAC_FREQ
        float COS[SAMPLES_PER_LINE];
        float SIN[SAMPLES_PER_LINE];
        // the colour burst
        uint8_t burstOdd[SAMPLES_BURST]; // for odd lines
        uint8_t burstEven[SAMPLES_BURST]; // for even lines

        uint8_t* buf; // image data we are displaying
        uint16_t currentline = 1;
        bool led = false;

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


            // display a test card by default
//            this->setBuf(test_card_f_332x250_R5G6B5_bmp);
        }

        void resetLines() {

            // sync is lower, blank is in the middle
            memset(line1, levelSync, SAMPLES_PER_LINE);
            memset(line3, levelSync, SAMPLES_PER_LINE);
            memset(line4, levelSync, SAMPLES_PER_LINE);
            memset(line313, levelSync, SAMPLES_PER_LINE);

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
            for (i = SAMPLES_HALFLINE-SAMPLES_GAP-1; i < SAMPLES_HALFLINE; i++) { // first half short pulse
                line313[i+SAMPLES_HALFLINE] = levelBlank;
            }
            for (i = SAMPLES_HALFLINE+SAMPLES_SHORT_PULSE; i < SAMPLES_PER_LINE; i++) { // second half broad sync
                line313[i-SAMPLES_HALFLINE] = levelBlank;
            }

            memcpy(line6odd, line6, SAMPLES_PER_LINE);
            memcpy(line6even, line6, SAMPLES_PER_LINE);
        }

        void calculateCarrier() {
            for (uint16_t i = 0; i < SAMPLES_PER_LINE; i++) {
                double x = (float(i) / DAC_FREQ) * 2.0 * M_PI * COLOUR_CARRIER + (135.0 / 180.0) * M_PI;
                COS[i] = cosf(x); // odd lines
                SIN[i] = sinf(x); // even lines (sin vs cos gives the phase shift)
            }
        }

        void populateBurst() {
            for (uint16_t i = 0; i < SAMPLES_BURST; i++) {
                burstOdd[i] = levelConversion(levelColorU*COS[i] + levelBlankU); // with out addition it would try and be negative
                burstEven[i] = levelConversion(levelColorU*SIN[i] + levelBlankU);
            }

            memcpy(line6odd+SAMPLES_UNTIL_BURST, burstOdd, SAMPLES_BURST);
            memcpy(line6even+SAMPLES_UNTIL_BURST, burstEven, SAMPLES_BURST);
        }

        void setBuf(uint8_t *in) {
            buf = in;
        }

        void dmaHandler() {

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
                case 6 ... 22:
                    if (currentline & 1) { // odd
                        dma_channel_set_read_addr(dma_chan, line6odd, true);
                    }
                    else {
                        dma_channel_set_read_addr(dma_chan, line6even, true);
                    }
                    break;
                case 23 ... 309: // in the absence of anything else, empty lines
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

            gpio_put(18, led = !led);
            currentline++;
            if (currentline == 313) {
                currentline = 1;
            }

            // prepare data for next line? raise semaphore for it?
        }

        void start() {
            dma_channel_set_read_addr(dma_chan, line1, true); // everything is set, start!
            currentline++; // onto line 2!
        }

};

ColourPal cp;

void cp_dma_handler() {
    cp.dmaHandler();
}
