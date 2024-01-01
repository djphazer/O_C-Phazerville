// Copyright (c) 2020, Logarhythm
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

// TB-3PO Hemisphere Applet
// A random generator of TB-303 style acid patterns, closely following 303 gate timings
// CV output 1 is pitch, CV output 2 is gates
// CV pitch out includes fixed-time exponential pitch slides timed as on 303s
// CV gates are output at 3v for normal notes and 5v for accented notes

// Contributions:
// Thanks to Github/Muffwiggler user Qiemem for adding reseed(), to break the small cycle of available seed values that was occurring in practice

#include "braids_quantizer.h"
#include "braids_quantizer_scales.h"
#include "OC_scales.h"

#define ACID_HALF_STEPS 16
#define ACID_MAX_STEPS 32

class TB_3PO: public HemisphereApplet {
  public:

    const char * applet_name() { // Maximum 10 characters
      return "TB-3PO";
    }

  void Start() {
    manual_reset_flag = 0;
    rand_apply_anim = 0;
    curr_step_semitone = 0;

    root = 0;
    octave_offset = 0;

    // Init the quantizer for selecting pitches / CVs from
    scale = 29; // GUNA scale sounds cool   //OC::Scales::SCALE_SEMI; // semi sounds pretty bunk
    quantizer = GetQuantizer(0);
    set_quantizer_scale(scale);

    density = 12;
    density_encoder_display = 0;

    num_steps = 16;

    gate_off_clock = 0;
    cycle_time = 0;

    curr_gate_cv = 0;
    curr_pitch_cv = 0;

    slide_start_cv = 0;
    slide_end_cv = 0;

    lock_seed = 0;
    reseed();
    regenerate_all();

  }

  void Controller() {
    const int this_tick = OC::CORE::ticks;

    if (Clock(1) || manual_reset_flag) {
      manual_reset_flag = 0;
      if (lock_seed == 0) {
        reseed();
      }

      regenerate_all();

      step = 0;
    }

    transpose_cv = 0;
    if (DetentedIn(0)) {
      transpose_cv = quantizer->Process(In(0), 0, 0); // Use root == 0 to start at c
    }

    density_cv = Proportion(DetentedIn(1), HEMISPHERE_MAX_INPUT_CV, 15);
    density = static_cast<uint8_t>(constrain(density_encoder + density_cv, 0, 14));

    if (Clock(0)) {
      cycle_time = ClockCycleTicks(0); // Track latest interval of clock 0 for gate timings

      regenerate_if_density_or_scale_changed(); // Flag to do the actual update at end of Controller()

      StartADCLag();
    }

    if (EndOfADCLag() && !Gate(1)) // Reset not held
    {
      int step_pv = step;

      step = get_next_step(step);

      if (step_is_slid(step_pv)) {
        slide_start_cv = get_pitch_for_step(step_pv);

        // TODO: Consider just gliding from whereever it is?
        curr_pitch_cv = slide_start_cv;

        slide_end_cv = get_pitch_for_step(step);
      } else {
        curr_pitch_cv = get_pitch_for_step(step);
        slide_start_cv = curr_pitch_cv;
        slide_end_cv = curr_pitch_cv;
      }

      if (step_is_gated(step) || step_is_slid(step_pv)) {
        curr_gate_cv = step_is_accent(step) ? HEMISPHERE_MAX_CV : HEMISPHERE_3V_CV;

        int gate_time = (cycle_time / 2); // multiplier of 2
        gate_off_clock = this_tick + gate_time;
      }

      curr_step_semitone = get_semitone_for_step(step);

    }

    if (curr_gate_cv > 0 && gate_off_clock > 0 && this_tick >= gate_off_clock) {
      gate_off_clock = 0;

      if (!step_is_slid(step)) {
        curr_gate_cv = 0;
      }
    }

    if (curr_pitch_cv != slide_end_cv) {
      int k = 0x0003; // expo constant:  0 = infinite time to settle, 0xFFFF ~= 1, fastest rate
      // Choose this to give 303-like pitch slide timings given the O&C's update rate
      // k = 0x3 sounds good here with >>=18    

      int x = slide_end_cv;
      x -= curr_pitch_cv;
      x >>= 18;
      x *= k;
      curr_pitch_cv += x;

      // TODO: Check constrain, set a bit if constrain was needed
      if (slide_start_cv < slide_end_cv) {
        curr_pitch_cv = constrain(curr_pitch_cv, slide_start_cv, slide_end_cv);
      } else {
        curr_pitch_cv = constrain(curr_pitch_cv, slide_end_cv, slide_start_cv);
      }
    }

    Out(0, curr_pitch_cv);
    Out(1, curr_gate_cv);

    // Timesliced generation of new patterns, if triggered
    // Do this last to not interfere with the body of the time for this hemisphere's update
    // (This is speculation without knowing how to best profile performance on this system)
    update_regeneration();
  }

