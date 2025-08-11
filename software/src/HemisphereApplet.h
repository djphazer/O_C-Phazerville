// Copyright (c) 2018, Jason Justian
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
// Thanks to Mike Thomas, for tons of help with the Buchla stuff
//

/* HEAVILY modified by djphazer for Phazerville Suite */

////////////////////////////////////////////////////////////////////////////////
//// Hemisphere Applet Base Class
////////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef _HEM_APPLET_H_
#define _HEM_APPLET_H_

#include "OC_core.h"

#include "OC_digital_inputs.h"
#include "OC_DAC.h"
#include "OC_ADC.h"
#include "src/drivers/FreqMeasure/OC_FreqMeasure.h"
#include "util/util_math.h"
#include "src/extern/bjorklund.h"
#include "HSicons.h"
#include "HSClockManager.h"

#include "HSUtils.h"
#include "HSIOFrame.h"
#include <variant>


namespace HS {

class HemisphereApplet;

struct Applet {
  const int id;
  const uint8_t categories;
  std::array<HemisphereApplet *, APPLET_SLOTS> instance;
};

struct EncoderEditor {
  bool isEditing;
};

static constexpr bool ALWAYS_SHOW_ICONS = false;

class HemisphereApplet {
public:

    static int cursor_countdown[APPLET_SLOTS + 1];
    static int16_t cursor_start_x;
    static int16_t cursor_start_y;
    static const char* help[HELP_LABEL_COUNT];
    static EncoderEditor enc_edit[APPLET_SLOTS + 1];

    // Virtual Method signatures
    // - These need to be defined by an actual Applet implementation
    // - Some have default implementations here.
    virtual const char* applet_name() = 0; // Maximum of 9 characters
    virtual const uint8_t* applet_icon() { return ZAP_ICON; }
    virtual void Start() = 0;
    virtual void Reset() { };
    virtual void Controller() = 0;
    virtual void View() = 0;
    virtual uint64_t OnDataRequest() = 0;
    virtual void OnDataReceive(uint64_t data) = 0;
    virtual void OnButtonPress() { CursorToggle(); };
    virtual void OnEncoderMove(int direction) = 0;
    virtual void Unload() { }
    virtual void DrawFullScreen() { View(); }
    virtual void AuxButton() { CancelEdit(); }

    // standard entry points
    void BaseView(bool full_screen = false, bool parked = true) const;
    void BaseStart(const HEM_SIDE hemisphere_);
    void SetDisplaySide(HEM_SIDE side) {
        hemisphere = side;
    }

    /* Formerly Help Screen */
    void DrawConfigHelp() const;

    // --- Cursor stuff
    static void ProcessCursors() {
      // Cursor countdowns. See CursorBlink(), ResetCursor(), gfxCursor()
      for (int i = 0; i < APPLET_SLOTS + 1; ++i) {
        if (--cursor_countdown[i] < -HEMISPHERE_CURSOR_TICKS)
          cursor_countdown[i] = HEMISPHERE_CURSOR_TICKS;
      }
    }

    /* Check cursor blink cycle. */
    bool CursorBlink() const { return (cursor_countdown[hemisphere] > 0); }
    void ResetCursor() { cursor_countdown[hemisphere] = HEMISPHERE_CURSOR_TICKS; }

    void CursorToggle() {
      enc_edit[hemisphere].isEditing ^= 1;
      ResetCursor();
    }
    void CancelEdit() {
      enc_edit[hemisphere].isEditing = false;
    }
    inline bool EditMode() const {
      return (enc_edit[hemisphere].isEditing);
    }

    template<typename T>
    void MoveCursor(T &cursor, int direction, int max) {
        cursor += direction;
        if (cursor_wrap) {
            if (cursor < 0) cursor = max;
            else cursor %= max + 1;
        } else {
            cursor = constrain(cursor, 0, max);
        }
        ResetCursor();
    }

    // DAC label helper
    const char* const OutputLabel(int ch) const {
      return OC::Strings::capital_letters[ch + io_offset];
    }

    // Buffered I/O functions
    int ViewIn(int ch) const {return frame.inputs[io_offset + ch];}
    int ViewOut(int ch) const {return frame.outputs[io_offset + ch];}
    uint32_t ClockCycleTicks(int ch) const {
      if (clock_m.IsRunning() && clock_m.GetMultiply(io_offset + ch) != 0)
          return clock_m.GetCycleTicks(io_offset + ch);
      return frame.cycle_ticks[io_offset + ch];
    }
    // XXX: does NOT use mappings! This should be deprecated.
    [[deprecated("Apps/Applets should keep track of CV deviation")]]
    bool Changed(int ch) {return frame.changed_cv[io_offset + ch];}

    // ----------------------
    // --- CV I/O Methods ---
    // ----------------------

