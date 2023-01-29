%
% The MIT License (MIT)
%
% Copyright (c) 2022-2023 guruthree
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

function converttoyuv(file)
% convert RGB image file to YUV header

clc
pkg load image % for octave

    if ~exist('file', 'var')
        file = 'testcardf.png';
    end
    
    im = imread(file);
    im = im / 2; % divide by two for 0-127 range
%     imshow(im)
    
    out = '';

    for yy=1:size(im,1)
        for xx=1:size(im,2)
            r = double(im(yy,xx,1));
            g = double(im(yy,xx,2));
            b = double(im(yy,xx,3));

            [y, u, v] = rgb2yuv(r, g, b);
            y = int8(y);
            u = int8(u);
            v = int8(v);
         
%             fprintf('%d %d %d to %d %d %d\n', r, g, b, y, u, v);

            out = sprintf('%s 0x%x,', out, y);

            if u >= 0
                out = sprintf('%s 0x%x,', out, u);
            else
                out = sprintf('%s -0x%x,', out, -u);
            end

            if v >= 0
                out = sprintf('%s 0x%x,', out, v);
            else
                out = sprintf('%s -0x%x,', out, -v);
            end
        end

        out = sprintf('%s\n', out);
    end

    file = strrep(file, '.', '');

    fprintf('int8_t __in_flash("%s") %s[] = {\n%s};\n\n', file, file, out);
    
    function [y, u, v] = rgb2yuv(r, g, b)
        y = 5 * r / 16 + 9 * g / 16 + b / 8;
        v = (b - y) / 2;
        u = 13 * (r - y) / 16;
    end

end
