# 20:43:01.038742 01.038742

set terminal png size 900,400

set title "esp8266 ntp time accuracy " . ` date +\"%T\"` 

set xlabel "time"
set ylabel "ntp time"
set xdata time
set timefmt "%H:%M:%S"
set format x " %H:%M"
set grid
set xtics rotate by -45
plot "./ntp2.txt" using 1:2 lw 3 lt rgb "#00bb00"
