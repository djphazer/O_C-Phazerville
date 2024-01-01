// Copyright (c) 2018, Roel Das
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef HEM_ARP_CHORD_H_
#define HEM_ARP_CHORD_H_

struct hem_arp_chord {
  const char* chord_name;
  const int chord_tones[12];
  const size_t nr_notes;
  const int octave_span;
};

const int Nr_of_arp_chords = 55; // counting from 0....

// 1 b2 2 b3 3 4 b5 5 b6 6 b7  7  8
// 0  1 2  3 4 5  6 7  8 9 10 11 12

const hem_arp_chord Arp_Chords[] = {
  //ARPEGGIATE
  {"Maj triad",{0, 4, 7}, 3,1},
  {"Maj inv 1",{4, 7, 12}, 3,1},
  {"Maj inv 2",{7, 12, 16}, 3,1},
  {"min triad",{0, 3, 7}, 3,1},
  {"min inv 1",{3, 7, 12}, 3,1},
  {"min inv 2",{7, 12, 15}, 3,1},
  {"dim triad",{0, 3, 6},3,1},
  {"Octaves",{0},1,1},
  {"1 5",{0, 7},2,1},

  {"aug triad",{0, 4, 8},3,1},
  {"sus2",{0,2,7},3,1},
  {"sus4",{0,5,7},3,1},
  {"add2",{0,2,4,7},4,1},
  {"min(add2)",{0,2,3,7},4,1},

  {"add4",{0,4,5,7},4,1},
  {"min(+4)",{0,3,5,7},4,1},
  {"sus4(+2)",{0,2,6,7},4,1},
  {"12345",{0,2,4,5,7},5,1},
  {"12b345",{0,2,3,5,7},5,1},

  {"add(#11)",{0,4,6,7},4,1},
  {"Maj6",{0,4,7,9},4,1},
  {"min6",{0,3,7,9},4,1},
  {"Maj7",{0,4,7,11},4,1},
  {"7",{0,4,7,10},4,1},
  ////////     20     /////
  {"7sus2",{0,2,7,10},4,1},
  {"7sus4",{0,5,7,10},4,1},
  {"min7",{0,3,7,10},4,1},
  {"dim7",{0,3,6,8},4,1},
  {"Maj9",{0,4,7,11,14},5,1},

  {"Maj6/9",{0,4,7,9,14},5,1},
  {"Maj#11",{0,4,7,11,14,18},6,2},
  {"9",{0,4,7,10,14},5,1},
  {"7(b9)",{0,4,7,10,13},5,1},
  {"7(b9,b13)",{0,4,7,10,13,20},6,2},
  ////////     30     /////
  {"Ionian",{0,2,4,5,7,9,11},7,1},
  {"Dorian",{0,2,3,5,7,9,10},7,1},
  {"Phrygian",{0,1,3,5,7,8,10},7,1},
  {"Lydian",{0,2,4,6,7,9,11},7,1},
  {"Mixolyd.",{0,2,4,5,7,9,10},7,1},

  {"Aeolian",{0,2,3,5,7,8,10},7,1},
  {"Locrian",{0,1,3,5,6,8,10},7,1},
  {"Harm Min",{0,2,3,5,7,8,11},7,1},
  {"Mel Min",{0,2,3,5,7,9,11},7,1},
  {"Penta",{0,2,4,7,9},5,1},
  //////////    40 ////////
  {"min Penta",{0,3,5,7,10},5,1},
  {"Maj Blues",{0,2,3,4,7,9},6,1},
  {"min Blues",{0,3,5,6,7,10},6,1},
  {"Bebop",{0,2,4,5,7,9,10,11},8,1},
  {"WholeTone",{0,2,4,6,8,10},6,1},

  {"Dim 1 1/2",{0,2,3,5,6,8,9,11},8,1},
  {"Dim 1/2 1",{0,2,3,5,6,8,9,11},8,1},
  {"Altered",{0,1,3,4,6,8,10},7,1},
  {"Chromatic",{0,1,2,3,4,5,6,7,8,9,10,11},12,1},
  {"All 4th", {0,5,10,15,20,26,31},7,3},
  //////////    50 ////////
  {"All 5th", {0,7,14,21,28,35,41},7,4}
};

#endif
