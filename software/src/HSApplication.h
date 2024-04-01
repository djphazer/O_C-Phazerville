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
    virtual void Start() = 0;
    virtual void Controller() = 0;
    virtual void View() = 0;
    virtual void Resume() = 0;

#ifdef PEWPEWPEW
    struct {
      bool go=0; int idx=0;
      struct{
        uint8_t x,y;
        int x_v,y_v;
      } pewpews[8];

      void PEWPEW(uint8_t mask)
      {
        uint32_t t=OC::CORE::ticks;
        for(int i=0;i<8;++i)
        {
          auto &p=pewpews[i];
          if(mask>>i&0x01){
            auto &pp=pewpews[idx++];
            pp.x=0+40*i;
            pp.y=55;
            pp.x_v=(6+random(3))*(i>1?-1:1);
            pp.y_v=-9;
            idx%=8;
          }
          if(t%500==0){
            p.x+=p.x_v;
            p.y+=p.y_v;
            if(p.y>=55&&p.y_v>0)p.y_v=-p.y_v;
            else ++p.y_v;
          }
          if(t%10000==0){
            p.x_v=p.x_v*100/101;
            p.y_v=p.y_v*10/11;
          }
        }
      }
    } PewPewTime;
#endif

    void BaseController() {
        // Load the IO frame from CV inputs
        HS::frame.Load();

        // Cursor countdowns. See CursorBlink(), ResetCursor(), gfxCursor()
        if (--cursor_countdown < -HSAPPLICATION_CURSOR_TICKS) cursor_countdown = HSAPPLICATION_CURSOR_TICKS;

        Controller();

#ifdef PEWPEWPEW
        PewPewTime.PEWPEW(Clock(3)<<3|Clock(2)<<2|Clock(1)<<1|Clock(0));
#endif

        // set outputs from IO frame
        HS::frame.Send();
    }

    void BaseStart() {
        // Initialize some things for startup
        for (uint8_t ch = 0; ch < DAC_CHANNEL_LAST; ch++)
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
#ifdef PEWPEWPEW
      for(int i=0;i<8;++i){
        auto &p=PewPewTime.pewpews[i];
        gfxIcon(p.x%128,p.y%64,ZAP_ICON);
      }
#endif
    }

    // general screensaver view, visualizing inputs and outputs
    void BaseScreensaver(bool notenames = 0) {
        gfxDottedLine(0, 32, 127, 32, 3); // horizontal baseline
        const size_t w = 128 / DAC_CHANNEL_LAST;
        for (int ch = 0; ch < DAC_CHANNEL_LAST; ++ch)
        {
            if (notenames) {
                // approximate notes being output
                gfxPrint(2 + w*ch, 55, midi_note_numbers[MIDIQuantizer::NoteNumber(HS::frame.outputs[ch])] );
            }

            // trigger/gate indicators
            if (HS::frame.gate_high[ch]) gfxIcon(4 + w*ch, 0, CLOCK_ICON);

            // input
            int height = ProportionCV(HS::frame.inputs[ch], 32);
            int y = constrain(32 - height, 0, 32);
            const int w_ = (w - 4) / 2;
            gfxFrame(2 + (w * ch), y, w_, abs(height));

            // output
            height = ProportionCV(HS::frame.outputs[ch], 32);
            y = constrain(32 - height, 0, 32);
            gfxInvert(3 + w_ + (w * ch), y, w_, abs(height));

            gfxDottedLine(w * ch, 0, w*ch, 63, 3); // vertical divider, left side
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
        if (HS::clock_m.IsRunning() && HS::clock_m.GetMultiply(ch) != 0)
            clocked = HS::clock_m.Tock(ch);
        else {
            clocked = frame.clocked[ch];
        }

        // manual triggers
        clocked = clocked || HS::clock_m.Beep(ch);

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
         gfxPrint(1, 1, str);
         gfxLine(0, 10, 127, 10);
         gfxLine(0, 11, 127, 11);
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
};

// --- Phazerville Screensaver Library ---
typedef struct {
    int x = 0;
    int y = 0;
    int x_v = 6;
    int y_v = 3;

    void Move(bool stars) {
        if (stars) Move(6100, 2900);
        else Move();
    }
    void Move(int target_x = -1, int target_y = -1) {
        x += x_v;
        y += y_v;
        if (x > 12700 || x < 0 || y > 6300 || y < 0) {
            if (target_x < 0 || target_y < 0) {
                x = random(12700);
                y = random(6300);
            } else {
                x = target_x + random(400);
                y = target_y + random(400);
                CONSTRAIN(x, 0, 12700);
                CONSTRAIN(y, 0, 6300);
            }

            x_v = random(30) - 15;
            y_v = random(30) - 15;
            if (x_v == 0) ++x_v;
            if (y_v == 0) ++y_v;
        }
    }
} Zap;
static constexpr int HOW_MANY_ZAPS = 30;
static Zap zaps[HOW_MANY_ZAPS];
static void ZapScreensaver(const bool stars = false) {
  static int frame_delay = 0;
  for (int i = 0; i < (stars ? HOW_MANY_ZAPS : 5); i++) {
    if (frame_delay & 0x1) {
        #if defined(__IMXRT1062__)
        zaps[i].Move(stars); // centered starfield
        #else
        // Zips respawn from their previous sibling
        if (0 == i) zaps[0].Move();
        else zaps[i].Move(zaps[i-1].x, zaps[i-1].y);
        #endif
    }

    if (stars && frame_delay == 0) {
      // accel
      zaps[i].x_v *= 2;
      zaps[i].y_v *= 2;
    }

    if (stars)
      gfxPixel(zaps[i].x/100, zaps[i].y/100);
    else
      gfxIcon(zaps[i].x/100, zaps[i].y/100, ZAP_ICON);
  }
  if (--frame_delay < 0) frame_delay = 100;
}

