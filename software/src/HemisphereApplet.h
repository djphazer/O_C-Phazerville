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

#include <Arduino.h>
#include "OC_core.h"
#include "OC_digital_inputs.h"
#include "OC_DAC.h"
#include "OC_ADC.h"
#include "src/drivers/FreqMeasure/OC_FreqMeasure.h"
#include "util/util_math.h"
#include "HSicons.h"
#include "HSClockManager.h"

#define LEFT_HEMISPHERE 0
#define RIGHT_HEMISPHERE 1
#ifdef BUCHLA_4U
#define PULSE_VOLTAGE 8
#define HEMISPHERE_MAX_CV 15360
#define HEMISPHERE_CENTER_CV 7680 // 5V
#define HEMISPHERE_MIN_CV 0
#elif defined(VOR)
#define PULSE_VOLTAGE 8
#define HEMISPHERE_MAX_CV (HS::octave_max * 12 << 7)
#define HEMISPHERE_CENTER_CV 0
#define HEMISPHERE_MIN_CV (HEMISPHERE_MAX_CV - 15360)
#else
#define PULSE_VOLTAGE 5
#define HEMISPHERE_MAX_CV 9216 // 6V
#define HEMISPHERE_CENTER_CV 0
#define HEMISPHERE_MIN_CV -4608 // -3V
#endif
#define HEMISPHERE_3V_CV 4608
#define HEMISPHERE_MAX_INPUT_CV 9216 // 6V
#define HEMISPHERE_CENTER_DETENT 80
#define HEMISPHERE_CLOCK_TICKS 17 // one millisecond
#define HEMISPHERE_CURSOR_TICKS 12000
#define HEMISPHERE_ADC_LAG 33
#define HEMISPHERE_CHANGE_THRESHOLD 32

// Codes for help system sections
enum HEM_HELP_SECTIONS {
HEMISPHERE_HELP_DIGITALS = 0,
HEMISPHERE_HELP_CVS = 1,
HEMISPHERE_HELP_OUTS = 2,
HEMISPHERE_HELP_ENCODER = 3
};
static const char * HEM_HELP_SECTION_NAMES[4] = {"Dig", "CV", "Out", "Enc"};

// Hemisphere-specific macros
#define BottomAlign(h) (62 - h)
#define ForEachChannel(ch) for(int_fast8_t ch = 0; ch < 2; ++ch)
#define ForAllChannels(ch) for(int_fast8_t ch = 0; ch < 4; ++ch)
#define gfx_offset (hemisphere * 64) // Graphics offset, based on the side
#define io_offset (hemisphere * 2) // Input/Output offset, based on the side

#define HEMISPHERE_SIM_CLICK_TIME 1000
#define HEMISPHERE_DOUBLE_CLICK_TIME 8000
#define HEMISPHERE_PULSE_ANIMATION_TIME 500
#define HEMISPHERE_PULSE_ANIMATION_TIME_LONG 1200

#include "OC_scales.h"
#include "HSUtils.h"
#include "HSIOFrame.h"

class HemisphereApplet;

namespace HS {

typedef struct Applet {
  int id;
  uint8_t categories;
  HemisphereApplet* instance;
  /*
  void (*Start)(bool); // Initialize when selected
  void (*Controller)(bool, bool);  // Interrupt Service Routine
  void (*View)(bool);  // Draw main view
  void (*OnButtonPress)(bool); // Encoder button has been pressed
  void (*OnEncoderMove)(bool, int); // Encoder has been rotated
  void (*ToggleHelpScreen)(bool); // Help Screen has been requested
  uint64_t (*OnDataRequest)(bool); // Get a data int from the applet
  void (*OnDataReceive)(bool, uint64_t); // Send a data int to the applet
  */
} Applet;

// editable by cursor, and CV assignable
typedef struct {
    uint8_t &p; int min, max;
    uint8_t x, y, w;
} AppletParam;

static IOFrame frame;

extern Applet available_applets[];
extern Applet clock_setup_applet;

}

using namespace HS;

class HemisphereApplet {
public:
    static int cursor_countdown[2];
    static int cursor[2];

    virtual const char* applet_name() = 0; // Maximum of 9 characters
    virtual void Start() = 0;
    virtual void Controller() = 0;
    virtual void View() = 0;
    virtual uint64_t OnDataRequest() = 0;
    virtual void OnDataReceive(uint64_t data) = 0;
    virtual void OnButtonPress() = 0;
    virtual void OnEncoderMove(int direction) = 0;

    void BaseStart(bool hemisphere_);
    void BaseController();
    void BaseView();

    // Screensavers are deprecated in favor of screen blanking, but the BaseScreensaverView() remains
    // to avoid breaking applets based on the old boilerplate
    void BaseScreensaverView() {}

    /* Help Screen Toggle */
    void ToggleHelpScreen() { full_screen = 1 - full_screen; }
    virtual void DrawFullScreen() {
        SetHelp();

        // TODO: standard CV input assignment stuff, capture the cursor somehow
        for (int section = 0; section < 4; section++)
        {
            int y = section * 12 + 16;
            graphics.setPrintPos(0, y);
            graphics.print( HEM_HELP_SECTION_NAMES[section] );
            graphics.invertRect(0, y - 1, 19, 9);

            graphics.setPrintPos(20, y);
            graphics.print(help[section]);
        }
    }

    /* Check cursor blink cycle. */
    bool CursorBlink() { return (cursor_countdown[hemisphere] > 0); }
    void ResetCursor() { cursor_countdown[hemisphere] = HEMISPHERE_CURSOR_TICKS; }

