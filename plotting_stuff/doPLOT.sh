#!/bin/bash

cat ntp.ck.txt | cut -d' ' -f1 | sed 's/.........$/& &/' | sed 's/....... / /'  >ntp2.txt

tail -6 ntp2.txt

echo plot
/usr/bin/gnuplot <plot.gnu >tmp.png
echo convert
convert tmp.png -mattecolor silver -frame 30x30+14+14  plot.png
echo done
timeout 28 display plot.png &
echo display done
