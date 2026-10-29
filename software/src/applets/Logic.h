// Copyright (c) 2018, Jason Justian
// Modified (:D) 2026, Beau Sterling
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

const uint8_t LOGIC_ICON[16][12] = {
    { 0x00, 0x3e, 0x7f, 0x41, 0x41, 0x7f, 0x3e, 0x00, 0x08, 0x08, 0x08, 0x08 },  // FALSE
    { 0x22, 0x22, 0x22, 0x7f, 0x41, 0x41, 0x41, 0x41, 0x3e, 0x08, 0x08, 0x08 },  // AND
    { 0x22, 0x52, 0x22, 0x7f, 0x41, 0x41, 0x41, 0x41, 0x3e, 0x08, 0x08, 0x08 },  // B_INH_A
    { 0x22, 0x22, 0x22, 0x22, 0x02, 0x06, 0x0c, 0x08, 0x08, 0x08, 0x08, 0x08 },  // BUFF_A
    { 0x22, 0x25, 0x22, 0x7f, 0x41, 0x41, 0x41, 0x41, 0x3e, 0x08, 0x08, 0x08 },  // A_INH_B
    { 0x22, 0x22, 0x22, 0x22, 0x20, 0x30, 0x18, 0x08, 0x08, 0x08, 0x08, 0x08 },  // BUFF_B
    { 0x22, 0x22, 0x22, 0x63, 0x5d, 0x63, 0x5d, 0x41, 0x36, 0x08, 0x08, 0x08 },  // XOR
    { 0x22, 0x22, 0x22, 0x63, 0x5d, 0x41, 0x41, 0x41, 0x36, 0x08, 0x08, 0x08 },  // OR
    { 0x22, 0x22, 0x22, 0x63, 0x5d, 0x41, 0x41, 0x41, 0x36, 0x08, 0x14, 0x08 },  // NOR
    { 0x22, 0x22, 0x22, 0x63, 0x5d, 0x63, 0x5d, 0x41, 0x36, 0x08, 0x14, 0x08 },  // XNOR
    { 0x22, 0x52, 0x22, 0x22, 0x20, 0x30, 0x18, 0x08, 0x08, 0x08, 0x08, 0x08 },  // NOT_B
    { 0x22, 0x52, 0x22, 0x63, 0x5d, 0x41, 0x41, 0x41, 0x36, 0x08, 0x08, 0x08 },  // B_IMPL_A
    { 0x22, 0x25, 0x22, 0x22, 0x02, 0x06, 0x0c, 0x08, 0x08, 0x08, 0x08, 0x08 },  // NOT_A
    { 0x22, 0x25, 0x22, 0x63, 0x5d, 0x41, 0x41, 0x41, 0x36, 0x08, 0x08, 0x08 },  // A_IMPL_B
    { 0x22, 0x22, 0x22, 0x7f, 0x41, 0x41, 0x41, 0x41, 0x3e, 0x08, 0x14, 0x08 },  // NAND
    { 0x00, 0x00, 0x42, 0x7f, 0x7f, 0x40, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08 }   // TRUE
};

class Logic : public HemisphereApplet {
public:

    const char* applet_name() {
        return "Logic";
    }
    const uint8_t* applet_icon() { return PhzIcons::logic; }

    void Start() {
        operation[0] = AND;
        operation[1] = XOR;
    }

    void Controller() {
        ForEachChannel(ch) {  // Set logical states
            input[ch] = Gate(ch);
        }

        ForEachChannel(ch) {
            uint8_t idx = operation[ch];
            if (operation[ch] == LAST_GATE + 1) {
                // The last selection puts the index under CV control
                int cv = In(ch);
                if (cv < 0) cv = -cv;  // So that CV input is bipolar (for use with LFOs, etc.)
                idx = ProportionCV(cv, 15, HEMISPHERE_MAX_INPUT_CV);
            }
            result[ch] = LogicGate(input[0], input[1], (LogicGateTypes)idx);
            source[ch] = idx;  // In case it comes from CV, need to display the right icon
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

        operation[selected] = constrain(operation[selected] + direction, 0, LAST_GATE + 1);
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0, 8}, operation[0]);
        Pack(data, PackLocation {8, 8}, operation[1]);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        operation[0] = constrain(Unpack(data, PackLocation {0, 8}), 0, 16);
        operation[1] = constrain(Unpack(data, PackLocation {8, 8}), 0, 16);
    }