  void View() {
    DrawGraphics();
  }

  void OnButtonPress() {
    CursorAction(cursor, 8);
  }

  void OnEncoderMove(int direction) {
    if (!EditMode()) { // move cursor
      MoveCursor(cursor, direction, 8);

      if (!lock_seed && cursor == 1) cursor = 5; // skip from 1 to 5 if not locked
      if (!lock_seed && cursor == 4) cursor = 0; // skip from 4 to 0 if not locked

      return;
    }

    // edit param
    switch (cursor) {
    case 0:
      lock_seed += direction;

      manual_reset_flag = (lock_seed > 1 || lock_seed < 0);

      lock_seed = constrain(lock_seed, 0, 1);
      break;
    case 1:
    case 2:
    case 3:
    case 4: { // Editing one of the 4 hex digits of the seed
      int byte_offs = 4 - cursor;
      int shift_amt = byte_offs * 4;

      uint32_t nib = (seed >> shift_amt) & 0xf; // Abduct the nibble
      uint8_t c = nib;
      c = constrain(c + direction, 0, 0xF); // Edit the nibble
      nib = c;
      uint32_t mask = 0xf;
      seed &= ~(mask << shift_amt); // Clear bits where this nibble lives
      seed |= (nib << shift_amt); // Move the nibble to its home
      break;
    }
    case 5: // density
      density_encoder = constrain(density_encoder + direction, 0, 14); // Treated as a bipolar -7 to 7 in practice
      density_encoder_display = 400; // How long to show the encoder version of density in the number display for

      break;
    case 6: { // Scale selection
      scale += direction;
      if (scale >= OC::Scales::NUM_SCALES) scale = 0;
      if (scale < 0) scale = OC::Scales::NUM_SCALES - 1;
      set_quantizer_scale(scale);
      break;
    }
    case 7: { // Root note selection

      int r = root + direction;
      const int max_root = 12;

      if (direction > 0 && r >= max_root && octave_offset < 3) {
        ++octave_offset; // Go up to next octave
        r = 0; // Roll around root note
      } else if (direction < 0 && r < 0 && octave_offset > -3) {
        --octave_offset;

        r = max_root - 1;
      }

      root = constrain(r, 0, max_root - 1);

      break;
    }
    case 8: // pattern length
      num_steps = constrain(num_steps + direction, 1, 32);
      break;
    } //switch
  } //OnEncoderMove

  uint64_t OnDataRequest() {
    uint64_t data = 0;

    Pack(data, PackLocation { 0, 8 }, scale);
    Pack(data, PackLocation { 8, 4 }, root);
    Pack(data, PackLocation { 12, 4 }, density_encoder);
    Pack(data, PackLocation { 16, 16 }, seed);
    Pack(data, PackLocation { 32, 8 }, octave_offset);
    Pack(data, PackLocation { 40, 5 }, num_steps - 1);
    return data;
  }

