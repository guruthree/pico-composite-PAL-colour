%
% The MIT License (MIT)
%
% Copyright (c) 2022 guruthree
%
% Permission is hereby granted, free of charge, to any person obtaining a copy
% of this software and associated documentation files (the "Software"), to deal
% in the Software without restriction, including without limitation the rights
% to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
% copies of the Software, and to permit persons to whom the Software is
% furnished to do so, subject to the following conditions:
%
% The above copyright notice and this permission notice shall be included in
% all copies or substantial portions of the Software.
%
% THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
% IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
% FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
% AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
% LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
% OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
% THE SOFTWARE.
%

clear

% pack the 3x5 bits of the character down into a uint16_t

pkg load image % for octave

im = imread('font.png');
im = im(:,:,1);

charx = 3+1;
chary = 5+1;


across = (size(im,2)+1) / charx;
down = (size(im,1)+1) / chary;

ascii = 31;
for jj=1:down
    for ii=1:across
        xrange = ((ii-1)*charx+1):(ii*charx-1);
        yrange = ((jj-1)*chary+1):(jj*chary-1);
        c = ~im(yrange,xrange);
        bits = ([(c'(:)') 0]);

        out = 0;
        for bb=1:16
            out = bitor(out, bitshift(bits(bb), bb-1));
        end

        outs = dec2bin(out);
        while length(outs) < 16
            outs = ['0' outs];
        end

        fprintf('0b%s, // %s\n', outs, ascii+((jj-1)*across+ii))

        % the right most bit of bits is the top left corner of the character
        % the left most bit is padding

%         imshow(c)
%         pause(0.1)
    end
end

% A = 0b0101101111101010 = 23530