protected:
    void SetHelp() {
        //                    "-------" <-- Label size guide
        help[HELP_DIGITAL1] = "In 1";
        help[HELP_DIGITAL2] = "In 2";
        help[HELP_CV1]      = operation[0] == LAST_GATE + 1 ? "Op 1" : " - ";
        help[HELP_CV2]      = operation[1] == LAST_GATE + 1 ? "Op 2" : " - ";
        help[HELP_OUT1]     = op_name[operation[0]];
        help[HELP_OUT2]     = op_name[operation[1]];
        help[HELP_EXTRA1]   = "Set: Operator";
        help[HELP_EXTRA2]   = "";
        //                    "---------------------" <-- Extra text size guide
    }

private:
    enum LogicGateTypes : uint8_t {  // https://mathcenter.oxford.emory.edu/site/cs170/gates/
        FALSE,     // 0000  0
        AND,       // 0001  A & B
        B_INH_A,   // 0010  A & ~B
        BUFF_A,    // 0011  A
        A_INH_B,   // 0100  ~A & B
        BUFF_B,    // 0101  B
        XOR,       // 0110  (A & ~B) | (~A & B)
        OR,        // 0111  A | B
        NOR,       // 1000  ~(A | B)
        XNOR,      // 1001  (A & B) | (~A & ~B)
        NOT_B,     // 1010  ~B
        B_IMP_A,   // 1011  A | ~B
        NOT_A,     // 1100  ~A
        A_IMP_B,   // 1101  ~A | B
        NAND,      // 1110  ~(A & B)
        TRUE,      // 1111  1
        LAST_GATE = TRUE
    };

    const char* op_name[17] = {
        "FALS", "AND",  "INHA", "BUFA",
        "INHB", "BUFB", "XOR",  "OR",
        "NOR",  "XNOR", "NOTB", "IMPA",
        "NOTA", "IMPB", "NAND", "TRUE",
        "-CV-"
    };
    uint8_t operation[2];
    bool input[2];
    bool result[2];
    uint8_t source[2];
    int8_t selected = 0;

    void DrawSelector() {
        ForEachChannel(ch) {
            gfxPrint(1 + (32 * ch), 15, op_name[operation[ch]]);
            if (ch == selected) gfxCursor(1 + (32 * ch), 23, 30);

            if (operation[ch] == LAST_GATE + 1) {
                gfxPrint(0 + 32*ch, 25, op_name[source[ch]]);
            }
        }
    }

    void DrawIndicator() {
        ForEachChannel(ch) {
            input[0] ? gfxRect(2 + (32 * ch), 42, 5, 5) : gfxFrame(2 + (32 * ch), 42, 5, 5);
            input[1] ? gfxRect(2 + (32 * ch), 50, 5, 5) : gfxFrame(2 + (32 * ch), 50, 5, 5);
            gfxBitmap(12 + (32 * ch), 45, 12, LOGIC_ICON[source[ch]]);
            if (result[ch]) gfxInvert(9 + (32 * ch), 42, 17, 13);
        }
    }

    /*
        combine inputs into a 2-bit truth table row index (0-3) then
        shift truth table pattern for specified gate right by
        (3 - index), to match oxford numbers, and use lowest bit
    */
    bool LogicGate(bool a, bool b, LogicGateTypes gate_type) {
        uint8_t idx = (a << 1) | b;
        return (gate_type >> (3-idx)) & 1;
    }
};
