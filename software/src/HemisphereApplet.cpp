#include "HemisphereApplet.h"

HS::IOFrame HS::frame;
HS::ClockManager HS::clock_m;

int HemisphereApplet::cursor_countdown[APPLET_CURSOR_COUNT];
int16_t HemisphereApplet::cursor_start_x;
int16_t HemisphereApplet::cursor_start_y;
const char* HemisphereApplet::help[HELP_LABEL_COUNT];
HS::EncoderEditor HemisphereApplet::enc_edit[APPLET_CURSOR_COUNT];

void HemisphereApplet::BaseStart(const HEM_SIDE hemisphere_) {
    SetDisplaySide(hemisphere_);
    ResetCursor();
    CancelEdit();

    // Maintain previous app state by skipping Start
    if (!applet_started) {
        applet_started = true;
        Start();
        ForEachChannel(ch) {
            Out(ch, 0); // reset outputs
        }
    }
}
void HemisphereApplet::BaseView(bool full_screen, bool parked) {
    //if (HS::select_mode == hemisphere)
    gfxHeader(applet_name(), (HS::ALWAYS_SHOW_ICONS || full_screen) ? applet_icon() : nullptr);
    // If active, draw the full screen view instead of the application screen
    if (full_screen) {
      if (parked)
        this->DrawFullScreen();
      else
        DrawConfigHelp();
    }
    else this->View();
}

void HemisphereApplet::DrawConfigHelp() {
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
        graphics.print( HS::trigmap[ch + io_offset].InputName() );
      }
      graphics.invertRect(ch*64, y - 1, 19, 9);

      graphics.setPrintPos(ch*64 + 20, y);
      graphics.print( help[HELP_DIGITAL1 + ch] );

      y += 10;

      graphics.setPrintPos(ch*64, y);
      graphics.print( cvmap[ch+io_offset].InputName() );
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
