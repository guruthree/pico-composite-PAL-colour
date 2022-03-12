#include "testcardf.h"

#define XRESOLUTION (332/2)
#define YRESOLUTION 250
#define YDATA_START 43
#define YDATA_END (YDATA_START + YRESOLUTION)
#define YDATA_NEXTFIELD 312

// timings - these are here as consts because the division needs to process
// a horizontal line is 64 microseconds
// this HAS to be divisible by 4 to use 32-bit DMA transfers
const uint32_t SAMPLES_PER_LINE = 64 * DAC_FREQ / 1e6; // 4256
const uint32_t SAMPLES_GAP = 4.693 * DAC_FREQ / 1e6; // 312
const uint32_t SAMPLES_SHORT_PULSE = 2.347 * DAC_FREQ / 1e6; // 156
const uint32_t SAMPLES_HSYNC = 4.693 * DAC_FREQ / 1e6; // 312
const uint32_t SAMPLES_BACK_PORCH = 5.599 * DAC_FREQ / 1e6; // 372
const uint32_t SAMPLES_FRONT_PORCH = 1.987 * DAC_FREQ / 1e6; // 132
const uint32_t SAMPLES_UNTIL_BURST = 5.538 * DAC_FREQ / 1e6; // burst starts at this time, 368
const uint32_t SAMPLES_BURST = 2.71 * DAC_FREQ / 1e6; // 180
const uint32_t SAMPLES_HALFLINE = SAMPLES_PER_LINE / 2; // 2128

// where we are copying data to in the scanline
const uint32_t SAMPLES_OFF = (1.025*(DAC_FREQ / 1000000)); // 68, delay after sync before colour data starts
const uint32_t SAMPLES_PER_PIXEL = 16; // was 20
const uint32_t SAMPLES_COLOUR = XRESOLUTION * SAMPLES_PER_PIXEL; // 3320 points of the colour data to send

//const uint32_t SAMPLES_SYNC_PORCHES = SAMPLES_FRONT_PORCH + SAMPLES_HSYNC + SAMPLES_BACK_PORCH + SAMPLES_OFF + <whatever is leftover at the end of the colour data>; // 816
const uint32_t SAMPLES_SYNC_PORCHES = SAMPLES_PER_LINE - SAMPLES_COLOUR; // 936
const uint32_t SAMPLES_DEAD_SPACE = SAMPLES_SYNC_PORCHES - SAMPLES_FRONT_PORCH - SAMPLES_HSYNC - SAMPLES_BACK_PORCH - SAMPLES_OFF; //    the samples at the end of signal right before front porch


// convert to YUV for PAL encoding, RGB should be 0-127
// no these are not the standard equations
// part of that is integer maths, fine, but other wise...
// no, I don't know why it's so far from the standard equations
void rgb2yuv(uint8_t r, uint8_t g, uint8_t b, int32_t &y, int32_t &u, int32_t &v) {
    y = 5 * r / 16 + 9 * g / 16 + b / 8; // luminance
    u = (r - y);
    v = 13 * (b - y) / 16;
}

// the line of colour data being displayed
// put it in its own SRAM bank for most reliable fast RAM access
uint8_t __scratch_y("screenbuffer") screenbuffer_B[SAMPLES_COLOUR];


// note this does psuedo-progressive, which displays the same lines every field
class ColourPal {

    private:
        // voltages
        const float SYNC_VOLTS = -0.3;
        const float BLANK_VOLTS = 0.0; // also black
        const float WHITE_VOLTS = 0.4; // any higher than 0.2 and integer wrapping on green since the DAC really should have been 0 to 1.25 volts
        const float BUSRT_VOLTS = 0.15;
        int32_t levelSync = 0, levelBlank, levelWhite, levelColor; // converted to DAC values

        // setup the DAC and DMA
        PIO pio;
        int pio_sm = 0;
        int pio_offset;
        uint8_t dma_channel_A;//, dma_channel_B;