  void OnDataReceive(uint64_t data) {

    scale = Unpack(data, PackLocation { 0, 8 });
    root = Unpack(data, PackLocation { 8, 4 });
    density_encoder = Unpack(data, PackLocation { 12, 4 });
    seed = Unpack(data, PackLocation { 16, 16 });
    octave_offset = Unpack(data, PackLocation { 32, 8 });
    num_steps = Unpack(data, PackLocation { 40, 5 }) + 1;

    set_quantizer_scale(scale);

    root = constrain(root, 0, 11);
    density_encoder = constrain(density_encoder, 0, 14); // Internally just positive
    density = density_encoder;
    octave_offset = constrain(octave_offset, -3, 3);

    // Restore all seed-derived settings!
    regenerate_all();

    // Reset step position
    step = 0;
  }

protected:
  void SetHelp() {
    //                               "------------------" <-- Size Guide
    help[HEMISPHERE_HELP_DIGITALS] = "1=Clock 2=Regen";
    help[HEMISPHERE_HELP_CVS]      = "1=Transp 2=Density";
    help[HEMISPHERE_HELP_OUTS]     = "A=CV+glide B=Gate";
    help[HEMISPHERE_HELP_ENCODER]  = "seed/dens/qnt/len";
    //                               "------------------" <-- Size Guide
  }

private:
  int cursor = 0;
  braids::Quantizer * quantizer;

  // User settings

  // Bool
  bool manual_reset_flag = 0; // Manual trigger to reset/regen

  // bool 
  int lock_seed; // If 1, the seed won't randomize (and manual editing is enabled)

  uint16_t seed; // The random seed that deterministically builds the sequence

  int scale; // Active quantization & generation scale
  uint8_t root; // Root note
  int8_t octave_offset; // Manual octave offset (based on size of current scale, added to root note)

  uint8_t density; // The density parameter controls a couple of things at once. Its 0-14 value is mapped to -7..+7 range
  // The larger the magnitude from zero in either direction, the more dense the note patterns are (fewer rests)
  // For values mapped < 0 (e.g. left range,) the more negative the value is, the less chance consecutive pitches will
  // change from the prior pitch, giving repeating lines (note: octave jumps still apply)

  uint8_t current_pattern_density; // Track what density value was used to generate the current pattern (to detect if regeneration is required)

  // Density controls (Encoder sets center point, CV can apply +-)
  int density_encoder; // density value contributed by the encoder (center point)
  int density_cv; // density value (+-) contributed by CV
  int density_encoder_display; // Countdown of frames to show the encoder's density value (centerpoint)
  uint8_t num_steps; // How many steps of the generated pattern to play before looping

  // Playback
  uint8_t step = 0; // Current sequencer step

  int32_t transpose_cv; // Quantized transpose in cv

  // Generated sequence data
  uint32_t gates = 0; // Bitfield of gates;  ((gates >> step) & 1) means gate
  uint32_t slides = 0; // Bitfield of slide steps; ((slides >> step) & 1) means slide
  uint32_t accents = 0; // Bitfield of accent steps; ((accents >> step) & 1) means accent
  uint32_t oct_ups = 0; // Bitfield of octave ups
  uint32_t oct_downs = 0; // Bitfield of octave downs
  uint8_t notes[ACID_MAX_STEPS]; // Note values

  uint8_t scale_size; // The size of the currently set quantizer scale (for octave detection, etc)
  uint8_t current_pattern_scale_size; // Track what size scale was used to render the current pattern (for change detection)

  // For gate timing as ~32nd notes at tempo, detect clock rate like a clock multiplier
  int gate_off_clock; // Scheduled cycle at which the gate should be turned off (when applicable)
  int cycle_time; // Cycle time between the last two clock inputs

  // CV output values
  int32_t curr_gate_cv = 0;
  int32_t curr_pitch_cv = 0;

  // Pitch slide cv tracking
  int32_t slide_start_cv = 0;
  int32_t slide_end_cv = 0;

  // Display
  int curr_step_semitone = 0; // The pitch converted to nearest semitone, for showing as an index onto the keyboard
  uint8_t rand_apply_anim = 0; // Countdown to animate icons for when regenerate occurs

  uint8_t regenerate_phase = 0; // Split up random generation over multiple frames