    // ***** Inputs *****
    int In(const int ch) const;
    float InF(int ch) const;
    // Apply small center detent to input, so it reads zero before a threshold
    int DetentedIn(int ch) const;
    int SemitoneIn(int ch) const;
    /* Has the specified Digital input been clocked this cycle? (rising edge of a gate)
     * This is pre-calculated in HS::IOFrame::Load() according to input mappings and internal clock settings
     */
    bool Clock(int ch, bool physical = 0) const;
    bool Gate(int ch) const;

    // outputs
    void Out(int ch, int value, int octave = 0) const;
    void SmoothedOut(int ch, int value, int kSmoothing) const;
    void ClockOut(const int ch, const int ticks = HEMISPHERE_CLOCK_TICKS * trig_length) const;
    void GateOut(int ch, bool high) const;

    // Standard bi-polar CV modulation scenario
    template <typename T>
    void Modulate(T &param, const int ch, const int min = 0, const int max = 255) {
        // small ranges use Semitone quantizer for hysteresis
        int increment = (max < 70) ? SemitoneIn(ch) :
          Proportion(DetentedIn(ch), HEMISPHERE_MAX_INPUT_CV, max);
        param = constrain(param + increment, min, max);
    }

    // Override HSUtils function to only return positive values
    // Not ideal, but too many applets rely on this.
    const int ProportionCV(const int cv_value, const int max_pixels, const int max_cv = HEMISPHERE_MAX_CV) {
        int prop = constrain(Proportion(cv_value, max_cv, max_pixels), 0, max_pixels);
        return prop;
    }

    // -------------------------------
    // --- Offset graphics methods ---
    // -------------------------------
    void gfxCursor(int x, int y, int w, int h = 9) const;
    void gfxSpicyCursor(int x, int y, int w, int h = 9) const;
    void gfxPos(int x, int y) const;
    void gfxPrint(int x, int y, const char *str) const;
    void gfxPrint(int x, int y, int num) const;
    void gfxPrint(const char *str) const;
    void gfxPrint(int num) const;
    void gfxPrint(int x, int y, float num, int digits) const;
    void gfxPrint(float num, int digits) const;
    void gfxPrint(CVInputMap &map) const;
    void gfxPrint(DigitalInputMap &map) const;
    void gfxPrint(int x_adv, int num) const; // Print number with character padding

    void gfxStartCursor(int x, int y);
    void gfxStartCursor();
    void gfxEndCursor(bool selected, bool spicy = false, const char *str = nullptr) const;

    void gfxPixel(int x, int y) const;
    void gfxFrame(int x, int y, int w, int h, bool dotted = false) const;
    void gfxRect(int x, int y, int w, int h) const;
    void gfxInvert(int x, int y, int w, int h) const;
    void gfxClear(int x, int y, int w, int h) const;
    void gfxLine(int x, int y, int x2, int y2) const;
    void gfxLine(int x, int y, int x2, int y2, bool dotted) const;
    void gfxDottedLine(int x, int y, int x2, int y2, uint8_t p = 2) const;
    void gfxCircle(int x, int y, int r) const;
    void gfxBitmap(int x, int y, int w, const uint8_t *data) const;
    void gfxBitmapBlink(int x, int y, int w, const uint8_t *data) const;
    void gfxIcon(int x, int y, const uint8_t *data) const;
    void gfxPrintIcon(const uint8_t *data, int16_t w = 8) const;

    void gfxSkyline() const;
    void gfxHeader(const char *str, const uint8_t *icon = nullptr, int y = 2, bool underline = true) const;
    void gfxHeader(int y = 2) const;
    void DrawSlider(uint8_t x, uint8_t y, uint8_t len, uint8_t value, uint8_t max_val, bool is_cursor) const;

    inline int gfxGetPrintPosX() const { return graphics.getPrintPosX() - gfx_offset; }
    inline int gfxGetPrintPosY() const { return graphics.getPrintPosY(); }

    template<typename... Args>
    void gfxPrintfn(int x, int y, int n, const char *format,  Args ...args) {
        graphics.setPrintPos(x + gfx_offset, y);
        graphics.printf(format, args...);
    }

    // ------- Implementations below ------
    // (TODO: move to .cpp file

    bool EditInputMap(CVInputMap& input_map) {
      if (!IsEditingInputMap()) {
        selected_input_map = &input_map;
        return true;
      }
      return false;
    }

    bool EditInputMap(DigitalInputMap& input_map) {
      if (!IsEditingInputMap()) {
        selected_input_map = &input_map;
        return true;
      }
      return false;
    }

    void ClearEditInputMap() {
      selected_input_map = std::monostate{};
      if (EditMode()) CursorToggle();
    }

