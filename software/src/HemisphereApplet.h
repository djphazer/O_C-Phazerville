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
#include "icons.h"
#include "HSClockManager.h"

#include "HSUtils.h"
#include "HSIOFrame.h"
#include "PhzConfig.h"
#include <variant>

#define APPLET_INTERFACE_LITE(name, icon) \
public: \
  virtual const char* applet_name() { return name; } \
  virtual const uint8_t* applet_icon() { return icon; }

#define APPLET_INTERFACE(clazz, name, icon) \
protected: \
  virtual void SetHelp() final; \
public: \
  virtual const char* applet_name() { return name; } \
  virtual const uint8_t* applet_icon() { return icon; } \
  virtual void Start() final; \
  virtual void Controller() final; \
  virtual void View() final; \
  virtual uint64_t OnDataRequest() final; \
  virtual void OnDataReceive(uint64_t data) final; \
  virtual void OnEncoderMove(int direction) final

namespace HS {

enum AppletCategory : uint8_t {
  CAT_NONE = 0,
  CAT_MODULATOR = (1 << 0), // 0x01
  CAT_SEQUENCER = (1 << 1), // 0x02
  CAT_CLOCKING  = (1 << 2), // 0x04
  CAT_QUANTIZER = (1 << 3), // 0x08
  CAT_UTILITY   = (1 << 4), // 0x10
  CAT_MIDI      = (1 << 5), // 0x20
  CAT_LOGIC     = (1 << 6), // 0x40
  CAT_OTHER     = (1 << 7), // 0x80
};

class HemisphereApplet;

struct EncoderEditor {
  bool isEditing;
  bool aux_action = false;
  const char* label;
};

static constexpr bool ALWAYS_SHOW_ICONS = false;

class HemisphereApplet {
public:
    static int cursor_countdown[APPLET_CURSOR_COUNT];
    static int16_t cursor_start_x;
    static int16_t cursor_start_y;
    static const char* help[HELP_LABEL_COUNT];
    static EncoderEditor enc_edit[APPLET_CURSOR_COUNT];

    // Virtual Method signatures
    // - These need to be defined by an actual Applet implementation
    // - Some have default implementations here.
    virtual const char* applet_name() const = 0; // Maximum of 9 characters
    virtual const uint8_t* applet_icon() const { return ZAP_ICON; }
    virtual void Start() = 0;
    virtual void Reset() { };
    virtual void Controller() = 0;
    virtual void View() = 0;
    virtual uint64_t OnDataRequest() = 0;
    virtual void OnDataReceive(uint64_t data) = 0;
    virtual void OnButtonPress() { CursorToggle(); };
    virtual void OnEncoderMove(int direction) = 0;
    virtual void Unload() { }
    virtual void DrawFullScreen() {
      View();
      graphics.drawBitmap8(96 - (hemisphere & 1)*64, 28, 8, (hemisphere & 1) ? RIGHT_ICON : LEFT_ICON);
    }
    virtual void AuxButton() { CancelEdit(); }

    // Arbitrary applet data blobs, key format:
    // 5-bit preset ID
    // 3-bit HEM_SIDE slot ID - assumes no more than 8 applet slots
    // 8-bit key
    bool GetData(PhzConfig::KEY key, PhzConfig::VALUE &data) {
      return PhzConfig::getData(
        (key & 0xff) | (uint16_t(hemisphere & 0x7) << 8) | (uint16_t(preset_id) << 11),
        data
      );
    }
    void SetData(PhzConfig::KEY key, const PhzConfig::VALUE data) {
      PhzConfig::setData(
        (key & 0xff) | (uint16_t(hemisphere & 0x7) << 8) | (uint16_t(preset_id) << 11),
        data
      );
    }

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
      for (int i = 0; i < APPLET_CURSOR_COUNT; ++i) {
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
      ClearEditInputMap();
    }
    inline bool EditMode() const {
      return (enc_edit[hemisphere].isEditing);
    }
    void SetLabel(const char *str) { enc_edit[hemisphere].label = str; }
    void SetAux(bool aux) { enc_edit[hemisphere].aux_action = aux; }

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
    int ViewIn(int ch) const {return frame.In(io_offset + ch);}
    int ViewOut(int ch) const {return frame.ViewOut(io_offset + ch);}
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
    float InF(int ch, int max = HEMISPHERE_MAX_INPUT_CV) const;
    // Apply small center detent to input, so it reads zero before a threshold
    int DetentedIn(int ch) const;
    int SemitoneIn(int ch) const;
    /* Has the specified Digital input been clocked this cycle? (rising edge of a gate)
     * This is pre-calculated in HS::IOFrame::Load() according to input mappings and internal clock settings
     */
    bool Clock(int ch, bool physical = 0) const;
    bool Gate(int ch) const;

