// 3D case for simple ESP32 device
// Copyright (c) 2019 Adrian Kennard, Andrews & Arnold Limited, see LICENSE file (GPL)

// PCBCase is normally a submodule so needs the directoy
use <PCBCase/case.scad>
use <PCBCase/parts.scad>

width=45;
height=37;

// Box thickness reference to component cube
base=10;
top=5;

$fn=48;

module pcb(s=0)
{
    translate([-1,-1,0])
    { // 1mm ref edge of PCB vs SVG design
        esp32(s,26.170,10.448,-90);
        oled(s,1,1);
        d24v5f3(s,12.430,14.659,180);
        spox(s,20.000,30.100,180,4,hidden=true);
        usbc(s,9.715,31.115,180);
    }
}

case(width,height,base,top){pcb(0);pcb(-1);pcb(1);};