    bool EditSelectedInputMap(int direction) {
      if (IsEditingInputMap()) {
        switch (selected_input_map.index()) {
          case CV_INPUT_MAP: {
            int8_t& att
              = std::get<CVInputMap*>(selected_input_map)->attenuversion;
            att = constrain(att + direction, -127, 127); // 448% range
            break;
          }
          case DIGITAL_INPUT_MAP: {
            int8_t& div
              = std::get<DigitalInputMap*>(selected_input_map)->division;
            div = constrain(div + direction, -64, 64);
            break;
          }
          default:
            break;
        }
        return true;
      }
      return false;
    }

    void gfxDisplayInputMapEditor() const {
      if (selected_input_map.index()) {
        gfxClear(0, 0, 63, 11);
        switch (selected_input_map.index()) {
          case CV_INPUT_MAP: {
            int tenths = std::get<CVInputMap*>(selected_input_map)->Atten();
            gfxPos(32 - 7 * 6 / 2 + pad(10000, tenths) - 6*(abs(tenths)<10), 2);
            if (tenths < 0) gfxPrint("-");
            graphics.printf("%d.%d%%", abs(tenths) / 10, abs(tenths) % 10);
            break;
          }
          case DIGITAL_INPUT_MAP: {
            gfxPos(32 - 4 * 6 / 2, 2);
            int8_t div = std::get<DigitalInputMap*>(selected_input_map)->division;
            if (div < 0) graphics.printf("/%3d", -div + 1);
            else graphics.printf("X%3d", div + 1);
            break;
          }
          default:
            break;
        }
        gfxInvert(0, 0, 63, 11);
      }
    }

    bool IsEditingInputMap() const {
      return selected_input_map.index() > 0;
    }

    template <typename... Pairs>
    bool CheckEditInputMapPress(int cursor, Pairs&&... indexed_input_maps) {
      if (IsEditingInputMap()) {
        ClearEditInputMap();
        return !EditMode();
      } else if (!EditMode()) {
        return false;
      }

      return (
        ...
        || (cursor == indexed_input_maps.first ? EditInputMap(indexed_input_maps.second) : false)
      );
    }

protected:
    enum SelectedInputMapType {
      NONE,
      CV_INPUT_MAP,
      DIGITAL_INPUT_MAP,
    };

    std::variant<std::monostate, CVInputMap*, DigitalInputMap*>
      selected_input_map;

    HEM_SIDE hemisphere; // Which hemisphere (0, 1, ...) this applet uses
    virtual void SetHelp() = 0;

    /* Forces applet's Start() method to run the next time the applet is selected. This
     * allows an applet to start up the same way every time, regardless of previous state.
     */
    void AllowRestart() {
        applet_started = 0;
    }


    /* ADC Lag: There is a small delay between when a digital input can be read and when an ADC can be
     * read. The ADC value lags behind a bit in time. So StartADCLag() and EndADCLag() are used to
     * determine when an ADC can be read. The pattern goes like this
     *
     * if (Clock(ch)) StartADCLag(ch);
     *
     * if (EndOfADCLog(ch)) {
     *     int cv = In(ch);
     *     // etc...
     * }
     */
    void StartADCLag(size_t ch = 0, int lag_ticks = HEMISPHERE_ADC_LAG) {
        frame.adc_lag_countdown[io_offset + ch] = lag_ticks;
    }

    bool EndOfADCLag(size_t ch = 0) {
        if (frame.adc_lag_countdown[io_offset + ch] < 0) return false;
        return (--frame.adc_lag_countdown[io_offset + ch] == 0);
    }

    // --- Quantizer helpers
    int GetLatestNoteNumber(int ch) {
      return HS::GetLatestNoteNumber(ch);
    }
    int Quantize(int ch, int cv, int root = 0, int transpose = 0) {
      return HS::Quantize(ch + io_offset, cv, root, transpose);
    }
    int QuantizerLookup(int ch, int note) {
      return HS::QuantizerLookup(ch + io_offset, note);
    }
    void QuantizerConfigure(int ch, int scale, uint16_t mask = 0xffff) {
      q_engine[io_offset + ch].Configure(scale, mask);
    }
    void SetScale(int ch, int scale) {
      QuantizerConfigure(ch, scale);
    }
    int GetScale(int ch) {
      return q_engine[io_offset + ch].scale;
    }
    int GetRootNote(int ch) {
      return q_engine[io_offset + ch].root_note;
    }
    int SetRootNote(int ch, int root) {
      CONSTRAIN(root, 0, 11);
      return (q_engine[io_offset + ch].root_note = root);
    }
    void NudgeScale(int ch, int dir) {
      HS::NudgeScale(ch + io_offset, dir);
    }

private:
    bool applet_started; // Allow the app to maintain state during switching
};

} // namespace HS
#endif // _HEM_APPLET_H_
