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

#include "HSicons.h"
#include "HSClockManager.h"
#include "src/drivers/FreqMeasure/OC_FreqMeasure.h"
#include "util/util_math.h"

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
const char * HEM_HELP_SECTION_NAMES[4] = {"Dig", "CV", "Out", "Enc"};

// Simulated fixed floats by multiplying and dividing by powers of 2
#ifndef int2simfloat
#define int2simfloat(x) (x << 14)
#define simfloat2int(x) (x >> 14)
typedef int32_t simfloat;
#endif

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

#define DECLARE_APPLET(id, categories, class_name) \
{ id, categories, class_name ## _Start, class_name ## _Controller, class_name ## _View, \
  class_name ## _OnButtonPress, class_name ## _OnEncoderMove, class_name ## _ToggleHelpScreen, \
  class_name ## _OnDataRequest, class_name ## _OnDataReceive \
}

#include "hemisphere_config.h"
#include "braids_quantizer.h"
#include "HSUtils.h"
#include "HSIOFrame.h"

namespace HS {

typedef struct Applet {
  int id;
  uint8_t categories;
  void (*Start)(bool); // Initialize when selected
  void (*Controller)(bool, bool);  // Interrupt Service Routine
  void (*View)(bool);  // Draw main view
  void (*OnButtonPress)(bool); // Encoder button has been pressed
  void (*OnEncoderMove)(bool, int); // Encoder has been rotated
  void (*ToggleHelpScreen)(bool); // Help Screen has been requested
  uint64_t (*OnDataRequest)(bool); // Get a data int from the applet
  void (*OnDataReceive)(bool, uint64_t); // Send a data int to the applet
} Applet;

// editable by cursor, and CV assignable
typedef struct {
    uint8_t &p; int min, max;
    uint8_t x, y, w;
} AppletParam;

Applet available_applets[] = HEMISPHERE_APPLETS;
Applet clock_setup_applet = DECLARE_APPLET(9999, 0x01, ClockSetup);

static IOFrame frame;

int octave_max = 5;

int select_mode = -1;
bool cursor_wrap = 0;
//uint8_t modal_edit_mode = 2; // 0=old behavior, 1=modal editing, 2=modal with wraparound
//static void CycleEditMode() { ++modal_edit_mode %= 3; }

}

using namespace HS;

class HemisphereApplet {
public:
    static int cursor_countdown[2];
    static int cursor[2];

    virtual const char* applet_name(); // Maximum of 9 characters
    virtual void Start();
    virtual void Controller();
    virtual void View();

    void BaseStart(bool hemisphere_) {
        hemisphere = hemisphere_;

        // Initialize some things for startup
        full_screen = 0;
        cursor_countdown[hemisphere] = HEMISPHERE_CURSOR_TICKS;

        // Shutdown FTM capture on Digital 4, used by Tuner
#ifdef FLIP_180
        if (hemisphere == 0)
#else
        if (hemisphere == 1)
#endif
        {
            FreqMeasure.end();
            OC::DigitalInputs::reInit();
        }

        // Maintain previous app state by skipping Start
        if (!applet_started) {
            applet_started = true;
            Start();
            ForEachChannel(ch) {
                Out(ch, 0); // reset outputs
            }
        }
    }

    void BaseController(bool master_clock_on = false) {
        // I moved the IO-related stuff to the parent HemisphereManager app.
        // The IOFrame gets loaded before calling Controllers, and outputs are handled after.
        // -NJM

        // Cursor countdowns. See CursorBlink(), ResetCursor(), gfxCursor()
        if (--cursor_countdown[hemisphere] < -HEMISPHERE_CURSOR_TICKS) cursor_countdown[hemisphere] = HEMISPHERE_CURSOR_TICKS;

        Controller();
    }

    void BaseView() {
        //if (HS::select_mode == hemisphere)
        gfxHeader(applet_name());
        // If active, draw the full screen view instead of the application screen
        if (full_screen) this->DrawFullScreen();
        else View();
    }

    // Screensavers are deprecated in favor of screen blanking, but the BaseScreensaverView() remains
    // to avoid breaking applets based on the old boilerplate
    void BaseScreensaverView() {}

    /* Help Screen Toggle */
    void HelpScreen() {
        full_screen = 1 - full_screen;
    }

    /* Check cursor blink cycle. */
    bool CursorBlink() {
        return (cursor_countdown[hemisphere] > 0);
    }

    void ResetCursor() {
        cursor_countdown[hemisphere] = HEMISPHERE_CURSOR_TICKS;
    }

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

    void Out(int ch, int value, int octave = 0) {
        frame.Out( (DAC_CHANNEL)(ch + io_offset), value + (octave * (12 << 7)));
    }

    void SmoothedOut(int ch, int value, int kSmoothing) {
        DAC_CHANNEL channel = (DAC_CHANNEL)(ch + io_offset);
        value = (frame.outputs_smooth[channel] * (kSmoothing - 1) + value) / kSmoothing;
        frame.outputs[channel] = frame.outputs_smooth[channel] = value;
    }

    /*
     * Has the specified Digital input been clocked this cycle?
     *
     * If physical is true, then logical clock types (master clock forwarding and metronome) will
     * not be used.
     */
    bool Clock(int ch, bool physical = 0) {
        bool clocked = 0;
        ClockManager *clock_m = clock_m->get();
        bool useTock = (!physical && clock_m->IsRunning());

        // clock triggers
        if (useTock && clock_m->GetMultiply(ch + io_offset) != 0)
            clocked = clock_m->Tock(ch + io_offset);
        else if (trigger_mapping[ch + io_offset] > 0 && trigger_mapping[ch + io_offset] < 5)
            clocked = frame.clocked[ trigger_mapping[ch + io_offset] - 1 ];

        // Try to eat a boop
        clocked = clocked || clock_m->Beep(io_offset + ch);

        if (clocked) {
            frame.cycle_ticks[io_offset + ch] = OC::CORE::ticks - frame.last_clock[io_offset + ch];
            frame.last_clock[io_offset + ch] = OC::CORE::ticks;
        }
        return clocked;
    }

    void ClockOut(const int ch, const int ticks = HEMISPHERE_CLOCK_TICKS * trig_length) {
        frame.ClockOut( (DAC_CHANNEL)(io_offset + ch), ticks);
    }

    bool Gate(int ch) {
        const int t = trigger_mapping[ch + io_offset];
        return (t && t < 5) ? frame.gate_high[t - 1] : false;
    }

    void GateOut(int ch, bool high) {
        Out(ch, 0, (high ? PULSE_VOLTAGE : 0));
    }

    // Buffered I/O functions
    int ViewIn(int ch) {return frame.inputs[io_offset + ch];}
    int ViewOut(int ch) {return frame.outputs[io_offset + ch];}
    int ClockCycleTicks(int ch) {return frame.cycle_ticks[io_offset + ch];}
    bool Changed(int ch) {return frame.changed_cv[io_offset + ch];}

protected:
    bool hemisphere; // Which hemisphere (0, 1) this applet uses
    bool isEditing = false; // modal editing toggle
    const char* help[4];
    virtual void SetHelp();

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

int HemisphereApplet::cursor_countdown[2];
int HemisphereApplet::cursor[2];

