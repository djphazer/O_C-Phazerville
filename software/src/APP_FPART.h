// Copyright (c) 2023 Jesse Dinneen
//
// Author of this app: Jesse Dinneen (jdinneen@gmail.com)
// Authors of O+C firmware and apps: Patrick Dowling (pld@gurkenkiste.com)
// Tim Churches (tim.churches@gmail.com), mxmxmx (https://github.com/mxmxmx)
// With great help from tutorials and examples by Chysn (https://github.com/Chysn)
// and Naomi Seyfer (https://github.com/sixolet)
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
//
// FPART: a 4-channel step sequencer with staff-like visualisation.
// To use, add to OC_app.ino app array:
//   DECLARE_APP('F','P', "4 Parts", FPART, FPART_isr),
//
// Calling the files/classes "4-Part" (w/ number) causes errors,
// so the admittedly unattractive "fpart" it is (for now)...

#ifdef ENABLE_APP_FPART

// refs to other files, data structures, etc
#include "OC_apps.h"
#include "util/util_settings.h"
#include "OC_menus.h"
#include "OC_strings.h"
#include "OC_DAC.h"
#include "OC_digital_inputs.h"
#include "OC_ADC.h"

/* list each setting that should appear in the settings menu
   _SETTING_LAST is required and must come last
   to read these in the class: int root = values_[FPART_SETTING_ROOT];
   to write one: apply_value(int index, int value) */
enum FPART_SETTINGS {
  FPART_SETTING_ROOT,
  FPART_SETTING_SCALE,
  FPART_SETTING_LOOPSTART,
  FPART_SETTING_LOOPEND,
  FPART_SETTING_A_OCTAVE,
  FPART_SETTING_B_OCTAVE,
  FPART_SETTING_C_OCTAVE,
  FPART_SETTING_D_OCTAVE,
  FPART_SETTING_LBUTTON_TOGGLE,
  FPART_SETTING_ACTIVECHORD,
  FPART_SETTING_CHORD0,
  FPART_SETTING_CHORD1,
  FPART_SETTING_CHORD2,
  FPART_SETTING_CHORD3,
  FPART_SETTING_CHORD4,
  FPART_SETTING_CHORD5,
  FPART_SETTING_CHORD6,
  FPART_SETTING_CHORD7,
  FPART_SETTING_CHORD8, 
  FPART_SETTING_CHORD9, 
  FPART_SETTING_CHORD10, 
  FPART_SETTING_CHORD11,
  FPART_SETTING_CHORD12,
  FPART_SETTING_CHORD13,
  FPART_SETTING_CHORD14,
  FPART_SETTING_CHORD15,
  FPART_SETTING_CHORD16,
  FPART_SETTING_CHORD17,
  FPART_SETTING_CHORD18,
  FPART_SETTING_CHORD19,
  FPART_SETTING_CHORD20,
  FPART_SETTING_CHORD21,
  FPART_SETTING_CHORD22,
  FPART_SETTING_CHORD23,
  FPART_SETTING_CHORD24,
  FPART_SETTING_CHORD25,
  FPART_SETTING_CHORD26,
  FPART_SETTING_CHORD27,
  FPART_SETTING_CHORD28,
  FPART_SETTING_CHORD29,
  FPART_SETTING_CHORD30,
  FPART_SETTING_CHORD31,
  FPART_SETTING_CHORD32, 
  FPART_SETTING_CHORD33, 
  FPART_SETTING_CHORD34, 
  FPART_SETTING_CHORD35, 
  FPART_SETTING_CHORD36, 
  FPART_SETTING_CHORD37, 
  FPART_SETTING_CHORD38, 
  FPART_SETTING_CHORD39, 
  FPART_SETTING_CHORD40,
  FPART_SETTING_CHORD41, 
  FPART_SETTING_CHORD42, 
  FPART_SETTING_CHORD43, 
  FPART_SETTING_CHORD44, 
  FPART_SETTING_CHORD45, 
  FPART_SETTING_CHORD46, 
  FPART_SETTING_CHORD47, 
  FPART_SETTING_CHORD48, 
  FPART_SETTING_CHORD49, 
  FPART_SETTING_CHORD50, 
  FPART_SETTING_CHORD51, 
  FPART_SETTING_CHORD52, 
  FPART_SETTING_CHORD53, 
  FPART_SETTING_CHORD54, 
  FPART_SETTING_CHORD55, 
  FPART_SETTING_CHORD56, 
  FPART_SETTING_CHORD57, 
  FPART_SETTING_CHORD58, 
  FPART_SETTING_CHORD59, 
  FPART_SETTING_CHORD60, 
  FPART_SETTING_CHORD61, 
  FPART_SETTING_CHORD62, 
  FPART_SETTING_CHORD63,
  FPART_SETTING_CHORD64, 
  FPART_SETTING_CHORD65, 
  FPART_SETTING_CHORD66, 
  FPART_SETTING_CHORD67, 
  FPART_SETTING_CHORD68, 
  FPART_SETTING_CHORD69, 
  FPART_SETTING_CHORD70,
  FPART_SETTING_CHORD71, 
  FPART_SETTING_CHORD72, 
  FPART_SETTING_CHORD73, 
  FPART_SETTING_CHORD74, 
  FPART_SETTING_CHORD75, 
  FPART_SETTING_CHORD76, 
  FPART_SETTING_CHORD77, 
  FPART_SETTING_CHORD78, 
  FPART_SETTING_CHORD79, 
  FPART_SETTING_CHORD80, 
  FPART_SETTING_CHORD81, 
  FPART_SETTING_CHORD82, 
  FPART_SETTING_CHORD83, 
  FPART_SETTING_CHORD84, 
  FPART_SETTING_CHORD85, 
  FPART_SETTING_CHORD86, 
  FPART_SETTING_CHORD87, 
  FPART_SETTING_CHORD88, 
  FPART_SETTING_CHORD89, 
  FPART_SETTING_CHORD90,
  FPART_SETTING_CHORD91, 
  FPART_SETTING_CHORD92, 
  FPART_SETTING_CHORD93, 
  FPART_SETTING_CHORD94, 
  FPART_SETTING_CHORD95, 
  FPART_SETTING_CHORD96, 
  FPART_SETTING_CHORD97, 
  FPART_SETTING_CHORD98,
  FPART_SETTING_LAST
};