    // outputs
    void Out(int ch, int value) const;
    // TODO: rework or delete
    [[deprecated("SmoothedOut() was a bad idea")]]
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
    const int ProportionCV(const int cv_value, const int max_pixels, const int max_cv = HEMISPHERE_MAX_CV) const {
        int prop = constrain(Proportion(cv_value, max_cv, max_pixels), 0, max_pixels);
        return prop;
    }

    // -------------------------------
    // --- Offset graphics methods ---
    // -------------------------------
    void gfxCursor(int x, int y, int w, int h = 9, const char *str = nullptr, const char *extra_str = nullptr) const;
    void gfxCursor(int x, int y, int w, const char *str, const char *extra_str = nullptr) const {
      gfxCursor(x, y, w, 9, str, extra_str);
    }
    void gfxSpicyCursor(int x, int y, int w, int h = 9, const char *str = nullptr, const char *extra_str = nullptr) const;
    void gfxSpicyCursor(int x, int y, int w, const char *str) const { gfxSpicyCursor(x, y, w, 9, str); }
    void gfxPos(int x, int y) const;
    void gfxPrint(int x, int y, const char *str) const;
    void gfxPrint(int x, int y, int num) const;
    void gfxPrint(const char *str) const;
    void gfxPrint(int num) const;
    void gfxPrint(int x, int y, float num, int digits) const;
    void gfxPrint(float num, int digits) const;
    void gfxPrint(CVInputMap &map) const;
    void gfxPrint(DigitalInputMap &map) const;
    void gfxPrint(int x, int y, HS::QuantEngine &q_eng, bool overlay = true) const;
    void gfxPrint(int x_adv, int num) const; // Print number with character padding

    void gfxStartCursor(int x, int y);
    void gfxStartCursor();
    void gfxEndCursor(bool selected, bool spicy = false, const char *str = nullptr, const char *extra_str = nullptr) const;
    void gfxEndCursor(bool selected, const char *extra_str) const {
      gfxEndCursor(selected, false, nullptr, extra_str);
    }

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
    void gfxIcon(int x, int y, const uint8_t *data, bool clearfirst = false) const;
    void gfxPrintIcon(const uint8_t *data, int16_t w = 8) const;

    void gfxSkyline() const;
    void gfxHeader(const char *str, const uint8_t *icon = nullptr, int y = 2, bool underline = true) const;
    void gfxHeader(int y = 2) const;
    void gfxParamHeader() const;
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
            std::get<DigitalInputMap*>(selected_input_map)->div_mult.Adjust(direction);
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
            int tenths = Atten(std::get<CVInputMap*>(selected_input_map)->attenuversion);
            gfxPos(32 - 7 * 6 / 2 + pad(10000, tenths) - 6*(abs(tenths)<10), 2);
            if (tenths < 0) gfxPrint("-");
            graphics.printf("%d.%d%%", abs(tenths) / 10, abs(tenths) % 10);
            break;
          }
          case DIGITAL_INPUT_MAP: {
            gfxPos(32 - 4 * 6 / 2, 2);
            DigitalInputMap* map = std::get<DigitalInputMap*>(selected_input_map);
            int8_t div = map->div_mult.steps;
            if (map->is_clock()) graphics.print(1 + 3*(map->index())); // "1" or "4"
            if (div > 0) graphics.printf("/%2d", div);
            else graphics.printf("x%2d", -div);
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
