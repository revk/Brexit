// 3D case for counttdown clock
// Copyright (c) 2019 Adrian Kennard, Andrews & Arnold Limited, see LICENSE file (GPL)

use <PCBCase/case.scad>

width=45;
height=37;

// Box thickness reference to component cube
base=1;
top=10.4;

$fn=48;

module pcb(s=0)
{
    translate([-1,-1,0])
    { // 1mm ref edge of PCB vs SVG design
        esp32(s,14.501,18.315);
        oled(s,1,1,180,screw=.5,smd=true);
        d24v5f3(s,19.415,1.000,180,smd=true);
        spox(s,6.080,1.000,0,4,hidden=true,smd=true);
        usbc(s,30.845,0.510);
        smd1206(s,31.515,9.010);
        smd1206(s,35.915,9.010);
        switch66(s,34.020,29.750,90,smd=true);
    }
}

case(width,height,base,top,cutoffset=6){pcb(0);pcb(-1);pcb(1);};

