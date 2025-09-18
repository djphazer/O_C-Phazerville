// Copyright (c) 2020, Jason Justian, Nick Beirne
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

// Uses the memory space in OC_patterns to sequence. Like a mini sequins.
// 4 sequences are played in parallel. The note values may be configured with sequins or an upcoming sequence editor. CV can manipulate the sequences in a variety of ways.
//
// There are two channels controlled with a single clock (GATE 1). Gate 2 is a reset to makes the internal sequwncer's step equal to 0.
// Each channel has a few different modes, controllable with the encoder.
//
// Modes:
// OCT:  you are in this mode when the sequence arrow is solid. The encoder will rotate through sequences and it will play the chosen sequence, with CV controlling the octave (by 1 or 2 octaves).
// PICK: In this mode CV will pick which sequence is playing. Sampled after each clock.
// RAND: The CV is ignored, and instead a random sequence is chosen at random.
// QUAN: The internal sequences are ignored and the CV will be quanitzed to a global scale. Right now that's c-dorian and only configurable in code.

#include "MiniSeq.h"

#define PICK_MODE -1
#define RAND_MODE -2
#define QUAN_MODE -3

#define OCTAVE_1 (12 << 7)

// 5.5 volts to play well with pressure points
#define PP_MAX_INPUT_CV (HEMISPHERE_MAX_INPUT_CV * 11 / 12)
#define OCTAVE_RANGE 2

class SwitchSeq : public HemisphereApplet
{
public:
    const char *applet_name() { return "SwitchSeq"; }
    const uint8_t* applet_icon() { return PhzIcons::switchSeq; }

    void Start()
    {
      for (int i = 0; i < 4; ++i) {
        miniseq[i].SetPattern(i);
      }
      Out(0, 0); 
      Out(1, 0);
    }

    void Controller()
    {
        // global clock. Do it before reset.
        if (Clock(0))
        {
            StartADCLag(0);
            Advance();
        }

        // reset
        if (Clock(1))
        {
            StartADCLag(1);
            Reset();
        }

        // only make changes on a clock'd signal.
        if (EndOfADCLag(0) || EndOfADCLag(1))
        {
            ForEachChannel(ch)
            {
                if (mode[ch] == RAND_MODE)
                {
                    cv[ch] = random(0, PP_MAX_INPUT_CV);
                }
                else
                {
                    cv[ch] = In(ch);
                }
                int32_t cv = ValueForChannel(ch);
                Out(ch, cv);
            }
        }
    }

    void View()
    {
        DrawMode();
        DrawIndicator();
    }

    void OnButtonPress()
    {
        if (++cursor > 1)
            cursor = 0;
        ResetCursor();
    }

    void OnEncoderMove(int direction)
    {
        mode[cursor] += direction;
        if (mode[cursor] < -3)
            mode[cursor] = -3;
        if (mode[cursor] > 3)
            mode[cursor] = 3;
    }

    uint64_t OnDataRequest()
    {
        uint64_t data = 0;
        Pack(data, PackLocation{0, 8}, mode[0] + 32);
        Pack(data, PackLocation{8, 8}, mode[1] + 32);
        return data;
    }

    void OnDataReceive(uint64_t data)
    {
        mode[0] = Unpack(data, PackLocation{0, 8}) - 32;
        mode[1] = Unpack(data, PackLocation{8, 8}) - 32;
    }

protected:
    void SetHelp() {
        //                    "-------" <-- Label size guide
        help[HELP_DIGITAL1] = "Clock";
        help[HELP_DIGITAL2] = "Reset";
        help[HELP_CV1]      = "CV Ch 1";
        help[HELP_CV2]      = "CV Ch 2";
        help[HELP_OUT1]     = "Ch 1";
        help[HELP_OUT2]     = "Ch 2";
        help[HELP_EXTRA1] = "";
        help[HELP_EXTRA2] = "Enc: Change Mode, Seq";
       //                   "---------------------" <-- Extra text size guide
    }

private:
    // Modes
    int mode[2] = {0, -1}; // 0-3: sequences 0-3 with CV octave transpose (0, 1, or 2 octaves). -1: CV picks sequence, -2 = CV quantize mode, -3 = pick random sequence
    int cv[2] = {0, 0};    // 0 -> HEMISPHERE_MAX_CV. RAND mode writes to this (and over-writes input CV)

