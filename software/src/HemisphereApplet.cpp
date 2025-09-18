#include "HemisphereApplet.h"

HS::IOFrame HS::frame;
HS::ClockManager HS::clock_m;

int HemisphereApplet::cursor_countdown[APPLET_SLOTS + 1];
int16_t HemisphereApplet::cursor_start_x;
int16_t HemisphereApplet::cursor_start_y;
const char* HemisphereApplet::help[HELP_LABEL_COUNT];
HS::EncoderEditor HemisphereApplet::enc_edit[APPLET_SLOTS + 1];

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

// seems weird to put this here, but alas, it worketh
void HS::IOFrame::Load() {
    gate_high[0] = OC::DigitalInputs::read_immediate<OC::DIGITAL_INPUT_1>();
    gate_high[1] = OC::DigitalInputs::read_immediate<OC::DIGITAL_INPUT_2>();
    gate_high[2] = OC::DigitalInputs::read_immediate<OC::DIGITAL_INPUT_3>();
    gate_high[3] = OC::DigitalInputs::read_immediate<OC::DIGITAL_INPUT_4>();
    for (int i = 0; i < ADC_CHANNEL_COUNT; ++i) {
        // Set CV inputs
        inputs[i] = OC::ADC::raw_pitch_value(ADC_CHANNEL(i));

        // calculate gates/clocks for all ADC inputs as well
        gate_high[OC::DIGITAL_INPUT_LAST + i] = inputs[i] > GATE_THRESHOLD;

        if (abs(inputs[i] - last_cv[i]) > HEMISPHERE_CHANGE_THRESHOLD) {
            changed_cv[i] = 1;
            last_cv[i] = inputs[i];
        } else changed_cv[i] = 0;
    }

    // Handle clock pulse timing
    for (int i = 0; i < DAC_CHANNEL_COUNT; ++i) {
        if (clock_countdown[i] > 0) {
            if (--clock_countdown[i] == 0) outputs[i] = 0;
        }
    }
    for (int i = 0; i < MIDIMAP_MAX; ++i) {
        MIDIMapping& m = MIDIState.mapping[i];
        if (m.trigout_countdown > 0) {
            if (--m.trigout_countdown == 0) m.output = 0;
        }
    }

    // pre-calculate clock triggers
    for (int ch = 0; ch < APPLET_SLOTS * 2; ++ch) {
      bool result = 0;
      const size_t virt_chan = (ch) % (APPLET_SLOTS * 2);

      // clock triggers
      // TODO: implement div/mult within DigitalInputMap and get rid of
      //       this call to clock_m
      if (clock_m.IsRunning() && clock_m.GetMultiply(virt_chan) != 0)
          result = clock_m.Tock(virt_chan);
      else {
          result = trigmap[ch].Clock();
      }

      // Try to eat a boop
      result = result || clock_m.Beep(virt_chan);

      if (result) {
          cycle_ticks[ch] = OC::CORE::ticks - last_clock[ch];
          last_clock[ch] = OC::CORE::ticks;
      }

      clocked[ch] = result;
    }
}
