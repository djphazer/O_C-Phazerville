// Copyright (c)  2021 Naomi Seyfer
//
// Author of original O+C firmware: Max Stadler (mxmlnstdlr@gmail.com)
// Author of app scaffolding: Patrick Dowling (pld@gurkenkiste.com)
// Quantizer code: Emilie Gillet
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

#ifdef ENABLE_APP_PASSENCORE

#include <algorithm>

#include "OC_apps.h"
#include "util/util_settings.h"
#include "util/util_trigger_delay.h"
#include "braids_quantizer.h"
#include "braids_quantizer_scales.h"
#include "OC_menus.h"
#include "OC_scales.h"
#include "OC_scale_edit.h"
#include "OC_strings.h"
#include "OC_chords.h"
#include "OC_chords_edit.h"
#include "OC_input_map.h"
#include "OC_input_maps.h"

#define POSSIBLE_LEN 32

enum PASSENCORE_SETTINGS {
  PASSENCORE_SETTING_SCALE,
  PASSENCORE_SETTING_MASK,
  PASSENCORE_SETTING_SAMPLE_TRIGGER,
  PASSENCORE_SETTING_TARGET_TRIGGER,
  PASSENCORE_SETTING_PASSING_TRIGGER,
  PASSENCORE_SETTING_RESET_TRIGGER,
  PASSENCORE_SETTING_A_OCTAVE,
  PASSENCORE_SETTING_A_MIDRANGE,
  PASSENCORE_SETTING_B_OCTAVE,
  PASSENCORE_SETTING_B_MIDRANGE,
  PASSENCORE_SETTING_C_OCTAVE,
  PASSENCORE_SETTING_C_MIDRANGE,
  PASSENCORE_SETTING_D_OCTAVE,
  PASSENCORE_SETTING_D_MIDRANGE,
  PASSENCORE_SETTING_BORROW_CHORDS,
  PASSENCORE_SETTING_BASE_COLOR,
  PASSENCORE_SETTING_CV3_ROLE,
  PASSENCORE_SETTING_CV4_ROLE,
  PASSENCORE_SETTING_LAST
};

enum PASSENCORE_FUNCTIONS {
  PASSENCORE_FUNCTIONS_TONIC,
  PASSENCORE_FUNCTIONS_TONIC_PREDOMINANT,
  PASSENCORE_FUNCTIONS_PREDOMINANT,
  PASSENCORE_FUNCTIONS_PREDOMINANT_DOMINANT,
  PASSENCORE_FUNCTIONS_DOMINANT,
  PASSENCORE_FUNCTIONS_MEDIANT,
  PASSENCORE_FUNCTIONS_LAST,
};

enum PASSENCORE_COLORS {
  PASSENCORE_COLORS_POWER,
  PASSENCORE_COLORS_CLASSIC,
  PASSENCORE_COLORS_EXTENDED,
  PASSENCORE_COLORS_SUBSTITUTED,
  PASSENCORE_COLORS_JAZZ,
  PASSENCORE_COLORS_LAST,
};

const char* const passencore_color_names[] = {"power", "classic", "interesting", "ext/subst", "iono jazz?"};

enum PASSENCORE_CV_ROLES {
  PASSENCORE_CV_ROLE_NONE,
  PASSENCORE_CV_ROLE_INCLUDE,
  PASSENCORE_CV_ROLE_ROOT,
  PASSENCORE_CV_ROLE_BASS,
  PASSENCORE_CV_ROLE_LAST,
  // Future.
  PASSENCORE_CV_ROLE_MELODY,
  PASSENCORE_CV_ROLE_CHROMATIC_TRANSPOSE,
};

const char* const passencore_cv_role_names[] = {"-", "include", "root", "bass"};

enum PASSENCORE_MELODY_QUANT {
  PASSENCORE_MELODY_QUANT_ACTIVE_SCALE,
  PASSENCORE_MELODY_QUANT_TONIC_PENTATONIC,
  PASSENCORE_MELODY_QUANT_CHORD_PENTATONIC,
  PASSENCORE_MELODY_QUANT_MODAL_TRANSPOSE,
  PASSENCORE_MELODY_QUANT_LAST,
};

PASSENCORE_FUNCTIONS PASSENCORE_FUNCTION_TABLE[] = {
  PASSENCORE_FUNCTIONS_TONIC,
  PASSENCORE_FUNCTIONS_PREDOMINANT_DOMINANT,
  PASSENCORE_FUNCTIONS_MEDIANT,
  PASSENCORE_FUNCTIONS_PREDOMINANT,
  PASSENCORE_FUNCTIONS_DOMINANT,
  PASSENCORE_FUNCTIONS_TONIC_PREDOMINANT,
  PASSENCORE_FUNCTIONS_DOMINANT,
};

enum PASSENCORE_CHORD_TYPES {
  CHORD_TYPES_NONE,
  CHORD_TYPES_MAJOR,
  CHORD_TYPES_MINOR,
  CHORD_TYPES_DIMINISHED,
  CHORD_TYPES_AUGMENTED,
  CHORD_TYPES_LAST,
};


struct PassenChord {
  // 1-indexed scale degree
  int8_t root;
  // In order of importance to the chord. 1-indexed from the root, ex [0, 2, 4, 7] is a triad w/8ve
  int8_t intervals[4];

  int8_t accidental;

  // Attributes to score by; mutable.
  PASSENCORE_FUNCTIONS function;
  PASSENCORE_COLORS color;

  // Fitness; mutable; we sort by this.
  int32_t score;

  int32_t samples[4];

  PASSENCORE_CHORD_TYPES triad_type;
  PASSENCORE_CHORD_TYPES seventh_type;
  PASSENCORE_CHORD_TYPES ninth_type;

