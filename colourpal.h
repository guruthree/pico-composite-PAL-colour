#include "testcardf.h"

#define XRESOLUTION (332/2)
#define YRESOLUTION 250
#define YDATA_START 43
#define YDATA_END (YDATA_START + YRESOLUTION)
#define YDATA_NEXTFIELD 312

// timings - these are here as consts because the division needs to process
// a horizontal line is 64 microseconds
// this HAS to be divisible by 4 to use 32-bit DMA transfers
const uint16_t SAMPLES_PER_LINE = 64 * DAC_FREQ / 1e6; // 4256
const uint16_t SAMPLES_GAP = 4.693 * DAC_FREQ / 1e6; // 312
const uint16_t SAMPLES_SHORT_PULSE = 2.347 * DAC_FREQ / 1e6; // 156
const uint16_t SAMPLES_HSYNC = 4.693 * DAC_FREQ / 1e6; // 312
const uint16_t SAMPLES_BACK_PORCH = 5.599 * DAC_FREQ / 1e6; // 372
const uint16_t SAMPLES_FRONT_PORCH = 1.987 * DAC_FREQ / 1e6; // 132
const uint16_t SAMPLES_UNTIL_BURST = 5.538 * DAC_FREQ / 1e6; // burst starts at this time, 368
const uint16_t SAMPLES_BURST = 2.71 * DAC_FREQ / 1e6; // 180
const uint16_t SAMPLES_HALFLINE = SAMPLES_PER_LINE / 2; // 2128
const uint16_t SAMPLES_SYNC_PORCHES = SAMPLES_FRONT_PORCH + SAMPLES_HSYNC + SAMPLES_BACK_PORCH; // 816
const uint16_t SAMPLES_COLOUR = SAMPLES_PER_LINE - SAMPLES_SYNC_PORCHES; // 3440 points of the colour data to send

// convert to YUV for PAL encoding, RGB should be 0-127
// no these are not the standard equations
// part of that is integer maths, fine, but other wise...
// no, I don't know why
void rgb2yuv(uint8_t r, uint8_t g, uint8_t b, int16_t &y, int16_t &u, int16_t &v) {
    y = 5 * r / 16 + 9 * g / 16 + b / 8;
    u = (r - y);
    v = 7 * (b - y) / 8;
}

// the next line coming up
// assign one to each buffer so one can be DMA read and the other written to
uint8_t __scratch_x("bufferodd") bufferOdd_B[SAMPLES_COLOUR];
uint8_t __scratch_y("buffereven") bufferEven_B[SAMPLES_COLOUR];
//uint8_t bufferOdd_B[SAMPLES_COLOUR];
//uint8_t bufferEven_B[SAMPLES_COLOUR];

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
        // each line is setup to be an A part and B part so that the colour data is < 4096 bytes and can be fit
        // within SRAM banks 5 and 6 (scratch x and y) to avoid contention with writing to one while the other
        // is read by the DMA

        // for colour
        const float COLOUR_CARRIER = 4433618.75; // this needs to divide in some fashion into the DAC_FREQ
        int16_t COS2[SAMPLES_COLOUR];
        int16_t SIN2[SAMPLES_COLOUR];
        // the colour burst
        uint8_t burstOdd[SAMPLES_BURST]; // for odd lines
        uint8_t burstEven[SAMPLES_BURST]; // for even lines

        uint8_t colourbarsOdd_B[SAMPLES_COLOUR];
        uint8_t colourbarsEven_B[SAMPLES_COLOUR];


        uint8_t* buf = NULL; // image data we are displaying
        uint16_t currentline = 1;
        bool led = false;

        // where we are copying data to in the scanline
//        uint16_t ioff = SAMPLES_HSYNC + SAMPLES_BACK_PORCH + (1.025*(DAC_FREQ / 1000000)); // 312 + 372 + 68 = 752
//        uint16_t irange = SAMPLES_PER_LINE - SAMPLES_FRONT_PORCH-(1.025*(DAC_FREQ / 1000000)) - ioff; // 4256 - 132 - 68 - 752 = 3304
        uint16_t ioff = (1.025*(DAC_FREQ / 1000000)); // 68
//        uint16_t irange = SAMPLES_COLOUR - (1.025*(DAC_FREQ / 1000000)) - ioff; // 3440 - 68 - 752 = 3372
//        uint16_t SAMPLES_PER_PIXEL = irange / XRESOLUTION; // 3372 / 166 = 20.3
        uint16_t SAMPLES_PER_PIXEL = 20;
        uint16_t irange = XRESOLUTION*SAMPLES_PER_PIXEL;

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

            // DMA channel for colour data
