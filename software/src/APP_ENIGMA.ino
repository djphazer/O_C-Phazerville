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

#ifdef ENABLE_APP_ENIGMA

#include "HSApplication.h"
#include "HSMIDI.h"
#include "enigma/TuringMachine.h"
#include "enigma/TuringMachineState.h"
#include "enigma/EnigmaStep.h"
#include "enigma/EnigmaOutput.h"
#include "enigma/EnigmaTrack.h"

// Modes
#define ENIGMA_MODE_LIBRARY 0  // Create, edit, save, favorite, sysex dump Turing Machines
#define ENIGMA_MODE_ASSIGN 1   // Assign CV and MIDI output
#define ENIGMA_MODE_SONG 2     // Assemble compositions by chaining Turing Machines
#define ENIGMA_MODE_PLAY 3     // Play information and transport controls
#define ENIGMA_CONFIRM_RESET 4 // Special page for confirming song reset

// Parameters for Manaage mode (per Turing Machine)
#define ENIGMA_TM_LENGTH 0
#define ENIGMA_TM_PROBABILITY 1
#define ENIGMA_TM_ROTATE 2

// Parameters for Assign mode (per output)
#define ENIGMA_OUTPUT_TRACK 0
#define ENIGMA_OUTPUT_TYPE 1
#define ENIGMA_OUTPUT_SCALE 2
#define ENIGMA_OUTPUT_MIDI_CH 3

// Parameters for Song mode (per step)
#define ENIGMA_STEP_NUMBER 0
#define ENIGMA_STEP_TM 1
#define ENIGMA_STEP_P 2
#define ENIGMA_STEP_REPEATS 3
#define ENIGMA_STEP_TRANSPOSE 4

// Parameters for Play Mode (per track)
#define ENIGMA_TRACK_DIVIDE 0
#define ENIGMA_TRACK_LOOP 1

// Settings for various things
#define ENIGMA_SETTING_LAST 150
#define ENIGMA_NO_STEP_AVAILABLE 0xffff
#define ENIGMA_INITIAL_HELP_TIME 65535

