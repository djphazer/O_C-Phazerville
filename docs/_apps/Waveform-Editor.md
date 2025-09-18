---
layout: default
---
# Waveform Editor

Waveform Editor is an application for creating, editing, and saving Vector Oscillator waveforms.

## What is a Vector Oscillator Waveform?

Waveforms are described by as few as two, and up to twelve segments. Each segment has a bi-polar _level_ (between -128 and 127) and a _time_ (between 0 and 9). The level and time are not fixed values, but depend on the application and its settings.

Vector Oscillators are used in four Hemisphere applets:

* VectorLFO, as a cycling bi-polar modulation source
* VectorEG, as a non-cycling uni-polar gated envelope generator with sustain and release
* VectorMod, as a bi-polar triggered modulation source that can be either cycling or non-cycling
* Boots 'n Cats, as an audio oscillator and drum envelope generators

## Level

The Level (-128 to 127) is scaled to the specific application. For example, the VectorLFO's range is -3V to 3V. When a Vector Oscillator is started, its signal level is set to the Level of the _last_ segment, and immediately starts moving to the Level of the first segment, based on the segment's Time (see below).

## Time

The Time of a segment specifies time relative to other segments in the waveform. Two segments with the same Time value will take the same time to complete, regardless of what that value actually is. The total time that it takes a segment to reach its Level will depend upon the Time value (specifically, its ratio to the sum of all Time values in the waveform) and the waveform's frequency (see below).

## Frequency

Each Vector Oscillator application provides one or more ways to affect an oscillator's frequency. In most cases, a value in Hertz (cycles per second) is provided, which expresses how fast the oscillator will cycle (if it is cycling). Each segment in the waveform will be scaled based on its Time and the current frequency. Note that Hz values are approximate, due to hardware and software numeric precision limits.

## Basic Controls and Navigation

* Left Encoder: Turn to move through the waveform's segments. The selected segment will be a solid line. Press the left encoder button to insert a new segment after the selected segment. Long-press the left encoder button to delete the selected segment. **Note**: You cannot delete a segment if it would leave the waveform with fewer than 2 segments, or if it would leave the waveform with a total Time of 0.
* Right Encoder: Press to switch the cursor between editing of Level and Time. Turn the right encoder to set the selected segment's Level between -128 and 128, or the selected segment's Time between 0 and 9.
* Up/Down Buttons: Press to select a waveform. Long-press the Down button to activate a page to add a new waveform, or delete a waveform. **Note**: You cannot delete the last waveform, and you cannot add a waveform if there are fewer than 3 segments remaining.
* For jack I/O functions, see **Monitoring** below.

## Segment Memory

Hemisphere Suite allocates 63 segments of waveform memory. The upper-right corner of the screen shows how many segments are remaining. You may add as many waveforms as you like, with the following limitations:

* At least 3 segments must be available to create a new waveform. This is because each waveform uses one segment as a table-of-contents entry, and each waveform must have at least two Level/Time segments.
* Each waveform may use up to 12 Level/Time segments

## Choosing a Waveform

Press the Up and Down buttons to choose a waveform.

## Selecting and Editing Segments

Use the left encoder to move through the segments. The selected segment will be shown as a solid line.

Press the right encoder button to alternate the cursor between Level and Time. Turn the right encoder to set that value for the selected segment.

## Adding and Deleting Segments

To add a new segment, press the left encoder button. A new segment will be inserted after the selected segment, and the newly-created segment will become selected. You cannot add a segment if the waveform already has 12 segments.

To delete a segment, long-press the left encoder button. The selected segment will be deleted, and the next segment will become selected. You cannot delete a segment if doing so would leave only one segment in the waveform. You also cannot delete a segment if doing so would leave the waveform with a total Time of 0.

## Monitoring

You may monitor the waveform you're editing in four different ways, one for each output:

* Out A: Bi-polar LFO (default 1Hz) - _Modulated by CV 1_
* Out B: Bi-polar audio rate oscillator (default 440Hz) - _Modulated by CV 2_
* Out C: Uni-polar gated EG with sustain and release - _Gated by Digital 3_
* Out D: Bi-polar triggered one-shot modulation - _Triggered by Digital 4_

## Saving Waveforms

Waveform data can be saved in two ways:

(1) **With the built-in Ornament and Crime storage system** Return to the main menu by long-pressing the right encoder. Then, long-press the right encoder again. This will save the state of all applications in the module.

(2) **System Exclusive Librarian** Your current waveform library can be dumped to a system exclusive librarian program on a computer for permanent storage. When you long-press the right button to return to the main menu, the sysex dump will be initiated. When setting up your librarian software, select Hemisphere as the MIDI device.

Waveform Editor sends your waveform library as four sysex messages, so make sure that your librarian captures all of them and stores them in the same file.

To restore your waveforms, simply send the dump back any time Waveform Editor is running.