        // these lines are the same every frame
        uint8_t line1_A[SAMPLES_SYNC_PORCHES];
        uint8_t line4_A[SAMPLES_SYNC_PORCHES];
        uint8_t line6odd_A[SAMPLES_SYNC_PORCHES];
        uint8_t line6even_A[SAMPLES_SYNC_PORCHES];
        uint8_t line6_B[SAMPLES_COLOUR];
        uint8_t line1_B[SAMPLES_COLOUR];
        uint8_t line3_B[SAMPLES_COLOUR];
        uint8_t line4_B[SAMPLES_COLOUR];
        // each PAL line is setup to be an A part and B part so that the colour data is < 4096 bytes and can be fit
        // within SRAM banks 5 and 6 (scratch x and y) to avoid contention with writing to one while the other
        // is read by the DMA... maybe...

        // for colour
        const float COLOUR_CARRIER = 4433618.75; // this ideally needs to divide in some fashion into the DAC_FREQ
        // the pre-calculated colour carrier
        int32_t __attribute__((__aligned__(4))) SIN3[24]; // aligned might not do anything here
        // the colour burst
        uint8_t burstOdd[SAMPLES_BURST]; // for odd lines
        uint8_t burstEven[SAMPLES_BURST]; // for even lines

        uint8_t colourbarsOdd_B[SAMPLES_COLOUR];
        uint8_t colourbarsEven_B[SAMPLES_COLOUR];

        uint8_t* buf = NULL; // image data we are displaying
        uint32_t currentline = 1;
        bool led = false;

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

            // DMA channel for syncs
            dma_channel_A = dma_claim_unused_channel(true);
            dma_channel_config channel_configA = dma_channel_get_default_config(dma_channel_A);

            channel_config_set_transfer_data_size(&channel_configA, DMA_SIZE_32); // transfer 8 bits at a time
            channel_config_set_read_increment(&channel_configA, true); // go down the buffer as it's read
            channel_config_set_write_increment(&channel_configA, false); // always write the data to the same place
            channel_config_set_dreq(&channel_configA, pio_get_dreq(pio, pio_sm, true));

            dma_channel_configure(dma_channel_A,
                                  &channel_configA,
                                  &pio->txf[pio_sm], // write address
                                  NULL, // read address
                                  SAMPLES_SYNC_PORCHES / 4, // number of data transfers to ( / 4 because 32-bit copies are faster)
                                  false // start immediately
            );

            // pre-calculate all the lines that don't change depending on what's being shown
            resetLines();
            // pre-calculate colour burst (includes convserion to DAC voltages)
            populateBurst();
            // default simple test pattern
            createColourBars();

            // repeating section of colour carrier
            // this would be 15, but is longer for the COS to be within the SIN as well
            for (uint32_t i = 0; i < 24; i++) {
                float x = float(i) / DAC_FREQ * 2.0 * M_PI * COLOUR_CARRIER + (135.0 / 180.0) * M_PI;
                SIN3[i] = levelWhite*sinf(x); 
            }

            // initialise front buffer
            memset(screenbuffer_B, levelBlank, SAMPLES_COLOUR);

