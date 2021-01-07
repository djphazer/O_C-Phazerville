#include "HemisphereApplet.h"

IOFrame HS::frame;

int HemisphereApplet::cursor_countdown[2];
int HemisphereApplet::cursor[2];

void HemisphereApplet::BaseStart(bool hemisphere_) {
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

void HemisphereApplet::BaseController() {
    // I moved the IO-related stuff to the parent HemisphereManager app.
    // The IOFrame gets loaded before calling Controllers, and outputs are handled after.
    // -NJM

    // Cursor countdowns. See CursorBlink(), ResetCursor(), gfxCursor()
    if (--cursor_countdown[hemisphere] < -HEMISPHERE_CURSOR_TICKS) cursor_countdown[hemisphere] = HEMISPHERE_CURSOR_TICKS;

    Controller();
}

void HemisphereApplet::BaseView() {
    //if (HS::select_mode == hemisphere)
    gfxHeader(applet_name());
    // If active, draw the full screen view instead of the application screen
    if (full_screen) this->DrawFullScreen();
    else this->View();
}

/*
 * Has the specified Digital input been clocked this cycle?
 *
 * If physical is true, then logical clock types (master clock forwarding and metronome) will
 * not be used.
 */
bool HemisphereApplet::Clock(int ch, bool physical) {
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

