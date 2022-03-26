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

% algorithmic generation of a flame sprite animation

function flames()
    pkg load statistics
    
    im1 = geneateFlames();
    im2 = geneateFlames();
    im3 = geneateFlames();
    
    im = (im1 + im2 + im3);
    pcolor(im)
    % red channel is solid, green goes from 0 to 1 (red to yellow);
    c = zeros(10,3);
    c(:,1) = 1;
    c(:,2) = (0:9)'/9;
    c(1,:) = [0 0 0];
    c(2,1) = 0.5; % fade darker at the edge of the fire
    c(3,1) = 0.7;

    colormap(c)
    shading flat
    caxis([0 9])
    
    function im = geneateFlames()
    
        im = zeros(20,32);
    
        y = 1;
    
        oldx = 2 + randi([5 7]);
        x = randi([2 3]);
        im = fillim(im, y, oldx, oldx+x, 1);
    
        oldx = oldx + x + 1;
        x = randi([1 2]);
        im = fillim(im, y, oldx, oldx+x, 2);
    
        oldx = oldx + x + 1;
        x = randi([2 4]);
        im = fillim(im, y, oldx, oldx+x, 3);
    
        oldx = oldx + x + 1;
        x = randi([1 2]);
        im = fillim(im, y, oldx, oldx+x, 2);
    
        oldx = oldx + x + 1;
        x = randi([2 3]);
        im = fillim(im, y, oldx, oldx+x, 1);
    
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
    
%                 x = find(im(y-1,:) == zz, 1)
                x = findfirstx(im, y-1, zz);
    
                r = rand();
                if r < probs(1) && im(y,x-1) == 0 && im(y,x) == 0 % expand
                    im(y,x-1) = zz;
                elseif r < probs(2) && im(y,x) == 0 % unchange
                    im(y,x) = zz;
                else%if r < probs(3) % contract
                    r = rand();
                    if r < 1/3 || y < 8
                        p = 1;
                    else
                        p = 2;
                    end
                    if im(y,x+p) == 0
                        im(y,x+p) = zz;
                    else
                        break
                    end
                end
    
%                 x = find(im(y-1,:) == zz, 1, 'last')
                x = findlastx(im, y-1, zz);
                if im(y,x) == zz || im(y,x-1) == zz % shape is closed
                    break
                end
                r = rand();
                if r < probs(1) && im(y,x+1) == 0 && im(y,x) == 0 % expand
                    im(y,x+1) = zz;
                elseif r < probs(2) && im(y,x) == 0 % unchange
                    im(y,x) = zz;
                else%if r < probs(3) % contract
                    r = rand();
                    if r < 1/3 || y < 8
                        p = 1;
                    else
                        p = 2;
                    end
                    if im(y,x+p) == 0
                        im(y,x-p) = zz;
                    else
                        break
                    end
                end
            end
        end
    
        % fill in flame insides
        for y=2:size(im,1)
%             for x=1:find(im(y,:)==1,1,'last')
            xgoal = findlastx(im, y, 1);
            if xgoal > 0
                for x=1:xgoal
                    if im(y,x) > 0 && im(y,x+1) == 0
                        im(y,x+1) = im(y,x);
                    end
                end
            end
        end
    
    end

    function im = setim(im, y, x, val)
        im(y,x) = val;
    end

    function im = fillim(im, y, x1, x2, val)
        for ii=x1:x2
            im = setim(im, y, ii, val);
        end
    end

    function x = findfirstx(im, y, val)
        x = 1;
        while im(y,x) ~= val
            x = x + 1;
            if x == size(im,2)+1
                % outside bounds, not found
                x = -1;
                break
            end
        end
    end

    function x = findlastx(im, y, val)
        x = size(im,2);
        while im(y,x) ~= val
            x = x - 1;
            if x == 0
                % outside bounds, not found
                x = -1;
                break
            end
        end
    end

end
