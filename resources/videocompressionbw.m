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
% this one is specialised for black and white video
% the format is number of pixels white, number of pixels black

folder = 'bad apple'; % source of frames, expects %05d.png for filenaming

name = 'badapple'; % name of video

% output folder
fid = fopen([name '.h'], 'w');
% fid = 1; % stdout

% output resolution
xsize = 220;
ysize = 110;

% what frames to read in and process
% framerange = 39:84;
% framerange = 1:1054;
% framerange = 1:5478;

% show 5 seconds of video at a time skipping a few sections
startframes = ([0:6:42]*5)*25+1;
endframes = startframes + 124;
framerange = [];
for ii=1:length(startframes)
    framerange = [ framerange startframes(ii):endframes(ii) ]; %#ok<AGROW>
end
framerange = framerange + 34; % skip the blackframes at the start

% how long each frame is
framelengths = [];

% loop through each frame
for zz=framerange

    %% compress the frame
    
    % read and resize... may want to do some cropping....
    im = imread(sprintf('%s/%05d.png', folder, zz));
    im = imresize(im, [ysize xsize], 'box');
    
    % make black and white
    im = rgb2gray(im);
    Y = imbinarize(im');

    % turn into one big vector
    Y = Y(:);

    % the count of each colour in a row
    % the first index is black, the second index is white
    if Y(1) == false
        compressed = [1];
    else
        compressed = [0; 1];
    end

    for ii=2:size(Y,1)

        if Y(ii) == Y(ii-1) && compressed(end,1) < 255
            compressed(end,1) = compressed(end,1) + 1;
        elseif Y(ii) == Y(ii-1) && compressed(end,1) == 255
            compressed(end+1,1) = 0; %#ok<SAGROW>
            compressed(end+1,1) = 1; %#ok<SAGROW>
        else
            compressed(end+1,1) = 1; %#ok<SAGROW>
        end

    end

    framelengths(end+1) = size(compressed,1); %#ok<SAGROW>
    fprintf('frame %03d is %d long\n', zz , size(compressed,1))

    %% reconstruct the frame

%     newim = zeros(0,1,'logical');
%     currentcolour = false;
%     for ii=1:size(compressed,1)
%         for jj=1:compressed(ii,1)
%             newim(end+1,1) = currentcolour; %#ok<SAGROW>
%         end
%         currentcolour = ~currentcolour;
%     end
%     im2 = reshape(newim', xsize, ysize)';
%     im2 = im2*255;
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
%     drawnow

    %% write out the frame

    frame = sprintf('%sframe%03d', name, zz);

    fprintf(fid, 'uint8_t __in_flash("%s") %s[] = {\n\n', frame, frame);

    for ii=1:size(compressed, 1)
        y = uint8(compressed(ii,1));

        out = sprintf('    0x%02x, \n', y);

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

fprintf(fid, 'uint8_t* __in_flash("%sframes") %sframes[NUM_%sFRAMES] = {\n', name, name, upper(name));
for ii=framerange
    fprintf(fid, '    %sframe%03d,\n', name, ii);
end
fprintf(fid, '};\n\n');

if fid ~= 1
    fclose(fid);
end

fprintf('done\n\n');
