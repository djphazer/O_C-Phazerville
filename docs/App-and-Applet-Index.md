---
title: App & Applet Index
nav_order: 7
---

# App and Applet Index

Jump to the lists of [Full Screen Apps](App-and-Applet-Index#full-screen-apps) or [Hemisphere Applets](App-and-Applet-Index#hemisphere-applets), also organized [by function](App-and-Applet-Index#apps-and-applets-by-function)

## Full Screen Apps
Full screen apps in Phazerville are mostly from the original Ornament and Crime firmware, with a few notable additions ([Calibr8or](Calibr8or), [Scenery](Scenery), and [Passencore](Passencore)). Each of the full screen apps takes advantage of all inputs and outputs in their own way, which is usually configurable.

Not all the apps can fit at once on Teensy 3.2 hardware, but you can use the [default set](https://github.com/djphazer/O_C-Phazerville/releases) or [choose your own selection](https://github.com/djphazer/O_C-Phazerville/discussions/38) with a custom build.

 * [Acid Curds](Acid-Curds) - Quad 8-step chord progression sequencer
 * [Automatonnetz](Automatonnetz) - Neo-Riemannian transformations on a 5x5 matrix sequence!
 * [Calibr8or](Calibr8or) - Quad performance quantizer with pitch tracking calibration
 * [Captain MIDI](Captain-MIDI) - Configurable CV-to-MIDI and MIDI-to-CV interface
 * [CopierMaschine](CopierMaschine) - Quantizing Analogue Shift Register
 * [The Darkest Timeline](The-Darkest-Timeline) - Parallel universe sequencer
 * [Dialectic Ping Pong](Dialectic-Ping-Pong)
 * [Enigma](Enigma) – Sequencer of shift registers (Turing Machines)
 * [Harrington 1200](Harrington-1200) - Neo-Riemannian transformations for triad chord progressions
 * Hemisphere - 2 [Applets](#hemisphere-applets) at a time
 * [Low-rents](Low-rents) - Lorenz attractor
 * [Meta-Q](Meta-Q) - Dual sequenced quantizer
 * [Neural Net](Neural-Net) - 6 Neuron logic processor
 * [Quadraturia](Quadraturia) - Quadrature wavetable LFO
 * [Quantermain](Quantermain) - Quad quantizer
 * [References](References) - Tuning utility
 * [Scenery](Scenery) - Macro CV switch / crossfader
 * [Passencore](Passencore) - Generate a chord progression from LFOs (from sixolet)
 * [Piqued](Piqued) - Quad envelope generator
 * [Pong](Pong) - It's Pong!
 * [Scale Editor](Scale-Editor) - Edit and save microtonal scales
 * [Sequins](Sequins) - Basic dual-channel sequancer
 * [Waveform Editor](Waveform-Editor) - Edit and save vector waveforms (for [LFOs](VectorLFO), [envelopes](VectorEG), [one-shots](VectorMod), and [phase scrubbing](VectorMorph))
 * [Viznutcracker, sweet!](Viznutcracker-sweet) - Quad Bytebeat generator
 * [Backup / Restore](Backup-and-Restore) - Transfer app and calibration data as SysEx
 * [Setup / About](Setup-About) - Check your version, change encoder directions, adjust display/DAC/ADC, screen off time


***

## Hemisphere Applets
Hemisphere splits the screen into two halves: each side available to load any one of a long list of applets. On o_C hardware with inputs and outputs arranged in 3 rows of 4 columns (i.e. most 8hp units), the I/O corresponding to an applet should be in line with that half of the display (i.e. paired into 1+2/A+B and 3+4/C+D).

If you're coming from any of the other Hemisphere forks, note that many of the applets have been upgraded for additional flexibility and functionality, and several are brand new.

* [ADSR](ADSR-EG) - Dual attack / decay / sustain / release envelope
* [AD EG](AD-EG) - Attack / decay envelope
* [ASR](ASR) - Analog Shift Register
* [AttenOff](AttenOff) - Attenu-vert, Offset, and Mix inputs (now with +/-200% range, mix control)
* [Binary Counter](Binary-Counter) - 1 bit per input, output as voltage
* [BootsNCat](BootsNCat) - Noisy percussion
* [Brancher](Brancher) - Bernoulli gate
* [BugCrack](BugCrack) - Sick drums, don't bug out
* [Burst](Burst) - Rapid trigger / ratchet generator
* [Button2](Button2) - 2 simple triggers or latching gates. Press the button!
* [Calculate](Calculate) - Dual Min, Max, Sum, Diff, Mean, Random, S&H
* [Calibr8](Calibr8) - 2-channel, mini Calibr8or for v/Oct correction
* [Carpeggio](Carpeggio) - X-Y table of pitches from a scale/chord
* [Chordinate](Chordinate) - Quantizer with scale mask, outputs root + scale degree (from qiemem)
* [ClockDivider](ClockDivider) - Dual complex clock pulse multiplier / divider.
* [Clk2Gate](Clk2Gate) - Variable-length gate generator
* [ClockSkip](Clock-Skipper) - Randomly skip pulses
* [Compare](Compare) - Basic comparator
* [Cumulus](Cumulus) - Bit accumulator, inspired by Schlappi Nibbler
* [CVRec](CV-Recorder) - Record / smooth / playback CV up to 384 steps on 2 tracks
* [DivSeq](DivSeq) - Two sequences of clock dividers
* [DrumMap](DrumMap) - Clone of Mutable Instruments Grids
* [DualQuant](Dual-Quantizer) - Basic 2-channel quantizer with sample and hold
* [DuoTET](DuoTET) - microtonal N-TET duophonic quantizer
* [Ebb & LFO](Ebb-&-LFO) - clone of Mutable Instruments Tides; oscillator / LFO with CV-controllable waveshape, slope, V/Oct, folding
* [Enigma Jr.](Enigma-Jr) - compact player of curated shift registers
* [EnvFollow](Envelope-Follower) - follows or ducks based on incoming audio
  - added Speed control
* [EuclidX](EuclidX) - Euclidean pattern generator (replacement for [AnnularFusion](https://github.com/Chysn/O_C-HemisphereSuite/wiki/Annular-Fusion-Euclidean-Drummer))
* [GameOfLife](GameOfLife) - experimental cellular automaton modulation source
* [GateDelay](Gate-Delay) - simple gate delay
* [GatedVCA](Gated-VCA) - simple VCA
* [Dr. LoFi](Dr.-LoFi) - super crunchy PCM delay line with bitcrushing and rate reduction
  - based on [LoFi Tape](https://github.com/Chysn/O_C-HemisphereSuite/wiki/LoFi-Tape) and Dr. Crusher
* [Logic](Logic) - AND / OR / XOR / NAND / NOR / XNOR
* [LowerRenz](LowerRenz) - orbiting particles, chaotic modulation source
* [Metronome](Metronome) - internal clock tempo control + multiplier output
* [MIDI In](MIDI-Input) - from USB to CV
* [MIDI Out](MIDI-Out) - from CV to USB
* [MixerBal](Mixer-Balance) - basic CV mixer
* [MultiScale](MultiScale) - like ScaleDuet, but with 4 scale masks
* [Palimpsest](Palimpsest) - accent sequencer
* [Pigeons](Pigeons) - dual Fibonacci-style melody generator
* [PolyDiv](PolyDiv) - four concurrent clock dividers with assignable outputs
* [ProbDiv](ProbDiv) - stochastic trigger generator
* [ProbMeloD](ProbMeloD) - stochastic melody generator
* [ResetClk](Reset-Clock) - rapidly advance a sequencer to the desired step (from [pkyme](https://github.com/pkyme/O_C-HemisphereSuite/tree/reset-additions))
* [RndWalk](Random-Walk) - clocked random walk CV generator (from [adegani](https://github.com/adegani/O_C-HemisphereSuite))
* [RunglBook](RunglBook) - chaotic shift-register modulation
* [ScaleDuet](ScaleDuet) - 2 quantizers with independent scale masks
* [Schmitt](Schmitt-Trigger) - Dual comparator with low and high threshold
* [Scope](Scope) - tiny CV scope / voltmeter / BPM counter
  - expanded with X-Y view
* [Seq32](Seq32) - compact 32-step sequencer using Sequins pattern storage — record, play, and CV switch between 8 sequences!
* [SeqPlay7](SeqPlay7) - Make a playlist of Seq32 sequences, with quantizer override
* [SequenceX](SequenceX) - up to 8 steps of CV, quantized to semitones
* [ShiftGate](ShiftGate) - dual shift register-based gate/trigger sequencer
* [Shredder](Shredder) - clone of Mimetic Digitalis
* [Shuffle](Shuffle) - it don't mean a thing if it ain't got that swing
  - triplets added on 2nd output
* [Slew](Slew) - Dual channel slew: one linear, the other exponential
* [Squanch](Squanch) - advanced quantizer with transpose
* [Stairs](Stairs) - stepped CV
* [Strum](Strum) - the ultimate arpeggiator (pairs well with Rings)
* [Switch](Switch) - CV switch & toggle
* [SwitchSeq](Switch-Seq) - multiple Seq32 patterns running in parallel
* [TB-3PO](TB-3PO) - a brilliant 303-style sequencer
* [TL Neuron](Threshold-Logic-Neuron) - clever logic gate
* [Trending](Trending) - rising / falling / moving / steady / state change / value change
* [TrigSeq](TrigSeq) - two 8-step trigger sequences
* [TrigSeq16](TrigSeq16) - one 16-step trigger sequence
* [Tuner](Tuner) - oscillator frequency detector
* [TwoRings](TwoRings) - Highly configurable pair of Turing-Machine-style shift registers (formerly DualTM, replacement for ShiftReg/TM)
* [VectorEG](VectorEG) - Dual envelopes from a library of bipolar and unipolar shapes (customizable with the [Waveform Editor](Waveform-Editor))
* [VectorLFO](VectorLFO) - Dual LFOs from a library of bipolar and unipolar shapes
* [VectorMod](VectorMod) - Dual One-shots from a library of bipolar and unipolar shapes
* [VectorMorph](VectorMorph) - Dual (or linked) phase scrubbing along a library of bipolar and unipolar shapes
* [Voltage](Voltage) - static output CV
* [WTVCO](WTVCO) - WaveTable Voltage Controlled Oscillator

***

## Apps and Applets by Function

| Function                 | Hemisphere Applets                                                                                                                                                      | Full Screen Apps                                                                         |
| ------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------- |
|**Accent Sequencer**       | [Palimpsest](Palimpsest)                                                                                                                                                        |                                                                                          |
| **Analog Logic**            | [Calculate](Calculate)                                                                                                                                                         |                                                                                          |
| **Clock Modulator**          | [ClockDivider](ClockDivider), [ClockSkip](Clock-Skipper), [DivSeq](DivSeq), [Metronome](Metronome), [PolyDiv](PolyDiv), [ProbDiv](ProbDiv), [ResetClk](Reset-Clock), [Shuffle](Shuffle)                                         |                                                                                          |
| **CV Recorder**              | [ASR](ASR), [CVRec](CV-Recorder)                                                                                                                                                 |                                                                                          |
| **Digital Logic**            | [Binary Counter](Binary-Counter), [Compare](Compare), [Cumulus](Cumulus), [Logic](Logic), [Schmitt](Schmitt-Trigger), [TL Neuron](Threshold-Logic-Neuron), [Trending](Trending)                                                         | [Neural Net](Neural-Net)                                                                             |
| **Delay**                    | [GateDelay](Gate-Delay)                                                                                                                                                         |                                                                                          |
| **Drums / Synth Voice**                    | [BootsNCat](BootsNCat), [BugCrack](BugCrack), [WTVCO](WTVCO)                                                                                                                                         | [Viznutcracker, sweet!](Viznutcracker-sweet)                                                                  |
| **Effect**                   | [Dr. LoFi](Dr.-LoFi)                                                                                                                                                          |                                                                                          |
| **Envelope Follower**        | [EnvFollow](Envelope-Follower), [Slew](Slew)                                                                                                                                             |                                                                                          |
| **Envelope Generator**       | [ADSR](ADSR-EG), [AD EG](AD-EG), [VectorEG](VectorEG)                                                                                                                                 | [Piqued](Piqued), [Dialectic Ping Pong](Dialectic-Ping-Pong)                                                          |
| **LFO**                      | [Ebb & LFO](Ebb-&-LFO), [LowerRenz](LowerRenz), [VectorLFO](VectorLFO)                                                                                                                       | [Quadraturia](Quadraturia)                                                                            |
| **MIDI**                     | [MIDI In](MIDI-Input), [MIDI Out](MIDI-Out) _(See also: [Auto MIDI Output](Hemisphere-General-Settings#auto-midi-output))_                                                                                                                                           | [Captain MIDI](Captain-MIDI)                                                                           |
| **Mixer**                    | [MixerBal](Mixer-Balance)                                                                                                                                                          |                                                                                          |
| **Modulation Source**        | [GameOfLife](GameOfLife), [Stairs](Stairs), [VectorMod](VectorMod), [VectorMorph](VectorMorph)                                                                                                                          | [Low-rents](Low-rents), [Pong](Pong)                                                                                   |
| **Performance Utility**      | [Button2](Button2)                                                                                                                                                           |  [Scenery](Scenery)                                                                                        |
| **Pitch Sequencer**          | [Carpeggio](Carpeggio), [TwoRings](TwoRings), [Enigma Jr.](Enigma-Jr), [Pigeons](Pigeons), [ProbMeloD](ProbMeloD), [Seq32](Seq32), [SeqPlay7](SeqPlay7), [SequenceX](SequenceX), [Shredder](Shredder), [Strum](Strum), [SwitchSeq](Switch-Seq), [TB-3PO](TB-3PO) | [Enigma](Enigma), [The Darkest Timeline](The-Darkest-Timeline), [Automatonnetz](Automatonnetz), [Sequins](Sequins), [Acid Curds](Acid-Curds), [Passencore](Passencore) |
| **Quantizer**               | [Calibr8](Calibr8), [Chordinate](Chordinate), [DualQuant](Dual-Quantizer), [DuoTET](DuoTET), [MultiScale](MultiScale), [ScaleDuet](ScaleDuet), [Squanch](Squanch)         | [Calibr8or](Calibr8or), [Harrington 1200](Harrington-1200), [Quantermain](Quantermain), [Meta-Q](Meta-Q)                                  |
| **Random / Chaos**           | [Brancher](Brancher), [LowerRenz](LowerRenz), [ProbDiv](ProbDiv), [ProbMeloD](ProbMeloD), [RndWalk](Random-Walk), [Shredder](Shredder)                                                        | [Low-rents](Low-rents)                                                                              |
| **Shift Register**           | [ASR](ASR), [TwoRings](TwoRings), [Enigma Jr.](Enigma-Jr), [RunglBook](RunglBook), [ShiftGate](ShiftGate)                                                                                               | [Enigma](Enigma), [CopierMaschine](Copiermaschine)                                                               |
| **Switch**                   | [Switch](Switch), [SwitchSeq](Switch-Seq)                                                                                                                                         | [Scenery](Scenery)                                                                                    |
| **Trigger / Gate Sequencer** | [DivSeq](DivSeq), [DrumMap](DrumMap), [EuclidX](EuclidX), [PolyDiv](PolyDiv), [ProbDiv](ProbDiv), [Seq32](Seq32), [ShiftGate](ShiftGate), [TrigSeq](TrigSeq), [TrigSeq16](TrigSeq16)                                 |                                                                                  |
| **VCA**                      | [GatedVCA](Gated-VCA)                                                                                                                                                          |                                                                                          |
| **Voltage Utility**          | [AttenOff](AttenOff), [Calculate](Calculate), [Calibr8](Calibr8), [Scope](Scope), [Slew](Slew), [Stairs](Stairs), [Switch](Switch), [Tuner](Tuner), [Trending](Trending), [Voltage](Voltage)                         | [Calibr8or](Calibr8or), [References](References)
