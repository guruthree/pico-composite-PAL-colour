/*
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 guruthree
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

% algorithmic generation of a flame sprite animation

clear
pkg load statistics

im = nan(20,32);
imold = nan(20,32);
imold2 = nan(20,32);

for frame=1:5%:1000

    if frame > 3
        imold2 = imold;
        imold = im;
    end
    im = nan(20,32);

    y = 1;

    oldx = 1;
    x = randi([5 7]);
    im(y, oldx:(oldx+x)) = nan;

    oldx = oldx + x + 1;
    x = randi([2 3]);
    im(y, oldx:(oldx+x)) = 1;

    oldx = oldx + x + 1;
    x = randi([1 2]);
    im(y, oldx:(oldx+x)) = 2;

    oldx = oldx + x + 1;
    x = randi([2 4]);
    im(y, oldx:(oldx+x)) = 3;

    oldx = oldx + x + 1;
    x = randi([1 2]);
    im(y, oldx:(oldx+x)) = 2;

    oldx = oldx + x + 1;
    x = randi([2 3]);
    im(y, oldx:(oldx+x)) = 1;

    %pcolor(im)
    %return

    % flame outlines
    for zz=1:3
        for y=2:size(im,1)
            if zz == 1
                if y <= 5
                    probs = [0.3 0.7 1];
                elseif y < 12
                    probs = [0.1 0.3 1];
                else
                    probs = [0 0.02 1];
                end
            elseif zz == 2
                if y <= 5
                    probs = [0.5 0.6 1];
                elseif y < 10
                    probs = [0 0.3 1];
                else
                    probs = [0 0.02 1];
                end
            elseif zz == 3
                if y <= 5
                    probs = [0.3 0.4 1];
                elseif y < 10
                    probs = [0 0.2 1];
                else
                    probs = [0 0.02 1];
                end
            end

            x = find(im(y-1,:) == zz, 1);

            r = rand();
            if r < probs(1) && isnan(im(y,x-1)) && isnan(im(y,x)) % expand
                im(y,x-1) = zz;
            elseif r < probs(2) && isnan(im(y,x)) % unchange
                im(y,x) = zz;
            else%if r < probs(3) % contract
                r = rand();
                if r < 1/3 || y < 8
                    p = 1;
                else
                    p = 2;
                end
                if isnan(im(y,x+p))
                    im(y,x+p) = zz;
                else
                    break
                end
            end

            x = find(im(y-1,:) == zz, 1, 'last');
            if im(y,x) == zz || im(y,x-1) == zz % shape is closed
                break
            end
            r = rand();
            if r < probs(1) && isnan(im(y,x+1)) && isnan(im(y,x)) % expand
                im(y,x+1) = zz;
            elseif r < probs(2) && isnan(im(y,x)) % unchange
                im(y,x) = zz;
            else%if r < probs(3) % contract
                r = rand();
                if r < 1/3 || y < 8
                    p = 1;
                else
                    p = 2;
                end
                if isnan(im(y,x+p))
                    im(y,x-p) = zz;
                else
                    break
                end
            end
        end
    end

    % fill in flame insides
    for y=2:size(im,1)
        for x=1:find(im(y,:)==1,1,'last')
            if ~isnan(im(y,x)) && isnan(im(y,x+1))
                im(y,x+1) = im(y,x);
            end
        end
    end

    if frame > 3
        imnew = (im + imold + imold2);
%         imnew(imnew > 1) = imnew(imnew > 1) - 0.25;

        pcolor(imnew)
        colormap autumn % red channel is sold, green goes from 0 to 1 (red to yellow)
        caxis([0 9])
%         pause(1/50)
%        drawnow
%        print('-dpng', sprintf('out/%06d.png', frame-3))
    end

end