enum FPART_MENU_PAGES {
  FPART_MENU_PARAMETERS,
  FPART_MENU_STAFFS,
  FPART_MENU_PAGES_LAST  
};

// define roots and scales and their names; root names come from OC_strings
const char * const  scale_names[7] = { // name the possible scales, indices match scale_pitches
  "IONI", "DORI", "PHRY", "LYDI", "MIXO", "AEOL", "LOCR"
};
int scale_pitches[7][7] = { // define the pitch values for those scales
  { 0, 256, 512, 640, 896, 1152, 1408 }, // Ionian
  { 0, 256, 384, 640, 896, 1152, 1280 }, // Dorian
  { 0, 128, 384, 640, 896, 1024, 1280 }, // Phrygian
  { 0, 256, 512, 768, 896, 1152, 1408 }, // Lydian
  { 0, 256, 512, 640, 896, 1152, 1280 }, // Mixolydian
  { 0, 256, 384, 640, 896, 1024, 1280 }, // Aeolian
  { 0, 128, 384, 640, 768, 1024, 1280 }  // Locrian
};
int root_degree_lookup[12] = { // for each semitone from C, convert it to a degree
 0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6
};
// define values for FPART_SETTING_LBUTTON_TOGGLE
const char * const  l_options[2] = { // user will set 0 for C&P or 1 for Reset
  "COPY/PASTE", "GO TO 0"
};

// define the main app class
class Fpart : public settings::SettingsBase<Fpart, FPART_SETTING_LAST> {
public:

  menu::ScreenCursor<menu::kScreenLines> cursor;   // instantiate a cursor
  
  bool a_or_b = true; // track which channel's note icon is being moved (true = a/c)
  bool c_or_d = true;
  int last_cv1_index; // for tracking changes to CV1 and CV2
  int last_cv2_index;
  bool copy = true;
  int copyBuffer = 0;

  // define functions to get values
  int get_root() const {
    return values_[FPART_SETTING_ROOT];
  }
  int get_scale() const {
    return values_[FPART_SETTING_SCALE];
  }
  int get_a_oct() const {
    return values_[FPART_SETTING_A_OCTAVE];
  }
  int get_b_oct() const {
    return values_[FPART_SETTING_B_OCTAVE];
  }
  int get_c_oct() const {
    return values_[FPART_SETTING_C_OCTAVE];
  }
  int get_d_oct() const {
    return values_[FPART_SETTING_D_OCTAVE];
  }
  int get_loopend() const {
    return values_[FPART_SETTING_LOOPEND];
  }
  int get_loopstart() const {
    return values_[FPART_SETTING_LOOPSTART];
  }
  int get_activechord() const {
    return values_[FPART_SETTING_ACTIVECHORD];
  }
  int get_chord_int(int chord_number) const {
    return values_[chord_number + 10]; // assumes chord_nums 0-98 and that SETTING_CHORD_0 is in slot 10
  }
  int get_l_option() const {
    return values_[FPART_SETTING_LBUTTON_TOGGLE];
  }
  
