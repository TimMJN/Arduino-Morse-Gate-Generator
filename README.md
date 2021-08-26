# Arduino Morse Gate Generator
This project contains schematics and firmware for a Morse code gate generator compatible with Eurorack and Kosmo modular synthesizers, based on an Arduino Nano. This is a fun little module anyone can build, only very little experience and basic tools are required. Turn your favourite book into your favourite jam, get funky!

Features:
- SD card module
- internal and external clocks
- `interrupt` based timing
- built-in clock divider
- buffered 9v gate output

#### Submodules
This repository uses submodules to include libraries in the firmware. Please make sure, when cloning this repository, to also include the submodules in the [`src`](https://github.com/TimMJN/Arduino-Morse-Gate-Generator/tree/main/arduino_morse_gate_generator_firmware/src) directory.

# Frequently Asked Questions
#### Do you have PCBs / panels available?
This is a project with a very low part count, most of the work happens inside the Arduino. Therefore, I will not be releasing a PCB layout; the module can easily be build on any protoboard/stripboard. Get creative!

#### How do I load my text files?
Simply load any .txt file(s) onto an SD card and pop it into the slot. The Arduino will read the first file on the card and start spitting out Morse code. If the end of the file is reached, it will be read the next file or go back to the first file if there are none. Directories on the SD card are not supported at the moment.

If the Arduino finds no SD card, or no suitable files, it will repeatidly spit out "SOS". This sounds like: "...---...". 

#### What does the clock switch do?
This switches between to internal and the external clock. When in internal clock mode, the tempo is determined by the rate knob. When in external clock mode, the tempo is controlled by the clock input. In this case, the rate knob selects a clock division from 1 to 8.

#### What are `interrupts`?
These are events which will cause the Arduino to stop doing whatever it was doing and handle the `interrupt` first. This ensures there is no delay between the triggering of the clock and the update of the gate. This module uses both pin `interrupts` (for the external clock) and timer `interrupts` (for the internal clock). This ensures both clocks are tracked accurately.

#### Why would I want Morse code in my synth?
Why not? By patching straight into a VCA you can indeed make it sound like Morse code. But you can also think of it as a random rythm generator. Use it to trigger envelopes or samples, get creative!
