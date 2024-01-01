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

/*
 * HSAppIO.h
 *
 * HSAppIO is a base class for full O_C apps that are designed to work (or act) like Hemisphere apps,
 * for consistency in development, or ease of porting apps or applets in either direction.
 */

#pragma once

#ifndef int2simfloat
#define int2simfloat(x) (x << 14)
#define simfloat2int(x) (x >> 14)
typedef int32_t simfloat;
#endif

#include "HSicons.h"
#include "HSClockManager.h"
#include "HemisphereApplet.h"
#include "HSUtils.h"

#define HSAPPLICATION_CURSOR_TICKS 12000
#define HSAPPLICATION_5V 7680
#define HSAPPLICATION_3V 4608
#define HSAPPLICATION_CHANGE_THRESHOLD 32

#if defined(BUCHLA_4U) || defined(VOR)
#define HSAPP_PULSE_VOLTAGE 8
#else
#define HSAPP_PULSE_VOLTAGE 5
#endif

using namespace HS;

class HSApplication {
public:
    virtual void Start();
    virtual void Controller();
    virtual void View();
    virtual void Resume();

    void BaseController() {
        // Load the IO frame from CV inputs
        HS::frame.Load();

        // Cursor countdowns. See CursorBlink(), ResetCursor(), gfxCursor()
        if (--cursor_countdown < -HSAPPLICATION_CURSOR_TICKS) cursor_countdown = HSAPPLICATION_CURSOR_TICKS;

        Controller();

        // set outputs from IO frame
        HS::frame.Send();
    }

    void BaseStart() {
        // Initialize some things for startup
        for (uint8_t ch = 0; ch < 4; ch++)
        {
            frame.clock_countdown[ch]  = 0;
            frame.inputs[ch] = 0;
            frame.outputs[ch] = 0;
            frame.outputs_smooth[ch] = 0;
            frame.adc_lag_countdown[ch] = 0;
        }
        cursor_countdown = HSAPPLICATION_CURSOR_TICKS;

        Start();
    }

    void BaseView() {
        View();
        last_view_tick = OC::CORE::ticks;
    }

    // general screensaver view, visualizing inputs and outputs
    void BaseScreensaver(bool notenames = 0) {
        gfxDottedLine(0, 32, 127, 32, 3); // horizontal baseline
        for (int ch = 0; ch < 4; ++ch)
        {
            if (notenames) {
                // approximate notes being output
                gfxPrint(8 + 32*ch, 55, midi_note_numbers[MIDIQuantizer::NoteNumber(HS::frame.outputs[ch])] );
            }

            // trigger/gate indicators
            if (HS::frame.gate_high[ch]) gfxIcon(11 + 32*ch, 0, CLOCK_ICON);

            // input
            int height = ProportionCV(HS::frame.inputs[ch], 32);
            int y = constrain(32 - height, 0, 32);
            gfxFrame(3 + (32 * ch), y, 6, abs(height));

            // output
            height = ProportionCV(HS::frame.outputs[ch], 32);
            y = constrain(32 - height, 0, 32);
            gfxInvert(11 + (32 * ch), y, 12, abs(height));

            gfxDottedLine(32 * ch, 0, 32*ch, 63, 3); // vertical divider, left side
        }
        gfxDottedLine(127, 0, 127, 63, 3); // vertical line, right side
    }

    //////////////// Hemisphere-like IO methods
    ////////////////////////////////////////////////////////////////////////////////
    void Out(int ch, int value, int octave = 0) {
        frame.Out( (DAC_CHANNEL)(ch), value + (octave * (12 << 7)));
    }

    int In(int ch) {
        return frame.inputs[ch];
    }

    // Apply small center detent to input, so it reads zero before a threshold
    int DetentedIn(int ch) {
        return (In(ch) > 64 || In(ch) < -64) ? In(ch) : 0;
    }

    // Standard bi-polar CV modulation scenario
    template <typename T>
    void Modulate(T &param, const int ch, const int min = 0, const int max = 255) {
        int cv = DetentedIn(ch);
        param = constrain(param + Proportion(cv, HEMISPHERE_MAX_INPUT_CV, max), min, max);
    }

    bool Changed(int ch) {
        return frame.changed_cv[ch];
    }

    bool Gate(int ch) {
        return frame.gate_high[ch];
    }

    void GateOut(int ch, bool high) {
        Out(ch, 0, (high ? HSAPP_PULSE_VOLTAGE : 0));
    }

    bool Clock(int ch) {
        bool clocked = 0;
        ClockManager *clock_m = clock_m->get();

        if (clock_m->IsRunning() && clock_m->GetMultiply(ch) != 0)
            clocked = clock_m->Tock(ch);
        else {
            clocked = frame.clocked[ch];
        }

        // manual triggers
        clocked = clocked || clock_m->Beep(ch);

        if (clocked) {
            frame.cycle_ticks[ch] = OC::CORE::ticks - frame.last_clock[ch];
            frame.last_clock[ch] = OC::CORE::ticks;
        }
        return clocked;
    }

    void ClockOut(int ch, int ticks = 100) {
        frame.ClockOut( (DAC_CHANNEL)ch, ticks );
    }

    // Buffered I/O functions for use in Views
    int ViewIn(int ch) {return frame.inputs[ch];}
    int ViewOut(int ch) {return frame.outputs[ch];}
    int ClockCycleTicks(int ch) {return frame.cycle_ticks[ch];}

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
    void StartADCLag(int ch) {frame.adc_lag_countdown[ch] = 96;}
    bool EndOfADCLag(int ch) {return (--frame.adc_lag_countdown[ch] == 0);}

    void gfxCursor(int x, int y, int w) {
        if (CursorBlink()) gfxLine(x, y, x + w - 1, y);
    }
    void gfxHeader(const char *str) {
         gfxPrint(1, 2, str);
         gfxLine(0, 10, 127, 10);
         gfxLine(0, 12, 127, 12);
    }

protected:
    // Check cursor blink cycle
    bool CursorBlink() {
        return (cursor_countdown > 0);
    }
    void ResetCursor() {
        cursor_countdown = HSAPPLICATION_CURSOR_TICKS;
    }

private:
    int cursor_countdown; // Timer for cursor blinkin'
    uint32_t last_view_tick; // Time since the last view, for activating screen blanking
};
