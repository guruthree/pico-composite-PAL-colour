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
    
    out_y = '';
    out_u = '';
    out_v = '';

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

            out_y = sprintf('%s 0x%x,', out_y, y);

            if u >= 0
                out_u = sprintf('%s 0x%x,', out_u, u);
            else
                out_u = sprintf('%s -0x%x,', out_u, -u);
            end

            if v >= 0
                out_v = sprintf('%s 0x%x,', out_v, v);
            else
                out_v = sprintf('%s -0x%x,', out_v, -v);
            end
        end

        out_y = sprintf('%s\n', out_y);
        out_u = sprintf('%s\n', out_u);
        out_v = sprintf('%s\n', out_v);
    end

    file = strrep(file, '.', '');

    fprintf('int8_t __not_in_flash("%s_y") %s_y[] = {\n%s};\n\n', file, file, out_y);
    fprintf('int8_t __not_in_flash("%s_u") %s_u[] = {\n%s};\n\n', file, file, out_u);
    fprintf('int8_t __not_in_flash("%s_v") %s_v[] = {\n%s};\n\n', file, file, out_v);
    
    function [y, u, v] = rgb2yuv(r, g, b)
        y = 5 * r / 16 + 9 * g / 16 + b / 8;
        u = (r - y);
        v = 13 * (b - y) / 16;
    end

end