  //define functions to set values
  void set_activechord(int chordnum) {
    int reasonable_num = constrain(chordnum, 0, 98);
    apply_value(FPART_SETTING_ACTIVECHORD, reasonable_num);
  }
  void set_chord_int(int chord_num, int chord_int) {
    int reasonable_chord_num = constrain(chord_num, 0, 98);
    int reasonable_chord_int = constrain(chord_int, 0, 32323232);
    apply_value(reasonable_chord_num + 10, reasonable_chord_int); // assumes chord_nums 0-98 and that SETTING_CHORD_0 is in slot 10
  }

  // given four 0-22 note values, set to 10-32, put in right place in 8-digit int, return it
  int build_chord_int(int a_note, int b_note, int c_note, int d_note) const {
    int built_integer = (a_note + 10) * 1000000; // e.g. turns 22 into 32000000
    built_integer += (b_note + 10) * 10000; // e.g. adds 320000, producing 32320000
    built_integer += (c_note + 10) * 100; // adds four digits
    built_integer += (d_note + 10); // adds last two
    return built_integer;
  }
  
  // given some chord (an 8-digit long integer),
  // and an index 0-3 referring to which set of digits are desired,
  // get and return those two digits as an int
  // TODO: replace with bitshift?
  int get_note_from_chord_int(int chord_int, int index) const {
    int note = 0;
    if (index == 0) { // digits 8 and 7 (counting from right) i.e. channel A
      note = (chord_int / 1000000) % 100;
      return note;
    } else if (index == 1) { //digits 6 and 5, ch. B
      note = (chord_int / 10000) % 100;
      return note;
    } else if (index == 2) { //digits 4 and 3, ch. C
      note = (chord_int / 100) % 100;
      return note;
    } else if (index == 3) { //digits 2 and 1, ch. D
      note = chord_int % 100;
      return note;
    }
    return EXIT_SUCCESS;
  }
  
  // define function to calculate a note's pitch in some key
  int32_t get_pitch_from_note(int note) const {
    // considering the current root (0-11, from note_names) and scale/mode (0-6, scale_pitches),
    // calculate the possible pitches of the resulting key.
    int root_val = get_root();
    int root_pitch = root_val * 128;
    int mode = get_scale();
    int key_pitches[7] = {
      root_pitch + scale_pitches[mode][0],
      root_pitch + scale_pitches[mode][1],
      root_pitch + scale_pitches[mode][2],
      root_pitch + scale_pitches[mode][3],
      root_pitch + scale_pitches[mode][4],
      root_pitch + scale_pitches[mode][5],
      root_pitch + scale_pitches[mode][6]
    };
    // then given a note's position on the staffs (0-22, see table below),
    // and where that note is relative to the root's distance from C (distance of 0-6),
    // get that note's output pitch from the generated key_pitches.
    // table:
    //  22-19 F just below bass clef to bass B
    //  18-12 bass C to to B (just below middle c)
    //  11-5 middle C to treble B
    //  4-0 treble C to G just above clef
    int note_val; //for conditionally converting screen positions 0-22 to notes 0-6 (i.e. C-B) plus octaves
    int root_shift_amt = root_degree_lookup[root_val]; // convert root from semitone (0-11) to wholetone (0-6)
    int32_t note_pitch; // 0C 1D 2E 3F 4G 5A 6B 0C 1D 2E 3F 4G 5A 6B 0C
    int note_scale_degree = 0;// for finding which pitch/degree the note is in our new scale
    if (note > 18) { // F-B::22-19, add no octaves
      note_val = 25 - note; // subtract val (<=22) from (4 + 3*7), because I foolishly assign higher note values the further from the top of the staff they go.
      note_scale_degree = note_val - root_shift_amt; //get the distance between the root and note
      if (note_scale_degree < 0) { //put it into a usable range if it's too low
        note_scale_degree += 7;
        note_pitch = key_pitches[note_scale_degree]; // get the pitch from our scale at the index we calculated
        note_pitch -= 12 << 7; //tone it back down
      } else {
        note_pitch = key_pitches[note_scale_degree]; // get the pitch from our scale at the index we calculated
      }
    } else if (note > 11) { // C-B::18-12, add an octave
      note_val = 18 - note; // 4 + 2*7
      note_scale_degree = note_val - root_shift_amt;
      if (note_scale_degree < 0) {
        note_scale_degree += 7;
        note_pitch = key_pitches[note_scale_degree];
      } else {
        note_pitch = key_pitches[note_scale_degree];
        note_pitch += 12 << 7;
      }
    } else if (note > 4) { // C-B::11-5, add two octaves
      note_val = 11 - note; // 4 + 1*7
      note_scale_degree = note_val - root_shift_amt;
      if (note_scale_degree < 0) {
        note_scale_degree += 7;
        note_pitch = key_pitches[note_scale_degree];
        note_pitch += 12 << 7;
      } else {
        note_pitch = key_pitches[note_scale_degree];
        note_pitch += 12 << 7;
        note_pitch += 12 << 7;
      }
    } else { // C-G::4-0, add three octaves  
      note_val = 4 - note; // 4 + 0*7
      note_scale_degree = note_val - root_shift_amt;
      if (note_scale_degree < 0) {
        note_scale_degree += 7;
        note_pitch = key_pitches[note_scale_degree];
        note_pitch += 12 << 7;
        note_pitch += 12 << 7;
      } else {
        note_pitch = key_pitches[note_scale_degree];
        note_pitch += 12 << 7;
        note_pitch += 12 << 7;
        note_pitch += 12 << 7;
      }
    }
    return note_pitch;
  }

