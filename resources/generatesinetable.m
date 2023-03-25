%
clear

s = 360.*(0:127)./128;
v = sind(s) * 127;

% return

fprintf('int8_t sine_table[128] = { 0x%02x', v(1));

for ii=2:length(v)
    tv = round(v(ii));

    if tv >= 0
        fprintf(', 0x%02x', tv);
    else
        fprintf(', -0x%02x', -tv);
    end

end

fprintf(' }; \n\n');
