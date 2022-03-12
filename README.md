# pico-composite-PAL-colour #

Trying to get the PIO to generate a colour PAL composite video signal, using only a [resistor ladder](https://en.wikipedia.org/wiki/Resistor_ladder) digital-to-analogue converter (DAC).

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
* https://shop.pimoroni.com/products/tiny-2040?variant=39560012300371

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