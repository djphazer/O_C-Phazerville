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

////////////////////////////////////////////////////////////////////////////////
//// Hemisphere Applet Base Class
////////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef _HEM_APPLET_H_
#define _HEM_APPLET_H_

#include <Arduino.h>
#include "OC_core.h"

#include "OC_digital_inputs.h"
#include "OC_DAC.h"
#include "OC_ADC.h"
#include "src/drivers/FreqMeasure/OC_FreqMeasure.h"
#include "util/util_math.h"
#include "bjorklund.h"
#include "HSicons.h"
#include "PhzIcons.h"
#include "HSClockManager.h"

#include "HSUtils.h"
#include "HSIOFrame.h"
#include <cstdint>
#include <variant>

class HemisphereApplet;

namespace HS {

struct Applet {
  const int id;
  const uint8_t categories;
  std::array<HemisphereApplet *, APPLET_SLOTS> instance;
};

struct EncoderEditor {
  bool isEditing;
};

static constexpr bool ALWAYS_SHOW_ICONS = false;
} // namespace HS

using namespace HS;

class HemisphereApplet {
public:
    static int cursor_countdown[APPLET_SLOTS + 1];
    static int16_t cursor_start_x;
    static int16_t cursor_start_y;
    static const char* help[HELP_LABEL_COUNT];
    static EncoderEditor enc_edit[APPLET_SLOTS + 1];

    static void ProcessCursors() {
      // Cursor countdowns. See CursorBlink(), ResetCursor(), gfxCursor()
      for (int i = 0; i < APPLET_SLOTS + 1; ++i) {
        if (--cursor_countdown[i] < -HEMISPHERE_CURSOR_TICKS)
          cursor_countdown[i] = HEMISPHERE_CURSOR_TICKS;
      }
    }

    virtual const char* applet_name() = 0; // Maximum of 9 characters
    virtual const uint8_t* applet_icon() { return ZAP_ICON; }
    const char* const OutputLabel(int ch) {
      return OC::Strings::capital_letters[ch + io_offset];
    }

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

    void BaseView(bool full_screen = false, bool parked = true);
    void BaseStart(const HEM_SIDE hemisphere_);

    /* Formerly Help Screen */
    void DrawConfigHelp();

    /* Check cursor blink cycle. */
    bool CursorBlink() { return (cursor_countdown[hemisphere] > 0); }
    void ResetCursor() { cursor_countdown[hemisphere] = HEMISPHERE_CURSOR_TICKS; }

