---
layout: default
---
# KrpsStrng (mono)

A Karplus-Strong physical string synthesizer. Plucks a virtual string by filling a delay line with an excitation burst and feeding it through a resonant feedback loop, producing plucked-string and bass tones with independent control over decay, brightness, and body character.

### Parameters

* Pitch: Base pitch of the string. CVable via V/Oct.
    * Note name: Adjusts pitch in semitone steps. Displays the closest chromatic note name.
    * Hz: Fine-tunes pitch in ~3-cent steps.
* Trg: Trigger source. Each incoming gate/clock plucks the string. Pitch CV is sampled at trigger time.
* Dcy: Decay time — how long the string rings before falling silent. CVable.
    * 0: Very short staccato (~14 ms RT60). Sounds percussive and dry.
    * 100: Long sustain (6 seconds-ish). Sounds almost infinitely sustained.
    * The curve is logarithmic, so the lower half of the range covers the most musically useful short-to-medium decays.
* Brt: Brightness — controls the tone of the sustained string. CVable.
    * 0: Dark, heavily low-pass filtered. Muted, hollow sound.
    * 100: Bright, full harmonic content. Cutting, present sound.
    * Acts as a tunable IIR low-pass in the feedback loop; affects the entire sustain, not just the attack.
* Bdy: Body — controls the character of the initial attack transient. CVable.
    * 0: Bright, noisy, percussive pluck.
    * 100: Clean, smooth, bow-like attack.
    * Middle values crossfade between the two. Body is independent of Brightness — Body shapes the attack, Brightness shapes the sustain.
* Mix: Blends between the dry input signal and the KS synthesizer output. CVable.
    * 0: Fully dry (input pass-through, no synthesis).
    * 100: Fully wet (synthesizer only).

### Suggested patches

* Route a sequencer's V/Oct output to Pitch CV and its gate to Trg for a melodic plucked bass or lead line.
* Set Dcy low and Brt high for a sharp, percussive pizzicato. Set Dcy high and Brt low for a deep, slowly fading tone.
* Use Bdy to distinguish between articulations — automate with CV for expressive variation.
* Stack two KrpsStrng applets on L and R channels tuned a few cents apart and blend with the Crosspan applet for a natural chorus/doubling effect.


### Credits

Authored by uglifruit (Andy Jenkinson) & ClaudeCode

### License

MIT License — Copyright (c) 2026 Andy Jenkinson

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