  // Get the cv value to use for a given step including root + transpose values
  int get_pitch_for_step(int step_num) {
    int quant_note = 64 + int(notes[step_num]);

    // transpose in scale degrees, proportioned from semitones
    quant_note += (MIDIQuantizer::NoteNumber(transpose_cv) - 60) * scale_size / 12;

    // Apply the manual octave offset
    quant_note += (int(octave_offset) * int(scale_size));

    // Transpose by one octave up or down if flagged to (note this is one full span of whatever scale is active to give doubling octave behavior)
    if (step_is_oct_up(step_num)) {
      quant_note += scale_size;
    } else if (step_is_oct_down(step_num)) {
      quant_note -= scale_size;
    }

    quant_note = constrain(quant_note, 0, 127);

    // root note is the semitone offset after quantization
    return quantizer->Lookup(quant_note) + (root << 7);
    //return quantizer->Lookup( 64 );  // Test: note 64 is definitely 0v=c4 if output directly, on ALL scales
  }

  int get_semitone_for_step(int step_num) {
    // Don't add in octaves-- use the current quantizer limited to the base octave
    int quant_note = 64 + notes[step_num]; // + transpose_note_in;
    int32_t cv_note = quantizer->Lookup(constrain(quant_note, 0, 127));
    return (MIDIQuantizer::NoteNumber(cv_note) + root) % 12;
  }

  void reseed() {
    randomSeed(micros());
    seed = random(0, 65535); // 16 bits
  }

  void regenerate_all() {
    regenerate_phase = 1; // Set to regenerate on loop
    rand_apply_anim = 40; // Show that regenerate started (anim for this many display updates)
  }

  void regenerate_if_density_or_scale_changed() {
    // Skip if density has not changed, or if currently regenerating
    if (regenerate_phase == 0) {
      if (density != current_pattern_density || scale_size != current_pattern_scale_size) {
        regenerate_phase = 1; // regenerate all since pitches take density into account
      }
    }
  }

  // Amortize random generation over multiple frames
  void update_regeneration() {
    if (regenerate_phase == 0) {
      return;
    }

    randomSeed(seed + regenerate_phase); // Ensure random()'s seed at each phase for determinism (note: offset to decouple phase behavior correllations that would result)

    switch (regenerate_phase) {
      // 1st set of 16 steps
    case 1:
      regenerate_pitches();
      ++regenerate_phase;
      break;
    case 2:
      apply_density();
      ++regenerate_phase;
      break;
      // 2nd set of 16 steps
    case 3:
      regenerate_pitches();
      ++regenerate_phase;
      break;
    case 4:
      apply_density();
      regenerate_phase = 0;
      break;
    default:
      break;
    }
  }

  // Generate the notes sequence based on the seed and modified by density
  void regenerate_pitches() {
    bool bFirstHalf = regenerate_phase < 3;

    // How much pitch variety to use from the available pitches (one of the factors of the 'density' control when < centerpoint)
    int pitch_change_dens = get_pitch_change_density();
    int available_pitches = 0;
    if (scale_size > 0) {
      if (pitch_change_dens > 7) {
        available_pitches = scale_size - 1;
      } else if (pitch_change_dens < 2) {
        // Give the behavior of just the root note (0) at lowest density, and 0&1 at 2nd lowest (for 303 half-step style)
        available_pitches = pitch_change_dens;
      } else // Range 3-7
      {
        int range_from_scale = scale_size - 3;
        if (range_from_scale < 4) // Ok to saturate at full note count
        {
          range_from_scale = 4;
        }
        // Range from 2 pitches to just <= full scale available
        available_pitches = 3 + Proportion(pitch_change_dens - 3, 4, range_from_scale);
        available_pitches = constrain(available_pitches, 1, scale_size - 1);
      }
    }

    if (bFirstHalf) {
      oct_ups = 0;
      oct_downs = 0;
    }

    int max_step = (bFirstHalf ? ACID_HALF_STEPS : ACID_MAX_STEPS);
    for (int s = (bFirstHalf ? 0 : ACID_HALF_STEPS); s < max_step; s++) {
      int force_repeat_note_prob = 50 - (pitch_change_dens * 6);
      if (s > 0 && rand_bit(force_repeat_note_prob)) {
        notes[s] = notes[s - 1];
      } else {
        notes[s] = random(0, available_pitches + 1); // Looking at the source, random(min,max) appears to return the range: min to max-1

        oct_ups <<= 1;
        oct_downs <<= 1;

        if (rand_bit(40)) {
          if (rand_bit(50)) {
            oct_ups |= 0x1;
          } else {
            oct_downs |= 0x1;
          }
        }
      }
    }

    if (scale_size == 0) {
      scale_size = 12;
    }

    current_pattern_scale_size = scale_size;
  }

