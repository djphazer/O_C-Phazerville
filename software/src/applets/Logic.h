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

// Logical gate functions and typedef to function pointer
#define HEMISPHERE_NUMBER_OF_LOGIC 7
bool hem_AND(bool s1, bool s2) {return s1 & s2;}
bool hem_OR(bool s1, bool s2) {return s1 | s2;}
bool hem_XOR(bool s1, bool s2) {return s1 != s2;}
bool hem_NAND(bool s1, bool s2) {return !hem_AND(s1, s2);}
bool hem_NOR(bool s1, bool s2) {return !hem_OR(s1, s2);}
bool hem_XNOR(bool s1, bool s2) {return !hem_XOR(s1, s2);}
bool hem_null(bool s1, bool s2) {return 0;} // Used when the section is under CV control
typedef bool(*LogicGateFunction)(bool, bool);

const uint8_t LOGIC_ICON[6][12] = {
    {0x22, 0x22, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x3e, 0x08, 0x08, 0x08}, // AND
    {0x22, 0x22, 0x63, 0x77, 0x7f, 0x7f, 0x3e, 0x1c, 0x1c, 0x08, 0x08, 0x08}, // OR
    {0x22, 0x22, 0x77, 0x08, 0x77, 0x7f, 0x3e, 0x1c, 0x1c, 0x08, 0x08, 0x08}, // XOR
    {0x22, 0x22, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x3e, 0x08, 0x0a, 0x0c}, // NAND
    {0x22, 0x22, 0x63, 0x77, 0x7f, 0x7f, 0x3e, 0x1c, 0x1c, 0x08, 0x0a, 0x0c}, // NOR
    {0x22, 0x22, 0x77, 0x08, 0x77, 0x7f, 0x3e, 0x1c, 0x1c, 0x08, 0x0a, 0x0c}  // XNOR
};

class Logic : public HemisphereApplet {
public:

    const char* applet_name() {
        return "Logic";
    }

    void Start() {
        selected = 0;
        operation[0] = 0;
        operation[1] = 2;
    }

    void Controller() {
        bool s1 = Gate(0); // Set logical states
        bool s2 = Gate(1);
        
        ForEachChannel(ch)
        {
            int idx = operation[ch];
            if (operation[ch] == HEMISPHERE_NUMBER_OF_LOGIC - 1) {
                // The last selection puts the index under CV control
                int cv = In(ch);
                if (cv < 0) cv = -cv; // So that CV input is bipolar (for use with LFOs, etc.)
                idx = constrain(ProportionCV(cv, 6), 0, 5);
            }
            result[ch] = logic_gate[idx](s1, s2);
            source[ch] = idx; // In case it comes from CV, need to display the right icon
            GateOut(ch, result[ch]);
        }
    }

    void View() {
        DrawSelector();
        DrawIndicator();
    }

    // void OnButtonPress() { }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(selected, direction, 1);
            return;
        }

        operation[selected] += direction;
        if (operation[selected] == HEMISPHERE_NUMBER_OF_LOGIC) operation[selected] = 0;
        if (operation[selected] < 0) operation[selected] = HEMISPHERE_NUMBER_OF_LOGIC - 1;
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0, 8}, operation[0]);
        Pack(data, PackLocation {8, 8}, operation[1]);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        operation[0] = Unpack(data, PackLocation {0, 8});
        operation[1] = Unpack(data, PackLocation {8, 8});
    }

protected:
  void SetHelp() {
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "In 1";
    help[HELP_DIGITAL2] = "In 2";
    help[HELP_CV1]      = operation[0] == HEMISPHERE_NUMBER_OF_LOGIC - 1 ? "Op 1" : " - ";
    help[HELP_CV2]      = operation[1] == HEMISPHERE_NUMBER_OF_LOGIC - 1 ? "Op 2" : " - ";
    help[HELP_OUT1]     = op_name[operation[0]];
    help[HELP_OUT2]     = op_name[operation[1]];
    help[HELP_EXTRA1] = "Set: Operator";
    help[HELP_EXTRA2] = "";
    //                  "---------------------" <-- Extra text size guide
  }
    
private:
    const char* op_name[HEMISPHERE_NUMBER_OF_LOGIC] = {"AND", "OR", "XOR", "NAND", "NOR", "XNOR", "-CV-"};
    LogicGateFunction logic_gate[HEMISPHERE_NUMBER_OF_LOGIC] = {hem_AND, hem_OR, hem_XOR, hem_NAND, hem_NOR, hem_XNOR, hem_null};
    int operation[2];
    bool result[2];
    int source[2];
    int selected;
    
    void DrawSelector()
    {
        ForEachChannel(ch)
        {
            gfxPrint(0 + (31 * ch), 15, op_name[operation[ch]]);
            if (ch == selected) gfxCursor(0 + (31 * ch), 23, 30);

            if (operation[ch] == HEMISPHERE_NUMBER_OF_LOGIC - 1) {
              gfxIcon(0 + 31*ch, 25, CV_ICON);
              gfxPrint(8 + 31*ch, 25, op_name[source[ch]]);
            }
        }
    }    
    
    void DrawIndicator()
    {
        ForEachChannel(ch)
        {
            gfxBitmap(8 + (36 * ch), 45, 12, LOGIC_ICON[source[ch]]);
            if (result[ch]) gfxFrame(5 + (36 * ch), 42, 17, 13);
        }
    }
};
