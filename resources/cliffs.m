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

% generate a landscape between two cliff faces to fly through

clear

basecliff =  [80 80 80 5 1 0 1 -2 1 0 1 5 80 80 80];
% xspacing = (1:length(basecliff)) * 20;

% plot(xspacing, basecliff, '.-')
% axis image

thecliff = zeros(100, length(basecliff));
thecliff(1,:) = basecliff;

for ii=2:size(thecliff,1)
    
    for jj=1:size(thecliff,2)
        
        d = basecliff(jj) - thecliff(ii-1,jj);
        thecliff(ii,jj) = thecliff(ii-1,jj)+randi([-5 5])+d/8;
        
    end
    
    if mod(ii,2) == 0
        for jj=2:size(thecliff,2)-1
            thecliff(ii,jj) = thecliff(ii,jj-1) / 128 + thecliff(ii,jj) * 126 / 128 + thecliff(ii,jj+1) / 128;
        end
    end
    
    thecliff(ii,4:end-3) = thecliff(ii,4:end-3) + sin(ii/10)*2;

    if ii > 10
        surf(thecliff(ii-10:ii,:))
        shading flat
        view([0 20])
        pause(1/10)
    end
    
end

% surf(thecliff)
% shading flat
% view([-75 72])

% plot(mean(thecliff))

% original: 80, next was 75
% want to bias random number up more
% 80 - 75 = 5
% add a portion of that difference to the random number to bias it