  //define copy and paste actions to be called by long_press_left
  void copyChord() {
    copyBuffer = get_chord_int(get_activechord());
  }
  void pasteChord() {
    set_chord_int(get_activechord(), copyBuffer);
  }

  //define how to draw each channel symbol
  void drawA(int x, int y) { //empty circle
    graphics.drawCircle(x+2, y+2, 2.5); //the circle is smaller than the square and its coords refer to its centre so an offset is necessary
  }
  void drawB(int x, int y) { //empty square
    graphics.drawFrame(x, y, 5, 5);
  }
  void drawC(int x, int y) { //filled circle
    graphics.drawCircle(x+2, y+2, 2); //offset circle
    graphics.drawRect(x+1, y+1, 3, 3);
  }
  void drawD(int x, int y) { //filled square
    graphics.drawRect(x, y, 5, 5);
  }
  
  //menu-related functions
  uint8_t get_menu_page() const {
    return menu_page_;  
  }
  void set_menu_page(uint8_t _menu_page) {
    menu_page_ = _menu_page;  
  }

  // initialise variables
  void Init() {
    InitDefaults();
    menu_page_ = FPART_MENU_STAFFS;
    last_cv1_index = 0;
    last_cv2_index = 0;
  }
  
  void Loop();
  
  void ISR() {
    //for each channel, get the note value, get that note's pitch
    int chord_int = get_chord_int(get_activechord());
    int a_note = get_note_from_chord_int(chord_int, 0) - 10; //subtract 10 to scale notes back down to 0-22 values expected
    int b_note = get_note_from_chord_int(chord_int, 1) - 10;
    int c_note = get_note_from_chord_int(chord_int, 2) - 10;
    int d_note = get_note_from_chord_int(chord_int, 3) - 10;
    int a_oct = get_a_oct();
    int b_oct = get_b_oct();
    int c_oct = get_c_oct();
    int d_oct = get_d_oct();
    int32_t a_pitch = get_pitch_from_note(a_note);
    int32_t b_pitch = get_pitch_from_note(b_note);
    int32_t c_pitch = get_pitch_from_note(c_note);
    int32_t d_pitch = get_pitch_from_note(d_note);
    //send pitches to the DAC
    //send channel (a-d), pitch (int32), and octave (int32)
    OC::DAC::set_pitch(DAC_CHANNEL_A, a_pitch, a_oct);
    OC::DAC::set_pitch(DAC_CHANNEL_B, b_pitch, b_oct);
    OC::DAC::set_pitch(DAC_CHANNEL_C, c_pitch, c_oct);
    OC::DAC::set_pitch(DAC_CHANNEL_D, d_pitch, d_oct);
  }

private:
  int8_t menu_page_;
};