  void print() {
    int acc = accidental;
    CONSTRAIN(acc, -2, 2);
    const char* accidental_part = OC::Strings::accidentals[2 + acc];
    int r = root;
    CONSTRAIN(r, 0, 7);
    const char* degree_part = (triad_type == CHORD_TYPES_MINOR || triad_type == CHORD_TYPES_DIMINISHED) ? OC::Strings::scale_degrees_min[root - 1] : OC::Strings::scale_degrees_maj[root - 1];
    char aug_dim = ' ';
    if (triad_type == CHORD_TYPES_DIMINISHED) {
      aug_dim = 0xb0;
    } else if (triad_type == CHORD_TYPES_AUGMENTED) {
      aug_dim = '+';
    }
    graphics.printf("%s%s%c", accidental_part, degree_part, aug_dim);
    if (triad_type == CHORD_TYPES_NONE) {
      graphics.print("sus");
    }
    switch (seventh_type) {
      case CHORD_TYPES_NONE:
        break;
      case CHORD_TYPES_MINOR:
        graphics.print("7");
        break;
      case CHORD_TYPES_MAJOR:
        graphics.print("maj7");
        break;
      case CHORD_TYPES_DIMINISHED:
        graphics.print("dim7");
        break;
      case CHORD_TYPES_AUGMENTED:
        graphics.print("aug7");
        break;
      case CHORD_TYPES_LAST:
        break;
    }
    switch (ninth_type) {
      case CHORD_TYPES_NONE:
        break;
      case CHORD_TYPES_MINOR:
        graphics.print("b9");
        break;
      case CHORD_TYPES_MAJOR:
        graphics.printf("add9");
        break;
      case CHORD_TYPES_DIMINISHED:
        graphics.printf("dim9");
        break;
      case CHORD_TYPES_AUGMENTED:
        graphics.printf("#9");
        break;
      case CHORD_TYPES_LAST:
        break;
    }
  }

  void print_notes() {
        for (int i = 0; i < 4; i++) {
      int tone = (samples[i]>>7)%12;
      if (tone < 0) tone += 12;
      int octave = (samples[i]>>7)/12;
      graphics.print(OC::Strings::note_names_unpadded[tone]);
      graphics.pretty_print(octave);
    }
  }
};

using OC::DUMMY;

class PASSENCORE : public settings::SettingsBase<PASSENCORE, PASSENCORE_SETTING_LAST> {
  public:
    int get_scale(uint8_t selected_scale_slot_) const {
      return values_[PASSENCORE_SETTING_SCALE];
    }
    uint16_t get_mask() const {
      return values_[PASSENCORE_SETTING_MASK];
    }

    // Wrappers for ScaleEdit
    void scale_changed() {
      force_update_ = true;
    }

    int get_root(uint8_t DUMMY) const {
      return 0x0;
    }
    // dummy
    int get_scale_select() const {
      return 0;
    }

    // dummy
    void set_scale_at_slot(int scale, uint16_t mask, int root, int transpose, uint8_t scale_slot) {

    }

    // dummy
    int get_transpose(uint8_t DUMMY) const {
      return 0;
    }
    uint16_t get_scale_mask(uint8_t scale_select) const {
      return get_mask();
    }

    void update_scale_mask(uint16_t mask, uint8_t scale_select) {
      apply_value(PASSENCORE_SETTING_MASK, mask);
      last_mask_ = mask;
      force_update_ = true;
    }

    void set_scale(int scale) {

      if (scale != get_scale(DUMMY)) {
        const OC::Scale &scale_def = OC::Scales::GetScale(scale);
        uint16_t mask = get_mask();
        if (0 == (mask & ~(0xffff << scale_def.num_notes)))
          mask = 0xffff;
        apply_value(PASSENCORE_SETTING_MASK, mask);
        apply_value(PASSENCORE_SETTING_SCALE, scale);
      }
    }

    void Init() {
      last_mask_ = 0;
      last_scale_ = -1;
      set_scale((int)OC::Scales::SCALE_SEMI + 1);
      quantizer_.Init();
      update_scale(true, false);
      // start the playing chord on somethig we can start leading pleasantly from
      add_chord(5, 1, 3, 5, 8, PASSENCORE_FUNCTIONS_DOMINANT, PASSENCORE_COLORS_CLASSIC);
      voice_basic(possibilities[0]);
      soundingChord = targetChord = possibilities[0];
    }

    void ISR();

    void Loop();

    void Draw();

  private:
    braids::Quantizer quantizer_;
    PassenChord possibilities[POSSIBLE_LEN];
    int8_t p_len;
    PassenChord soundingChord;
    PassenChord targetChord;
    int8_t function;
    int8_t color;
    int last_scale_;
    uint16_t last_mask_;
    bool force_update_;

    bool calc_new_chord_;
    bool play_target_;
    bool play_passing_chord_;
    bool reset_;

    int32_t sample_a, sample_b, sample_c, sample_d;

    void reset_possibilities() {
      p_len = 0;
    }

    void find_new_chord();
    void add_variants();
    void add_basic_triads();
    void take_top_four();
    void score_by_function();
    void score_by_root();
    void score_by_include();
    void score_by_bass();
    void score_by_color();
    void add_voicings();
    void score_voicings();
    void add_passing_chords();
    void score_passing_voicing();
    void find_passing_chord();
    int score_voicing(const PassenChord& from, const PassenChord& to);
    void voice(PassenChord& target, const PassenChord& previous, uint8_t leading_mask, bool force_bass_movement, bool constant);
    void voice_basic(PassenChord& target);

