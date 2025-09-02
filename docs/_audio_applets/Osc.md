---
layout: default
---
# Osc (mono)

![Osc screenshot](images/Osc.png)

A basic oscillator voice with built-in waveshaping, FM/PM, VCA, and mixing.
Supports a sine, variable slope triangle, and variable pulse width square wave.
Processes the output of the preceding applet in the chain by mixing it with the voice and/or by using as a modulation source.

### Parameters
* Waveform: Which waveform to use.
    * SIN: Sine wave.
    * TRI: Variable slope triangle wave.
    * PLS: Variable pulse-width square.
* Duty-cycle %: Only active for TRI and PLS waveforms. CVable.
    * TRI: Changes the angle of the slopes, ranging from reverse sawtooth at 0%, to triangle at 50%, to sawtooth at 100%.
    * PLS: Changes the width of the pulse.
* Frequency: Adjusts the frequency of the wave. CVable using v/oct.
    * Note name: Indicates the closest note in a 12-tone chromatic scale (with A4 = 440 Hz). Changing the note name will change the frequency in semitone increments.
    * Hz: Allows finer grained adjustments of frequency.
* PM/FM: Phase modulation or frequency modulation. The incoming audio signal will be used as the modulator.
    * PM/FM label: Changes whether to use PM or exponential FM. Note that PM with sine modulator and carrier is equivalent to TZFM and will give you classic DX7-like FM.
    * Depth:
        * For PM: Unit is period of the carrier.
        * For FM: Unit is octaves.
    * CV: Acts as a linear VCA on the modulator.
* Lvl: The gain applied to the output signal in dBs. CV responds exponentially. If a CV source is present, lvl will be the maximum gain applied.
* Mix: Mixes the incoming signal with the waveform. CVable.

### Suggested patches

* Use an AD or ADSR CV applet as a source for lvl to easily create a simple voice with an envelope.
* Stack multiple Osc applets in a chain, and use the Mix parameter to blend them together. Then, tune them in relative harmonics to create chords. Patch the same sequencer source into the v/oct inputs of each so they'll maintain their harmonic relationships.
* Stack multiple Osc applet together and turn up their PM depth to create chains of FM operators.
* Create even more complex FM topologies by use the Crosspan applet to mix modulator signals together from both channels.

### Credits

Authored by qiemem (Bryan Head), with mods by djphazer.