// TOTAL EEPROM SIZE: 406 bytes
// declare sources to generate the settings menu (need 4 or more)
SETTINGS_DECLARE(Fpart, FPART_SETTING_LAST) {
  { 0, 0, 11,  "tonic", OC::Strings::note_names_unpadded, settings::STORAGE_TYPE_U8 },
  { 0, 0, 6,   "mode", scale_names, settings::STORAGE_TYPE_U8 },
  { 0, 0, 97,  "loop start", NULL, settings::STORAGE_TYPE_U8 },
  { 3, 1, 98,  "loop end", NULL, settings::STORAGE_TYPE_U8 },
  { 0, -3, 6,  "chan a oct", NULL, settings::STORAGE_TYPE_I8 },
  { 0, -3, 6,  "chan b oct", NULL, settings::STORAGE_TYPE_I8 },
  { 0, -3, 6, "chan c oct", NULL, settings::STORAGE_TYPE_I8 },
  { 0, -3, 6, "chan d oct", NULL, settings::STORAGE_TYPE_I8 },
  { 0, 0, 1, "long L", l_options, settings::STORAGE_TYPE_I8 },
  {  0,  0, 98, "active chord", NULL, settings::STORAGE_TYPE_I8 },   // FPART_SETTING_ACTIVECHORD
  { 10101010, 10101010, 32323232, "chord0 int", NULL, settings::STORAGE_TYPE_U32 },// FPART_SETTING_CHORD0
  { 10101010, 10101010, 32323232, "chord1 int", NULL, settings::STORAGE_TYPE_U32 },// FPART_SETTING_CHORD1
  { 10101010, 10101010, 32323232, "chord2 int", NULL, settings::STORAGE_TYPE_U32 },// ...2
  { 10101010, 10101010, 32323232, "chord3 int", NULL, settings::STORAGE_TYPE_U32 },// 3
  { 10101010, 10101010, 32323232, "chord4 int", NULL, settings::STORAGE_TYPE_U32 },// 4
  { 10101010, 10101010, 32323232, "chord5 int", NULL, settings::STORAGE_TYPE_U32 },// 5
  { 10101010, 10101010, 32323232, "chord6 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord7 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord8 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord9 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord10 int", NULL, settings::STORAGE_TYPE_U32 },// 10
  { 10101010, 10101010, 32323232, "chord11 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord12 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord13 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord14 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord15 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord16 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord17 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord18 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord19 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord20 int", NULL, settings::STORAGE_TYPE_U32 },// 20
  { 10101010, 10101010, 32323232, "chord21 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord22 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord23 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord24 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord25 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord26 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord27 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord28 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord29 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord30 int", NULL, settings::STORAGE_TYPE_U32 },// 30
  { 10101010, 10101010, 32323232, "chord31 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord32 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord33 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord34 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord35 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord36 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord37 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord38 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord39 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord40 int", NULL, settings::STORAGE_TYPE_U32 },// 40
  { 10101010, 10101010, 32323232, "chord41 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord42 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord43 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord44 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord45 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord46 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord47 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord48 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord49 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord50 int", NULL, settings::STORAGE_TYPE_U32 },// 50
  { 10101010, 10101010, 32323232, "chord51 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord52 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord53 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord54 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord55 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord56 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord57 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord58 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord59 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord60 int", NULL, settings::STORAGE_TYPE_U32 },// 60
  { 10101010, 10101010, 32323232, "chord61 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord62 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord63 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord64 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord65 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord66 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord67 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord68 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord69 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord70 int", NULL, settings::STORAGE_TYPE_U32 },// 70
  { 10101010, 10101010, 32323232, "chord71 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord72 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord73 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord74 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord75 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord76 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord77 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord78 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord79 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord80 int", NULL, settings::STORAGE_TYPE_U32 },// 80
  { 10101010, 10101010, 32323232, "chord81 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord82 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord83 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord84 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord85 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord86 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord87 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord88 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord89 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord90 int", NULL, settings::STORAGE_TYPE_U32 },// 90
  { 10101010, 10101010, 32323232, "chord91 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord92 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord93 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord94 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord95 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord96 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord97 int", NULL, settings::STORAGE_TYPE_U32 },
  { 10101010, 10101010, 32323232, "chord98 int", NULL, settings::STORAGE_TYPE_U32 } // 98 / last chord slot
};

// create an instance of the main app class
Fpart fpart_instance;

// required global functions
void FPART_init() {
  fpart_instance.cursor.Init(FPART_SETTING_ROOT, FPART_SETTING_LAST - 1);
  fpart_instance.Init();
}

static constexpr size_t FPART_storageSize() {
  return Fpart::storageSize();
}

static size_t FPART_save(void *storage) {
  return fpart_instance.Save(storage);
}

static size_t FPART_restore(const void *storage) {
  return fpart_instance.Restore(storage);
}

