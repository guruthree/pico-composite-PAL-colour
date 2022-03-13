function converttoyuv(file)
% convert RGB image file to YUV header

clc
% pkg load image % for octave

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

    fprintf('int8_t __not_in_flash("%s") %s[] = {\n%s};\n\n', file, file, out);
    
    function [y, u, v] = rgb2yuv(r, g, b)
        y = 5 * r / 16 + 9 * g / 16 + b / 8;
        u = (r - y);
        v = 13 * (b - y) / 16;
    end

end
