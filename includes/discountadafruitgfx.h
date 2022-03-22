/*  graphics functions ported from AdafruitGFX to the YUV format used in this project
    this file is therefore released under the BSD License

    Copyright (c) 2013 Adafruit Industries.  All rights reserved.
    Copyright (c) 2022 guruthree

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    - Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    - Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
*/

template<typename T>
inline void swapInt(T &a, T &b) {
    T t;
    t = a;
    a = b;
    b = t;
}

// void setPixelRGB(int8_t *buf, uint8_t xcoord, uint8_t ycoord, uint8_t r, uint8_t g, uint8_t b);


// reference: Adafruit_GFX::writeLine()
void drawLineRGB(int8_t *buf, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t r, uint8_t g, uint8_t b) {
    int8_t steep = abs(y1 - y0) > abs(x1- x0);
    if (steep) {
        swapInt(x0, y0);
        swapInt(x1, y1);
    }

    if (x0 > x1) {
        swapInt(x0, x1);
        swapInt(y0, y1);
    }

    int8_t dx = x1 - x0;
    int8_t dy = abs(y1 - y0);

    int8_t err = dx / 2;
    int8_t ystep;


    if (y0 < y1) {
        ystep = 1;
    }
    else {
        ystep = -1;
    }

    for (; x0 <= x1; x0++) {
        if (steep) {
            setPixelRGB(buf, y0, x0, r, g, b);
        }
        else {
            setPixelRGB(buf, x0, y0, r, g, b);
        }
        err -= dy;
        if (err < 0) {
            y0 += ystep;
            err += dx;
        }
    }
}


// reference: Adafruit_GFX::fillTriangle
void fillTriangle(int8_t *buf, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t r, uint8_t g, uint8_t _b) {



  int8_t a, b, y, last;

  // Sort coordinates by Y order (y2 >= y1 >= y0)
  if (y0 > y1) {
    swapInt(y0, y1);
    swapInt(x0, x1);
  }
  if (y1 > y2) {
    swapInt(y2, y1);
    swapInt(x2, x1);
  }
  if (y0 > y1) {
    swapInt(y0, y1);
    swapInt(x0, x1);
  }

  if (y0 == y2) { // Handle awkward all-on-same-line case as its own thing
    a = b = x0;
    if (x1 < a)
      a = x1;
    else if (x1 > b)
      b = x1;
    if (x2 < a)
      a = x2;
    else if (x2 > b)
      b = x2;
//    writeFastHLine(a, y0, b - a + 1, color);
	drawLineRGB(buf, a, y0, b, y2, r, g, _b);
    return;
  }

  int8_t dx01 = x1 - x0, dy01 = y1 - y0, dx02 = x2 - x0, dy02 = y2 - y0,
          dx12 = x2 - x1, dy12 = y2 - y1;
  int32_t sa = 0, sb = 0;

  // For upper part of triangle, find scanline crossings for segments
  // 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y1
  // is included here (and second loop will be skipped, avoiding a /0
  // error there), otherwise scanline y1 is skipped here and handled
  // in the second loop...which also avoids a /0 error here if y0=y1
  // (flat-topped triangle).
  if (y1 == y2)
    last = y1; // Include y1 scanline
  else
    last = y1 - 1; // Skip it

  for (y = y0; y <= last; y++) {
    a = x0 + sa / dy01;
    b = x0 + sb / dy02;
    sa += dx01;
    sb += dx02;
    /* longhand:
    a = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
    b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    */
    if (a > b)
      swapInt(a, b);
//    writeFastHLine(a, y, b - a + 1, color);
	drawLineRGB(buf, a, y, b, y, r, g, _b);
  }

  // For lower part of triangle, find scanline crossings for segments
  // 0-2 and 1-2.  This loop is skipped if y1=y2.
  sa = (int32_t)dx12 * (y - y1);
  sb = (int32_t)dx02 * (y - y0);
  for (; y <= y2; y++) {
    a = x1 + sa / dy12;
    b = x0 + sb / dy02;
    sa += dx12;
    sb += dx02;
    /* longhand:
    a = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
    b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    */
    if (a > b)
      swapInt(a, b);
//    writeFastHLine(a, y, b - a + 1, color);
	drawLineRGB(buf, a, y, b, y, r, g, _b);
  }


}
