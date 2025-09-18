---
layout: default
---
# Passencore

[Originally published on lines by Sixolet](https://llllllll.co/t/passencore-chord-ornament-music-theory-crime/45925)

Turn an LFOs into a chord progression? Harmonize a bass line that doesn’t just play roots, with an eye to functional harmony and keeping voices within a range? Just want something in the lines `library` category for a platform not yet represented?

(this clip is my test patch right now, rather than an attempt at a “piece of music” so it’s nothing special, but maybe it gives an idea)

Patch notes: Pam’s triangle LFO into Passencore `tension` input, TR1 set to “sample” and “passing” and TR2 set to “target”, both from Pam’s at different times. Passencore 4x CV into Stages configured as a sequential switch into rings 4x polyphonic to make chords

Passencore is an Ornament & Crime app that replaces Meta-Q in an alternate firmware. It’s a chord sequencer without the sequence, where you don’t (have to) pick the chords. Instead, you patch in an LFO or two and some triggers for when you want it to pick a new chord. One of your trigger options is for a “passing chord”, to lead you from whatever chord you’re playing now to your target chord. I have imbued it with just enough music theory to be dangerous, because that’s how much music theory I know. It knows:

- basic voice leading, but dgaf about parallel fifths because idgaf about parallel fifths
- functional harmony, ish, with the ability to substitute chords
- dominants are pretty cool
- sus chords are even cooler

The inspiration is somewhere between Acid Curds (for the s&h chord workflow), MI Grids (for how it’s repeatable, but not exactly _predictable_), and some kind of “synf-onion” that I have only heard about but never seen.

### [](https://llllllll.co/t/passencore-chord-ornament-music-theory-crime/45925#requirements-2)Requirements

Ornament & Crime, Teensy bootloader, uUSB cable, willingness to try something new.

### [](https://llllllll.co/t/passencore-chord-ornament-music-theory-crime/45925#documentation-3)Documentation

#### [](https://llllllll.co/t/passencore-chord-ornament-music-theory-crime/45925#basic-use-5)Basic use

Patch a slow 0to5v LFO of some kind to CV1. This is “tension”.

Scroll down in the settings to make sure “sample trigger” and “target trigger” are both set to TR1 (that’s the default I set, but I’m somewhat new to o_C development, and sometimes I find my defaults set wrong, so ymmv.) Anyway, patch a trigger to TR1 that goes more than once per cycle of your LFO. Tune some oscillators to each other, and give all four outputs to them, or maybe give them to one oscillator with a sequential switch.

#### [](https://llllllll.co/t/passencore-chord-ornament-music-theory-crime/45925#your-fancy-trigger-inputs-6)Your fancy trigger inputs

All assignable, so you can duplicate “sample” and “target” or “sample” and “passing” or whatever.

- `sample`: Read the values of _tension_ and _color_ and whatever other constraints and pick the next target chord based on them. Don’t necessarily play it yet, but do play it yet if `target` is also triggered.
- `target`: Play the target chord.
- `passing`: Play a passing chord to get to the target chord. If you’re already at the target chord, play a passing chord to get back there anyway. The algorithm currently likes sus chords and secondary dominants, but chromatic or stepwise passing chords are also possible.
- `reset`: Pick the next target chord with a root voicing. Useful for if you find yourself with a pattern that kind of wanders around based on the voice leading rules without repeating in as short a time as you like. Since the algorithm is deterministic (even if it’s complicated) `reset` can be a forced repetition point.

#### [](https://llllllll.co/t/passencore-chord-ornament-music-theory-crime/45925#your-trusty-cv-inputs-7)Your trusty CV inputs

**CV1** _tension_, roughly equivalent to “function” in functional harmony. Goes from 0v is roughly “tonic”, 5v is roughly “dominant”, and intermediate voltages do predominants and other chords and stuff.  
[include instructions inline, or provide link to external documentation, or attach a PDF/etc.]

**CV2** _color_, roughly equivalent to how consonant the chord is going to be on its own. Goes from “power” (roots and fifths) through “classic” (triads) to “interesting” and “ext/subst” (which start adding in 7th and sus chords, and have weightings that allow for more functional substitutions, ex vi for I or iii for V) to “iono jazz?”. You can set the “base color” in settings, and CV2 adds to that. Default base color is “classic” for triads and the occasional seventh chord.

**CV3 & CV4** Assignable. For now options that constrain the target chords we’re picking, but not the passing chords.

- `-`: nuthin’
- `include`: Input a note. It gets quantized to your active scale, and the resulting _target_ chord must include that note.
- `root`: Input a note. It gets quantized to your active scale, and the resulting _target_ chord has that root. Any inversion remains possible.
- `bass`: Input a note. It gets quantized to your active scale, and the resulting _target_ chord has that note in output A.

#### [](https://llllllll.co/t/passencore-chord-ornament-music-theory-crime/45925#other-business-8)Other business

- Settings for the center of each voice’s range. Controls for octave and half-steps up or down from that.
- In certain cases Passencore wants to borrow a chord from another mode — chromatic passing chords and dominants or secondary dominants, usually. Allow or disallow, as you please.

#### [](https://llllllll.co/t/passencore-chord-ornament-music-theory-crime/45925#how-does-it-pick-chords-9)How does it pick chords?

It does a few rounds of “add some chords to a buffer, evaluate them on some criteria, narrow down to just a few”.

- Round 1: Add all triads, narrow by function and by required root note
- Round 2: Add variations like sus chords, sevenths, and ninths, narrow by function, color, required included note.
- Round 3: Add different voicings, narrow by all criteria above plus voice leading rules and required bass notes.

Pick the top-rated chord.

It does something similar for passing chords, but simpler, and populates with secondary dominants, sus chords, and chords rooted in between the notes it’s trying to get between.

#### [](https://llllllll.co/t/passencore-chord-ornament-music-theory-crime/45925#bonus-10)Bonus

Passencore works with all the modes of the diatonic scale and the harmonic minor scale; the harmonic minor and its modes are now in the global scale list.

NB I’m not actually that sure how applicable functional harmony is to the ultralocrian or whatever, but whatever my code does with it sounds interesting at least.

### [](https://llllllll.co/t/passencore-chord-ornament-music-theory-crime/45925#roadmap-11)Roadmap

Features I want to add:

- CV assignments that let you modulate key & scale
- Ability to limit chord output to 3 outputs, and reserve one for a melody quantizer channel.
- Given a melody quantizer channel, options to let it quantize to the home scale or an “appropriate” pentatonic for the chord.
