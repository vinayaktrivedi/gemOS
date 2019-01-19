#!/bin/bash

make /home/vinayak/Desktop/5th\ sem/Operating\ System/gemOS
rm /home/vinayak/gem5/gemos/binaries/gemOS.kernel 
cp /home/vinayak/Desktop/5th\ sem/Operating\ System/gemOS/gemOS.kernel /home/vinayak/gem5/gemos/binaries/
/home/vinayak/gem5/build/X86/gem5.opt configs/example/fs.py

