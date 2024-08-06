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
#include <optional>
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
#include "HSClockManager.h"

#include "HSUtils.h"
#include "HSIOFrame.h"

#ifdef ARDUINO_TEENSY41
#include <Audio.h>
#endif

class HemisphereApplet;

namespace HS {

class BaseApplet {
public:
    BaseApplet(const int id, const uint8_t categories) : id(id), categories(categories) {}
    const int id;
    const uint8_t categories;
    virtual HemisphereApplet & GetInstance(int slot) = 0;
};

template<class A, int Id, uint8_t Categories>
class StaticApplet : public BaseApplet {
public:
    StaticApplet() : BaseApplet(Id, Categories) {}
    A instance[APPLET_SLOTS];

    HemisphereApplet & GetInstance(int slot) override {
        return instance[slot];
    }
};

extern IOFrame frame;

}

using namespace HS;

enum AudioChannels {
  None = 0,
  Mono = 1,
  Stereo = 2,
};

class HemisphereApplet {
public:
    static int cursor_countdown[APPLET_SLOTS];
    static const char* help[HELP_LABEL_COUNT];

    virtual const char* applet_name() = 0; // Maximum of 9 characters
    const char* const OutputLabel(int ch) {
      return OC::Strings::capital_letters[ch + io_offset];
    }

    virtual void Start() = 0;
    virtual void Controller() = 0;
    virtual void View() = 0;
    virtual uint64_t OnDataRequest() = 0;
    virtual void OnDataReceive(uint64_t data) = 0;
    virtual void OnButtonPress() { CursorToggle(); };
    virtual void OnEncoderMove(int direction) = 0;

    #ifdef ARDUINO_TEENSY41
    template <typename T> using optional_ref = std::optional<std::reference_wrapper<T>>;
    virtual optional_ref<AudioStream> InputStream() { return std::nullopt; }
    virtual optional_ref<AudioStream> OutputStream() { return std::nullopt; }
    virtual AudioChannels NumAudioChannels() { return None; }
    #endif

    //void BaseStart(const HEM_SIDE hemisphere_);
    void BaseController();
    void BaseView(bool full_screen = false);

    void BaseStart(const HEM_SIDE hemisphere_) {
        hemisphere = hemisphere_;

        // Initialize some things for startup
        cursor_countdown[hemisphere] = HEMISPHERE_CURSOR_TICKS;

        // Maintain previous app state by skipping Start
        if (!applet_started) {
            applet_started = true;
            Start();
            ForEachChannel(ch) {
                Out(ch, 0); // reset outputs
            }
        }
    }
    virtual void Unload() { }

    // Screensavers are deprecated in favor of screen blanking, but the BaseScreensaverView() remains
    // to avoid breaking applets based on the old boilerplate
    void BaseScreensaverView() {}

    /* Formerly Help Screen */
    virtual void DrawFullScreen() {
        for (int i=0; i<HELP_LABEL_COUNT; ++i) help[i] = "";
        SetHelp();
        const bool clockrun = HS::clock_m.IsRunning();

        for (int ch = 0; ch < 2; ++ch) {
          int y = 14;
          const int mult = clockrun ? HS::clock_m.GetMultiply(ch + io_offset) : 0;

          graphics.setPrintPos(ch*64, y);
          if (mult != 0) { // Multipliers
            graphics.print( (mult > 0) ? "x" : "/" );
            graphics.print( (mult > 0) ? mult : 1 - mult );
          } else { // Trigger mapping
            graphics.print( OC::Strings::trigger_input_names_none[ HS::trigger_mapping[ch + io_offset] ] );
          }
          graphics.invertRect(ch*64, y - 1, 19, 9);

          graphics.setPrintPos(ch*64 + 20, y);
          graphics.print( help[HELP_DIGITAL1 + ch] );

          y += 10;

          graphics.setPrintPos(ch*64, y);
          graphics.print( OC::Strings::cv_input_names_none[ HS::cvmapping[ch + io_offset] ] );
          graphics.invertRect(ch*64, y - 1, 19, 9);

          graphics.setPrintPos(ch*64 + 20, y);
          graphics.print( help[HELP_CV1 + ch] );

          y += 10;

          graphics.setPrintPos(6 + ch*64, y);
          graphics.print( OC::Strings::capital_letters[ ch + io_offset ] );
          graphics.invertRect(ch*64, y - 1, 19, 9);

          graphics.setPrintPos(ch*64 + 20, y);
          graphics.print( help[HELP_OUT1 + ch] );
        }

        graphics.setPrintPos(0, 45);
        graphics.print( help[HELP_EXTRA1] );
        graphics.setPrintPos(0, 55);
        graphics.print( help[HELP_EXTRA2] );
    }
    virtual void AuxButton() {
      isEditing = false;
    }

    /* Check cursor blink cycle. */
    bool CursorBlink() { return (cursor_countdown[hemisphere] > 0); }
    void ResetCursor() { cursor_countdown[hemisphere] = HEMISPHERE_CURSOR_TICKS; }

    // legacy cursor mode has been removed
    [[deprecated("Use CursorToggle() instead")]] void CursorAction(int &cursor, int max) {
      CursorToggle();
    }