class EnigmaTMWS : public HSApplication, public SystemExclusiveHandler,
    public settings::SettingsBase<EnigmaTMWS, ENIGMA_SETTING_LAST> {
public:
	void Start() {
	    for (byte ix = 0; ix < HS::TURING_MACHINE_COUNT; ix++) state_prob[ix] = 0;
	    tm_cursor = 0;
	    SwitchTuringMachine(0);

	    // Configure outputs and tracks
	    for (byte o = 0; o < 4; o++)
	    {
	        output[o].InitAs(o);
	        track[o].InitAs(o);
	    }

	    ResetSong();

	    // Clear track list
	    BuildTrackStepList(0);
	}

	void Resume() {
	    SwitchTuringMachine(tm_cursor);
	    LoadFromEEPROMStage();
	}

    void Controller() {
        ListenForSysEx();
        if (help_countdown) --help_countdown;

        switch(mode) {
            case ENIGMA_MODE_LIBRARY : LibraryController();
                                       break;

            case ENIGMA_MODE_ASSIGN  : AssignController();
                                       break;

            default                  :
            case ENIGMA_MODE_SONG    :
            case ENIGMA_MODE_PLAY    : SongController();
                                       break;
        }
    }

    void View() {
        DrawHeader();
        DrawInterface();
    }

    // Public access to save method
    void OnSaveSettings() {SaveToEEPROMStage();}

    void OnSendSysEx() {
        SendTuringMachineLibrary();
        SendOutputAssignments();
        SendSong();
    }

    void OnReceiveSysEx() {
        byte V[48];
        if (ExtractSysExData(V, 'T')) {
            char type = V[0]; // Type of Enigma data:r=Register, s=Song step, c=Song Config, 1=single TM
            if (type == 'r') ReceiveTuringMachine(V);
            if (type == 's') ReceiveSongSteps(V);
            if (type == 't') ReceiveTrackSettings(V);
            if (type == 'o') ReceiveOutputAssignments(V);
            if (type == '1') {
                // Receive if the current occupant is not a Favorite
                if (mode == ENIGMA_MODE_LIBRARY) V[1] = tm_cursor;
                if (!HS::user_turing_machines[V[1]].favorite) {
                    ReceiveTuringMachine(V);
                    HS::user_turing_machines[V[1]].favorite = 0; // Favorite off, so that update may be automated in performance
                    SwitchTuringMachine(V[1]);
                }
            }
        }
    }

    /////////////////////////////////////////////////////////////////
    // Control handlers
    /////////////////////////////////////////////////////////////////
    // Left button press changes the Mode
    void OnLeftButtonPress() {
        if (mode == ENIGMA_CONFIRM_RESET) mode = last_mode; // Cancel erase
        else if (++mode > ENIGMA_MODE_PLAY) mode = ENIGMA_MODE_LIBRARY;
        if (mode == ENIGMA_MODE_LIBRARY && play) ++mode;
        ResetCursor();
        help_countdown = help_time;
    }

    void OnLeftButtonLongPress() {
        if (mode == ENIGMA_MODE_LIBRARY) SendTuringMachineLibrary();
        if (mode == ENIGMA_MODE_ASSIGN) SendOutputAssignments();
        if (mode >= ENIGMA_MODE_SONG) SendSong();
    }

    // Right button sets the parameter for the data type
    void OnRightButtonPress() {
        if (help_countdown) DismissHelp();
        else {
            if (mode == ENIGMA_CONFIRM_RESET) {
                total_steps = 0;
                Start();
                mode = last_mode;
            }
            if (mode == ENIGMA_MODE_LIBRARY && !tm_state.IsFavorite()) {
                if (++tm_param > ENIGMA_TM_ROTATE) tm_param = 0;
            }
            if (mode == ENIGMA_MODE_ASSIGN) {
                if (++output_param > ENIGMA_OUTPUT_MIDI_CH) output_param = 0;
                if (output[output_cursor].type() > EnigmaOutputType::NOTE7) {
                    // Skip scale parameters for trigger and gate outputs
                    if (output_param == ENIGMA_OUTPUT_SCALE) output_param = ENIGMA_OUTPUT_MIDI_CH;
                }

            }
            if (mode == ENIGMA_MODE_SONG) {
                if (++step_param > ENIGMA_STEP_TRANSPOSE) step_param = ENIGMA_STEP_NUMBER;
            }
            if (mode == ENIGMA_MODE_PLAY) {
                if (++track_param > ENIGMA_TRACK_LOOP) track_param = ENIGMA_TRACK_DIVIDE;
            }
            ResetCursor();
        }
    }

    void OnUpButtonPress() {
        if (help_countdown) DismissHelp();
        else {
            if (mode == ENIGMA_MODE_LIBRARY) {
                tm_state.SetFavorite(1 - tm_state.IsFavorite());
                state_prob[tm_cursor] = 0;
                if (tm_state.IsFavorite()) {
                    tm_param = ENIGMA_TM_ROTATE;
                    SendSingleTuringMachine(tm_cursor);
                }
            }
            if (mode == ENIGMA_MODE_ASSIGN && !play) assign_audition = 0;
            if (mode == ENIGMA_MODE_SONG) InsertStep();
            if (mode == ENIGMA_MODE_PLAY) {
                play = 1 - play;
                if (play) {
                    // Save the audition assign state for when play is stopped
                    last_assign_audition = assign_audition;
                    assign_audition = 1;
                }
                else assign_audition = last_assign_audition;
            }
        }
    }

    void OnDownButtonPress() {
        if (help_countdown) DismissHelp();
        else {
            if (mode == ENIGMA_MODE_LIBRARY) tm_state.Reset();
            if (mode == ENIGMA_MODE_ASSIGN) assign_audition = 1;
            if (mode == ENIGMA_MODE_SONG) DeleteStep();
            if (mode == ENIGMA_MODE_PLAY) ResetSong();
        }
    }

    void OnDownButtonLongPress() {
        last_mode = mode;
        mode = ENIGMA_CONFIRM_RESET;
    }

    // Left encoder selects the main object in the mode
    void OnLeftEncoderMove(int direction) {
        if (help_countdown) DismissHelp();
        else {
            if (mode == ENIGMA_MODE_LIBRARY) {
                // For Library mode, that's the Turing Machine
                tm_cursor += direction;
                if (tm_cursor < 0) tm_cursor = HS::TURING_MACHINE_COUNT - 1;
                if (tm_cursor >= HS::TURING_MACHINE_COUNT) tm_cursor = 0;
                SwitchTuringMachine(tm_cursor);
            }
            if (mode == ENIGMA_MODE_ASSIGN) {
                output_cursor += direction;
                if (output_cursor < 0) output_cursor = 3;
                if (output_cursor > 3) output_cursor = 0;
            }
            if (mode == ENIGMA_MODE_SONG || mode == ENIGMA_MODE_PLAY) {
                track_cursor += direction;
                if (track_cursor < 0) track_cursor = 3;
                if (track_cursor > 3) track_cursor = 0;
                BuildTrackStepList(track_cursor);
            }
            ResetCursor();
        }
    }

    void OnRightEncoderMove(int direction) {
        if (help_countdown) DismissHelp();
        else {
            switch(mode) {
                case ENIGMA_MODE_LIBRARY : DelegateTMParam(direction);
                                           break;

                case ENIGMA_MODE_ASSIGN  : DelegateOutputParam(direction);
                                           break;

                case ENIGMA_MODE_SONG    : DelegateStepParam(direction);
                                           break;

                default:
                case ENIGMA_MODE_PLAY    : DelegatePlayParam(direction);
                                           break;

            }
            ResetCursor();
        }
    }

private:
    //////// OPERATING STATES
    TuringMachineState tm_state; // The currently-selected state in Library mode
    byte state_prob[HS::TURING_MACHINE_COUNT]; // Remember the last probability
    bool assign_audition = 0; // Which area does Assign monitor? 0=Library, 1=Song
    bool last_assign_audition; // Temporarily save the old audition state during playback
    uint16_t track_step[100]; // List of steps in the current track
    uint16_t total_steps = 0; // Total number of song_step[] entries used; index of the next step
    byte last_track_step_index = 0; // For adding the next step
    uint16_t help_countdown = 0; // Display help screen for this many more ticks
    uint16_t help_time = ENIGMA_INITIAL_HELP_TIME; // Starting time for help, per mode

    //////// PLAYBACK
    bool play = 0; // Playback mode
    uint32_t clock_counter; // Counts clocks for clock division
    uint16_t playback_step_index[4]; // Index within song_step[]
    byte playback_step_number[4]; // Step for each track, ordinal
    byte playback_step_repeat[4]; // Which repeat
    byte playback_step_beat[4]; // Which beat number
    bool playback_end[4]; // End non-looping playback until reset
    TuringMachineState track_tm[4]; // Turing Machine states for each track

    //////// DATA
    EnigmaStep song_step[400]; // Max 99 steps per track
    EnigmaOutput output[4];
    EnigmaTrack track[4];

    //////// NAVIGATION
    byte mode = 0; // 0=Library 1=Assign 2=Song
    byte last_mode = 0; // Stores previous mode for special screen(s)
    int16_t edit_index = 0; // Current Song Mode step within track_step[]

    // Primary object cursors, one for each mode
    int8_t tm_cursor = 0; // For Library mode, choose the Turing Machine (0-39: A1-F8)
    int8_t output_cursor = 0; // For Output mode, choose the Output (0-3: A~D)
    int8_t track_cursor = 0; // For Song mode, choose the Track number (0-3: Tr1-Tr4)

    // Secondary object parameter cursors, one for each mode
    int8_t tm_param = 0; // 0=Length, 1=Probability
    int8_t output_param = 0; // 0=Track, 1=Type, 2=Scale, 3=MIDI Out Channel
    int8_t step_param = 0; // 0=Step#, 1=TM, 2=p, 3=Repeats, 4=Transpose
    int8_t track_param = 0; // 0=Clock Divide, 1=Loop

    void DrawHeader() {
        gfxHeader("Enigma - ");
        if (mode == ENIGMA_CONFIRM_RESET) gfxPrint("New Song");
        if (mode == ENIGMA_MODE_LIBRARY) gfxPrint("Library");
        if (mode == ENIGMA_MODE_ASSIGN) gfxPrint("Assign");
        if (mode == ENIGMA_MODE_SONG) gfxPrint("Song");
        if (mode == ENIGMA_MODE_PLAY) gfxPrint("Play");
    }

    // DrawInterface()'s job is to delegate to the various mode screens.
    void DrawInterface() {
        switch(mode) {
            case ENIGMA_CONFIRM_RESET : DrawResetScreen();
                                        break;

            case ENIGMA_MODE_LIBRARY  : DrawLibraryInterface();
                                        break;

            case ENIGMA_MODE_ASSIGN   : DrawAssignInterface();
                                        break;

            case ENIGMA_MODE_SONG     : DrawSongInterface();
                                        break;

            default                   :
            case ENIGMA_MODE_PLAY     : DrawPlayInterface();
                                        break;
        }
    }

    void DrawResetScreen() {
        gfxPrint(0, 15, "Erase song and reset");
        gfxPrint(0, 25, "outputs: Sure?");
        gfxPrint(96, 55, "[YES]");
        gfxPrint(0, 55, "[NO]");
    }

    void DrawLibraryInterface() {
        // Draw the left side, the selector
        for (byte line = 0; line < 4; line++)
        {
            byte y = 24 + (line * 10);
            byte ix = (tm_cursor + line) % HS::TURING_MACHINE_COUNT;
            char name[4];
            HS::TuringMachine::SetName(name, ix);
            gfxPrint(3, y, name);
            if (HS::user_turing_machines[ix].favorite) gfxIcon(36, y, FAVORITE_ICON);
        }
        DrawSelectorBox("Register");

        if (help_countdown) DrawLibraryHelp();
        else {
            // The right side is for editing
            // Length
            byte length = tm_state.GetLength();
            gfxIcon(64, 14, NOTE_ICON);
            gfxPrint(76 + pad(10, length), 15, length);

            // Probability
            byte p = state_prob[tm_cursor];
            gfxPrint(96, 15, "p=");
            if (tm_state.IsFavorite()) gfxIcon(116, 15, FAVORITE_ICON);
            else gfxPrint(pad(100, p), p);

            // Cursors
            if (!tm_state.IsFavorite()) {
                if (tm_param == ENIGMA_TM_PROBABILITY) gfxCursor(109, 23, 18);
                if (tm_param == ENIGMA_TM_LENGTH) gfxCursor(77, 23, 12);
            }
            if (tm_param == ENIGMA_TM_ROTATE) {
                gfxIcon(87, 27, ROTATE_L_ICON);
                gfxIcon(95, 27, ROTATE_R_ICON);
            }

            // TM Graphic
            tm_state.DrawAt(64, 40);
        }
    }

    void DrawAssignInterface() {
        // Play status at top of screen
        if (play) gfxIcon(118, 0, PLAY_ICON);
        else gfxIcon(118, 0, PAUSE_ICON);

        // Draw the left side, the selector
        for (byte line = 0; line < 4; line++)
        {
            byte y = 24 + (line * 10);
            byte ix = (output_cursor + line) % 4;
            char out_name[2] = {static_cast<char>(ix + 'A'), '\0'};
            gfxPrint(3, y, out_name);
            gfxPrint(": Tk");
            gfxPrint(output[ix].track() + 1);
        }
        DrawSelectorBox("Output");

        if (help_countdown) DrawAssignHelp();
        else {
            // The right side is for editing
            // Track
            gfxPrint(56, 25, "Track ");
            gfxPrint(output[output_cursor].track() + 1);

            // Type
            gfxPrint(56, 35, enigma_type_names[output[output_cursor].type()]);

            // Scale
            if (output[output_cursor].type() <= EnigmaOutputType::NOTE7) {
                gfxPrint(56, 45, OC::scale_names_short[output[output_cursor].scale()]);
            }

            // MIDI Channel
            gfxPrint(56, 55, "MIDI Ch ");
            gfxPrint(midi_channels[output[output_cursor].midi_channel()]);

            // Cursor
            if (output_param == ENIGMA_OUTPUT_TRACK) gfxCursor(57, 33, 42);
            if (output_param == ENIGMA_OUTPUT_TYPE) gfxCursor(57, 43, 70);
            if (output_param == ENIGMA_OUTPUT_SCALE) gfxCursor(57, 53, 30);
            if (output_param == ENIGMA_OUTPUT_MIDI_CH) gfxCursor(105, 63, 18);

            // Audition
            gfxIcon(56, 15, AUDITION_ICON);
            gfxPrint(68, 15, assign_audition ? "Song" : "Library");
            gfxLine(48, 23, 127, 23);
        }
    }

    void DrawSongInterface() {
        // Draw the memory indicator at the top
        int pct = (39600 - (total_steps * 100)) / 396;
        gfxPrint(104 + pad(100, pct), 1, pct);
        gfxPrint("%");
        if (total_steps > 32) gfxInvert(110, 0, 18, 9);

        // Draw the left side, the selector
        for (byte line = 0; line < 4; line++)
        {
            byte y = 24 + (line * 10);
            byte t = (track_cursor + line) % 4;
            gfxPrint(3, y, "Track");
            gfxPrint(39, y, t + 1);
        }
        DrawSelectorBox("Track");


        if (help_countdown) DrawSongHelp();
        else {
            // The right side is for editing
            // Step parameters
            uint16_t ssi = track_step[edit_index]; // The song step index
            if (ssi < ENIGMA_NO_STEP_AVAILABLE) {
                gfxPrint(56 + pad(10, edit_index + 1), 15, edit_index + 1); // Step number (1-99)
                gfxPrint(": ");
                char name[4];
                HS::TuringMachine::SetName(name, song_step[ssi].tm());
                gfxPrint(name); // Turing machine name
                gfxPrint(" ");

                if (step_param == ENIGMA_STEP_TM) {
                    // If the Turing Machine is being selected, display the length and favorite
                    // status instead of the probability
                    byte length = HS::user_turing_machines[song_step[ssi].tm()].len;
                    bool favorite = HS::user_turing_machines[song_step[ssi].tm()].favorite;

                    if (length > 0) gfxPrint(pad(10, length), length);
                    else gfxPrint("--");
                    if (favorite) gfxIcon(119, 15, FAVORITE_ICON);
                } else {
                    gfxPrint(pad(100, song_step[ssi].p()), song_step[ssi].p());
                    gfxPrint("%");
                }
                gfxPrint(80, 25, "x");
                gfxPrint(pad(10, song_step[ssi].repeats()), song_step[ssi].repeats()); // Number of times played
                gfxPrint("  ");
                gfxPrint(song_step[ssi].transpose() > -1 ? "+" : "");
                gfxPrint(song_step[ssi].transpose()); // Transpose

                // Cursor
                if (step_param == ENIGMA_STEP_NUMBER && CursorBlink()) gfxInvert(56, 14, 12, 9);
                if (step_param == ENIGMA_STEP_TM) gfxCursor(81, 23, 18);
                if (step_param == ENIGMA_STEP_P) gfxCursor(105, 23, 18);
                if (step_param == ENIGMA_STEP_REPEATS) gfxCursor(87, 33, 12);
                if (step_param == ENIGMA_STEP_TRANSPOSE) gfxCursor(117, 33, 12);
            } else {
                DrawSongHelp();
            }

            // Draw the next three steps
            if (last_track_step_index > 0) {
                bool stop = 0; // Show the Stop/Loop step only once
                for (byte n = 0; n < 3; n++)
                {
                    if (edit_index + n + 1 < 99) {
                        uint16_t ssi = track_step[edit_index + n + 1];
                        byte y = 35 + (10 * n);
                        if (ssi < ENIGMA_NO_STEP_AVAILABLE) {
                            gfxPrint(56 + pad(10, edit_index + n + 2), y, edit_index + n + 2); // Step number (1-99)
                            gfxPrint(": ");
                            char name[4];
                            HS::TuringMachine::SetName(name, song_step[ssi].tm());
                            gfxPrint(name); // Turing machine name
                            gfxPrint(" x");
                            gfxPrint(song_step[ssi].repeats()); // Number of times played
                        } else if (!stop) {
                            stop = 1;
                            gfxPrint(56 + pad(10, edit_index + n + 2), y, edit_index + n + 2); // Step number (1-99)
                            gfxPrint(": ");
                            gfxPrint(track[track_cursor].loop() ? "< Loop >" : "< Stop >");
                        }
                    }
                }
            }
        }
    }

    void DrawPlayInterface() {
        // The Play interface is different from the others; it's four rows, with one row
        // for each track

        // Play status at top of screen
        if (play) gfxIcon(118, 0, PLAY_ICON);
        else gfxIcon(118, 0, PAUSE_ICON);

        if (help_countdown) DrawPlayHelp();
        else {
            // Headers
            // Track, Step/Repeat, Register, Divide, Loop
            gfxPrint(0, 15, "Tk Step  Reg");
            gfxLine(0, 23, 127, 23);
            gfxIcon(80, 14, CLOCK_ICON);
            gfxIcon(106, 14, LOOP_ICON);

            for (int t = 0; t < 4; t++)
            {
                uint16_t ssi = playback_step_index[t]; // playback_step_index is the index of the song_step
                byte y = 25 + (t * 10);
                gfxPrint(0, y, t + 1);
                if (playback_step_number[t] == 0) {
                    gfxIcon(30, y, RESET_ICON);
                } else if (ssi != ENIGMA_NO_STEP_AVAILABLE) {
                    gfxPrint(18 + pad(10, playback_step_number[t]), y, playback_step_number[t]);
                    gfxPrint(":");
                    gfxPrint(playback_step_repeat[t] + 1);
                    char name[4];
                    HS::TuringMachine::SetName(name, song_step[ssi].tm());
                    gfxPrint(54, y, name);
                    track_tm[t].DrawSmallAt(54, y + 8);
                }

                // Clock Divide
                gfxPrint(78, y, "/");
                gfxPrint(track[t].divide());

                // Loop
                if (track[t].loop()) gfxIcon(106, y, LOOP_ICON);
                else gfxIcon(106, y, PLAYONCE_ICON);

                // Play status
                if (ssi != ENIGMA_NO_STEP_AVAILABLE) {
                    if (playback_end[t]) gfxIcon(118, y, STOP_ICON);
                    else if (play || !CursorBlink()) gfxIcon(118, y, PLAY_ICON);
                }

                if (t == track_cursor) {
                    if (track_param == ENIGMA_TRACK_DIVIDE) gfxCursor(84, y + 8, 12);
                    if (track_param == ENIGMA_TRACK_LOOP) gfxCursor(106, y + 8, 8);
                }
            }
        }
    }

    //////// Help Splash Screens
    void DrawLibraryHelp() {
        gfxIcon(56, 15, UP_BTN_ICON);
        gfxIcon(66, 15, FAVORITE_ICON);
        gfxIcon(56, 25, DOWN_BTN_ICON);
        gfxPrint(66, 25, "Reset");
    }

    void DrawAssignHelp() {
        gfxIcon(56, 15, UP_BTN_ICON);
        gfxIcon(66, 15, AUDITION_ICON);
        gfxPrint(76, 15, "Library");
        gfxIcon(56, 25, DOWN_BTN_ICON);
        gfxIcon(66, 25, AUDITION_ICON);
        gfxPrint(76, 25, "Song");
    }

    void DrawSongHelp() {
        gfxIcon(56, 15, UP_BTN_ICON);
        gfxPrint(66, 15, "Add Step");
        gfxIcon(56, 25, DOWN_BTN_ICON);
        gfxPrint(66, 25, "Del Step");
    }

    void DrawPlayHelp() {
        gfxIcon(56, 15, UP_BTN_ICON);
        gfxPrint(66, 15, "Pause/Play");
        gfxIcon(56, 25, DOWN_BTN_ICON);
        gfxPrint(66, 25, "Reset");
    }



    /*
     *  Used by all Modes to allow selection of Primary Objects. This should go after the
     *  lines are drawn, because the first name on the list, as the selected object, is
     *  reversed
     */
    void DrawSelectorBox(const char* object) {
        gfxPrint(0, 15, object);
        gfxFrame(0, 23, 48, 40); // Selector window
        for (byte line = 0; line < 3; line++) gfxLine(0, 32 + (10 * line),  47, 32 + (10 * line));
        gfxInvert(1, 24, 46, 9);
    }

    // When a new TM is selected, load it here
    void SwitchTuringMachine(byte ix) {
        tm_state.Init(ix);
        tm_state.SetWriteMode(1); // This method is called from the Library, so set write access
        if (tm_state.IsFavorite()) tm_param = ENIGMA_TM_ROTATE;
    }

    //////// Parameter delegates
    void DelegateTMParam(int direction) {
        if (!tm_state.IsFavorite()) {
            if (tm_param == ENIGMA_TM_LENGTH) tm_state.ChangeLength(direction);
            if (tm_param == ENIGMA_TM_PROBABILITY)
                state_prob[tm_cursor] = constrain(state_prob[tm_cursor] + direction, 0, 100);
        }
        if (tm_param == ENIGMA_TM_ROTATE) tm_state.Rotate(direction);
    }

    void DelegateOutputParam(int direction) {
        if (output_param == ENIGMA_OUTPUT_TRACK) {
            if (output[output_cursor].track() > 0 || direction > 0)
                output[output_cursor].set_track(output[output_cursor].track() + direction);
        }
        if (output_param == ENIGMA_OUTPUT_TYPE) {
            if (output[output_cursor].type() > 0 || direction > 0)
                output[output_cursor].set_type(output[output_cursor].type() + direction);
        }
        if (output_param == ENIGMA_OUTPUT_SCALE) {
            if (output[output_cursor].scale() > 0 || direction > 0)
                output[output_cursor].set_scale(output[output_cursor].scale() + direction);
        }
        if (output_param == ENIGMA_OUTPUT_MIDI_CH) {
            if (output[output_cursor].midi_channel() > 0 || direction > 0)
                output[output_cursor].set_midi_channel(output[output_cursor].midi_channel() + direction);
        }
    }

    void DelegateStepParam(int direction) {
        if (last_track_step_index > 0) {
            // Select a step
            if (step_param == ENIGMA_STEP_NUMBER) {
                if (edit_index > 0 || direction > 0) {
                    // If there's a step to move into, move into it
                    if (edit_index + direction < 100 && edit_index + direction >= 0)
                        if (track_step[edit_index + direction] != ENIGMA_NO_STEP_AVAILABLE) edit_index += direction;
                }
            }

            // Edit a step
            if (step_param > ENIGMA_STEP_NUMBER) {
                uint16_t ssi = track_step[edit_index]; // song step index
                if (step_param == ENIGMA_STEP_TM) {
                    if (song_step[ssi].tm() > 0 || direction > 0)
                        song_step[ssi].set_tm(song_step[ssi].tm() + direction);
                }
                if (step_param == ENIGMA_STEP_P) {
                    if (song_step[ssi].p() > 0 || direction > 0)
                        song_step[ssi].set_p(song_step[ssi].p() + direction);
                }
                if (step_param == ENIGMA_STEP_REPEATS) {
                    if (song_step[ssi].repeats() > 0 || direction > 0)
                        song_step[ssi].set_repeats(song_step[ssi].repeats() + direction);
                }
                if (step_param == ENIGMA_STEP_TRANSPOSE) {
                    if (song_step[ssi].transpose() > -48 || direction > 0)
                        song_step[ssi].set_transpose(song_step[ssi].transpose() + direction);
                }
            }
        }
    }

    void DelegatePlayParam(int direction) {
        // Track paramters
        if (track_param == ENIGMA_TRACK_DIVIDE) {
            if (track[track_cursor].divide() > 0 || direction > 0)
                track[track_cursor].set_divide(track[track_cursor].divide() + direction);
        }
        if (track_param == ENIGMA_TRACK_LOOP)
            track[track_cursor].set_loop(1 - track[track_cursor].loop());
    }

    //////// Controllers
    void LibraryController() {
        // If a clock is present, advance the TuringMachineState
        if (Clock(0)) {
            uint16_t reg = tm_state.GetRegister();
            int deferred_note = -1;
            for (byte o = 0; o < 4; o++)
            {
                output[o].SendToDAC<EnigmaTMWS>(this, reg);

                if (deferred_note > -1) output[o].SetDeferredNote(deferred_note);
                output[o].SendToMIDI(reg);
                if (output[o].GetDeferredNote() > -1) deferred_note = output[o].GetDeferredNote();
            }
            tm_state.Advance(state_prob[tm_cursor]);
        }
    }

    void AssignController() {
        if (assign_audition) SongController();
        else LibraryController();
    }

    void SongController() {
        // Digital 1: Advance
        // Digital 2: Reset
        // Digital 3: Reset and Play
        // Digital 4: Toggle Play
        // CV 1: Gate Song End
        // CV 2: Gate Song Restart

        if (Clock(1) || Clock(2)) ResetSong();
        if (Clock(2)) play = 1;
        if (Clock(3)) play = 1 - play;

        if (play && Clock(0)) {
            clock_counter++;
            int deferred_note = -1;
            for (byte t = 0; t < 4; t++)
            {
                if (!playback_end[t] && clock_counter % track[t].divide() == 0) {
                    uint16_t ssi = playback_step_index[t]; // song_step index
                    if (ssi != ENIGMA_NO_STEP_AVAILABLE) {
                        // If the repeat and beat are both at 0, set the Turing Machine state
                        if (playback_step_repeat[t] == 0 && playback_step_beat[t] == 0) {
                            track_tm[t].Init(song_step[ssi].tm());
                            playback_step_number[t]++;
                        }

                        // Advance to the next beat
                        playback_step_beat[t]++;

                        // If that was the last beat, reset the beat to 0 and start a new repeat
                        if (playback_step_beat[t] >= track_tm[t].GetLength()) {
                            playback_step_beat[t] = 0;
                            playback_step_repeat[t]++;

                            // If that was the last repeat, advance to the next step
                            if (playback_step_repeat[t] >= song_step[ssi].repeats()) {
                                playback_step_repeat[t] = 0;
                                // Find the next step for this track
                                bool found = 0;
                                for (uint16_t s = ssi + 1; s < total_steps; s++)
                                {
                                    if (song_step[s].track() == t) {
                                        playback_step_index[t] = s;
                                        found = 1;
                                        break;
                                    }
                                }
                                // At this point, beat and repeat are both 0, so the Turing Machine State
                                // will be initialized on the next clock

                                if (!found) {
                                    // If no next step for the track was found, either end playback by setting
                                    // the step index to ENIGMA_NO_STEP_AVAILABLE, or loop to the beginning
                                    if (track[t].loop()) {
                                        playback_step_index[t] = GetFirstStep(t);
                                        playback_step_number[t] = 0;
                                    } else {
                                        playback_end[t] = 1;
                                    }
                                }
                            }
                        }

                        // Send the track to the appopriate outputs
                        for (byte o = 0; o < 4; o++)
                        {
                            if (output[o].track() == t) {
                                uint16_t reg = track_tm[t].GetRegister();
                                output[o].SendToDAC<EnigmaTMWS>(this, reg, song_step[ssi].transpose() * 128);

                                if (deferred_note > -1) output[o].SetDeferredNote(deferred_note);
                                output[o].SendToMIDI(reg, song_step[ssi].transpose() * 128);
                                if (output[o].GetDeferredNote() > -1) deferred_note = output[o].GetDeferredNote();
                            }
                        }

                        track_tm[t].Advance(song_step[ssi].p());
                    } else { // End of step availability check
                        playback_end[t] = 1;
                    }
                } // End of track playback and clock divider
            } // End of track loop

            // Song End/Repeat
            // If CV 1 is gated:
            //     If all non-looping tracks have ended, the song is over, and playback stops
            //
            // If CV 2 is gated:
            //     As above, except the song restarts from the top instead
            //
            // If CV 1 and CV 2 are ungated:
            //     End the song only if ALL tracks are non-looping
            bool keep_going = 0;
            if (In(0) > HSAPPLICATION_3V || In(1) > HSAPPLICATION_3V) {
                for (byte t = 0; t < 4; t++) if (!track[t].loop() && !playback_end[t]) keep_going = 1;
            } else {
                for (byte t = 0; t < 4; t++) if (track[t].loop() || !playback_end[t]) keep_going = 1;
            }
            if (!keep_going) {
                if (In(1) > HSAPPLICATION_3V) ResetSong();
                else play = 0;
            }

        } // End of Digital 1 handler
    }

    //////// Data Collection
    void BuildTrackStepList(byte track) {
        byte ts_ix = 0;
        for (uint16_t ix = 0; ix < total_steps; ix++)
        {
            if (song_step[ix].track() == track) {
                // Found a step for the selected track; add it to the track list
                track_step[ts_ix++] = ix;
                if (ts_ix > 99) break;
            }
        }
        last_track_step_index = ts_ix;

        // Avoid beaming into a point after the end of the buffer
        if (edit_index >= last_track_step_index) edit_index = last_track_step_index - 1;
        if (edit_index < 0) edit_index = 0;

        // ENIGMA_NO_STEP_AVAILABLE indictates end of track
        while (ts_ix < 100) track_step[ts_ix++] = ENIGMA_NO_STEP_AVAILABLE;
    }

    // Insert a step to the end of the current track
    void InsertStep() {
        if (last_track_step_index < 99) {
            uint16_t insert_point = track_step[edit_index] + 1;
            if (insert_point < total_steps) {
                // Insert a step after the current step; otherwise, it'll just go at the end
                for (int i = total_steps + 1; i > insert_point; i--)
                {
                    memcpy(&song_step[i], &song_step[i - 1], sizeof(song_step[i - 1]));
                }
            }
            song_step[insert_point].Init(track_cursor);

            // Housekeeping tasks: increment the total steps and index and rebuild the step list for the track
            total_steps++;
            edit_index++;
            BuildTrackStepList(track_cursor);
        }
    }

    // Delete a step at the current edit index point
    void DeleteStep() {
        if (last_track_step_index > 0) { // Can't delete if there are no steps
            // If the last step is being deleted, move the index back. Otherwise, it will
            // just stay the same and the next step up will become active.
            if (edit_index == last_track_step_index) edit_index--;

            // Move the steps down to fill in the memory
            uint16_t mark_to_delete = track_step[edit_index];
            for (int i = mark_to_delete; i < total_steps; i++)
            {
                memcpy(&song_step[i], &song_step[i + 1], sizeof(song_step[i + 1]));
            }

            // Housekeeping tasks: decrement the total steps and rebuild the step list for the track
            total_steps--;
            BuildTrackStepList(track_cursor);
        }
    }

    void ResetSong() {
        clock_counter = 0;
        for (byte t = 0; t < 4; t++)
        {
            playback_step_index[t] = GetFirstStep(t);
            playback_step_number[t] = 0;
            playback_step_repeat[t] = 0;
            playback_step_beat[t] = 0;
            playback_end[t] = 0;

            output[t].NoteOff();
        }
    }

    uint16_t GetFirstStep(byte track) {
        uint16_t step = ENIGMA_NO_STEP_AVAILABLE;
        for (uint16_t s = 0; s < total_steps; s++)
        {
            if (song_step[s].track() == track) {
                step = s;
                break;
            }
        }
        return step;
    }

    void DismissHelp() {
        uint16_t help_seen_for = help_time - help_countdown;
        help_time = help_seen_for;
        if (help_time < 16667) help_time = 0; // If dismissed within 1 second, stop showing help
        help_countdown = 0;
    }

    //////// SysEx
    void SendTuringMachineLibrary() {
        for (byte tm = 0; tm < HS::TURING_MACHINE_COUNT; tm++)
        {
            uint16_t reg = HS::user_turing_machines[tm].reg;
            byte len = HS::user_turing_machines[tm].len;
            byte favorite = HS::user_turing_machines[tm].favorite;

            byte V[6];
            byte ix = 0;
            V[ix++] = 'r'; // Indicates a register is being sent
            V[ix++] = tm; // Register index in Library
            V[ix++] = static_cast<byte>(reg & 0xff);  // Low Byte
            V[ix++] = static_cast<byte>((reg >> 8) & 0xfff);  // High Byte
            V[ix++] = len;
            V[ix++] = favorite;
            UnpackedData unpacked;
            unpacked.set_data(ix, V);
            PackedData packed = unpacked.pack();
            SendSysEx(packed, 'T');
        }
    }

    void SendSingleTuringMachine(byte tm) {
        uint16_t reg = HS::user_turing_machines[tm].reg;
        byte len = HS::user_turing_machines[tm].len;
        byte favorite = HS::user_turing_machines[tm].favorite;

        byte V[6];
        byte ix = 0;
        V[ix++] = '1'; // Indicates a single register is being sent
        V[ix++] = tm; // Register index in Library (origin, but unused on receive)
        V[ix++] = static_cast<byte>(reg & 0xff);  // Low Byte
        V[ix++] = static_cast<byte>((reg >> 8) & 0xfff);  // High Byte
        V[ix++] = len;
        V[ix++] = favorite;
        UnpackedData unpacked;
        unpacked.set_data(ix, V);
        PackedData packed = unpacked.pack();
        SendSysEx(packed, 'T');
    }

    void SendSong() {
        byte V[48];
        byte ix;

        // Send song data
        ix = 0;
        byte pages = (total_steps / 8) + 1;
        for (byte p = 0; p < pages; p++)
        {
            V[ix++] = 's'; // Indicates a song step page is being sent
            V[ix++] = p; // Page number
            V[ix++] = static_cast<byte>(total_steps & 0xff); // Total steps, low byte
            V[ix++] = static_cast<byte>((total_steps >> 8) & 0xff); // Total steps, high byte
            for (byte s = 0; s < 10; s++)
            {
                byte ssi = (p * 8) + s;
                V[ix++] = song_step[ssi].tk;
                V[ix++] = song_step[ssi].pr;
                V[ix++] = song_step[ssi].re;
                V[ix++] = song_step[ssi].tr;
            }
            UnpackedData unpacked;
            unpacked.set_data(ix, V);
            PackedData packed = unpacked.pack();
            SendSysEx(packed, 'T');
        }

        // Send track settings
        ix = 0;
        V[ix++] = 't'; // Indicates a set of output assignments
        for (int t = 0; t < 4; t++) V[ix++] = track[t].data;
        UnpackedData unpacked;
        unpacked.set_data(ix, V);
        PackedData packed = unpacked.pack();
        SendSysEx(packed, 'T');

    }

    void SendOutputAssignments() {
        byte V[48];
        byte ix = 0;
        V[ix++] = 'o'; // Indicates a set of output assignments
        for (byte o = 0; o < 4; o++)
        {
            V[ix++] = output[o].tk;
            V[ix++] = output[o].ty;
            V[ix++] = output[o].sc;
            V[ix++] = output[o].mc;
        }
        UnpackedData unpacked;
        unpacked.set_data(ix, V);
        PackedData packed = unpacked.pack();
        SendSysEx(packed, 'T');
    }

    void ReceiveTuringMachine(uint8_t *V) {
        byte ix = 1; // index 0 was already handled
        byte tm = V[ix++];
        if (tm < HS::TURING_MACHINE_COUNT) {
            uint8_t low = V[ix++];
            uint8_t high = V[ix++];
            uint16_t reg = static_cast<uint16_t>((high << 8) | low);
            byte len = V[ix++];
            byte favorite = V[ix++];
            HS::user_turing_machines[tm].reg = reg;
            HS::user_turing_machines[tm].len  = len;
            HS::user_turing_machines[tm].favorite = favorite;
        }
    }

    void ReceiveSongSteps(uint8_t *V) {
        byte ix = 1;
        byte page = V[ix++];
        byte low = V[ix++];
        byte high = V[ix++];
        total_steps = static_cast<uint16_t>((high << 8) | low);
        for (byte s = 0; s < 8; s++)
        {
            byte ssi = (page * 8) + s;
            song_step[ssi].tk = V[ix++];
            song_step[ssi].pr = V[ix++];
            song_step[ssi].re = V[ix++];
            song_step[ssi].tr = V[ix++];
        }
        BuildTrackStepList(0);
        track_cursor = 0;
        edit_index = 0;
    }

    void ReceiveTrackSettings(uint8_t *V) {
        byte ix = 1;
        for (byte t = 0; t < 4; t++) track[t].data = V[ix++];
    }

    void ReceiveOutputAssignments(uint8_t *V) {
        byte ix = 1;
        for (byte o = 0; o < 4; o++)
        {
            output[o].tk = V[ix++];
            output[o].ty = V[ix++];
            output[o].sc = V[ix++];
            output[o].mc = V[ix++];
        }
    }

    //////// Data Storage
    void SaveToEEPROMStage() {
        byte ix = 0;

        // Outputs
        for (byte o = 0; o < 4; o++)
        {
            values_[ix++] = output[o].tk;
            values_[ix++] = output[o].ty;
            values_[ix++] = output[o].sc;
            values_[ix++] = output[o].mc;
        }

        int song_steps = constrain(total_steps, 0, 32);
        values_[ix++] = static_cast<byte>(song_steps);

        // First 32 song steps
        for (byte s = 0; s < 32; s++)
        {
            values_[ix++] = song_step[s].tk;
            values_[ix++] = song_step[s].pr;
            values_[ix++] = song_step[s].re;
            values_[ix++] = song_step[s].tr;
        }

        // Track settings
        for (byte t = 0; t < 4; t++) values_[ix++] = track[t].data;
    }

    void LoadFromEEPROMStage() {
        byte ix = 0;

        // Outputs
        for (byte o = 0; o < 4; o++)
        {
            output[o].tk = values_[ix++];
            output[o].ty = values_[ix++];
            output[o].sc = values_[ix++];
            output[o].mc = values_[ix++];
        }

        // Song length
        byte song_steps = values_[ix++];
        if (song_steps > total_steps) total_steps = song_steps;

        // Song Steps
        for (byte s = 0; s < 32; s++)
        {
            song_step[s].tk = values_[ix++];
            song_step[s].pr = values_[ix++];
            song_step[s].re = values_[ix++];
            song_step[s].tr = values_[ix++];
        }

        // Track settings
        for (byte t = 0; t < 4; t++) track[t].data = values_[ix++];

        // Reset everything if there's no data (meaning, song steps is 0)
        if (song_steps == 0) Start();

        // Clear track list
        else BuildTrackStepList(0);
        track_cursor = 0;
        edit_index = 0;
    }
};

