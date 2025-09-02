---
layout: default
---
# Viznutcracker, sweet!

This is an experimental implementation of several byte beats signal generators. "Byte beats" are equations, expressed usually as a single line of programme code, typically involving various bit-level operators, which when evaluated with an incrementing phase value at audio rates produce all manner of harsh digital noises, some of which sound musical, or at least, interesting; "Byte beats" were first [described](http://countercomplex.blogspot.com/2011/10/algorithmic-symphonies-from-one-line-of.html) in 2011 by viznut (aka Ville-Matias Heikkilä).

The _Viznutcracker, sweet!_ app allows to run a generator on each of the 4 channels, allowing to choose from 8 different byte beat equations; the equation, speed/frequency and three equation parameter values (p0, p1 and p2) can be set via the menus and/or voltage-controlled for each generator via mappable CV inputs.

The output, if used as an audio signal, usually needs to be fairly heavily filtered through a low-pass filter to remove at least some of the unpleasant digital "screech" due to high-frequency aliasing and other effects which are characteristics of byte beats. This digital aliasing is a fundamental characteristic of the way byte beats work, and isn't due to any hardware limitations of the O+C module.

Perhaps uniquely amongst byte beat generator modules, the _Viznutcracker, sweet!_ apps permits the byte beat generators to be run at very slow rates, and because the O+C module outputs are DC-coupled, they can therefore be used as sources of stepped control voltages. For example, the outputs can be fed into a quantiser (such as another O+C module) to create potentially interesting pitch sequences (possibly even melodies...). Furthermore, the app allows each byte beat equation to be incremented by an external clock/trigger input, so that these stepped voltages can be generated in synchrony with other external processes.

### Available settings (per-channel)

|Setting | Meaning |
| --- | --- |
|`Equation` | Byte beat equation (see table below)|
|`Speed` | 0 to 255. 255 equates to a 16.6 kHz sample rate (i.e. the phase accumulator advances 16,666 times per second). There is a rough quadratic scaling of lower rates, meaning that 0 is quite slow.|
|`Parameter 0`| The first adjustable parameter in the chosen equation. Range 0 to 255, but some parameter settings in some equations do not produce any output, or do not produce output for all values of the phase accumulator (be patient!)|
|`Parameter 1`| ditto for the second adjustable parameter in the equation|
|`Parameter 2`| ditto for the third adjustable parameter in the equation|
|`Loop mode`| Enables loop mode, in which the phase accumulator is constrained to loop between specific start and end values, instead of between 0 and 4,294,967,296.|
|`Loop begin ++`| Coarse loop begin point, range 0-255|
|`Loop begin +`| Medium loop begin point, range 0-255|
|`Loop begin`| Fine loop begin point, range 0 - 255|
|`Loop end ++`| Coarse loop end point, range 0-255  |
|`Loop end +`| Medium loop end point, range 0-255 |
|`Loop end`| Fine loop end point, range 0 - 255 |
|`Trigger input`| Specified which of the 4 trigger inputs (TR1 to TR4) is used for the trigger input for `Step mode`, or when `Step Mode` is off, which trigger input is used to reset the phase accumulator for that channel.|
|`Step mode`| When set to on, the phase accumulator is incremented when a trigger or clock pulse is received on the digital (trigger) input specified by the `Trigger input` setting. When set to off, a trigger (or rising edge of a pulse or clock) received on the trigger input specified by the `Trigger Input` setting will reset the phase accumulator (the _t_ variable in byte beat equations), which has the effect of resetting the byte beats "tune" or "melody" back to its beginning, or back to the start of the loop start point if loop mode is enabled.|
|`CV1 ->`| specifies which parameter CV1 is mapped to for this channel. Choices are `off`, `eqn`, `spd`, `p0`, `p1`, `p2`, `beg++`, `beg`, `end++`, `end`. |
|`CV2 ->`| ditto for CV2|
|`CV3 ->`| ditto for CV3|
|`CV4 ->`| ditto for CV4|

| Equation name | Source of equation |
|---------------|--------------------|
|  `hope`       | "atmospheric, hopeful" from http://royal-paw.com/2012/01/bytebeats-in-c-and-python-generative-symphonies-from-extremely-small-programs/ |
|  `love`       | the equation by stephth via https://www.youtube.com/watch?v=tCRPUv8V22o at 3:38 |
|  `life`       | the second equation listed at from http://xifeng.weebly.com/bytebeats.html |
|  `age`        | "Arp rotator" via [Microbe Modular](http://microbemodular.com/products/equation-composer/overview) Equation Composer [Ptah bank](https://github.com/clone45/EquationComposer/blob/master/EquationBankPtah.cpp) |
|  `clysm`      | "BitWiz Transplant" via [Microbe Modular](http://microbemodular.com/products/equation-composer/overview) Equation Composer [Ptah bank](https://github.com/clone45/EquationComposer/blob/master/EquationBankPtah.cpp) |
|  `monk`       | "Vocaliser" via [Microbe Modular](http://microbemodular.com/products/equation-composer/overview) Equation Composer [Khepri bank](https://github.com/clone45/EquationComposer/blob/master/EquationBankKhepri.cpp)|
|  `NERV`      | "Chewie" via [Microbe Modular](http://microbemodular.com/products/equation-composer/overview) Equation Composer [Khepri bank](https://github.com/clone45/EquationComposer/blob/master/EquationBankKhepri.cpp)  |
|  `Trurl`      | "Tinbot" via [Microbe Modular](http://microbemodular.com/products/equation-composer/overview) Equation Composer [Sobek bank](https://github.com/clone45/EquationComposer/blob/master/EquationBankSobek.cpp) |
|  `Pirx`      | "My Loud Friend" via [Microbe Modular](http://microbemodular.com/products/equation-composer/overview) Equation Composer [Ptah bank](https://github.com/clone45/EquationComposer/blob/master/EquationBankPtah.cpp) |
|  `Snaut`      | ""A bit high-frequency, but keeper anyhow" via [Microbe Modular](http://microbemodular.com/products/equation-composer/overview) Equation Composer [Khepri bank](https://github.com/clone45/EquationComposer/blob/master/EquationBankKhepri.cpp) |
|  `Hari`      | "The Signs" via [Microbe Modular](http://microbemodular.com/products/equation-composer/overview) Equation Composer [Ptah bank](https://github.com/clone45/EquationComposer/blob/master/EquationBankPtah.cpp) |
|  `Kris`      | "Light Reactor" via [Microbe Modular](http://microbemodular.com/products/equation-composer/overview) Equation Composer [Ptah bank](https://github.com/clone45/EquationComposer/blob/master/EquationBankPtah.cpp) |
|  `Tichy`      | "Alpha" via [Microbe Modular](http://microbemodular.com/products/equation-composer/overview) Equation Composer [Khepri bank](https://github.com/clone45/EquationComposer/blob/master/EquationBankKhepri.cpp) |
|  `Bregg`      | "Hooks" via [Microbe Modular](http://microbemodular.com/products/equation-composer/overview) Equation Composer [Khepri bank](https://github.com/clone45/EquationComposer/blob/master/EquationBankKhepri.cpp) |
|  `Avon`      | "Widerange" via [Microbe Modular](http://microbemodular.com/products/equation-composer/overview) Equation Composer [Khepri bank](https://github.com/clone45/EquationComposer/blob/master/EquationBankKhepri.cpp) |
|  `Orac`      | "Abducted" via [Microbe Modular](http://microbemodular.com/products/equation-composer/overview) Equation Composer [Ptah bank](https://github.com/clone45/EquationComposer/blob/master/EquationBankPtah.cpp) |

### Screensaver display

[My God, it's full of stars!](https://www.youtube.com/watch?v=yEFw419Nbg8)


### Examples
  - [Viznutcracker, sweet!](https://soundcloud.com/bennelong-bicyclist/viznutcracker-sweet) - a track created using the _Viznutcracker, sweet!_ app as the only sound source, except for a bass drum from a Mutable Instruments Peaks module.

*This documentation has been reformatted and republished from the [original Ornament and Crime manual](https://ornament-and-cri.me/user-manual-v1_3/), with permission from the copyright holders* 