    void add_chord(int8_t root, int8_t i1, int8_t i2, int8_t i3, int8_t i4, PASSENCORE_FUNCTIONS function, PASSENCORE_COLORS color);
    void play_chord(PassenChord& chord) {
      for (int i = 0; i < 4; i++) {
        OC::DAC::set_pitch((DAC_CHANNEL)i, chord.samples[i], 0);
      }
      soundingChord = chord;
    }

    bool update_scale(bool force, int32_t mask_rotate) {

      force_update_ = false;
      const int scale = get_scale(DUMMY);
      uint16_t mask = get_mask();

      if (mask_rotate)
        mask = OC::ScaleEditor<PASSENCORE>::RotateMask(mask, OC::Scales::GetScale(scale).num_notes, mask_rotate);

      if (force || (last_scale_ != scale || last_mask_ != mask)) {
        last_scale_ = scale;
        last_mask_ = mask;
        quantizer_.Configure(OC::Scales::GetScale(scale), mask);
        return true;
      } else {
        return false;
      }
    }
};

// Returns the version of `sample` less than an octave above `comparison`
int32_t nearest_above(int32_t sample, int32_t comparison) {
  int octaves_above = (sample - comparison) / (12 << 7);
  return sample - octaves_above * (12 << 7);
}

int32_t nearest_below(int32_t sample, int32_t comparison) {
  int32_t above = nearest_above(sample, comparison);
  if (above == sample) return above;
  return above - (12 << 7);
}

int32_t nearest_note(int32_t sample, int32_t comparison) {
  int32_t above = nearest_above(sample, comparison);
  int32_t below = nearest_below(sample, comparison);
  return (abs(comparison - above) < abs(comparison - below)) ? above : below;
}

void PASSENCORE::voice_basic(PassenChord& target) {
  for (int i = 0; i < 4; i++) {
    target.samples[i] = nearest_note(
                          target.samples[i],
                          (values_[PASSENCORE_SETTING_A_OCTAVE + 2 * i] + values_[PASSENCORE_SETTING_A_MIDRANGE + 2 * i]) << 7
                        );
  }
}

// Voice the target with reference to previous, using the leading mask to determine whether
// the voices should go up or down from previous - 0 is down, 1 is up, lsb is bass.
void PASSENCORE::voice(PassenChord& target, const PassenChord& previous, uint8_t leading_mask, bool force_bass_movement, bool constant) {
  for (int v = 0; v < 4; v++) {
    bool up = (leading_mask >> v) & 0x1;
    // Look at voices we have not assigned yet, find the one that's nearest in the given direction to the same voice in the prev. chord.
    int32_t best_score = 12 << 7;
    int best_p = 0;
    int32_t note, best_note = 0;
    for (int p = v; constant ? (p == v) : (p < 4); p++) {
      if (constant) {
        note = nearest_note(target.samples[p], previous.samples[v]);
      } else if (up) {
        note = nearest_above(target.samples[p], previous.samples[v]);
      } else {
        note = nearest_below(target.samples[p], previous.samples[v]);
      }
      int score = abs(note - previous.samples[v]);
      if (v == 0 && force_bass_movement && score == 0) score = (12 << 7);
      if (score < best_score) {
        best_score = score;
        best_p = p;
        best_note = note;
      }
    }
    // Now swap the best note into place.
    target.samples[best_p] = target.samples[v];
    target.samples[v] = best_note;
    std::swap(target.intervals[best_p], target.intervals[v]);
  }
}

uint8_t PASSENCORE_VOICINGS[] = {0b0011, 0b1100, 0b0101, 0b1010, 0b1001, 0b0110};

void PASSENCORE::add_voicings() {
  int len = p_len;
  for (int i = 0; i < len; i++) {
    for (int j = 0; j < 5; j++) {
      if (p_len >= POSSIBLE_LEN) break;
      possibilities[p_len] = possibilities[i];
      voice(possibilities[p_len], soundingChord, PASSENCORE_VOICINGS[j], j > 2, false);
      p_len++;
    }
    // voice the orig.
    voice(possibilities[i], soundingChord, PASSENCORE_VOICINGS[5], true, true);
  }
}

int PASSENCORE::score_voicing(const PassenChord& from, const PassenChord& to) {
  int score = 8;
  int held = 0;
  bool prefer_root = false;
  int32_t last_sample = -4 * (12 << 7);
  for (int v = 0; v < 4; v++) {
    if (to.intervals[v] > 8) {
      prefer_root = true;
    }
    int movement = (abs(to.samples[v] - from.samples[v])) >> 7;
    if (movement < 4) {
      score += 2;
    }
    // We like staying constant and half-steps
    if (movement == 0) {
      score += 1;
      held += 1;
    }
    if (movement == 1) {
      score += 2;
    }
    if (v == 1 || v == 2) {
      if (movement > 4) score -= 1;
      if (movement > 8) score -= (movement - 8);
    } else {
      // Soprano and Bass can move more
      if (movement > 7) score -= 1;
      if (movement > 12) score -= (movement - 12);
    }
    // Gently keep our voices in range.
    int32_t octave = values_[PASSENCORE_SETTING_A_OCTAVE + 2 * v];
    int32_t midrange = values_[PASSENCORE_SETTING_A_MIDRANGE + 2 * v];
    int32_t center = (12 * octave + midrange);
    int32_t distance = abs((to.samples[v] >> 7) - center);

    if (distance > 8) score -= (distance - 8);
    if (distance > 12) score -= (distance - 12);

    // penalize neighboring half-steps in the same chord. This should eliminate "false" sus chords.
    if ((abs(last_sample - to.samples[v]) >> 7) == 1) {
      score -= 10;
    }
    last_sample = to.samples[v];
  }
  // penalize holding the same chord by the voicing score it got for holding those notes.
  if (held == 4) {
    score -= 12;
  }
  if (to.intervals[0] > 5) {
    // avoid third inversions.
    score -= 3;
  }
  if (to.intervals[0] == 1) {
    // prefer root position, but not strongly
    // score += 1;
  } else if (prefer_root) {
    score -= 3;
  }
  return score;
}

