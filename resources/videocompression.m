%
% The MIT License (MIT)
%
% Copyright (c) 2023 guruthree
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
clc

% read in video and convert it to a header file of runlength encoded frames

folder = 'frames'; % source of frames, expects %05d.png for filenaming

name = 'badger'; % name of video

% output folder
fid = fopen([name '.h'], 'w');
% fid = 1; % stdout

% output resolution
xsize = 220;
ysize = 110;

% what frames to read in and process
framerange = 43:127; % 3.4 seconds

framelengths = [];

% loop through each frame
for zz=framerange

    %% compress the frame
    
    % read and resize... may want to do some cropping....
    im = imread(sprintf('%s/%05d.png', folder, zz));
    im = imresize(im, [ysize xsize], 'box');

    % separate the colour channels and scale them to 0-127 for pico composite
    R = im(:,:,1)';
    G = im(:,:,2)';
    B = im(:,:,3)';

    % convert to one big column since that's the format for reading out
    % into the display buffer
    R = R(:);
    G = G(:);
    B = B(:);

    R = R(:) / 2;
    G = G(:) / 2;
    B = B(:) / 2;

    % if the next pixel is +- 1 pixel of the previous one, just use the
    % previous, kind of blocking to increase the effectiveness of runlength
    % encoding
    compressrange = 10;

    % we'll also consider matching intensities (i.e., luminance, Y)
    YUV = uint8(rgb2ycbcr([R G B])*255);

    for ii=2:length(R)

        % reuse if in range and not the first pixel of the lien
        if R(ii)-compressrange <= R(ii-1) && R(ii)+compressrange >= R(ii-1) && ...
                G(ii)-compressrange <= G(ii-1) && G(ii)+compressrange >= G(ii-1) && ...
                B(ii)-compressrange <= B(ii-1) && B(ii)+compressrange >= B(ii-1) && ...
                    mod(ii-1, xsize) ~= 0

            % trigger a refresh if there's been a signicant change in Y
            if ii > 5 && abs(YUV(ii,1)-YUV(ii-5,1)) > 10 && YUV(ii-5,1) < 60 && YUV(ii,1) > 40
                continue
            end

            R(ii) = R(ii-1);
            G(ii) = G(ii-1);
            B(ii) = B(ii-1);
        end
    end

    RGB = [R G B];

    % size(unique(RGB, 'rows')) % number of colours

    % last column is the count
    compressed = double([RGB(1,:) 1]);

    for ii=2:size(RGB,1)

        if RGB(ii,:) == compressed(end,1:3) & compressed(end,4) < 127 %#ok<AND2>
            compressed(end,4) = compressed(end,4) + 1;
        else
            compressed(end+1,:) = [RGB(ii,:) 1]; %#ok<SAGROW>
        end

    end

    framelengths(end+1) = size(compressed,1); %#ok<SAGROW>
    fprintf('frame %03d is %d long\n', zz , size(compressed,1))

    %% reconstruct the frame

%     newRGB = zeros(0,3);
%     for ii=1:size(compressed,1)
%         for jj=1:compressed(ii,4)
%             newRGB(end+1,:) = compressed(ii,1:3);
%         end
%     end
% 
%     im2 = zeros(110, 220, 3, 'uint8');
%     for ii=1:3
%         im2(:,:,ii) = reshape(2*newRGB(:,ii)', xsize, ysize)';
%     %     im2(:,:,ii) = reshape(16*newRGB(:,ii)', xsize, ysize)';
%     end
% 
% 
%     subplot(1,2,1)
%     imshow(im)
%     set(gca, 'DataAspectRatio', [4 3 1])
% 
%     subplot(1,2,2)
%     imshow(im2)
%     set(gca, 'DataAspectRatio', [4 3 1])
% 
%     pause(1/25)

    %% write out the frame

    frame = sprintf('%sframe%03d', name, zz);

    fprintf(fid, 'int8_t __in_flash("%s") %s[] = {\n\n', frame, frame);

    for ii=1:size(compressed, 1)
        r = compressed(ii,1);
        g = compressed(ii,2);
        b = compressed(ii,3);
        [y, u, v] = rgb2yuv(r, g, b);
        y = int8(y);
        u = int8(u);
        v = int8(v);

        out = sprintf('    0x%02x,', y);

        if u >= 0
            out = sprintf('%s  0x%02x,', out, u);
        else
            out = sprintf('%s -0x%02x,', out, -u);
        end

        if v >= 0
            out = sprintf('%s  0x%02x,', out, v);
        else
            out = sprintf('%s -0x%02x,', out, -v);
        end

        out = sprintf('%s 0x%02x, \n', out, uint8(compressed(ii,4)));

        fprintf(fid, out);

    end

    fprintf(fid, '};\n\n');

end % zz

%% write out metadata

fprintf(fid, '#define NUM_%sFRAMES %d\n\n', upper(name), length(framelengths));

fprintf(fid, 'uint32_t __in_flash("%sframelengths") %sframelengths[NUM_%sFRAMES] = {\n', name, name, upper(name));
for ii=1:length(framelengths)
    fprintf(fid, '    %d,\n', framelengths(ii));
end
fprintf(fid, '};\n\n');

fprintf(fid, 'int8_t* __in_flash("%sframes") %sframes[NUM_%sFRAMES] = {\n', name, name, upper(name));
for ii=framerange
    fprintf(fid, '    %sframe%03d,\n', name, ii);
end
fprintf(fid, '};\n\n');

if fid ~= 1
    fclose(fid);
end

fprintf('done\n\n');

function [y, u, v] = rgb2yuv(r, g, b)
    y = 5 * r / 16 + 9 * g / 16 + b / 8;
    v = (b - y) / 2;
    u = 13 * (r - y) / 16;
end