    // Sequencer
    MiniSeq miniseq[4];

    // UI
    int cursor = 0;

    void DrawMode()
    {
        ForEachChannel(ch)
        {
            int ypos = 15 + (30 * ch);
            if (mode[ch] >= 0)
            {
                gfxPrint(0, ypos, "OCT");
            }
            else if (mode[ch] == PICK_MODE)
            {
                gfxPrint(0, ypos, "PCK");
            }
            else if (mode[ch] == RAND_MODE)
            {
                gfxPrint(0, ypos, "RNG");
            }
            else if (mode[ch] == QUAN_MODE)
            {
                gfxPrint(0, ypos, "QNT");
            }

            int notenum = MIDIQuantizer::NoteNumber(ValueForChannel(ch));
            gfxPrintfn(0, ypos + 10, 4, "%3s", midi_note_numbers[notenum]);

            // cursor when not picking sequence directly
            if (cursor == ch && mode[ch] < 0)
            {
                gfxCursor(0, ypos + 8, 24);
            }
        }
    }

    void DrawIndicator()
    {
        // step
        for (int i = 0; i < 4; i++)
        {
          int note = NoteForSequence(i);
          gfxPrintfn(6*7 + 1, 25 + (10 * i), 3, "%3d", note);
        }

        // arrow indicators
        ForEachChannel(ch)
        {
            int seq = SequenceForChannel(ch);
            if (seq >= 0)
            {
                int x = (6*5) - 2 + (6 * ch); 
                int y = 25 + (10 * seq);
                if (mode[ch] >= 0)
                {
                    gfxBitmap(x, y, 8, RIGHT_BTN_ICON);
                    // cursor when picking sequence in OCT mode.
                    if (cursor == ch)
                    {
                        gfxCursor(x + 3, 63, 5);
                    }
                }
                else
                {
                    gfxBitmap(x, y, 8, RIGHT_BTN_ICON_UNFILLED);
                }
            }
        }
    }

    void Advance() {
        for (int seq = 0; seq < 4; seq++) {
            miniseq[seq].Advance();
        }
    }

    void Reset() {
        for (int seq = 0; seq < 4; seq++) {
            miniseq[seq].Reset();
            miniseq[seq].Advance();
        }
    }

    int StepForSequence(int seq) {
        return miniseq[seq].step;
    }

    // uses cv + encoder. -1 if in QUAN mode.
    int SequenceForChannel(int ch)
    {
        if (mode[ch] == QUAN_MODE)
            return -1;

        int seq = mode[ch];
        if (mode[ch] >= 0)
            seq = mode[ch]; // OCT mode
        else
            seq = Proportion(cv[ch], PP_MAX_INPUT_CV, 4); // +1 to give space for the final seq

        // basic bounds checking
        if (seq < 0)
            seq = 0;
        if (seq > 3)
            seq = 3;
        return seq;
    }

    // returns the index for the quantizer. Must be used in QuantizerLookup.
    int NoteForSequence(int seq)
    {
        int play_note = miniseq[seq].GetNote() + 64;
        CONSTRAIN(play_note, 0, 127);
        return play_note;
    }

    // get quantized output as cv
    int ValueForChannel(int ch)
    {
        int value = 0;
        int seq = SequenceForChannel(ch);
        if (seq >= 0)
        {
            value = QuantizerLookup(ch, NoteForSequence(seq));
            if (mode[ch] >= 0)
            {
                value = value + (Proportion(cv[ch], PP_MAX_INPUT_CV, OCTAVE_RANGE) * OCTAVE_1);
            }
        }
        else // QUAN mode... or we were not able to find a sequence.
        {
            value = Proportion(cv[ch], PP_MAX_INPUT_CV, OCTAVE_RANGE * OCTAVE_1);
        }

        int quantized = Quantize(ch, value);
        return quantized;
    }
};