void PASSENCORE::score_voicings() {
  for (int c = 0; c < p_len; c++) {
    int score = score_voicing(soundingChord, possibilities[c]);
    possibilities[c].score *= score;
  }
}

void PASSENCORE::Draw() {
  graphics.setPrintPos(0, 0);
  for (int i = 0; i <= function; i++) {
    graphics.print("**");
  }
  graphics.setPrintPos(20, 10);
  soundingChord.print();
  graphics.setPrintPos(20, 20);
  soundingChord.print_notes();
  graphics.setPrintPos(50, 30);
  graphics.print("to");
  graphics.setPrintPos(20, 40);
  targetChord.print();
  graphics.setPrintPos(50, 50);
  graphics.print(passencore_color_names[color]);
  /*
    graphics.setPrintPos(0, 0);
    graphics.print("r");
    graphics.pretty_print(possibilities[0].root);
    graphics.print("f");
    graphics.pretty_print(function);
    graphics.print("c");
    graphics.pretty_print(color);
    graphics.pretty_print(p_len);
    graphics.setPrintPos(0, 10);
    graphics.print("r");
    graphics.pretty_print(soundingChord.root);
    //graphics.print("f");
    //graphics.pretty_print(sounding_chord.function);
    graphics.print("c");
    graphics.pretty_print(soundingChord.color);
    graphics.pretty_print(soundingChord.intervals[0]);
    graphics.pretty_print(soundingChord.intervals[1]);
    graphics.pretty_print(soundingChord.intervals[2]);
    graphics.pretty_print(soundingChord.intervals[3]);
    graphics.print("s");
    graphics.pretty_print(soundingChord.score);
    for (int i = 1; i < 5; i++) {
    graphics.setPrintPos(0, (i + 1) * 10);
    graphics.print("r");
    graphics.pretty_print(possibilities[i].root);
    //graphics.print("f");
    //graphics.pretty_print(possibilities[i].function);
    graphics.print("c");
    graphics.pretty_print(possibilities[i].color);
    graphics.pretty_print(possibilities[i].intervals[0]);
    graphics.pretty_print(possibilities[i].intervals[1]);
    graphics.pretty_print(possibilities[i].intervals[2]);
    graphics.pretty_print(possibilities[i].intervals[3]);
    graphics.print("s");
    graphics.pretty_print(possibilities[i].score);

    }
  */
}

void PASSENCORE::Loop() {
}

void PASSENCORE::find_passing_chord() {
  add_passing_chords();
  for (int i = 0; i < p_len; i++) {
    possibilities[i].score = 1;
  }
  score_by_color();
  take_top_four();
  add_voicings();
  score_passing_voicing();
  take_top_four();
}

int chromatic_tone(int32_t sample) {
  int ret = (sample>>7)%12;
  if (ret < 0) {
    return ret + 12;
  }
  return ret;
}

void PASSENCORE::score_by_root() {
  bool root = false;
  int root_sample = 0;

  if (values_[PASSENCORE_SETTING_CV3_ROLE] == PASSENCORE_CV_ROLE_ROOT) {
    root = true;
    root_sample = chromatic_tone(quantizer_.Process(OC::ADC::raw_pitch_value(ADC_CHANNEL_3), 0, 0));
  } else if (values_[PASSENCORE_SETTING_CV4_ROLE] == PASSENCORE_CV_ROLE_ROOT) {
    root = true;
    root_sample = chromatic_tone(quantizer_.Process(OC::ADC::raw_pitch_value(ADC_CHANNEL_4), 0, 0));
  }

  for (int i = 0; i < p_len; i++) {
    if (root) {
      if (chromatic_tone(possibilities[i].samples[0]) == root_sample) {
        possibilities[i].score += 1;
      } else {
        possibilities[i].score = 0;
      }
    }
  }
}

void PASSENCORE::score_by_bass() {
  bool bass = false;
  int bass_sample = 0;

  if (values_[PASSENCORE_SETTING_CV3_ROLE] == PASSENCORE_CV_ROLE_BASS) {
    bass = true;
    bass_sample = chromatic_tone(quantizer_.Process(OC::ADC::raw_pitch_value(ADC_CHANNEL_3), 0, 0));
  } else if (values_[PASSENCORE_SETTING_CV4_ROLE] == PASSENCORE_CV_ROLE_BASS) {
    bass = true;
    bass_sample = chromatic_tone(quantizer_.Process(OC::ADC::raw_pitch_value(ADC_CHANNEL_4), 0, 0));
  }

  for (int i = 0; i < p_len; i++) {
    if (bass) {
      if (chromatic_tone(possibilities[i].samples[0]) == bass_sample) {
        possibilities[i].score += 1;
      } else {
        possibilities[i].score = 0;
      }
    }
  }
}

void PASSENCORE::score_by_include() {
  bool include = false;
  int include_sample = 0;
  if (values_[PASSENCORE_SETTING_CV3_ROLE] == PASSENCORE_CV_ROLE_INCLUDE) {
    include = true;
    include_sample = chromatic_tone(quantizer_.Process(OC::ADC::raw_pitch_value(ADC_CHANNEL_3), 0, 0));
  } else if (values_[PASSENCORE_SETTING_CV4_ROLE] == PASSENCORE_CV_ROLE_INCLUDE) {
    include = true;
    include_sample = chromatic_tone(quantizer_.Process(OC::ADC::raw_pitch_value(ADC_CHANNEL_4), 0, 0));
  } else if (values_[PASSENCORE_SETTING_CV3_ROLE] == PASSENCORE_CV_ROLE_BASS) {
    include = true;
    include_sample = chromatic_tone(quantizer_.Process(OC::ADC::raw_pitch_value(ADC_CHANNEL_3), 0, 0));
  } else if (values_[PASSENCORE_SETTING_CV4_ROLE] == PASSENCORE_CV_ROLE_BASS) {
    include = true;
    include_sample = chromatic_tone(quantizer_.Process(OC::ADC::raw_pitch_value(ADC_CHANNEL_4), 0, 0));
  }

  for (int i = 0; i < p_len; i++) {
    bool found = false;
    if (include) {
      for (int j = 0; j < 4; j++) {
        if (chromatic_tone(possibilities[i].samples[j]) == include_sample) {
          possibilities[i].score += 1;
          found = true;
        }
      }
      if (!found) {
        possibilities[i].score = 0;
      }
    }
  }
}