    void CursorToggle() {
      enc_edit[hemisphere].isEditing ^= 1;
      ResetCursor();
    }
    void CancelEdit() {
      enc_edit[hemisphere].isEditing = false;
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

    // Buffered I/O functions
    int ViewIn(int ch) {return frame.inputs[io_offset + ch];}
    int ViewOut(int ch) {return frame.outputs[io_offset + ch];}
    uint32_t ClockCycleTicks(int ch) {
      if (clock_m.IsRunning() && clock_m.GetMultiply(io_offset + ch) != 0)
          return clock_m.GetCycleTicks(io_offset + ch);
      return frame.cycle_ticks[io_offset + ch];
    }
    bool Changed(int ch) {return frame.changed_cv[io_offset + ch];}

    // --- CV Input Methods
    int In(const int ch) {
      return cvmap[ch + io_offset].In();
    }

#ifdef __IMXRT1062__
    float InF(int ch) {
        return static_cast<float>(In(ch)) / HEMISPHERE_MAX_INPUT_CV;
    }
#endif

    // Apply small center detent to input, so it reads zero before a threshold
    int DetentedIn(int ch) {
        if (NorthernLightModular && In(ch) < HEMISPHERE_CENTER_DETENT)
          return 0;

        if (In(ch) > (HEMISPHERE_CENTER_INPUT_CV + HEMISPHERE_CENTER_DETENT)
          || In(ch) < (HEMISPHERE_CENTER_INPUT_CV - HEMISPHERE_CENTER_DETENT))
          return In(ch);

        return HEMISPHERE_CENTER_INPUT_CV;
    }
    int SemitoneIn(int ch) {
      return input_quant[ch + io_offset].Process(In(ch));
    }

    /* Has the specified Digital input been clocked this cycle? (rising edge of a gate)
     * This is pre-calculated in HS::IOFrame::Load() according to input mappings and internal clock settings
     */
    bool Clock(int ch, bool physical = 0) {
        return frame.clocked[ch + io_offset];
    }
    bool Gate(int ch) {
        return trigmap[ch + io_offset].Gate();
    }

    // --- CV Output methods
    void Out(int ch, int value, int octave = 0) {
        frame.Out( (DAC_CHANNEL)(ch + io_offset), value + (octave * (12 << 7)));
    }

    void SmoothedOut(int ch, int value, int kSmoothing) {
      if (OC::CORE::ticks % kSmoothing == 0) {
        DAC_CHANNEL channel = (DAC_CHANNEL)(ch + io_offset);
        value = (frame.outputs_smooth[channel] * (kSmoothing - 1) + value) / kSmoothing;
        frame.outputs[channel] = frame.outputs_smooth[channel] = value;
      }
    }
    void ClockOut(const int ch, const int ticks = HEMISPHERE_CLOCK_TICKS * trig_length) {
        frame.ClockOut( (DAC_CHANNEL)(io_offset + ch), ticks);
    }
    void GateOut(int ch, bool high) {
        Out(ch, 0, (high ? PULSE_VOLTAGE : 0));
    }

    // Quantizer helpers
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

    // Standard bi-polar CV modulation scenario
    template <typename T>
    void Modulate(T &param, const int ch, const int min = 0, const int max = 255) {
        // small ranges use Semitone quantizer for hysteresis
        int increment = (max < 70) ? SemitoneIn(ch) :
          Proportion(DetentedIn(ch), HEMISPHERE_MAX_INPUT_CV, max);
        param = constrain(param + increment, min, max);
    }

    inline bool EditMode() {
        return (enc_edit[hemisphere].isEditing);
    }

    // Override HSUtils function to only return positive values
    // Not ideal, but too many applets rely on this.
    const int ProportionCV(const int cv_value, const int max_pixels, const int max_cv = HEMISPHERE_MAX_CV) {
        int prop = constrain(Proportion(cv_value, max_cv, max_pixels), 0, max_pixels);
        return prop;
    }

    //////////////// Offset graphics methods
    ////////////////////////////////////////////////////////////////////////////////
    void gfxCursor(int x, int y, int w, int h = 9) { // assumes standard text height for highlighting
      if (EditMode()) {
        gfxInvert(x, y - h, w, h);
      } else if (CursorBlink()) {
        gfxLine(x, y, x + w - 1, y);
        gfxPixel(x, y-1);
        gfxPixel(x + w - 1, y-1);
      }
    }
    void gfxSpicyCursor(int x, int y, int w, int h = 9) {
      if (EditMode()) {
        if (CursorBlink())
          gfxFrame(x, y - h, w, h, true);
        gfxInvert(x, y - h, w, h);
      } else {
        gfxLine(x - CursorBlink(), y, x + w - 1, y, 2);
        gfxPixel(x, y-1);
        gfxPixel(x + w - 1, y-1);
      }
    }

    void gfxPos(int x, int y) {
        graphics.setPrintPos(x + gfx_offset, y);
    }

    inline int gfxGetPrintPosX() {
        return graphics.getPrintPosX() - gfx_offset;
    }

    inline int gfxGetPrintPosY() {
        return graphics.getPrintPosY();
    }

    void gfxPrint(int x, int y, const char *str) {
        graphics.setPrintPos(x + gfx_offset, y);
        graphics.print(str);
    }
    void gfxPrint(int x, int y, int num) {
        graphics.setPrintPos(x + gfx_offset, y);
        graphics.print(num);
    }
    void gfxPrint(const char *str) {
        graphics.print(str);
    }
    void gfxPrint(int num) {
        graphics.print(num);
    }

    void gfxPrint(int x, int y, float num, int digits) {
        graphics.setPrintPos(x + gfx_offset, y);
        gfxPrint(num, digits);
    }

    void gfxPrint(float num, int digits) {
        int i = static_cast<int>(num);
        float dec = num - i;
        gfxPrint(i);
        if (digits > 0) {
            gfxPrint(".");
            while (digits--) {
                dec *= 10;
                i = static_cast<int>(dec);
                gfxPrint(i);
                dec -= i;
            }
        }
    }

    void gfxPrint(DigitalInputMap &map) {
      gfxPrintIcon(map.Icon());
      if (map.Gate()) gfxInvert(gfxGetPrintPosX()-8, gfxGetPrintPosY(), 8, 8);
    }
    void gfxPrint(CVInputMap &map) {
      gfxPrintIcon(map.Icon());
      const int xpos = gfxGetPrintPosX() - 1;
      const int ypos = gfxGetPrintPosY() + 4;
      const int height = map.InRescaled(24);
      gfxLine(xpos, ypos, xpos, ypos - height);
    }

    void gfxStartCursor(int x, int y) {
        gfxPos(x, y);
        gfxStartCursor();
    }

    void gfxStartCursor() {
        cursor_start_x = gfxGetPrintPosX();
        cursor_start_y = gfxGetPrintPosY();
    }

    void gfxEndCursor(bool selected, bool spicy = false, const char *str = nullptr) {
        if (selected) {
          if (str) {
            gfxClear(cursor_start_x - 14, cursor_start_y-1, 24, 10);
            gfxFrame(cursor_start_x - 13, cursor_start_y-1, 22, 10, spicy);
            gfxPrint(cursor_start_x - 11, cursor_start_y+1, str);
            if (EditMode())
              gfxInvert(cursor_start_x - 14, cursor_start_y-1, 24, 10);
          } else {
            int16_t w = gfxGetPrintPosX() - cursor_start_x;
            int16_t y = gfxGetPrintPosY() + 8;
            int h = y - cursor_start_y;
            if (spicy)
              gfxSpicyCursor(cursor_start_x, y, w, h);
            else
              gfxCursor(cursor_start_x, y, w, h);
          }
        }
    }

    void gfxPrint(int x_adv, int num) { // Print number with character padding
        for (int c = 0; c < (x_adv / 6); c++) gfxPrint(" ");
        gfxPrint(num);
    }

    template<typename... Args>
    void gfxPrintfn(int x, int y, int n, const char *format,  Args ...args) {
        graphics.setPrintPos(x + gfx_offset, y);
        graphics.printf(format, args...);
    }

    /* Convert CV value to voltage level and print  to two decimal places */
    void gfxPixel(int x, int y) {
        graphics.setPixel(x + gfx_offset, y);
    }

    void gfxFrame(int x, int y, int w, int h, bool dotted = false) {
      if (dotted) {
        gfxLine(x, y, x + w - 1, y, 2); // top
        gfxLine(x, y + 1, x, y + h - 1, 2); // vert left
        gfxLine(x + w - 1, y + 1, x + w - 1, y + h - 1, 2); // vert rigth
        gfxLine(x, y + h - 1, x + w - 1, y + h - 1, 2); // bottom
      } else
        graphics.drawFrame(x + gfx_offset, y, w, h);
    }

    void gfxRect(int x, int y, int w, int h) {
        graphics.drawRect(x + gfx_offset, y, w, h);
    }

    void gfxInvert(int x, int y, int w, int h) {
        graphics.invertRect(x + gfx_offset, y, w, h);
    }

    void gfxClear(int x, int y, int w, int h) {
        graphics.clearRect(x + gfx_offset, y, w, h);
    }

    void gfxLine(int x, int y, int x2, int y2) {
        graphics.drawLine(x + gfx_offset, y, x2 + gfx_offset, y2);
    }

    void gfxLine(int x, int y, int x2, int y2, bool dotted) {
        graphics.drawLine(x + gfx_offset, y, x2 + gfx_offset, y2, dotted ? 2 : 1);
    }

    void gfxDottedLine(int x, int y, int x2, int y2, uint8_t p = 2) {
        graphics.drawLine(x + gfx_offset, y, x2 + gfx_offset, y2, p);
    }

    void gfxCircle(int x, int y, int r) {
        graphics.drawCircle(x + gfx_offset, y, r);
    }

    void gfxBitmap(int x, int y, int w, const uint8_t *data) {
        graphics.drawBitmap8(x + gfx_offset, y, w, data);
    }

    void gfxBitmapBlink(int x, int y, int w, const uint8_t *data) {
        if (CursorBlink()) gfxBitmap(x, y, w, data);
    }

    void gfxIcon(int x, int y, const uint8_t *data) {
        gfxBitmap(x, y, 8, data);
    }

    void gfxPrintIcon(const uint8_t *data, int16_t w = 8) {
        gfxIcon(gfxGetPrintPosX(), gfxGetPrintPosY(), data);
        gfxPos(gfxGetPrintPosX() + w, gfxGetPrintPosY());
    }

    //////////////// Hemisphere-specific graphics methods
    ////////////////////////////////////////////////////////////////////////////////

    /* Show channel-grouped bi-lateral display */
    void gfxSkyline() {
        ForEachChannel(ch)
        {
            int height = ProportionCV(In(ch), 32);
            gfxFrame(23 + (10 * ch), BottomAlign(height), 6, 63);

            height = ProportionCV(ViewOut(ch), 32);
            gfxInvert(3 + (46 * ch), BottomAlign(height), 12, 63);
        }
    }

    void gfxHeader(int y = 2) {
      gfxHeader(applet_name(), applet_icon(), y, false);
    }

    void gfxHeader(const char *str, const uint8_t *icon = nullptr, int y = 2,
        bool underline = true) {
      int x = 1;
      if (icon) {
        gfxIcon(x, y, icon);
        x += 9;
      }
      if (hemisphere & 1) // right side
        x = 62 - strlen(str) * 6;
      gfxPrint(x, y, str);
      if (underline)
        gfxDottedLine(0, y + 8, 62, y + 8);
    }

    void DrawSlider(uint8_t x, uint8_t y, uint8_t len, uint8_t value, uint8_t max_val, bool is_cursor) {
        uint8_t p = is_cursor ? 1 : 3;
        uint8_t w = Proportion(value, max_val, len-1);
        gfxDottedLine(x, y + 4, x + len, y + 4, p);
        gfxRect(x + w, y, 2, 8);
        if (EditMode() && is_cursor) gfxInvert(x-1, y, len+3, 8);
    }

    void SetDisplaySide(HEM_SIDE side) {
        hemisphere = side;
    }

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

    void gfxDisplayInputMapEditor() {
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

private:
    bool applet_started; // Allow the app to maintain state during switching
};

#endif // _HEM_APPLET_H_