void FPART_isr() {
  // call class's isr function
  fpart_instance.ISR();
  //first the digital inputs (clocks/triggers)
  uint32_t triggers = OC::DigitalInputs::clocked();
  int current_chord = fpart_instance.get_activechord();
  if (triggers & (1 << OC::DIGITAL_INPUT_1)) { //input_1 means decrement chord
    if (current_chord > fpart_instance.get_loopstart()){ //assuming it's not at loopstart
      fpart_instance.set_activechord(current_chord - 1);
    }
    else { //go to loopend
      fpart_instance.set_activechord(fpart_instance.get_loopend());
    }
  }
  if (triggers & (1 << OC::DIGITAL_INPUT_2)) { //input_2 means increment chord
    if (current_chord < fpart_instance.get_loopend()) { //assuming it's not at loopend
      fpart_instance.set_activechord(current_chord + 1);
    }
    else { //go to loopstart
      fpart_instance.set_activechord(fpart_instance.get_loopstart());
    }
  }
  if (triggers & (1 << OC::DIGITAL_INPUT_3)) { //input_3 means go straight to loopstart
    fpart_instance.set_activechord(fpart_instance.get_loopstart());
  }
  if (triggers & (1 << OC::DIGITAL_INPUT_4)) { //input_4 means go straight to loopend
    fpart_instance.set_activechord(fpart_instance.get_loopend());
  }
  
  // TODO: these work but don't seem to get you to the final possible chords
  //now the CV inputs
  int32_t adc1_value = OC::ADC::raw_pitch_value(ADC_CHANNEL_1);
  if (adc1_value < 90) adc1_value = 0; 
  int cv1_index = adc1_value / (7800 / 99); // Chysn suggests 7800 is the highest value the ADC method used will return
  cv1_index = constrain(cv1_index, 0, 98);
  if (cv1_index != fpart_instance.last_cv1_index) {
    fpart_instance.last_cv1_index = cv1_index;
    fpart_instance.set_activechord(cv1_index);
  }
  
  int32_t adc2_value = OC::ADC::raw_pitch_value(ADC_CHANNEL_2);
  if (adc2_value < 90) adc2_value = 0;
  int loop_length = fpart_instance.get_loopend() - fpart_instance.get_loopstart();
  loop_length = constrain(loop_length, 0, fpart_instance.get_loopend());
  int cv2_index = adc2_value / (7800 / loop_length); // Chysn suggests 7800 is the highest value the ADC method used will return
  cv2_index = constrain(cv2_index, 0, loop_length);
  cv2_index += fpart_instance.get_loopstart(); //offset it by loop start amount
  if (cv2_index != fpart_instance.last_cv2_index) {
    fpart_instance.last_cv2_index = cv2_index;
    fpart_instance.set_activechord(cv2_index);
  }
}

// has to be here, but currently unused
void FPART_handleAppEvent(OC::AppEvent event) {
  //if (event ==  OC::APP_EVENT_RESUME) {
  //break;
}

void FPART_loop() {
  //FPART_updateChannels();
}

void FPART_menu() {
  if (fpart_instance.get_menu_page() == FPART_MENU_STAFFS) {
    // draw two staffs
    for (int s = 0; s < 11; s++) // the screen fits two staffs and middle C line, i.e. 11 lines
    {
      int y = 7 + (5 * s); // initial offset plus distance between lines
      if (s == 5) { // middle C row
        //graphics.drawHLineDots(5, y, 64); //could use this to draw the middle C line
        continue;
      }
      graphics.drawLine(8, y, 114, y);
    }
    //vertical lines to indicate active chord on staffs
    graphics.drawVLine(40,0,64);
    graphics.drawVLine(52,0,64);
    //prepare to draw note symbols
    //x coordinates for chord columns. Note: changing active_x entails changing the box/VLines.
    int prior_x = 12;
    int active_x = 44;
    int next_x = 75;
    int last_x = 106;
    //y coordinates for each of the possible note locations; inconsistent but carefully chosen for clear presentation
    int y[23] = {2, 5, 7, 10, 12, 15, 17, 20, 22, 25, 27, 30, 32, 35, 37, 40, 42, 45, 47, 50, 52, 55, 57};
    int chordID = fpart_instance.get_activechord();
    if (chordID > 0) { //if there could be a prior chord, get and display its values
      int chord_int = fpart_instance.get_chord_int(fpart_instance.get_activechord() -1);
      int a_note = fpart_instance.get_note_from_chord_int(chord_int, 0) - 10; //subtract 10 to scale notes back down to 0-22 values expected
      int b_note = fpart_instance.get_note_from_chord_int(chord_int, 1) - 10;
      int c_note = fpart_instance.get_note_from_chord_int(chord_int, 2) - 10;
      int d_note = fpart_instance.get_note_from_chord_int(chord_int, 3) - 10;
      fpart_instance.drawA(prior_x,y[a_note]);
      fpart_instance.drawB(prior_x,y[b_note]);
      fpart_instance.drawC(prior_x,y[c_note]);
      fpart_instance.drawD(prior_x,y[d_note]);
    }
    // get and display the active chord's values
    int chord_int = fpart_instance.get_chord_int(fpart_instance.get_activechord());
    int a_note = fpart_instance.get_note_from_chord_int(chord_int, 0) - 10;
    int b_note = fpart_instance.get_note_from_chord_int(chord_int, 1) - 10;
    int c_note = fpart_instance.get_note_from_chord_int(chord_int, 2) - 10;
    int d_note = fpart_instance.get_note_from_chord_int(chord_int, 3) - 10;
    fpart_instance.drawA(active_x,y[a_note]);
    fpart_instance.drawB(active_x,y[b_note]);
    fpart_instance.drawC(active_x,y[c_note]);
    fpart_instance.drawD(active_x,y[d_note]);
    if (chordID <= 99 - 2 ) {  //if there could be a next chord, get and display its values
      int chord_int = fpart_instance.get_chord_int(fpart_instance.get_activechord() + 1);
      int a_note = fpart_instance.get_note_from_chord_int(chord_int, 0) - 10;
      int b_note = fpart_instance.get_note_from_chord_int(chord_int, 1) - 10;
      int c_note = fpart_instance.get_note_from_chord_int(chord_int, 2) - 10;
      int d_note = fpart_instance.get_note_from_chord_int(chord_int, 3) - 10;
      fpart_instance.drawA(next_x,y[a_note]);
      fpart_instance.drawB(next_x,y[b_note]);
      fpart_instance.drawC(next_x,y[c_note]);
      fpart_instance.drawD(next_x,y[d_note]);
    }
    if (chordID <= 99 - 3 ) { //if there could be a next-next chord, get and display its values
      int chord_int = fpart_instance.get_chord_int(fpart_instance.get_activechord() + 2);
      int a_note = fpart_instance.get_note_from_chord_int(chord_int, 0) - 10;
      int b_note = fpart_instance.get_note_from_chord_int(chord_int, 1) - 10;
      int c_note = fpart_instance.get_note_from_chord_int(chord_int, 2) - 10;
      int d_note = fpart_instance.get_note_from_chord_int(chord_int, 3) - 10;
      fpart_instance.drawA(last_x,y[a_note]);
      fpart_instance.drawB(last_x,y[b_note]);
      fpart_instance.drawC(last_x,y[c_note]);
      fpart_instance.drawD(last_x,y[d_note]);
    }
    //draw a number to indicate current step
    graphics.setPrintPos(116, 9); // any further right and some 2-digit values go to next line
    graphics.print(chordID);
    //draw L-button setting
    graphics.setPrintPos(116, 19);
    graphics.print("L");
    graphics.setPrintPos(122, 19);
    if (fpart_instance.get_l_option() == 1) {
      graphics.print(0);
    } else if (fpart_instance.copy) {
      graphics.print("C");
    } else {
      graphics.print("P");
    }
    //draw output-selection indicators
    graphics.setPrintPos(116, 39);
    if (fpart_instance.a_or_b) {
      graphics.print("1");
      fpart_instance.drawA(123, 40);
    } else { 
      graphics.print("2");
      fpart_instance.drawB(123, 40);
    }
    graphics.setPrintPos(116, 49);;
    if (fpart_instance.c_or_d) {
      graphics.print("3");
      fpart_instance.drawC(123, 50);
    } else {
      graphics.print("4");
      fpart_instance.drawD(123, 50);
    }
  } else if (fpart_instance.get_menu_page() == FPART_MENU_PARAMETERS) { //draw the settings
	  menu::DefaultTitleBar::Draw();
      graphics.print("4 Parts");
      menu::SettingsList<menu::kScreenLines, 0, menu::kDefaultValueX - 1> settings_list(fpart_instance.cursor);
      menu::SettingsListItem list_item;
      while (settings_list.available())
      {
          const int current = settings_list.Next(list_item);
          const int value = fpart_instance.get_value(current);
          if (current < 10) { // draw settings 0-9, i.e. don't show chord ints
            list_item.DrawDefault(value, Fpart::value_attr(current));
          } else {
            break;
          }
      }
  }
}