void PASSENCORE::find_new_chord() {
  function = (int)((OC::ADC::value((ADC_CHANNEL)0) + 128) >> 9);
  CONSTRAIN(function, 0, PASSENCORE_FUNCTIONS_MEDIANT - 1);
  color = values_[PASSENCORE_SETTING_BASE_COLOR] + (int)((OC::ADC::value((ADC_CHANNEL)1) + 255) >> 9);
  CONSTRAIN(color, 0, PASSENCORE_COLORS_LAST - 1);

  reset_possibilities();
  add_basic_triads();
  score_by_function();
  score_by_root();
  take_top_four();
  add_variants();
  score_by_function();
  score_by_root();
  score_by_color();
  score_by_include();
  if (reset_) {
    score_by_bass();
    take_top_four();
    voice_basic(possibilities[0]);
    reset_ = false;
  } else {
    take_top_four();
    add_voicings();
    score_voicings();
    score_by_bass();
    take_top_four();
  }
  targetChord = possibilities[0];
}

void PASSENCORE::add_basic_triads() {
  for (int degree = 1; degree < 8; degree++) {
    add_chord(degree, 1, 3, 5, 8, PASSENCORE_FUNCTION_TABLE[degree - 1], PASSENCORE_COLORS_CLASSIC);
  }
}

void PASSENCORE::score_passing_voicing() {

  for (int i = 0; i < p_len; i++) {
    int to_passing = score_voicing(soundingChord, possibilities[i]);
    if (to_passing < 1) {
      to_passing = 1;
    }
    int from_passing = score_voicing(possibilities[i], targetChord);
    // Slightly nudge away from the same root as the target chord, since otherwise we'll only play sus chords as passing chords, they lead so well.
    if (possibilities[i].root == targetChord.root) {
      from_passing -= 2;
    }
    if (from_passing < 1) {
      from_passing = 1;
    }
    possibilities[i].score *= (to_passing * from_passing);
  }
}

void PASSENCORE::add_passing_chords() {
  p_len = 1;
  // sus variants of target, but at lower color
  add_chord(targetChord.root, 1, 2, 5, 8, targetChord.function, PASSENCORE_COLORS_CLASSIC);
  add_chord(targetChord.root, 1, 4, 5, 8, targetChord.function, PASSENCORE_COLORS_CLASSIC);
  add_chord(targetChord.root, 1, 3, 7, 9, targetChord.function, PASSENCORE_COLORS_EXTENDED);
  add_chord(targetChord.root, 1, 4, 7, 5, targetChord.function, PASSENCORE_COLORS_EXTENDED);
  // variants of the secondary dominant
  int secondary_dominant = (targetChord.root + 3) % 7 + 1;
  add_chord(secondary_dominant, 1, 3, 5, 8, PASSENCORE_FUNCTIONS_DOMINANT, PASSENCORE_COLORS_CLASSIC);
  add_chord(secondary_dominant, 1, 3, 5, 7, PASSENCORE_FUNCTIONS_DOMINANT, PASSENCORE_COLORS_EXTENDED);
  add_chord(secondary_dominant, 1, 4, 9, 7, PASSENCORE_FUNCTIONS_DOMINANT, PASSENCORE_COLORS_SUBSTITUTED);
  // Stepwise passing chord
  if (abs(soundingChord.root - targetChord.root) == 2) {
    add_chord((soundingChord.root + targetChord.root) / 2, 1, 3, 7, 5, PASSENCORE_FUNCTIONS_PREDOMINANT, PASSENCORE_COLORS_EXTENDED);
    add_chord((soundingChord.root + targetChord.root) / 2, 1, 3, 5, 8, PASSENCORE_FUNCTIONS_PREDOMINANT, PASSENCORE_COLORS_CLASSIC);
  } else if (values_[PASSENCORE_SETTING_BORROW_CHORDS] && abs(soundingChord.samples[0] - targetChord.samples[0]) == (2 << 7)) {
    int increment = (targetChord.samples[0] - soundingChord.samples[0]) / 2;
    possibilities[p_len] = soundingChord;
    possibilities[p_len].color = PASSENCORE_COLORS_JAZZ;
    possibilities[p_len].accidental = (increment >> 7);
    for (int i = 0; i < 4; i++) {
      possibilities[p_len].samples[i] += increment;
    }
    p_len++;
  }
  // Neighbor of the secondary dominant
  int neighbor_dominant = (secondary_dominant + 4) % 7 + 1;
  add_chord(neighbor_dominant, 1, 3, 5, 8, PASSENCORE_FUNCTIONS_PREDOMINANT_DOMINANT, PASSENCORE_COLORS_CLASSIC);
  add_chord(neighbor_dominant, 1, 3, 7, 5, PASSENCORE_FUNCTIONS_PREDOMINANT_DOMINANT, PASSENCORE_COLORS_SUBSTITUTED);
}