    // handle modal edit mode toggle or cursor advance
    void CursorAction(int &cursor, int max) {
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
    int In(int ch) {
        return frame.inputs[io_offset + ch];
    }

    // Apply small center detent to input, so it reads zero before a threshold
    int DetentedIn(int ch) {
        return (In(ch) > (HEMISPHERE_CENTER_CV + HEMISPHERE_CENTER_DETENT) || In(ch) < (HEMISPHERE_CENTER_CV - HEMISPHERE_CENTER_DETENT))
            ? In(ch) : HEMISPHERE_CENTER_CV;
    }
    int SmoothedIn(int ch) {
        ADC_CHANNEL channel = (ADC_CHANNEL)(ch + io_offset);
        return OC::ADC::value(channel);
    }

    bool Clock(int ch, bool physical = 0);

    bool Gate(int ch) {
        const int t = trigger_mapping[ch + io_offset];
        return (t && t < 5) ? frame.gate_high[t - 1] : false;
    }
    void Out(int ch, int value, int octave = 0) {
        frame.Out( (DAC_CHANNEL)(ch + io_offset), value + (octave * (12 << 7)));
    }

    void SmoothedOut(int ch, int value, int kSmoothing) {
        DAC_CHANNEL channel = (DAC_CHANNEL)(ch + io_offset);
        value = (frame.outputs_smooth[channel] * (kSmoothing - 1) + value) / kSmoothing;
        frame.outputs[channel] = frame.outputs_smooth[channel] = value;
    }
    void ClockOut(const int ch, const int ticks = HEMISPHERE_CLOCK_TICKS * trig_length) {
        frame.ClockOut( (DAC_CHANNEL)(io_offset + ch), ticks);
    }

    void GateOut(int ch, bool high) {
        Out(ch, 0, (high ? PULSE_VOLTAGE : 0));
    }


    braids::Quantizer* GetQuantizer(int ch) {
        return &HS::quantizer[io_offset + ch];
    }
    int Quantize(int ch, int cv, int root, int transpose) {
        return HS::quantizer[io_offset + ch].Process(cv, root, transpose);
    }
    int QuantizerLookup(int ch, int note) {
        return HS::quantizer[io_offset + ch].Lookup(note) + (HS::root_note[io_offset+ch] << 7);
    }
    void QuantizerConfigure(int ch, int scale, uint16_t mask = 0xffff) {
        HS::quant_scale[io_offset + ch] = scale;
        HS::quantizer[io_offset + ch].Configure(OC::Scales::GetScale(scale), mask);
    }
    int GetScale(int ch) {
        return HS::quant_scale[io_offset + ch];
    }
    int GetRootNote(int ch) {
        return HS::root_note[io_offset + ch];
    }
    int SetRootNote(int ch, int root) {
        return (HS::root_note[io_offset + ch] = root);
    }
    void NudgeScale(int ch, int dir) {
        const int max = OC::Scales::NUM_SCALES;
        int &s = HS::quant_scale[io_offset + ch];

        s+= dir;
        if (s >= max) s = 0;
        if (s < 0) s = max - 1;
        QuantizerConfigure(ch, s);
    }

    // Standard bi-polar CV modulation scenario
    template <typename T>
    void Modulate(T &param, const int ch, const int min = 0, const int max = 255) {
        int cv = DetentedIn(ch);
        param = constrain(param + Proportion(cv, HEMISPHERE_MAX_INPUT_CV, max), min, max);
    }

    bool EditMode() {
        return (isEditing);
    }

    // Override HSUtils function to only return positive values
    // Not ideal, but too many applets rely on this.
    int ProportionCV(int cv_value, int max_pixels) {
        int prop = constrain(Proportion(cv_value, HEMISPHERE_MAX_INPUT_CV, max_pixels), 0, max_pixels);
        return prop;
    }

    //////////////// Offset graphics methods
    ////////////////////////////////////////////////////////////////////////////////
    void gfxCursor(int x, int y, int w, int h = 9) { // assumes standard text height for highlighting
        if (isEditing) gfxInvert(x, y - h, w, h);
        else if (CursorBlink()) {
            gfxLine(x, y, x + w - 1, y);
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

    /* Convert CV value to voltage level and print  to two decimal places */
    void gfxPixel(int x, int y) {
        graphics.setPixel(x + gfx_offset, y);
    }

    void gfxFrame(int x, int y, int w, int h) {
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
    void gfxIcon(int x, int y, const uint8_t *data) {
        gfxBitmap(x, y, 8, data);
    }

    //////////////// Hemisphere-specific graphics methods
    ////////////////////////////////////////////////////////////////////////////////

    /* Show channel-grouped bi-lateral display */
    void gfxSkyline() {
        ForEachChannel(ch)
        {
            int height = ProportionCV(ViewIn(ch), 32);
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
    bool hemisphere; // Which hemisphere (0, 1) this applet uses
    bool isEditing = false; // modal editing toggle
    const char* help[4];
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
    void StartADCLag(bool ch = 0) {
        frame.adc_lag_countdown[io_offset + ch] = HEMISPHERE_ADC_LAG;
    }

    bool EndOfADCLag(bool ch = 0) {
        if (frame.adc_lag_countdown[io_offset + ch] < 0) return false;
        return (--frame.adc_lag_countdown[io_offset + ch] == 0);
    }

private:
    bool applet_started; // Allow the app to maintain state during switching
    bool full_screen;
};

