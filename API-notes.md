API Notes
===

## Overview

The o_C firmware operates with two primary execution paths - the _loop_ and the _ISR_.

1. The _loop_ is an infinite loop that handles secondary duties.
  - UI events are dispatched here, i.e. buttons, encoders
  - MIDI is processed here
  - Potentially blocking operations like SD card read/write and USB services should happen here
2. The _ISR_ (Interrupt Service Routine) is the "critical path" that executes time-sensitive core logic computations. This call interrupts the _loop_ every 60 microseconds.
  - display and DAC updates happen here, as well as CV input scanning/polling
  - app or applet `Controller()` functions are called from here
  - higher priority than the _loop_

It's "critical" that code in the _ISR_ doesn't take longer than 60 microseconds to execute. This can be challenging on the original Teensy 3.2 MCU, where floating point operations are very expensive. Teensy 4.x has more power, running at 600Mhz instead of 120Mhz.
There is a separate ISR for UI polling, , but UI events are still dispatched and handled by the _loop_.

There are various top-level *Apps*, as well as many *Applets* as originally implemented in the _Hemisphere_ *App*.
An *Applet* inherits the base class _HemisphereApplet_ and gains the superpowers necessary to live on half of your module's screen and use two of the CV outputs.

### App/Applet Registration

Apps are declared for inclusion in the `apps/_config.h` file, or inside `OC_apps.cpp` on older code branches.

Applets are declared for inclusion in the `applets/_config.h` file, or the `hemisphere_config.h` file on older code branches.

## Base Classes

The two primary interfaces in the Hemisphere API are _HSApplication_ and _HemisphereApplet_.
They both have many similarly named methods for I/O and graphics, with _HemisphereApplet_ taking extra considerations for offsetting both in the right side.
All of the hardware details are neatly abstracted with member functions in these base classes.

A lot of miscellaneous functions (graphics helpers, math stuff) are tucked into _HSUtils_ and I/O is shuttled via the _HSIOFrame_ structure.

### Applets

There are a few different things an Applet must do:
* **Controller** - the main logic computed every tick (every time the ISR fires)
* **View** - draw the pixels on the screen
* UI Event Handling:
  * **OnButtonPress** - what to do when the encoder button is pressed
  * **OnEncoderMove** - what to do when the encoder is rotated
* **OnDataRequest** / **OnDataReceive** - how to save / load state, packed into 64 bits
  - **SetData**/**GetData** functions added for extra applet storage on T4.x

There is also a `Start()` function for initializing things at runtime, plus some Help text. That's about it.

You can get started from scratch by making a copy of the "Boilerplate.h" file, and adding your computations to its skeleton.
Or you could make a copy of an existing applet as a template, and transform it into something else.

### Applet Functions

Member Functions? Methods? Either way, this is how you do the things.

#### I/O Functions
The main argument of each is the channel to operate on. Each Applet gets 2 channels, so _ch_ is typically either 0 or 1.

Here are some of the essentials:

**Input**:
* `bool Clock(int ch)` - has the digital input received a new clock pulse?
* `bool Gate(int ch)` - is the digital input held high?
* `int In(int ch)` - Raw value of the CV input
* `int DetentedIn(int ch)` - this one reads 0 until it's past a threshold ~ a quartertone

**Output**:
* `void ClockOut(int ch)` - set and hold the output high for a pulse length
* `void GateOut(int ch, bool on_off)` - set the output high or low
* `void Out(int ch, int raw)` - set the output to an explicit value...
  - 128 == a semitone; `ONE_OCTAVE` macro for 12 semitones
  - Assuming 1V/Oct scaling, 12 semitones is 1 Volt: (128 * 12) == (12 << 7) == 1536

I've added a generic case function for modulating a parameter with a certain input.
* `void Modulate(auto &param, int ch, int min, int max)` - automatically scales the input and modifies param

#### gfx Functions
There are many strategies for drawing things on the screen, and therefore, many
graphics related functions. You can see them for yourself in `HemisphereApplet.h` or `HSUtils.*`
All of them typically take _x_ and _y_ coordinates for the first two arguments,
followed by _width_ and _height_, or another _x,y_ pair.
_x_ is how many pixels from the left edge of the screen.
_y_ is how many pixels from the top edge of the screen.

Some essentials: *gfxPrint*, *gfxPixel*, *gfxLine*, *gfxCursor*,
                *gfxFrame*, *gfxBitmap*, *gfxInvert*