void PASSENCORE::add_variants() {
  int len = p_len;
  for (int i = 0; i < len; i++) {
    // Power chord
    add_chord(possibilities[i].root, 1, 5, 8, 12, possibilities[i].function, PASSENCORE_COLORS_POWER);
    // Don't resolve to a sus chord here; tonic sus chords can go as passing chords though
    if (possibilities[i].root != 1) {
      //Sus 2
      add_chord(possibilities[i].root, 1, 2, 5, 8, possibilities[i].function, PASSENCORE_COLORS_SUBSTITUTED);
      // Sus 4
      add_chord(possibilities[i].root, 1, 4, 5, 8, possibilities[i].function, PASSENCORE_COLORS_SUBSTITUTED);
    }
    // 7th
    add_chord(possibilities[i].root, 1, 3, 7, 5, possibilities[i].function, PASSENCORE_COLORS_EXTENDED);
    // 7th+9
    add_chord(possibilities[i].root, 1, 3, 7, 9, possibilities[i].function, PASSENCORE_COLORS_JAZZ);
    // 7th Sus 4
    add_chord(possibilities[i].root, 1, 4, 7, 5, possibilities[i].function, PASSENCORE_COLORS_JAZZ);

    if (possibilities[i].function == PASSENCORE_FUNCTIONS_DOMINANT) {
      // Dominant 9 sus 4
      add_chord(possibilities[i].root, 1, 4, 9, 7, possibilities[i].function, PASSENCORE_COLORS_JAZZ);
    }
  }
}


void PASSENCORE::take_top_four() {
  std::sort(possibilities, possibilities + p_len, [](PassenChord lhs, PassenChord rhs) {
    return lhs.score > rhs.score;
  });
  p_len = 4;
}

void PASSENCORE::score_by_function() {
  for (int c = 0; c < p_len; c++) {
    int function_score = 2 - abs(function - possibilities[c].function);
    // Major chords lead to the tonic better in both major (V) and minor (VII).
    if (function == PASSENCORE_FUNCTIONS_DOMINANT && possibilities[c].triad_type == CHORD_TYPES_MAJOR) {
      function_score++;
      // Extra bonus function score for dominant 7ths
      if (possibilities[c].seventh_type == CHORD_TYPES_MINOR) {
        function_score++;
      }
    }
    if (function_score < 0) {
      function_score = 0;
    } else if (function_score > 0) {
      // Increase it a little so it won't have such a huge effect when we multiply by it later.
      function_score += 2;
    }
    possibilities[c].score = function_score;
  }
}

void PASSENCORE::score_by_color() {
  for (int c = 0; c < p_len; c++) {
    int function_score = possibilities[c].score;
    // Chords with some resemblance to the right function gain a color bonus as valid substitutions.
    int color_modifier = (function_score > 0 && function_score < 3 && possibilities[c].color > 0) ? 2 : 0;
    // Diminished and augmented chords are weird and only deserve to be played at high color.
    if (possibilities[c].triad_type == CHORD_TYPES_DIMINISHED || possibilities[c].triad_type == CHORD_TYPES_AUGMENTED) {
      color_modifier += 2;
    }
    int color_score = 3 - abs(color - (possibilities[c].color + color_modifier));
    if (color_score > 0) {
      color_score += 1;
    } else if (color_score < 0) {
      // Increase it a little so it won't have such a huge effect when we multiply by it later.
      color_score = 1;
    }
    possibilities[c].score *= color_score;
  }
}


void PASSENCORE::ISR() {
  // value()
  uint32_t triggers = OC::DigitalInputs::clocked();
  calc_new_chord_ = triggers & DIGITAL_INPUT_MASK(values_[PASSENCORE_SETTING_SAMPLE_TRIGGER]);
  play_target_ = triggers & DIGITAL_INPUT_MASK(values_[PASSENCORE_SETTING_TARGET_TRIGGER]);
  play_passing_chord_ = triggers & DIGITAL_INPUT_MASK(values_[PASSENCORE_SETTING_PASSING_TRIGGER]);
  reset_ = triggers & DIGITAL_INPUT_MASK(values_[PASSENCORE_SETTING_RESET_TRIGGER]);
  update_scale(force_update_, false);
  if (calc_new_chord_) {
    find_new_chord();
    calc_new_chord_ = false;
  }
  if (play_passing_chord_) {
    find_passing_chord();
    play_chord(possibilities[0]);
    play_passing_chord_ = false;
  }
  if (play_target_) {
    play_chord(targetChord);
    play_target_ = false;
  }
}