    void CursorToggle() {
      isEditing = !isEditing;
      ResetCursor();
    }
    void MoveCursor(int &cursor, int direction, int max) {
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
    int ClockCycleTicks(int ch) {return frame.cycle_ticks[io_offset + ch];}
    bool Changed(int ch) {return frame.changed_cv[io_offset + ch];}

    //////////////// Offset I/O methods
    ////////////////////////////////////////////////////////////////////////////////
    int In(const int ch) {
        const int c = cvmapping[ch + io_offset];
        if (!c) return 0;
        return (c <= ADC_CHANNEL_LAST) ? frame.inputs[c - 1] : frame.outputs[c - 1 - ADC_CHANNEL_LAST];
    }

    float InF(int ch) {
        return static_cast<float>(In(ch)) / HEMISPHERE_MAX_INPUT_CV;
    }

    // Apply small center detent to input, so it reads zero before a threshold
    int DetentedIn(int ch) {
        return (In(ch) > (HEMISPHERE_CENTER_CV + HEMISPHERE_CENTER_DETENT) || In(ch) < (HEMISPHERE_CENTER_CV - HEMISPHERE_CENTER_DETENT))
            ? In(ch) : HEMISPHERE_CENTER_CV;
    }
    int SmoothedIn(int ch) {
      const int x = cvmapping[ch + io_offset];
      if (x && x <= ADC_CHANNEL_LAST) {
        ADC_CHANNEL channel = (ADC_CHANNEL)( x - 1 );
        return OC::ADC::value(channel);
      }
      return 0;
    }
    int SemitoneIn(int ch) {
      return input_quant[ch].Process(In(ch));
    }

    // defined in HemisphereApplet.cpp
    bool Clock(int ch, bool physical = 0);

    bool Gate(int ch) {
        const int t = trigger_mapping[ch + io_offset];
        const int offset = OC::DIGITAL_INPUT_LAST + ADC_CHANNEL_LAST;
        if (!t) return false;
        return (t <= offset) ? frame.gate_high[t - 1] : (frame.outputs[t - 1 - offset] > GATE_THRESHOLD);
    }
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
    braids::Quantizer* GetQuantizer(int ch) {
      return &HS::quantizer[io_offset + ch];
    }
    int GetLatestNoteNumber(int ch) {
      return HS::quantizer[io_offset + ch].GetLatestNoteNumber();
    }
    int Quantize(int ch, int cv, int root = 0, int transpose = 0) {
      return HS::Quantize(ch + io_offset, cv, root, transpose);
    }
    int QuantizerLookup(int ch, int note) {
      return HS::QuantizerLookup(ch + io_offset, note);
    }
    void SetScale(int ch, int scale) {
      QuantizerConfigure(ch, scale);
    }
    void QuantizerConfigure(int ch, int scale, uint16_t mask = 0xffff) {
      HS::QuantizerConfigure(ch + io_offset, scale, mask);
    }
    int GetScale(int ch) {
      return HS::quant_scale[io_offset + ch];
    }
    int GetRootNote(int ch) {
      return HS::root_note[io_offset + ch];
    }
    int SetRootNote(int ch, int root) {
      CONSTRAIN(root, 0, 11);
      return (HS::root_note[io_offset + ch] = root);
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
        return (isEditing);
    }

    // Override HSUtils function to only return positive values
    // Not ideal, but too many applets rely on this.
    constexpr int ProportionCV(const int cv_value, const int max_pixels) {
        int prop = constrain(Proportion(cv_value, HEMISPHERE_MAX_INPUT_CV, max_pixels), 0, max_pixels);
        return prop;
    }

    //////////////// Offset graphics methods
    ////////////////////////////////////////////////////////////////////////////////
    void gfxCursor(int x, int y, int w, int h = 9) { // assumes standard text height for highlighting
      if (isEditing) {
        gfxInvert(x, y - h, w, h);
      } else if (CursorBlink()) {
        gfxLine(x, y, x + w - 1, y);
        gfxPixel(x, y-1);
        gfxPixel(x + w - 1, y-1);
      }
    }
    void gfxSpicyCursor(int x, int y, int w, int h = 9) {
      if (isEditing) {
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
    void gfxPrint(int x_adv, int num) { // Print number with character padding
        for (int c = 0; c < (x_adv / 6); c++) gfxPrint(" ");
        gfxPrint(num);
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

    void gfxHeader(const char *str) {
        gfxPrint(1, 2, str);
        gfxDottedLine(0, 10, 62, 10);
        //gfxLine(0, 11, 62, 11);
    }

    void DrawSlider(uint8_t x, uint8_t y, uint8_t len, uint8_t value, uint8_t max_val, bool is_cursor) {
        uint8_t p = is_cursor ? 1 : 3;
        uint8_t w = Proportion(value, max_val, len-1);
        gfxDottedLine(x, y + 4, x + len, y + 4, p);
        gfxRect(x + w, y, 2, 8);
        if (EditMode() && is_cursor) gfxInvert(x-1, y, len+3, 8);
    }

protected:
    HEM_SIDE hemisphere; // Which hemisphere (0, 1, ...) this applet uses
    bool isEditing = false; // modal editing toggle
    virtual void SetHelp() = 0;

    #ifdef ARDUINO_TEENSY41
    AudioConnection passThru{};
    #endif

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
    void StartADCLag(size_t ch = 0) {
        frame.adc_lag_countdown[io_offset + ch] = HEMISPHERE_ADC_LAG;
    }

    bool EndOfADCLag(size_t ch = 0) {
        if (frame.adc_lag_countdown[io_offset + ch] < 0) return false;
        return (--frame.adc_lag_countdown[io_offset + ch] == 0);
    }

private:
    bool applet_started; // Allow the app to maintain state during switching
};

#endif // _HEM_APPLET_H_
