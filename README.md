# pico-composite-PAL-colour #

Trying to get the [Raspberry Pi RP2040](https://www.raspberrypi.com/documentation/microcontrollers/rp2040.html) chip's PIO to generate a colour PAL composite video signal, using only a [resistor ladder](https://en.wikipedia.org/wiki/Resistor_ladder) digital-to-analogue converter (DAC).

![Photograph of a CRT screen running the code](resources/onacrt.jpg)

**WARNING: THERE IS A CHANCE THIS SOFTWARE MAY DAMAGE YOUR PICO. I TAKE NO RESPONSIBILITY.**
I'm running the RP2040 at 266 MHz. This is a 200% overclock. The clock divider on the PIO is about 4. That is a lot of data to process (4256 bytes) in not a very long period of time (64 microseconds). I have a sneaking suspicion that the only reason it's working is because running at high overclock so long may have permanently damaged the RP2040 chip. It no longer registers when plugged into some computers. The GPIO seems to have 5 V coming out.

As for my specific hardware, I'm using a [Pimoroni Tiny2040](https://shop.pimoroni.com/products/tiny-2040?variant=39560012300371). The resistor DAC is of the simpler (read: less precise) single resistor variety. It consists of one each of a 39k, 22k, 10k, 5.6k, 2.7k, 1.2k, 680, and 330 Ω resistors hooked up to GPIO pins 0 through 7. That DAC, when combined with the 75 Ω termination load of a composite signal should have a 0-1 V range. When making it I did not know colour PAL should have a peak voltage of 1.25 V, so oops, please design your own resistor DAC to the appropriate voltage. To calculate, you can use 1.25 V = 3.3 V * 75 Ω / (75 Ω + sum of 8 resistor values each roughly doubling). You will ideally want 1 V to be produced when a value of 128 is written to the DAC.

Due to the technical limitations of RAM and timings, the best I was able to get displayed was a 64x125 anamorphic image, stretched back out to 128x125. It's possible to get a little bit more X resolution, up to (78x125) without the use of the anamorphic doubling. I have a lot more appreciation for the engineers of the 80s that did better than this with a chip running 1/200th the speed!

![Passive resistor DAC and Tiny2040](resources/chipanddac.jpg)

## Resources ##

### Composite Video ###

* http://martin.hinner.info/vga/pal.html
* http://www.batsocks.co.uk/readme/video_timing.htm
* https://www.broadcaststore.com/pdf/model/793698/TT148%20-%204053.pdf
* https://elinux.org/images/e/eb/Howtocolor.pdf
* http://martin.hinner.info/vga/pal.html
* https://web.archive.org/web/20150306030906/http://www.pembers.freeserve.co.uk/World-TV-Standards/Colour-Standards.html
* https://web.archive.org/web/20150305012205/http://www.pembers.freeserve.co.uk/World-TV-Standards/Line-Standards.html
* https://web.archive.org/web/20080201032125/http://www.rickard.gunee.com/projects/video/sx/howto.php
* https://sagargv.blogspot.com/2014/07/ntsc-demystified-color-demo-with.html
* http://people.ece.cornell.edu/land/courses/ece5760/video/gvworks/GV%27s%20works%20%20NTSC%20demystified%20-%20Color%20Encoding%20-%20Part%202.htm

### PICO/RP2040/Tiny2040 ###

* https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf
* https://datasheets.raspberrypi.com/pico/raspberry-pi-pico-c-sdk.pdf
* https://raspberrypi.github.io/pico-sdk-doxygen/

#### DMA ####

* https://www.youtube.com/watch?v=OenPIsmKeDI

##### DMA Chaining #####
* https://forums.raspberrypi.com/viewtopic.php?t=330348
* https://forums.raspberrypi.com/viewtopic.php?t=311306
* https://forums.raspberrypi.com/viewtopic.php?t=319709

### Similar projects ###

#### Pico ####

* https://areed.me/posts/2021-07-14_implementing_composite_video_output_using_the_pi_picos_pio/
* https://github.com/obstruse/pico-composite8
* http://www.breakintoprogram.co.uk/projects/pico/composite-video-on-the-raspberry-pi-pico
* https://github.com/breakintoprogram/pico-mposite

#### Other platforms ####

* https://bitluni.net/esp32-color-pal
* https://github.com/bitluni/DawnOfAV/tree/master/DawnOfAV
* https://github.com/rossumur/esp_8_bit
* http://javiervalcarce.eu/html/arduino-tv-signal-generator-en.html
* https://web.archive.org/web/20080201140901/https://www.rickard.gunee.com/projects/video/sx/gamesys.php
* https://web.archive.org/web/20070707125203/http://www.ondraszek.ds.polsl.gliwice.pl/~looser/avr/index.php
* https://www.electronicsforu.com/electronics-projects/tv-pattern-generator