void PASSENCORE::add_chord(int8_t root, int8_t i1, int8_t i2, int8_t i3, int8_t i4, PASSENCORE_FUNCTIONS f, PASSENCORE_COLORS c) {
  if (p_len >= POSSIBLE_LEN) return;
  possibilities[p_len].root = root;
  possibilities[p_len].intervals[0] = i1;
  possibilities[p_len].intervals[1] = i2;
  possibilities[p_len].intervals[2] = i3;
  possibilities[p_len].intervals[3] = i4;
  possibilities[p_len].function = f;
  possibilities[p_len].color = c;
  possibilities[p_len].triad_type = CHORD_TYPES_NONE;
  possibilities[p_len].seventh_type = CHORD_TYPES_NONE;
  possibilities[p_len].ninth_type = CHORD_TYPES_NONE;

  int32_t root_note = quantizer_.Process(0, 0, root - 1);
  int third = -1;
  int seventh = -1;
  bool borrow_dominant = values_[PASSENCORE_SETTING_BORROW_CHORDS] && f == PASSENCORE_FUNCTIONS_DOMINANT && (root == 5 || color >= PASSENCORE_COLORS_SUBSTITUTED);

  for (int i = 0; i < 4; i++) {
    int32_t note = quantizer_.Process(0, 0, root + possibilities[p_len].intervals[i] - 2);
    possibilities[p_len].samples[i] = note;
    int halfsteps = ((note - root_note) >> 7);
    switch (halfsteps) {
      case 0: break;
      case 1:
        break;
      case 2: break;
      case 3:
        third = i;
        possibilities[p_len].triad_type = CHORD_TYPES_MINOR;
        break;
      case 4:
        third = i;
        possibilities[p_len].triad_type = CHORD_TYPES_MAJOR;
        break;
      case 5: break;
      case 6:
        possibilities[p_len].triad_type = CHORD_TYPES_DIMINISHED;
        break;
      case 7: break;
      case 8:
        possibilities[p_len].triad_type = CHORD_TYPES_AUGMENTED;
        break;
      case 9:
        seventh = i;
        possibilities[p_len].seventh_type = CHORD_TYPES_DIMINISHED;
        break;
      case 10:
        seventh = i;
        possibilities[p_len].seventh_type = CHORD_TYPES_MINOR;
        break;
      case 11:
        seventh = i;
        possibilities[p_len].seventh_type = CHORD_TYPES_MAJOR;
        break;
      case 12: break;

      case 13:
        possibilities[p_len].ninth_type = CHORD_TYPES_MINOR;
        break;
      case 14:
        possibilities[p_len].ninth_type = CHORD_TYPES_MAJOR;
        break;
    }
  }
  if (borrow_dominant) {
    if (third >= 0 && possibilities[p_len].triad_type == CHORD_TYPES_MINOR) {
      // Raise the minor third to major
      possibilities[p_len].samples[third] += (1 << 7);
      possibilities[p_len].triad_type = CHORD_TYPES_MAJOR;
    }
    if (seventh >= 0 && possibilities[p_len].seventh_type == CHORD_TYPES_MAJOR) {
      possibilities[p_len].samples[seventh] -= (1 << 7);
    }
  }
  p_len++;
}

class PassencoreState {
  public:
    void Init() {
      cursor.Init(PASSENCORE_SETTING_SCALE, PASSENCORE_SETTING_LAST - 1);
      scale_editor.Init(false);
      //chord_editor.Init();
      left_encoder_value = OC::Scales::SCALE_SEMI + 1;
    }

    inline bool editing() const {
      return cursor.editing();
    }

    inline int cursor_pos() const {
      return cursor.cursor_pos();
    }

    menu::ScreenCursor<menu::kScreenLines> cursor;
    OC::ScaleEditor<PASSENCORE> scale_editor;
    //OC::ChordEditor<PASSENCORE> chord_editor;
    int left_encoder_value;
};


// TOTAL EEPROM SIZE: 17 bytes
SETTINGS_DECLARE(PASSENCORE, PASSENCORE_SETTING_LAST) {
  //PASSENCORE_SETTING_SCALE,
  { OC::Scales::SCALE_SEMI + 1, OC::Scales::SCALE_SEMI, OC::Scales::NUM_SCALES - 1, "scale", OC::scale_names, settings::STORAGE_TYPE_U8 },
  //PASSENCORE_SETTING_MASK,
  { 65535, 1, 65535, "mask  -->", NULL, settings::STORAGE_TYPE_U16 }, // mask
  //PASSENCORE_SETTING_SAMPLE_TRIGGER,
  { 0, 0, 3, "sample trigger", OC::Strings::trigger_input_names, settings::STORAGE_TYPE_U4},
  //PASSENCORE_SETTING_TARGET_TRIGGER,
  { 0, 0, 3, "target trigger", OC::Strings::trigger_input_names, settings::STORAGE_TYPE_U4},
  //PASSENCORE_SETTING_PASSING_TRIGGER,
  { 2, 0, 3, "passing trigger", OC::Strings::trigger_input_names, settings::STORAGE_TYPE_U4},
  //PASSENCORE_SETTING_RESET_TRIGGER,
  { 3, 0, 3, "reset trigger", OC::Strings::trigger_input_names, settings::STORAGE_TYPE_U4},
  //PASSENCORE_SETTING_A_OCTAVE,
  { -1, -2, 3, "A octave", NULL, settings::STORAGE_TYPE_I8 },
  //PASSENCORE_SETTING_A_MIDRANGE,
  { 0, -6, 6,  "A mid range", NULL, settings::STORAGE_TYPE_I8},
  //PASSENCORE_SETTING_B_OCTAVE,
  { -1, -2, 3, "B octave", NULL, settings::STORAGE_TYPE_I8 },
  //PASSENCORE_SETTING_B_MIDRANGE,
  { 0, -6, 6,  "B mid range", NULL, settings::STORAGE_TYPE_I8},
  //PASSENCORE_SETTING_C_OCTAVE,
  { -1, -2, 3, "C octave", NULL, settings::STORAGE_TYPE_I8 },
  //PASSENCORE_SETTING_C_MIDRANGE,
  { 0, -6, 6,  "C mid range", NULL, settings::STORAGE_TYPE_I8},
  //PASSENCORE_SETTING_D_OCTAVE,
  { -1, -2, 3, "D octave", NULL, settings::STORAGE_TYPE_I8 },
  //PASSENCORE_SETTING_D_MIDRANGE,
  { 0, -6, 6,  "D mid range", NULL, settings::STORAGE_TYPE_I8},
  //PASSENCORE_SETTING_BORROW_CHORDS,
  { 1, 0, 1,  "Borrow chords?", OC::Strings::no_yes, settings::STORAGE_TYPE_I8},
  // PASSENCORE_SETTING_BASE_COLOR,
  { PASSENCORE_COLORS_CLASSIC, PASSENCORE_COLORS_POWER, PASSENCORE_COLORS_JAZZ, "color", passencore_color_names, settings::STORAGE_TYPE_I8},
  // PASSENCORE_SETTING_CV3_ROLE
  { PASSENCORE_CV_ROLE_NONE, PASSENCORE_CV_ROLE_NONE, PASSENCORE_CV_ROLE_LAST - 1, "CV3", passencore_cv_role_names, settings::STORAGE_TYPE_I8},
  // PASSENCORE_SETTING_CV4_ROLE

  { PASSENCORE_CV_ROLE_NONE, PASSENCORE_CV_ROLE_NONE, PASSENCORE_CV_ROLE_LAST - 1, "CV4", passencore_cv_role_names, settings::STORAGE_TYPE_I8},

};

