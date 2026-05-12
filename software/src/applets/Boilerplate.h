// Copyright (c) 2026, _________
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

/* Step 1: Add your name to the copyright notice at the top.
 * Step 2: Replace all "MyApplet" with a unique class name:
 *         :%s/MyApplet/YourAppletName/g
 * Step 3: Add #include and DeclareApplet<> lines to "_config.h"
 */

// Your code editor and LSP might appreciate this include, but it's not necessary here:
//#include "HemisphereApplet.h"

/* Declare a derived class with forward declarations and member variables as needed.
 * Try to keep things inside the class instead of polluting the global scope.
 */
class MyApplet : public HemisphereApplet {
  // Crucial properties and default function overrides
  APPLET_INTERFACE(MyApplet, "MyApplet", ZAP_ICON);

  enum MyAppletCursor {
    PARAM1,
    PARAM2,

    CURSOR_LAST = PARAM2
  };
  // -- more enums or other constants here --

  /* The default encoder press action is to toggle editing.
   * You can override this for more complex behavior. */
  // void OnButtonPress() final { }

  /* Pressing the select button after highlighting a parameter for editing
   * can invoke a secondary action here. By default, it just cancels editing. */
  // void AuxButton() final { }

  /* All applets can draw an alternate view with a full 128x64 canvas. */
  /* Declare it here and write the body later in the file. */
  // void DrawFullScreen() final;

private:
  int cursor;

  int8_t param1;
  int8_t param2;

  void DrawInterface() {
    int y = 14;
    gfxPrint(1, y, "p1: ");
    gfxPrint(param1);
    if (PARAM1 == cursor) gfxCursor(25, y + 9, 13, "Param1");

    y += 11;
    gfxLine(4, y, 60, y); // ---------------------------------- //

    y += 3;
    gfxPrint(1, y, "p2: ");
    gfxPrint(param2);
    if (PARAM2 == cursor) gfxCursor(25, y + 9, 13, "Param2");

    y += 11;
    gfxIcon(32, y, ZAP_ICON);
  }
};

/* -- Function bodies --
 * refer to the virtual functions in HemisphereApplet.h and APPLET_INTERFACE macro
 *
 * Large functions should be down here, outside the class declaration, so
 * non-critical code (anything other than Controller()) can be placed in
 * FLASHMEM to conserve RAM1 usage.
 */

FLASHMEM void MyApplet::Start() {
  // Initialize default values
  param1 = 12;
  param2 = -3;
}

// The Controller is called from the core ISR timer function,
// every 60 microseconds (16.6kHz). It shouldn't take too long, but most of the
// important, time-critical calculations are done here.
void MyApplet::Controller() {
  /*
   * basic example of checking for rising-edge triggers with Clock(ch),
   * reading inputs with In(ch), quantizing with the default Q-engine for each
   * channel, and setting outputs with Out(ch, val)
   */
  ForEachChannel(ch) {
    if (Clock(ch)) {
      int cv = In(ch);
      cv = Quantize(ch, cv); // uses global quantizer settings
      Out(ch, cv);
    }
  }
}

// The graphics functions are called from the lower priority main loop()
// View() is the normal half-screen viewport, 64x64 pixels
FLASHMEM void MyApplet::View() {
  DrawInterface();
}

/*
FLASHMEM void MyApplet::DrawFullScreen() {
  // This defaults to just View();
  // To avoid being offset on the Right side, you'll need to use the
  // `graphics` object functions directly...
  graphics.setPrintPos(2, 52);
  graphics.print("-= Full-screen UI! =-");

  DrawInterface();
}
*/

// UI events (buttons/encoders) are also dispatched from the loop()
// This may be interrupted by the Controller, so avoid assigning invalid values
// to shared variables temporarily.
FLASHMEM void MyApplet::OnEncoderMove(int direction) {
  if (!EditMode()) {
    MoveCursor(cursor, direction, CURSOR_LAST);
    return;
  }

  // param LUT
  const struct {
    int8_t& p;
    int min, max;
  } params[] = {
    {param1, -63, 63}, // PARAM1
    {param2, 0, 100}, // PARAM2
  };

  // adjust param
  params[cursor].p = constrain(
    params[cursor].p + direction, params[cursor].min, params[cursor].max
  );
}

// -- Settings for an applet should fit into 64 bits
uint64_t MyApplet::OnDataRequest() {
  uint64_t data = 0;
  // Pack paramaters into as few bits as necesary. For example:
  // 4 bits can hold values from 0 to 15
  // 5 bits can hold values from 0 to 31
  // 6 bits can hold values from 0 to 63
  // Signed integers should be cast to unsigned when packing
  // to avoid problems with negative values.
  Pack(data, PackLocation{0, 8}, (uint8_t)param1);
  Pack(data, PackLocation{8, 8}, (uint8_t)param2);

  // Another utility function for packing 64-bit blobs.
  // Type sizes are automatically deduced.
  //data = PackPackables(param1, param2);
  return data;
}

void MyApplet::OnDataReceive(uint64_t data) {
  param1 = Unpack(data, PackLocation{0, 8});
  param2 = Unpack(data, PackLocation{8, 8});

  // Or the more sophisticated:
  //UnpackPackables(data, param1, param2);
}

// -- Labels for the Applet Help/Config view
void MyApplet::SetHelp() {
  //                    "-------" <-- Label size guide
  help[HELP_DIGITAL1] = "Clk Ch1";
  help[HELP_DIGITAL2] = "Clk Ch2";
  help[HELP_CV1] = "CV In1";
  help[HELP_CV2] = "CV In2";
  help[HELP_OUT1] = "Pitch1";
  help[HELP_OUT2] = "Pitch2";
  help[HELP_EXTRA1] = "I have just enough";
  help[HELP_EXTRA2] = "space to say hello!";
  //                  "---------------------" <-- Extra text size guide
}