  // Change pattern density without affecting pitches
  void apply_density() {
    int latest_slide = 0; // Track previous bit for some algos
    int latest_accent = 0; // Track previous bit for some algos

    // Get gate probability from the 'density' value
    int on_off_dens = get_on_off_density();
    int densProb = 10 + on_off_dens * 14; // Should start >0 and reach 100+

    bool bFirstHalf = regenerate_phase < 3;
    if (bFirstHalf) {
      gates = 0;
      slides = 0;
      accents = 0;
    }

    for (int i = 0; i < ACID_HALF_STEPS; ++i) {
      gates <<= 1;
      gates |= rand_bit(densProb);

      // Less probability of consecutive slides
      slides <<= 1;
      latest_slide = rand_bit((latest_slide ? 10 : 18));
      slides |= latest_slide;

      // Less probability of consecutive accents
      accents <<= 1;
      latest_accent = rand_bit((latest_accent ? 7 : 16));
      accents |= latest_accent;
    }

    current_pattern_density = density;
  }

  int get_on_off_density() {
    int note_dens = int(density) - 7;
    return abs(note_dens);
  }

  int get_pitch_change_density() {
    return constrain(density, 0, 8); // Note that the right half of the slider is clamped to full range
  }

  bool step_is_gated(int step_num) {
    return (gates & (0x01 << step_num));
  }

  bool step_is_slid(int step_num) {
    return (slides & (0x01 << step_num));
  }

  bool step_is_accent(int step_num) {
    return (accents & (0x01 << step_num));
  }

  bool step_is_oct_up(int step_num) {
    return (oct_ups & (0x01 << step_num));
  }

  bool step_is_oct_down(int step_num) {
    return (oct_downs & (0x01 << step_num));
  }

  int get_next_step(int step_num) {
    if (++step_num >= num_steps) {
      return 0;
    }
    return step_num; // Advanced by one
  }

  int rand_bit(int prob) {
    return (random(1, 100) <= prob) ? 1 : 0;
  }

  void set_quantizer_scale(int new_scale) {
    const braids::Scale & quant_scale = OC::Scales::GetScale(new_scale);
    quantizer->Configure(quant_scale, 0xffff);
    scale_size = quant_scale.num_notes; // Track this scale size for octaves and display
  }