            // display a test card by default
//            this->setBuf(test_card_f);
        }

        void resetLines() {
            // calculate the sync signals that are used repeatedly

            // sync is lower, blank is in the middle
            memset(line1_A, levelBlank, SAMPLES_SYNC_PORCHES);
            memset(line4_A, levelBlank, SAMPLES_SYNC_PORCHES);
            memset( line6odd_A, levelBlank, SAMPLES_SYNC_PORCHES);
            memset(line6even_A, levelBlank, SAMPLES_SYNC_PORCHES);

            memset(line1_B, levelSync, SAMPLES_COLOUR); // this one is mostly low
            memset(line3_B, levelBlank, SAMPLES_COLOUR);
            memset(line4_B, levelBlank, SAMPLES_COLOUR);
            memset(line6_B, levelBlank, SAMPLES_COLOUR); // this one is easy, nothing else needed

            memset(line1_A + SAMPLES_DEAD_SPACE + SAMPLES_FRONT_PORCH, levelSync, SAMPLES_HSYNC + SAMPLES_BACK_PORCH);
            memset(line4_A + SAMPLES_DEAD_SPACE + SAMPLES_FRONT_PORCH, levelSync, SAMPLES_SHORT_PULSE);
            memset( line6odd_A + SAMPLES_DEAD_SPACE + SAMPLES_FRONT_PORCH, levelSync, SAMPLES_HSYNC);
            memset(line6even_A + SAMPLES_DEAD_SPACE + SAMPLES_FRONT_PORCH, levelSync, SAMPLES_HSYNC);

            memset(line1_B + SAMPLES_HALFLINE - SAMPLES_HSYNC - SAMPLES_BACK_PORCH - SAMPLES_GAP - SAMPLES_OFF, levelBlank, SAMPLES_GAP); // 1st gap
            memset(line1_B + SAMPLES_COLOUR - SAMPLES_GAP + SAMPLES_FRONT_PORCH + SAMPLES_OFF, levelBlank, SAMPLES_GAP - SAMPLES_FRONT_PORCH - SAMPLES_OFF); // 2nd gap

            memset(line3_B, levelSync, SAMPLES_HALFLINE - SAMPLES_HSYNC - SAMPLES_BACK_PORCH - SAMPLES_GAP - SAMPLES_OFF);
            memset(line3_B + SAMPLES_HALFLINE - SAMPLES_HSYNC - SAMPLES_BACK_PORCH - SAMPLES_OFF, levelSync, SAMPLES_SHORT_PULSE);

            memset(line4_B + SAMPLES_HALFLINE - SAMPLES_HSYNC - SAMPLES_BACK_PORCH - SAMPLES_OFF, levelSync, SAMPLES_SHORT_PULSE); // 2nd half of line 4 matches line 3

        }

        void populateBurst() {
            // put the colour burst in the sync data of the lines that need it
            for (uint32_t i = 0; i < SAMPLES_BURST; i++) {
                // the + here is a really fine adjustment on the carrier?
                float x = float(i-6.5) / DAC_FREQ * 2.0 * M_PI * COLOUR_CARRIER + (135.0 / 180.0) * M_PI;
                burstOdd[i] = (levelColor*cosf(x) + levelBlank); // odd lines, with out addition it would try and be negative
                burstEven[i] = (levelColor*sinf(x) + levelBlank); // even lines (sin vs cos gives the phase shift)
            }

            memcpy( line6odd_A + SAMPLES_FRONT_PORCH + SAMPLES_UNTIL_BURST + SAMPLES_DEAD_SPACE, burstOdd, SAMPLES_BURST);
            memcpy(line6even_A + SAMPLES_FRONT_PORCH + SAMPLES_UNTIL_BURST + SAMPLES_DEAD_SPACE, burstEven, SAMPLES_BURST);
        }

        void createColourBars() {

            memset( colourbarsOdd_B, levelBlank, SAMPLES_COLOUR);
            memset(colourbarsEven_B, levelBlank, SAMPLES_COLOUR);

            for (uint32_t i = 0; i < SAMPLES_COLOUR; i++) {

                int32_t y, u, v;
                if (i < (SAMPLES_COLOUR / 8))
                    rgb2yuv(127, 127, 127, y, u, v);
                else if (i < (2 * SAMPLES_COLOUR / 8))
                    rgb2yuv(96, 96, 0, y, u, v);
                else if (i < (3 * SAMPLES_COLOUR / 8))
                    rgb2yuv(0, 96, 96, y, u, v);
                else if (i < (4 * SAMPLES_COLOUR / 8))
                    rgb2yuv(0, 96, 0, y, u, v);
                else if (i < (5 * SAMPLES_COLOUR / 8))
                    rgb2yuv(96, 0, 96, y, u, v);
                else if (i < (6 * SAMPLES_COLOUR / 8))
                    rgb2yuv(96, 0, 0, y, u, v);
                else if (i < (7 * SAMPLES_COLOUR / 8))
                    rgb2yuv(0, 0, 96, y, u, v);
                else
                    rgb2yuv(0, 0, 0, y, u, v);

                float x = float(i) / DAC_FREQ * 2.0 * M_PI * COLOUR_CARRIER + (135.0 / 180.0) * M_PI;
                int32_t COS2 = levelWhite*cosf(x); 
                int32_t SIN2 = levelWhite*sinf(x);

                // odd lines of fields 1 & 2 and even lines of fields 3 & 4?
                // +- cos is flipped...?
                colourbarsOdd_B[i]  = levelBlank + (y * levelWhite + u * SIN2 - v * COS2) / 128;
                // even lines of fields 1 & 2 and odd lines of fields 3 & 4?
                colourbarsEven_B[i] = levelBlank + (y * levelWhite + u * SIN2 + v * COS2) / 128;
            }
        }

        void setBuf(uint8_t *in) {
            buf = in;
        }


        void start() {
            loop();
        }

        inline void __time_critical_func(writepixels)(int32_t dmavfactor, uint8_t *backbuffer_B, uint32_t startpixel, uint32_t endpixel ) {

            // current colour being processed
            int32_t y = 0, u = 0, v = 0;

            // pointer to the buffer data we're displaying
            // note the Y resolution stored is 1/2 the YRESOLUTION, so the offset is divided by 2
            uint8_t *idx = buf + ((currentline - YDATA_START) / 2) * XRESOLUTION + startpixel;
            uint32_t dmai2; // position in line

            // pointer to colour carrier 
            int32_t *SIN3p;
            int32_t *COS3p;

            for (uint32_t i = startpixel*(SAMPLES_PER_PIXEL-1); i < ((SAMPLES_PER_PIXEL-1)*endpixel); i += SAMPLES_PER_PIXEL-1) {
                // 2 bits y, 1 bit sign, 2 bits u, 1 bit sign, 2 bits v
                // make y, u, v out of 127
                y = ((*idx >> 1) & 0b01100000) + levelBlank; // would multiply by levelWhite, then divide by 128, so do nothing and leave it be
                u = (((*idx >> 3) & 7) - 3) << 5;
                v = dmavfactor * (((*(idx++) & 7) - 3) << 5);

                // for each pixel back to the start of the colour carrier
                SIN3p = &SIN3[0];
                COS3p = &SIN3[9];

                for (dmai2 = i; dmai2 < i + SAMPLES_PER_PIXEL-1; dmai2++) {
                    // with SAMPLES_PER_PIXEL-1 for the 15 pixel cycle of the carrier
                    // original equation: levelBlank + (y * levelWhite + u * SIN3[dmai2-i] + dmavfactor * v * SIN3[dmai2-i+9]) / 128;
                    backbuffer_B[dmai2] = y + ((u * (*(SIN3p++)) + v * (*(COS3p++))) >> 7) & 0xFF;
                }
            }
        }

        void __time_critical_func(loop)() {
            // declare the backbuffer locally so it hopefully ends up in the best memory bank...
            uint8_t backbuffer_B[SAMPLES_COLOUR];
            memset(  backbuffer_B, levelBlank, SAMPLES_COLOUR);

            while (true) { // could set this to a bool running; so that we can stop?
                int32_t dmavfactor = 0; // multiply v by +1 or -1 depending on even or odd line

                switch (currentline) {
                    case 1 ... 2:
                        dma_channel_set_read_addr(dma_channel_A, line1_A, true);
                        dma_channel_wait_for_finish_blocking(dma_channel_A);
                        dma_channel_set_trans_count(dma_channel_A, SAMPLES_COLOUR / 4, false);
                        dma_channel_set_read_addr(dma_channel_A, line1_B, true);
                        break;

                    case 3:
                        dma_channel_set_read_addr(dma_channel_A, line1_A, true); // lines 1 and 3 start the same way
                        dma_channel_wait_for_finish_blocking(dma_channel_A);
                        dma_channel_set_trans_count(dma_channel_A, SAMPLES_COLOUR / 4, false);
                        dma_channel_set_read_addr(dma_channel_A, line3_B, true);
                        break;

                    case 4 ... 5:
                        dma_channel_set_read_addr(dma_channel_A, line4_A, true);
                        dma_channel_wait_for_finish_blocking(dma_channel_A);
                        dma_channel_set_trans_count(dma_channel_A, SAMPLES_COLOUR / 4, false);
                        dma_channel_set_read_addr(dma_channel_A, line4_B, true);
                        break;

                    case 6 ... YDATA_START-1:
                        if (currentline & 1) { // odd
                            dma_channel_set_read_addr(dma_channel_A, line6odd_A, true);
                            dma_channel_wait_for_finish_blocking(dma_channel_A);
                            dma_channel_set_trans_count(dma_channel_A, SAMPLES_COLOUR / 4, false);
                            dma_channel_set_read_addr(dma_channel_A, line6_B, true);
                        }
                        else {
                            dma_channel_set_read_addr(dma_channel_A, line6even_A, true);
                            dma_channel_wait_for_finish_blocking(dma_channel_A);
                            dma_channel_set_trans_count(dma_channel_A, SAMPLES_COLOUR / 4, false);
                            dma_channel_set_read_addr(dma_channel_A, line6_B, true);
                        }
                        break;

                    case YDATA_START ... YDATA_END: // in the absence of anything else, empty lines
                        if (currentline & 1) { // odd
                            dma_channel_set_read_addr(dma_channel_A, line6odd_A, true);
                            dmavfactor = 1; // next up is even
                        }
                        else {
                            dma_channel_set_read_addr(dma_channel_A, line6even_A, true);
                            dmavfactor = -1; // next up is odd
                        }

                        // copy the back to front buffer for the upcoming line
                        dmacpy(screenbuffer_B, backbuffer_B, SAMPLES_COLOUR); // 3.2 us

                        // if there's a buffer to show, compute a few lines here while we wait
                        if (buf != NULL) {
//                            gpio_put(26, 1);
                            writepixels(dmavfactor, backbuffer_B, 0, 24); // 20 us
//                            gpio_put(26, 0);
                        }

                        dma_channel_wait_for_finish_blocking(dma_channel_A); // 24 us
                        dma_channel_set_trans_count(dma_channel_A, SAMPLES_COLOUR / 4, false);
                        dma_channel_set_read_addr(dma_channel_A, screenbuffer_B, true);

                        break;

                    case YDATA_END+1 ...309:
                        if (currentline & 1) { // odd
                            dma_channel_set_read_addr(dma_channel_A, line6odd_A, true);
                            dma_channel_wait_for_finish_blocking(dma_channel_A);
                            dma_channel_set_trans_count(dma_channel_A, SAMPLES_COLOUR / 4, false);
                            dma_channel_set_read_addr(dma_channel_A, line6_B, true);
                        }
                        else {
                            dma_channel_set_read_addr(dma_channel_A, line6even_A, true);
                            dma_channel_wait_for_finish_blocking(dma_channel_A);
                            dma_channel_set_trans_count(dma_channel_A, SAMPLES_COLOUR / 4, false);
                            dma_channel_set_read_addr(dma_channel_A, line6_B, true);
                        }
                        break;

                    case 310 ... 312:
                        dma_channel_set_read_addr(dma_channel_A, line4_A, true);
                        dma_channel_wait_for_finish_blocking(dma_channel_A);
                        dma_channel_set_trans_count(dma_channel_A, SAMPLES_COLOUR / 4, false);
                        dma_channel_set_read_addr(dma_channel_A, line4_B, true);
                        break;

                    default:
                        // should never get here
                        break;
                }

                // next up is a picture line, we know this if dmvfactor is -1 or 1
                // otherwise based on line number
                if (dmavfactor != 0 || (currentline >= YDATA_START - 1 && currentline < YDATA_END)) {

                    if (buf == NULL) {
                        // no buffer set so show colour bars instead
                        if (currentline & 1) { // odd, next line is even
                            dmacpy(backbuffer_B, colourbarsEven_B, SAMPLES_COLOUR);
                        }
                        else {
                            dmacpy(backbuffer_B, colourbarsOdd_B, SAMPLES_COLOUR);
                        }
                    }
                    else {
                        // calculate data to show
                        writepixels(dmavfactor, backbuffer_B, 24, 24+50); // 40 us
                    }
                }

                // only continue to the beginning of the loop and restart A after B is finished
                dma_channel_wait_for_finish_blocking(dma_channel_A);
                dma_channel_set_trans_count(dma_channel_A, SAMPLES_SYNC_PORCHES / 4, false);


                gpio_put(18, led = !led); // this really should be flickering more? 

                currentline++;
                if (currentline == 313) {
                    currentline = 1;
                }

            } // while (true)
        } // loop


}; // end class