// Setting declarations for EEPROM storage, which consists of the following:
//     Four output assignments @ 4 bytes each =  16 bytes
//     The first 32 song steps @ 4 bytes each = 128 bytes
//     Four track settings @ 1 byte each      =   4 bytes
//     Song length                            =   1 byte
// TOTAL EEPROM SIZE: 150 bytes
#define ENIGMA_EEPROM_DATA {0,0,255,"St",NULL,settings::STORAGE_TYPE_U8},
#define ENIGMA_DO_THIRTY_TIMES(A) A A A A A A A A A A A A A A A A A A A A A A A A A A A A A A
SETTINGS_DECLARE(EnigmaTMWS, ENIGMA_SETTING_LAST) {
    ENIGMA_DO_THIRTY_TIMES(ENIGMA_EEPROM_DATA)
    ENIGMA_DO_THIRTY_TIMES(ENIGMA_EEPROM_DATA)
    ENIGMA_DO_THIRTY_TIMES(ENIGMA_EEPROM_DATA)
    ENIGMA_DO_THIRTY_TIMES(ENIGMA_EEPROM_DATA)
    ENIGMA_DO_THIRTY_TIMES(ENIGMA_EEPROM_DATA)
};

EnigmaTMWS EnigmaTMWS_instance;

// App stubs
void EnigmaTMWS_init() {
    EnigmaTMWS_instance.BaseStart();
}