/*            dma_channel_B = dma_claim_unused_channel(true);
            dma_channel_config channel_configB = dma_channel_get_default_config(dma_channel_B);

            channel_config_set_transfer_data_size(&channel_configB, DMA_SIZE_32); // transfer 8 bits at a time
            channel_config_set_read_increment(&channel_configB, true); // go down the buffer as it's read
            channel_config_set_write_increment(&channel_configB, false); // always write the data to the same place
            channel_config_set_dreq(&channel_configB, pio_get_dreq(pio, pio_sm, true));

            dma_channel_configure(dma_channel_B,
                                  &channel_configB,
                                  &pio->txf[pio_sm], // write address
                                  NULL, // read address
                                  SAMPLES_COLOUR / 4, // number of data transfers to ( / 4 because 32-bit copies are faster)
                                  false // start immediately
            );

            // when channel A has finished sendying the sync + porches, start channel B to send the data
            channel_config_set_chain_to(&channel_configA, dma_channel_B);
            channel_config_set_chain_to(&channel_configB, dma_channel_A); */

//            dma_channel_set_irq0_enabled(dma_channel_B, true);
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

            // buffers by default show nothing
            memset( bufferOdd_B, levelBlank, SAMPLES_COLOUR);
            memset(bufferEven_B, levelBlank, SAMPLES_COLOUR);

            // display a test card by default
            this->setBuf(test_card_f);
        }

        void resetLines() {
            // calculate the repeating syncs

            // sync is lower, blank is in the middle
            memset(line1_A, levelBlank, SAMPLES_SYNC_PORCHES);
            memset(line4_A, levelBlank, SAMPLES_SYNC_PORCHES);
            memset( line6odd_A, levelBlank, SAMPLES_SYNC_PORCHES);
            memset(line6even_A, levelBlank, SAMPLES_SYNC_PORCHES);

            memset(line1_B, levelSync, SAMPLES_COLOUR); // this one is mostly low
            memset(line3_B, levelBlank, SAMPLES_COLOUR);
            memset(line4_B, levelBlank, SAMPLES_COLOUR);
            memset(line6_B, levelBlank, SAMPLES_COLOUR); // this one is easy, nothing else needed

            memset(line1_A + SAMPLES_FRONT_PORCH, levelSync, SAMPLES_HSYNC + SAMPLES_BACK_PORCH);
            memset(line4_A + SAMPLES_FRONT_PORCH, levelSync, SAMPLES_SHORT_PULSE);
            memset( line6odd_A + SAMPLES_FRONT_PORCH, levelSync, SAMPLES_HSYNC);
            memset(line6even_A + SAMPLES_FRONT_PORCH, levelSync, SAMPLES_HSYNC);

            memset(line1_B + SAMPLES_HALFLINE - SAMPLES_HSYNC - SAMPLES_BACK_PORCH - SAMPLES_GAP, levelBlank, SAMPLES_GAP); // 1st gap
            memset(line1_B + SAMPLES_COLOUR - SAMPLES_GAP + SAMPLES_FRONT_PORCH, levelBlank, SAMPLES_GAP - SAMPLES_FRONT_PORCH); // 2nd gap

            memset(line3_B, levelSync, SAMPLES_HALFLINE - SAMPLES_HSYNC - SAMPLES_BACK_PORCH - SAMPLES_GAP);
            memset(line3_B + SAMPLES_HALFLINE - SAMPLES_HSYNC - SAMPLES_BACK_PORCH, levelSync, SAMPLES_SHORT_PULSE);

            memset(line4_B + SAMPLES_HALFLINE - SAMPLES_HSYNC - SAMPLES_BACK_PORCH, levelSync, SAMPLES_SHORT_PULSE); // 2nd half of line 4 matches line 3

        }

        void calculateCarrier() {
            for (uint16_t i = 0; i < SAMPLES_COLOUR; i++) {
                // the + here is a really fine adjustment on the carrier? it tunes colour bars fading at the top.
                // the phase offset here is also odd
                float x = float(i+2.2) / DAC_FREQ * 2.0 * M_PI * COLOUR_CARRIER + (-90.0 / 180.0) * M_PI;
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

            memcpy( line6odd_A + SAMPLES_FRONT_PORCH + SAMPLES_UNTIL_BURST, burstOdd, SAMPLES_BURST);
            memcpy(line6even_A + SAMPLES_FRONT_PORCH + SAMPLES_UNTIL_BURST, burstEven, SAMPLES_BURST);
        }

        void createColourBars() {

            memset( colourbarsOdd_B, levelBlank, SAMPLES_COLOUR);
            memset(colourbarsEven_B, levelBlank, SAMPLES_COLOUR);

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
                // +- cos is flipped...
                colourbarsOdd_B[i+ioff]  = levelBlank + (y * levelWhite + u * SIN2[i] - v * COS2[i]) / 128;
                // even lines of fields 1 & 2 and odd lines of fields 3 & 4?
                colourbarsEven_B[i+ioff] = levelBlank + (y * levelWhite + u * SIN2[i] + v * COS2[i]) / 128;
            }
        }

        void setBuf(uint8_t *in) {
            buf = in;
        }




        void __time_critical_func(dmaHandler)() {

uint16_t scroll = 8;
while (true) {
            switch (currentline) {
                case 1 ... 2:
                    dma_channel_set_read_addr(dma_channel_A, line1_A, true);
dma_channel_wait_for_finish_blocking(dma_channel_A); dma_channel_set_trans_count(dma_channel_A, SAMPLES_COLOUR / 4, false);
//                    dma_channel_set_read_addr(dma_channel_B, line1_B, false);
                    dma_channel_set_read_addr(dma_channel_A, line1_B, true);
                    break;
                case 3:
                    dma_channel_set_read_addr(dma_channel_A, line1_A, true); // lines 1 and 3 start the same way
dma_channel_wait_for_finish_blocking(dma_channel_A); dma_channel_set_trans_count(dma_channel_A, SAMPLES_COLOUR / 4, false);
//                    dma_channel_set_read_addr(dma_channel_B, line3_B, false);
                    dma_channel_set_read_addr(dma_channel_A, line3_B, true);
                    break;
                case 4 ... 5:
                    dma_channel_set_read_addr(dma_channel_A, line4_A, true);
dma_channel_wait_for_finish_blocking(dma_channel_A); dma_channel_set_trans_count(dma_channel_A, SAMPLES_COLOUR / 4, false);
//                    dma_channel_set_read_addr(dma_channel_B, line4_B, false);
                    dma_channel_set_read_addr(dma_channel_A, line4_B, true);
                    break;
                case 6 ... YDATA_START-1:
                    if (currentline & 1) { // odd
                        dma_channel_set_read_addr(dma_channel_A, line6odd_A, true);
dma_channel_wait_for_finish_blocking(dma_channel_A); dma_channel_set_trans_count(dma_channel_A, SAMPLES_COLOUR / 4, false);
//                        dma_channel_set_read_addr(dma_channel_B, line6_B, false);
                    dma_channel_set_read_addr(dma_channel_A, line6_B, true);
                    }
                    else {
                        dma_channel_set_read_addr(dma_channel_A, line6even_A, true);
dma_channel_wait_for_finish_blocking(dma_channel_A); dma_channel_set_trans_count(dma_channel_A, SAMPLES_COLOUR / 4, false);
//                        dma_channel_set_read_addr(dma_channel_B, line6_B, false);
                    dma_channel_set_read_addr(dma_channel_A, line6_B, true);
                    }
                    break;
                case YDATA_START ... YDATA_END: // in the absence of anything else, empty lines
                    if (currentline & 1) { // odd
                        dma_channel_set_read_addr(dma_channel_A, line6odd_A, true);
dma_channel_wait_for_finish_blocking(dma_channel_A); dma_channel_set_trans_count(dma_channel_A, SAMPLES_COLOUR / 4, false);
//                        dma_channel_set_read_addr(dma_channel_B, bufferOdd_B, false);
                    dma_channel_set_read_addr(dma_channel_A, bufferOdd_B, true);
                    }
                    else {
                        dma_channel_set_read_addr(dma_channel_A, line6even_A, true);
dma_channel_wait_for_finish_blocking(dma_channel_A); dma_channel_set_trans_count(dma_channel_A, SAMPLES_COLOUR / 4, false);
//                        dma_channel_set_read_addr(dma_channel_B, bufferEven_B, false);
                    dma_channel_set_read_addr(dma_channel_A, bufferEven_B, true);
                    }
                    break;
                case YDATA_END+1 ...309:
                    if (currentline & 1) { // odd
                        dma_channel_set_read_addr(dma_channel_A, line6odd_A, true);
dma_channel_wait_for_finish_blocking(dma_channel_A); dma_channel_set_trans_count(dma_channel_A, SAMPLES_COLOUR / 4, false);
//                        dma_channel_set_read_addr(dma_channel_B, line6_B, false);
                    dma_channel_set_read_addr(dma_channel_A, line6_B, true);
                    }
                    else {
                        dma_channel_set_read_addr(dma_channel_A, line6even_A, true);
dma_channel_wait_for_finish_blocking(dma_channel_A); dma_channel_set_trans_count(dma_channel_A, SAMPLES_COLOUR / 4, false);
//                        dma_channel_set_read_addr(dma_channel_B, line6_B, false);
                    dma_channel_set_read_addr(dma_channel_A, line6_B, true);
                    }
                    break;
                case 310 ... 312:
                    dma_channel_set_read_addr(dma_channel_A, line4_A, true);
dma_channel_wait_for_finish_blocking(dma_channel_A); dma_channel_set_trans_count(dma_channel_A, SAMPLES_COLOUR / 4, false);
//                    dma_channel_set_read_addr(dma_channel_B, line4_B, false);
                    dma_channel_set_read_addr(dma_channel_A, line4_B, true);
                    break;
                default:
                    break;
            }

    if (currentline >= YDATA_START - 1 && currentline < YDATA_END) {

                if (buf == NULL) {
                    if (currentline & 1) { // odd, next line is even
                        memcpy(bufferEven_B, colourbarsEven_B, SAMPLES_COLOUR);
                    }
                    else {
                        memcpy(bufferOdd_B, colourbarsOdd_B, SAMPLES_COLOUR);
                    }
                }
                else {

                    uint8_t y = 0, u = 0, v = 0;
                    int16_t dmavfactor;
                    uint8_t *dmatargetbuffer;

                    uint8_t tbuf[SAMPLES_COLOUR];
                    if (currentline & 1) {// this is multiplied by v to get even odd lines
                        dmatargetbuffer = bufferEven_B+ioff;
                        dmavfactor = 1;
                    }
                    else {
                        dmatargetbuffer = bufferOdd_B+ioff;
                        dmavfactor = -1;
                    }
                      
                    // note the Y resolution stored is 1/2 the YRESOLUTION, so the offset is / 2
                    uint8_t *idx = buf + ((currentline - YDATA_START) / 2) * XRESOLUTION;
                    idx = idx + scroll; // shuffle over for a more interesting image for now...


//                    for (uint16_t i = 0; i < irange; i += SAMPLES_PER_PIXEL) { // irange
//                    for (uint16_t i = 0; i < (SAMPLES_PER_PIXEL*14); i += SAMPLES_PER_PIXEL) {
                    for (uint16_t i = SAMPLES_PER_PIXEL*scroll; i < (SAMPLES_PER_PIXEL*(14+scroll)); i += SAMPLES_PER_PIXEL) {
                        y = (*idx >> 1) & 0b01111000; // y * 8
                        u = (*idx << 3) & 0b01100000; // u * 32
                        v = (*(idx++) << 5) & 0b01100000; // u * 32
                        // make y, u, v out of 127... these may be negative, so this could be an issue
//                        y = y * 8;
//                        u = u * 32;
//                        v = v * 32; 

                        for (uint16_t dmai2 = i; dmai2 < i + SAMPLES_PER_PIXEL; dmai2++) { // SAMPLES_PER_PIXEL
                            // ioff added to the buffer earlier
                            dmatargetbuffer[dmai2] = levelBlank + (y * levelWhite + u * SIN2[dmai2] + dmavfactor * v * COS2[dmai2]) / 128 ;
                        }
                    }

/*                    if (currentline & 1) {// this is multiplied by v to get even odd lines
                        memcpy(bufferEven_B, tbuf, SAMPLES_COLOUR);
                    }
                    else {
                        memcpy(bufferOdd_B, tbuf, SAMPLES_COLOUR);
                    }*/

                } // buf != null

            } // ydata lines


            // only continue to the beginning of the loop and restart A after B is finished
            dma_channel_wait_for_finish_blocking(dma_channel_A); dma_channel_set_trans_count(dma_channel_A, SAMPLES_SYNC_PORCHES / 4, false);
//            dma_channel_wait_for_finish_blocking(dma_channel_B);


            gpio_put(18, led = !led); // not flashing as it should be?

            currentline++;
            if (currentline == 313) {
                currentline = 1;
//scroll++;
//if (scroll == XRESOLUTION-14)
//scroll =0;
            }

} // while (true)


            // prepare data for next line? raise semaphore for it?
//            dma_hw->ints0 = 1u << dma_channel_A;
// dma_channel_acknowledge_irq0(dma_channel_A)?
        }

        void start() {
//            dma_channel_set_read_addr(dma_channel_A, line1_A, true); // everything is set, start!
//dma_channel_wait_for_finish_blocking(dma_channel_A);
//            dma_channel_set_read_addr(dma_channel_A, line1_B, true);
//            dma_channel_set_read_addr(dma_channel_B, line1_B, false); // this will be started by the chain from A
            currentline++; // onto line 2!
//dma_channel_wait_for_finish_blocking(dma_channel_A);
dmaHandler();
        }

};

ColourPal cp;

void cp_dma_handler() {
    cp.dmaHandler();
}