// define what should happen when the screensaver is activated
void FPART_screensaver() {
  if (fpart_instance.get_menu_page() == FPART_MENU_PARAMETERS) { //go to staffs
    fpart_instance.set_menu_page(FPART_MENU_STAFFS);
  }
  FPART_menu(); //this effectively blocks the screensaver; good while composing, but bad for screen
}

// direct button presses to correct functions
void FPART_handleButtonEvent(const UI::Event &event) {
  if (UI::EVENT_BUTTON_LONG_PRESS == event.type) { //long presses
     switch (event.control) {
       case OC::CONTROL_BUTTON_DOWN:
         FPART_downButtonLong();
         break;
       case OC::CONTROL_BUTTON_L:
         FPART_leftButtonLong();
         break;  
     }
  }
  if (UI::EVENT_BUTTON_PRESS == event.type) { //short presses
    switch (event.control) {
      case OC::CONTROL_BUTTON_UP:
        FPART_upButton();
        break;
      case OC::CONTROL_BUTTON_DOWN:
        FPART_downButton();
        break;
      case OC::CONTROL_BUTTON_L:
        FPART_leftButton();
        break;
      case OC::CONTROL_BUTTON_R:
        FPART_rightButton();
        break;
    }
  } 
}

// handle encoder turns
void FPART_handleEncoderEvent(const UI::Event &event) {
  if (fpart_instance.get_menu_page() == FPART_MENU_STAFFS) { //staffs page
    int chordID = fpart_instance.get_activechord();
    int chord_int = fpart_instance.get_chord_int(chordID);
    int a_note = fpart_instance.get_note_from_chord_int(chord_int, 0) - 10; //subtract 10 to scale notes back down to 0-22 values expected
    int b_note = fpart_instance.get_note_from_chord_int(chord_int, 1) - 10;
    int c_note = fpart_instance.get_note_from_chord_int(chord_int, 2) - 10;
    int d_note = fpart_instance.get_note_from_chord_int(chord_int, 3) - 10;
    if (OC::CONTROL_ENCODER_L == event.control) {
      if (event.value > 0) { // increment A or B value depending on what is selected
        if (fpart_instance.a_or_b) { //if channel A
          if (a_note < 22) {
            ++a_note;
          }
        }
        else { // channel B
          if (b_note < 22) {
            ++b_note;
          }
        }
      } else if (event.value < 0) { // decrement A or B value
        if (fpart_instance.a_or_b) { // channel A
          if (a_note > 0) {
            --a_note;
          }
        }
        else { // channel B
          if (b_note > 0) {
            --b_note;
          }
        }
      }
    } else if (OC::CONTROL_ENCODER_R == event.control) { // now the same for channels C and D
      if (event.value > 0) { //increment C or D
        if (fpart_instance.c_or_d) {
          if (c_note < 22) {
            ++c_note;
          }
        }
        else {
          if (d_note < 22) {
            ++d_note;
          }
        }
      } else if (event.value < 0) { // decrement C or D
        if (fpart_instance.c_or_d) {
          if (c_note > 0) {
            --c_note;
          }
        }
        else {
          if (d_note > 0) {
            --d_note;
          }
        }
      }
    }
    //save the adjustment: build the chord integer,  store it in the right place
    int new_chord_int = fpart_instance.build_chord_int(a_note, b_note, c_note, d_note);
    fpart_instance.set_chord_int(chordID, new_chord_int);
  } else if (fpart_instance.get_menu_page() == FPART_MENU_PARAMETERS) { //on menu page, encoder scrolls through setting names/values
      if (OC::CONTROL_ENCODER_R == event.control) {
        if (fpart_instance.cursor.editing()) { //scroll setting values
          fpart_instance.change_value(fpart_instance.cursor.cursor_pos(), event.value);
        } else { // TODO: stop user scrolling past last drawn setting
            fpart_instance.cursor.Scroll(event.value); //scroll setting names (i.e. rows)
        }
      } else if (OC::CONTROL_ENCODER_L == event.control){
        //left encoder behaviour in settings menu?
      }
  }
}

