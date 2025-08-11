#include "HemisphereApplet.h"

using namespace HS;

HS::IOFrame HS::frame;
HS::ClockManager HS::clock_m;

int HemisphereApplet::cursor_countdown[APPLET_SLOTS + 1];
int16_t HemisphereApplet::cursor_start_x;
int16_t HemisphereApplet::cursor_start_y;
const char* HemisphereApplet::help[HELP_LABEL_COUNT];
HS::EncoderEditor HemisphereApplet::enc_edit[APPLET_SLOTS + 1];

//
// standard entry points
//
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
void HemisphereApplet::BaseView(bool full_screen, bool parked) const {
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

void HemisphereApplet::DrawConfigHelp() const {
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

    // ----------------------
    // --- CV I/O Methods ---
    // ----------------------

int HemisphereApplet::In(const int ch) const {
  return cvmap[ch + io_offset].In();
}

#ifdef __IMXRT1062__
float HemisphereApplet::InF(int ch) const {
    return static_cast<float>(In(ch)) / HEMISPHERE_MAX_INPUT_CV;
}
#endif

// Apply small center detent to input, so it reads zero before a threshold
int HemisphereApplet::DetentedIn(int ch) const {
    if (NorthernLightModular && In(ch) < HEMISPHERE_CENTER_DETENT)
      return 0;

    if (In(ch) > (HEMISPHERE_CENTER_INPUT_CV + HEMISPHERE_CENTER_DETENT)
      || In(ch) < (HEMISPHERE_CENTER_INPUT_CV - HEMISPHERE_CENTER_DETENT))
      return In(ch);

    return HEMISPHERE_CENTER_INPUT_CV;
}
int HemisphereApplet::SemitoneIn(int ch) const {
  return input_quant[ch + io_offset].Process(In(ch));
}

/* Has the specified Digital input been clocked this cycle? (rising edge of a gate)
 * This is pre-calculated in HS::IOFrame::Load() according to input mappings and internal clock settings
 */
bool HemisphereApplet::Clock(int ch, bool physical) const {
    return frame.clocked[ch + io_offset];
}
bool HemisphereApplet::Gate(int ch) const {
    return trigmap[ch + io_offset].Gate();
}

// --- CV Output methods
void HemisphereApplet::Out(int ch, int value, int octave) const {
    frame.Out( (DAC_CHANNEL)(ch + io_offset), value + (octave * (12 << 7)));
}

void HemisphereApplet::SmoothedOut(int ch, int value, int kSmoothing) const {
  if (OC::CORE::ticks % kSmoothing == 0) {
    DAC_CHANNEL channel = (DAC_CHANNEL)(ch + io_offset);
    value = (frame.outputs_smooth[channel] * (kSmoothing - 1) + value) / kSmoothing;
    frame.outputs[channel] = frame.outputs_smooth[channel] = value;
  }
}
void HemisphereApplet::ClockOut(const int ch, const int ticks) const {
    frame.ClockOut( (DAC_CHANNEL)(io_offset + ch), ticks);
}
void HemisphereApplet::GateOut(int ch, bool high) const {
    Out(ch, 0, (high ? PULSE_VOLTAGE : 0));
}

    // -------------------------------
    // --- Offset graphics methods ---
    // -------------------------------

void HemisphereApplet::gfxCursor(int x, int y, int w, int h) const { // assumes standard text height for highlighting
  if (EditMode()) {
    gfxInvert(x, y - h, w, h);
  } else if (CursorBlink()) {
    gfxLine(x, y, x + w - 1, y);
    gfxPixel(x, y-1);
    gfxPixel(x + w - 1, y-1);
  }
}
void HemisphereApplet::gfxSpicyCursor(int x, int y, int w, int h) const {
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

void HemisphereApplet::gfxPos(int x, int y) const {
    graphics.setPrintPos(x + gfx_offset, y);
}

void HemisphereApplet::gfxPrint(int x, int y, const char *str) const {
    graphics.setPrintPos(x + gfx_offset, y);
    graphics.print(str);
}
void HemisphereApplet::gfxPrint(int x, int y, int num) const {
    graphics.setPrintPos(x + gfx_offset, y);
    graphics.print(num);
}
void HemisphereApplet::gfxPrint(const char *str) const {
    graphics.print(str);
}
void HemisphereApplet::gfxPrint(int num) const {
    graphics.print(num);
}

void HemisphereApplet::gfxPrint(int x, int y, float num, int digits) const {
    graphics.setPrintPos(x + gfx_offset, y);
    gfxPrint(num, digits);
}

void HemisphereApplet::gfxPrint(float num, int digits) const {
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

void HemisphereApplet::gfxPrint(DigitalInputMap &map) const {
  gfxPrintIcon(map.Icon());
  if (map.Gate()) gfxInvert(gfxGetPrintPosX()-8, gfxGetPrintPosY(), 8, 8);
}
void HemisphereApplet::gfxPrint(CVInputMap &map) const {
  gfxPrintIcon(map.Icon());
  const int xpos = gfxGetPrintPosX() - 1;
  const int ypos = gfxGetPrintPosY() + 4;
  const int height = map.InRescaled(24);
  gfxLine(xpos, ypos, xpos, ypos - height);
}

void HemisphereApplet::gfxPrint(int x_adv, int num) const { // Print number with character padding
    for (int c = 0; c < (x_adv / 6); c++) gfxPrint(" ");
    gfxPrint(num);
}

// -- cursor gfx
void HemisphereApplet::gfxStartCursor(int x, int y) {
    gfxPos(x, y);
    gfxStartCursor();
}
void HemisphereApplet::gfxStartCursor() {
    cursor_start_x = gfxGetPrintPosX();
    cursor_start_y = gfxGetPrintPosY();
}

void HemisphereApplet::gfxEndCursor(bool selected, bool spicy, const char *str) const {
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

void HemisphereApplet::gfxPixel(int x, int y) const {
    graphics.setPixel(x + gfx_offset, y);
}
void HemisphereApplet::gfxFrame(int x, int y, int w, int h, bool dotted) const {
  if (dotted) {
    gfxLine(x, y, x + w - 1, y, 2); // top
    gfxLine(x, y + 1, x, y + h - 1, 2); // vert left
    gfxLine(x + w - 1, y + 1, x + w - 1, y + h - 1, 2); // vert rigth
    gfxLine(x, y + h - 1, x + w - 1, y + h - 1, 2); // bottom
  } else
    graphics.drawFrame(x + gfx_offset, y, w, h);
}
void HemisphereApplet::gfxRect(int x, int y, int w, int h) const {
    graphics.drawRect(x + gfx_offset, y, w, h);
}
void HemisphereApplet::gfxInvert(int x, int y, int w, int h) const {
    graphics.invertRect(x + gfx_offset, y, w, h);
}
void HemisphereApplet::gfxClear(int x, int y, int w, int h) const {
    graphics.clearRect(x + gfx_offset, y, w, h);
}
void HemisphereApplet::gfxLine(int x, int y, int x2, int y2) const {
    graphics.drawLine(x + gfx_offset, y, x2 + gfx_offset, y2);
}
void HemisphereApplet::gfxLine(int x, int y, int x2, int y2, bool dotted) const {
    graphics.drawLine(x + gfx_offset, y, x2 + gfx_offset, y2, dotted ? 2 : 1);
}
void HemisphereApplet::gfxDottedLine(int x, int y, int x2, int y2, uint8_t p) const {
    graphics.drawLine(x + gfx_offset, y, x2 + gfx_offset, y2, p);
}
void HemisphereApplet::gfxCircle(int x, int y, int r) const {
    graphics.drawCircle(x + gfx_offset, y, r);
}
void HemisphereApplet::gfxBitmap(int x, int y, int w, const uint8_t *data) const {
    graphics.drawBitmap8(x + gfx_offset, y, w, data);
}
void HemisphereApplet::gfxBitmapBlink(int x, int y, int w, const uint8_t *data) const {
    if (CursorBlink()) gfxBitmap(x, y, w, data);
}
void HemisphereApplet::gfxIcon(int x, int y, const uint8_t *data) const {
    gfxBitmap(x, y, 8, data);
}
void HemisphereApplet::gfxPrintIcon(const uint8_t *data, int16_t w) const {
    gfxIcon(gfxGetPrintPosX(), gfxGetPrintPosY(), data);
    gfxPos(gfxGetPrintPosX() + w, gfxGetPrintPosY());
}

/* Show channel-grouped bi-lateral display */
void HemisphereApplet::gfxSkyline() const {
    ForEachChannel(ch)
    {
        int height = ProportionCV(In(ch), 32);
        gfxFrame(23 + (10 * ch), BottomAlign(height), 6, 63);

        height = ProportionCV(ViewOut(ch), 32);
        gfxInvert(3 + (46 * ch), BottomAlign(height), 12, 63);
    }
}

void HemisphereApplet::gfxHeader(int y) const {
  gfxHeader(applet_name(), applet_icon(), y, false);
}

void HemisphereApplet::gfxHeader(const char *str, const uint8_t *icon, int y, bool underline) const {
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

void HemisphereApplet::DrawSlider(uint8_t x, uint8_t y, uint8_t len, uint8_t value, uint8_t max_val, bool is_cursor) const {
    uint8_t p = is_cursor ? 1 : 3;
    uint8_t w = Proportion(value, max_val, len-1);
    gfxDottedLine(x, y + 4, x + len, y + 4, p);
    gfxRect(x + w, y, 2, 8);
    if (EditMode() && is_cursor) gfxInvert(x-1, y, len+3, 8);
}
