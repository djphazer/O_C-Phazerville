---
layout: default
nav_order: 4
---
# UI Gestures

Operating **Hemispheres** or **Quadrants** makes use of button combinations to perform various actions. See below for a complete list.

Certain applet (or parameter editing) contexts may override the default behaviours of the buttons and/or encoders. Typically, in split-screen contexts, the left side uses the UP button and LEFT encoder; right side uses DOWN button and RIGHT encoder. In full-screen contexts, the LEFT encoder jumps between pages while the RIGHT encoder moves the cursor and activates selections.

Note for T4.1 hardware, "UP" has been re-labeled as "A" and "DOWN" as "B".

## Global

| Action    | Gesture    |   Notes   |
| --------- |:----------:|:---------:|
| **Invoke screensaver** | Long-press UP (or A) button from any App | |
| **Return to main menu** | Long-press RIGHT encoder | Execution continues in the background. |
| **[Save Global Settings to EEPROM](Saving-State)** | Long-press RIGHT encoder _again_ while on main menu | This includes the currently selected App, user scales, saved sequence patterns, etc. Use [Presets](Hemisphere-Config#presets-floating-menu) instead to store Applet settings inside Hemispheres or Quadrants |

## Hemispheres

| Action    | Gesture    |   Notes   |
| --------- |:----------:|:---------:|
| **Change left applet**             | Press UP button - a solid border will appear around the applet. Rotate LEFT encoder to scroll through each applet one by one. Press either UP or LEFT encoder to exit selection mode. | Alternatively, double-press UP button to enter help/config screen. Select applet name with RIGHT encoder cursor and press to enter applet selection menu. Scroll menu with RIGHT encoder and press to select. Press UP or DOWN to escape without changing applet. |
| **Change right applet**            | Press DOWN button - a solid border will appear around the applet. Rotate RIGHT encoder to scroll through each applet one by one. Press either DOWN or RIGHT encoder to exit selection mode. | Alternatively, double-press DOWN button to enter help/config screen. Select applet name with RIGHT encoder cursor and press to enter applet selection menu. Scroll menu with RIGHT encoder and press to select. Press UP or DOWN to escape without changing applet. |
| **Open full-screen applet help/config** | Double press UP button for left hemisphere, double press DOWN button for right hemisphere. Press UP or DOWN again to exit. | RIGHT encoder is always used to move cursor and edit params; LEFT encoder swaps between applet config and full-screen view.     |
| **View applet UI in full-screen**   | Open help screen (see above), and rotate LEFT encoder. Then, use RIGHT encoder to interact with the Applet UI. | Applets that make use of full-screen view currently include Scope, EuclidX, and Ebb&LFO. More to come.    |
| **Open [Clock / Trigger Setup](Clock-Setup)**      | Press both UP + DOWN buttons together.  | Adjust internal BPM, swing, external sync, and per-trigger clock mult/div; remap trigger inputs (also available within the [Config menus](Hemisphere-Config) or in applet help/config view); and manually perform triggers.  |
| **Cycle Internal Clock state**     | Long-press LEFT encoder (or press Z on T4.1, or VOR button on Plum Audio units) | Stop -> Arm _(i.e. play on next input trigger)_ -> Start |
| **Open [Hemisphere Config menu](Hemisphere-Config)** | Long-press DOWN button. Scroll pages with LEFT encoder, move cursor with RIGHT encoder | Load/save presets; adjust trigger length; select screensaver and cursor mode; toggle auto-MIDI out; trigger and CV input mapping; quantizer settings; applet filtering |
| **AuxButton**  | _**After pushing encoder to highlight a parameter for editing**_, press the corresponding select button (UP or DOWN) | This gesture is only enabled in certain applets for secondary functions, typically indicated with a dotted cursor line. Use it to mute/unmute steps in DivSeq, SequenceX, Seq32, etc.; re-randomize sequences in Shredder; Toggle additional CV mapping in DuoTET; and directly edit the current [Quantizer engine](Hemisphere-Quantizer-Setup) |

### VOR Gestures
_Variable Output Range compatible hardware only (i.e. Plum Audio 4robots, OCP, and OCP x)_
* **Start / Stop internal clock** (Hemispheres only): press VOR button
* **Cycle VBias offset (-5V, -3V, 0V)** (global): Press LEFT and RIGHT encoders together

## Quadrants

_(T4.1 only)_
This mode, an alternative to Hemispheres, allows 4 applets to be loaded simultaneously. More info on the [**Quadrants**](Quadrants) page.

For reference, the applet slots are regarded as Northwest / Northeast / Southwest / Southeast, oriented in the 4 corners of the screen (NW being the upper left, SE being the lower right), with the corresponding A/B/X/Y buttons as Select (or AuxButton). "Southern" applets display with an inverted header.

Only 2 applets are visible at a given time (although all are active). To swap visibility between NW and SW, press A or X respectively. To swap between NE and SE, press B or Y respectively. The gestures below may first require the target applet to be visible.

| Action    | Gesture    |   Notes   |
| --------- |:----------:|:---------:|
| **Quick-change applets** | Hold X or Y and rotate either encoder | Switches the visible applet on each side without menus, like Hemispheres select mode |
| **Change NW applet**  | Double press A button to enter help/config screen. Select applet name with RIGHT Encoder cursor and press to enter applet selection menu. Scroll with RIGHT encoder and press to select. | Press A/B/X/Y to escape without changing applet.   |
| **Change NE applet**  | Double press B button to enter help/config screen. Select applet name with RIGHT Encoder cursor and press to enter applet selection menu. Scroll with RIGHT encoder and press to select. | Press A/B/X/Y to escape without changing applet.   |
| **Change SW applet**  | Double press X button to enter help/config screen. Select applet name with RIGHT Encoder cursor and press to enter applet selection menu. Scroll with RIGHT encoder and press to select. | Press A/B/X/Y to escape without changing applet.    |
| **Change SE applet**  | Double press Y button to enter help/config screen. Select applet name with RIGHT Encoder cursor and press to enter applet selection menu. Scroll with RIGHT encoder and press to select. | Press A/B/X/Y to escape without changing applet.    |
| **Open Overview** | Press A + Y or B + X together (diagonal button combos) | Monitor input and output voltages of all 4 quadrants at once |
| **Open full-screen applet help/config** | With applet visible, double-press the corresponding A/B/X/Y button. Press the same button again to exit. Press a different Select button to switch to that applet slot. | RIGHT encoder is always used to move cursor and edit params; LEFT encoder swaps between applet config and full-screen view. |
| **View applet UI in full-screen**   | Open help/config screen (see above), and rotate LEFT encoder. Then, use RIGHT encoder to interact with the Applet. | Applets that make use of full-screen view currently include Scope, EuclidX, and Ebb&LFO. More to come.    |
| **Open [Clock / Trigger Setup](Clock-Setup)**  |  Press both A + B buttons together  |  Adjust internal BPM, swing, external sync, and per-trigger clock mult/div; remap trigger inputs (also available within the [Config menus](Hemisphere-Config) or in applet help/config view).  |
| **Cycle Internal Clock state**  |  Press Z button  |  Stop -> Arm _(i.e. play on next input trigger)_ -> Start  |
| **Open [Config menu](Hemisphere-Config)** |  Long-press B button, or press B+Y to jump to Input Mapping page. Scroll pages with LEFT encoder, scroll options with RIGHT encoder  |  Load/save presets; adjust trigger length; select screensaver and cursor mode; toggle auto-MIDI out; trigger and CV input mapping; quantizer settings; applet filtering  |
| **AuxButton**  | _**After pushing encoder to highlight a parameter for editing**_, press the corresponding select button (A/B/X/Y) | This gesture is only enabled in certain applets for secondary functions, typically indicated with a dotted cursor line. Use it to mute/unmute steps in DivSeq, SequenceX, Seq32, etc.; re-randomize sequences in Shredder; Toggle additional CV mapping in DuoTET; and directly edit the current [Quantizer engine](Hemisphere-Quantizer-Setup) |
| **Quick access Save/Load preset slots**  |  Press A + X together. Toggle between load and save by rotating LEFT encoder. Press LEFT encoder to switch banks. Select slot by rotating RIGHT encoder, and press to engage.  |  This menu is also accessible via the floating config menu  |
| **Enter Audio DSP**  |  Press X + Y buttons together. Exit by pressing A or B  |  |