void FPART_upButton() {
  if (fpart_instance.get_menu_page() == FPART_MENU_STAFFS) { //staff page, down a chord
    int chordID = fpart_instance.get_activechord();
    if (chordID > 0){
      --chordID;
      fpart_instance.set_activechord(chordID);
    }
    else {
      fpart_instance.set_activechord(98); //go to last chord
    }
  }
  else {
    //settings menu, scroll cursor up?
  }
}

void FPART_downButton() {
  if (fpart_instance.get_menu_page() == FPART_MENU_STAFFS) {  //staff page, up a chord
    int chordID = fpart_instance.get_activechord();
    if (chordID < 98) {
      ++chordID;
      fpart_instance.set_activechord(chordID);
    }
    else {
      fpart_instance.set_activechord(0); //go to first chord
    }
  } else {
    //settings menu, scroll cursor down?
  }
}

void FPART_leftButton() {
  if (fpart_instance.get_menu_page() == FPART_MENU_STAFFS) { //staff page, toggle channels A/B
    fpart_instance.a_or_b = !fpart_instance.a_or_b;
  } 
  else {
    //menu stuff?
  }
}

void FPART_rightButton() {
  if (fpart_instance.get_menu_page() == FPART_MENU_STAFFS) {
    fpart_instance.c_or_d = !fpart_instance.c_or_d; //staff page, toggle channels C/D
  } 
  else if (fpart_instance.get_menu_page() == FPART_MENU_PARAMETERS) { //menu page, toggle editing
    fpart_instance.cursor.toggle_editing();
  }
}

void FPART_leftButtonLong() {
  if (fpart_instance.get_menu_page() == FPART_MENU_STAFFS) { //staff page, jump to first chord
    if (fpart_instance.get_l_option() == 1) {
      fpart_instance.set_activechord(0);
    } else if (fpart_instance.copy) {
      fpart_instance.copyChord();
      fpart_instance.copy = !fpart_instance.copy;
    } else {
      fpart_instance.pasteChord();
      fpart_instance.copy = !fpart_instance.copy;
    }
  } 
  else {
    //do menu stuff?
  }
}

void FPART_downButtonLong() { // toggle between settings/staff pages
  uint8_t _menu_page = fpart_instance.get_menu_page();
  if (_menu_page == FPART_MENU_PARAMETERS) {
    fpart_instance.set_menu_page(FPART_MENU_STAFFS);
  }
  else { fpart_instance.set_menu_page(FPART_MENU_PARAMETERS); }
}

#endif // ENABLE_APP_FPART