constexpr size_t EnigmaTMWS_storageSize() {
    return EnigmaTMWS::storageSize();
}
size_t EnigmaTMWS_save(void *storage) {
    return EnigmaTMWS_instance.Save(storage);
}
size_t EnigmaTMWS_restore(const void *storage) {
    return EnigmaTMWS_instance.Restore(storage);
}

void EnigmaTMWS_isr() {
	return EnigmaTMWS_instance.BaseController();
}

void EnigmaTMWS_handleAppEvent(OC::AppEvent event) {
    if (event ==  OC::APP_EVENT_RESUME) {
        EnigmaTMWS_instance.Resume();
    }
    if (event == OC::APP_EVENT_SUSPEND) {
        EnigmaTMWS_instance.OnSaveSettings();
        EnigmaTMWS_instance.OnSendSysEx();
    }
}

void EnigmaTMWS_loop() {} // Deprecated

void EnigmaTMWS_menu() {
    EnigmaTMWS_instance.BaseView();
}

void EnigmaTMWS_screensaver() {} // Deprecated

void EnigmaTMWS_handleButtonEvent(const UI::Event &event) {
    // For left encoder, handle press and long press
    if (event.control == OC::CONTROL_BUTTON_L) {
        if (event.type == UI::EVENT_BUTTON_LONG_PRESS) EnigmaTMWS_instance.OnLeftButtonLongPress();
        if (event.type == UI::EVENT_BUTTON_PRESS) EnigmaTMWS_instance.OnLeftButtonPress();
    }

    // For right encoder, only handle press (long press is reserved)
    if (event.control == OC::CONTROL_BUTTON_R && event.type == UI::EVENT_BUTTON_PRESS) EnigmaTMWS_instance.OnRightButtonPress();

    // For up button, handle only press (long press is reserved)
    if (event.control == OC::CONTROL_BUTTON_UP && event.type == UI::EVENT_BUTTON_PRESS) EnigmaTMWS_instance.OnUpButtonPress();

    // For down button, handle press and long press
    if (event.control == OC::CONTROL_BUTTON_DOWN) {
        if (event.type == UI::EVENT_BUTTON_PRESS) EnigmaTMWS_instance.OnDownButtonPress();
        if (event.type == UI::EVENT_BUTTON_LONG_PRESS) EnigmaTMWS_instance.OnDownButtonLongPress();
    }
}

void EnigmaTMWS_handleEncoderEvent(const UI::Event &event) {
    // Left encoder turned
    if (event.control == OC::CONTROL_ENCODER_L) EnigmaTMWS_instance.OnLeftEncoderMove(event.value);

    // Right encoder turned
    if (event.control == OC::CONTROL_ENCODER_R) EnigmaTMWS_instance.OnRightEncoderMove(event.value);
}


#endif