  void DrawGraphics() {
    int heart_y = 15;
    int die_y = 15;
    if (rand_apply_anim > 0) {
      --rand_apply_anim;

      if (rand_apply_anim > 20) {
        heart_y = 13;
      } else {
        die_y = 13;
      }
    }

    // Heart represents the seed/favorite
    gfxIcon(4, heart_y, FAVORITE_ICON);
    gfxIcon(15, (lock_seed ? 15 : die_y), (lock_seed ? LOCK_ICON : RANDOM_ICON));

    // Show the 16-bit seed as 4 hex digits
    int disp_seed = seed; //0xABCD // test display accuracy
    char sz[2];
    sz[1] = 0; // Null terminated string for easy print
    gfxPos(25, 15);
    for (int i = 3; i >= 0; --i) {
      // Grab each nibble in turn, starting with most significant
      int nib = (disp_seed >> (i * 4)) & 0xF;
      if (nib <= 9) {
        gfxPrint(nib);
      } else {
        sz[0] = 'a' + nib - 10;
        gfxPrint(static_cast<const char *>(sz));
      }
    }

    // Display density 

    int gate_dens = get_on_off_density();
    int pitch_dens = get_pitch_change_density();

    int xd = 5 + 7 - gate_dens;
    int yd = (64 * pitch_dens) / 256; // Multiply for better fidelity
    gfxBitmap(12 - xd, 27 + yd, 8, NOTE4_ICON);
    gfxBitmap(12, 27 - yd, 8, NOTE4_ICON);
    gfxBitmap(12 + xd, 27, 8, NOTE4_ICON);

    // Display a number value for density
    int dens_display = gate_dens;
    bool dens_neg = false;
    if (density_encoder_display > 0) {
      // The density encoder value was recently changed, so show it momentarily instead of the cv+encoder value normally shown
      --density_encoder_display;
      dens_display = abs(density_encoder - 7); //Map from 0 to 14 --> -7 to 7
      dens_neg = density_encoder < 7;

      if (density_cv != 0) // When cv is applied, show that this is the centered value being displayed
      {
        // Draw a knob to the left to represent the centerpoint being set
        gfxCircle(3, 40, 3);
        gfxLine(3, 38, 3, 40);
      }

    } else {
      dens_display = gate_dens;
      dens_neg = density < 7;
      // Indicate if cv is affecting the density
      if (density_cv != 0) // Density integer contribution from CV (not raw cv)
      {
        gfxBitmap(22, 37, 8, CV_ICON);
      }
    }

    if (dens_neg) {
      gfxPrint(8, 37, "-"); // Print minus sign this way to right-align the number
    }
    gfxPrint(14, 37, dens_display);

    // Scale and root note select
    gfxPrint(39, 26, OC::scale_names_short[scale]);

    gfxPrint((octave_offset == 0 ? 45 : 39), 36, OC::Strings::note_names_unpadded[root]);
    if (octave_offset != 0) {
      gfxPrint(51, 36, octave_offset);
    }

    // Current / total steps
    int display_step = step + 1; // Protocol droids know that humans count from 1
    gfxPrint(1 + pad(10, display_step), 47, display_step); // Pad x enough to hold width steady
    gfxPrint("/");
    gfxPrint(num_steps);

    // Show octave icons
    if (step_is_oct_down(step)) {
      gfxBitmap(41, 54, 8, DOWN_BTN_ICON);
    } else if (step_is_oct_up(step)) {
      gfxBitmap(41, 54, 8, UP_BTN_ICON);
    }

    gfxPrint(49, 55, curr_step_semitone);

    // Draw a TB-303 style octave of a piano keyboard, indicating the playing pitch 
    int x = 1;
    int y;
    const int keyPatt = 0x054A; // keys encoded as 0=white 1=black, starting at c, backwards:  b  0 0101 0100 1010
    for (int i = 0; i < 12; ++i) {
      // Black key?
      y = ((keyPatt >> i) & 0x1) ? 56 : 61;

      // Two white keys in a row E and F
      if (i == 5) x += 3;

      if (curr_step_semitone == i && step_is_gated(step)) // Only render a pitch if gated
      {
        gfxRect(x - 1, y - 1, 5, 4); // Larger box

      } else {
        gfxRect(x, y, 3, 2); // Small filled box
      }
      x += 3;
    }

    if (step_is_accent(step)) {
      gfxPrint(37, 46, "!");
    }

    if (step_is_slid(step)) {
      gfxBitmap(42, 46, 8, BEND_ICON);
    }

    // Show that the "slide circuit" is actively
    // sliding the pitch (one step after the slid step)
    if (curr_pitch_cv != slide_end_cv) {
      gfxBitmap(52, 46, 8, WAVEFORM_ICON);
    }

    // Draw edit cursor
    switch (cursor) {
    case 0:
      // Set length to indicate length
      gfxCursor(14, 23, lock_seed ? 11 : 36); // Seed = auto-randomize / locked-manual
      break;
    case 1:
    case 2:
    case 3:
    case 4: // seed, 4 positions (1-4)
      gfxCursor(25 + 6 * (cursor - 1), 23, 7);
      break;
    case 5:
      gfxCursor(9, 45, 14); // density
      break;
    case 6:
      gfxCursor(39, 34, 25); // scale
      break;
    case 7:
      gfxCursor(39, 44, 24); // root note
      break;
    case 8:
      gfxCursor(20, 54, 12, 8); // step
      break;
    }
  }

};
