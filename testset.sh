#!/bin/bash

touch kod

while [ 1 ]
do
    sntp 192.168.4.1 | grep "s1" | ts
    # sntp time.google.com | grep "s1" | ts  # for debugging
    sleep 5
done