PassencoreState passencore_state;
PASSENCORE passencore_instance;

void PASSENCORE_init() {
  passencore_instance.Init();
  passencore_state.Init();
}

static constexpr size_t PASSENCORE_storageSize() {
  return PASSENCORE::storageSize();
}

static size_t PASSENCORE_save(void *storage) {
  return passencore_instance.Save(storage);
}

void PASSENCORE_isr() {
  return passencore_instance.ISR();
}

void PASSENCORE_leftButton() {

  if (passencore_state.left_encoder_value != passencore_instance.get_scale(DUMMY)) {
    passencore_instance.set_scale(passencore_state.left_encoder_value);
  }
}


void PASSENCORE_handleAppEvent(OC::AppEvent event) {
  switch (event) {
    case OC::APP_EVENT_RESUME:
      passencore_state.cursor.set_editing(false);
      passencore_state.scale_editor.Close();
      //passencore_state.chord_editor.Close();
      break;
    case OC::APP_EVENT_SUSPEND:
    case OC::APP_EVENT_SCREENSAVER_ON:
    case OC::APP_EVENT_SCREENSAVER_OFF:
      break;
  }
}

void PASSENCORE_loop() {
  passencore_instance.Loop();
}

void PASSENCORE_menu() {
  menu::TitleBar<0, 4, 0>::Draw();

  // print scale
  int scale = passencore_state.left_encoder_value;
  graphics.movePrintPos(5, 0);
  graphics.print(OC::scale_names[scale]);
  if (passencore_instance.get_scale(DUMMY) == scale)
    graphics.drawBitmap8(1, menu::QuadTitleBar::kTextY, 4, OC::bitmap_indicator_4x8);
  menu::SettingsList < menu::kScreenLines, 0, menu::kDefaultValueX - 1 > settings_list(passencore_state.cursor);
  menu::SettingsListItem list_item;
  while (settings_list.available())
  {
    const int current = settings_list.Next(list_item);
    const int value = passencore_instance.get_value(current);
    if (current == PASSENCORE_SETTING_MASK) {
        menu::DrawMask<false, 16, 8, 1>(menu::kDisplayWidth, list_item.y, passencore_instance.get_mask(), OC::Scales::GetScale(passencore_instance.get_scale(DUMMY)).num_notes);
        list_item.DrawNoValue<false>(value, PASSENCORE::value_attr(current));
    } else {
        list_item.DrawDefault(value, PASSENCORE::value_attr(current));
    }

   if (passencore_state.scale_editor.active())
     passencore_state.scale_editor.Draw();
  }
}

void PASSENCORE_screensaver() {
  passencore_instance.Draw();
}

void PASSENCORE_handleButtonEvent(const UI::Event & event) {
  if (passencore_state.scale_editor.active()) {
    passencore_state.scale_editor.HandleButtonEvent(event);
    return;
  }
  //else if (passencore_state.chord_editor.active()) {
  //  passencore_state.chord_editor.HandleButtonEvent(event);
  //  return;
  //}
  if (UI::EVENT_BUTTON_PRESS == event.type) {
    switch (event.control) {
      case OC::CONTROL_BUTTON_UP:
        //PASSENCORE_topButton();
        break;
      case OC::CONTROL_BUTTON_DOWN:
        //PASSENCORE_lowerButton();
        break;
      case OC::CONTROL_BUTTON_L:
        PASSENCORE_leftButton();
        break;
      case OC::CONTROL_BUTTON_R:
        if (passencore_state.cursor_pos() == PASSENCORE_SETTING_MASK) {
          int scale = passencore_instance.get_scale(DUMMY);
          if (OC::Scales::SCALE_NONE != scale)
            passencore_state.scale_editor.Edit(&passencore_instance, scale);
        } else {
          passencore_state.cursor.toggle_editing();
        }
        break;
    }
  }
}

void PASSENCORE_handleEncoderEvent(const UI::Event & event) {
  if (passencore_state.scale_editor.active()) {
    passencore_state.scale_editor.HandleEncoderEvent(event);
    return;
  }
  //else if (passencore_state.chord_editor.active()) {
  //  passencore_state.chord_editor.HandleEncoderEvent(event);
  //  return;
  //}
  if (OC::CONTROL_ENCODER_L == event.control) {
    int value = passencore_state.left_encoder_value + event.value;
    CONSTRAIN(value, OC::Scales::SCALE_SEMI, OC::Scales::NUM_SCALES - 1);
    passencore_state.left_encoder_value = value;
  } else if (OC::CONTROL_ENCODER_R == event.control) {
    if (passencore_state.cursor.editing()) {
      passencore_instance.change_value(passencore_state.cursor.cursor_pos(), event.value);
    } else {
      passencore_state.cursor.Scroll(event.value);
    }
  }
}

static size_t PASSENCORE_restore(const void *storage) {
  size_t storage_size = passencore_instance.Restore(storage);
  passencore_state.left_encoder_value = passencore_instance.get_scale(DUMMY);
  passencore_instance.set_scale(passencore_state.left_encoder_value);
  return storage_size;
}

#endif // ENABLE_APP_PASSENCORE
