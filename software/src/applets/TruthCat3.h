// Copyright (c) 2026, uglifruit
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

/*
 * TruthCat3 — Catalan Boolean Gate (C₃ = 5 trees)
 *
 * Truth-table rhythm generator based on the 5 Catalan binary parse trees
 * of the expression A op0 B op1 C op2 D, where each operator is
 * independently NOR or NAND.  A 4-bit counter drives A-D row by row;
 * OUT1 emits the Boolean result, OUT2 emits a related rhythm.
 *
 * TR1 = clock   CV1 = ops select   OUT1 = main gate
 * TR2 = mode    CV2 = tree select  OUT2 = related gate
 */

class TruthCat3 : public HemisphereApplet {
public:

    enum TruthCat3Cursor {
        OPS,
        STEPS,
        TREE,
        TR2_MODE,
        OUT2_MODE,
        LAST_SETTING = OUT2_MODE
    };

    enum GateOp : uint8_t { NOR_OP, NAND_OP };

    enum Tr2Mode : uint8_t { TR2_RESET, TR2_HOLD, TR2_ADV_TREE, TR2_ADV_OPS };

    enum Out2Mode : uint8_t {
        O2_NEXT, O2_NOT, O2_AND, O2_OR, O2_XOR, O2_FLIP,
        O2_LAST = O2_FLIP
    };

    static constexpr int NUM_OPS_PATTERNS = 8;
    static constexpr int NUM_TREES        = 5;
    static constexpr int OPS_CV_CTRL      = 8;  // sentinel: ops under CV control
    static constexpr int TREE_CV_CTRL     = 5;  // sentinel: tree under CV control

    const char* applet_name() { return "TruthCat3"; }
    const uint8_t* applet_icon() { return PhzIcons::logic; }

    void Start() {
        cursor     = STEPS;
        steps_idx  = 9;         // 16 steps (4-bit max)
        ops_base   = 0;
        tree_base  = 0;
        tr2_mode   = TR2_RESET;
        out2_mode  = O2_XOR;
        counter    = 0;
        out1_state = false;
        out2_state = false;
        hist1      = 0;
        hist2      = 0;
    }

    void Controller() {
        // --- TR2 actions (rising edge) ---
        if (Clock(1)) {
            switch (tr2_mode) {
            case TR2_RESET:
                counter = 0;
                break;
            case TR2_ADV_TREE:
                tree_base = (tree_base >= TREE_CV_CTRL - 1) ? 0 : tree_base + 1;
                break;
            case TR2_ADV_OPS:
                ops_base = (ops_base >= OPS_CV_CTRL - 1) ? 0 : ops_base + 1;
                break;
            case TR2_HOLD:
                break;
            }
        }

        if (Clock(0)) {
            // TR2_HOLD: freeze counter while gate is high
            if (tr2_mode == TR2_HOLD && Gate(1)) {
                return;
            }

            // Resolve CV-controlled or manual ops/tree index
            int ops_mod  = (ops_base  == OPS_CV_CTRL)
                ? constrain(Proportion(In(0), HEMISPHERE_MAX_INPUT_CV, NUM_OPS_PATTERNS - 1), 0, NUM_OPS_PATTERNS - 1)
                : ops_base;
            int tree_mod = (tree_base == TREE_CV_CTRL)
                ? constrain(Proportion(In(1), HEMISPHERE_MAX_INPUT_CV, NUM_TREES - 1), 0, NUM_TREES - 1)
                : tree_base;

            // Decode 4 bits from counter
            bool A = (counter >> 3) & 1;
            bool B = (counter >> 2) & 1;
            bool C = (counter >> 1) & 1;
            bool D = (counter >> 0) & 1;

            out1_state = evalTree(tree_mod, A, B, C, D, opPatterns[ops_mod]);
            out2_state = computeOut2(out1_state, tree_mod, ops_mod, A, B, C, D);

            if (out1_state) ClockOut(0);
            if (out2_state) ClockOut(1);

            hist1 = (hist1 << 1) | (out1_state ? 1u : 0u);
            hist2 = (hist2 << 1) | (out2_state ? 1u : 0u);

            counter = (counter + 1) % stepLength();
        }
    }

    void View() {
        DrawInterface();
    }

    void OnButtonPress() {
        CursorToggle();
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, LAST_SETTING);
            return;
        }
        switch (cursor) {
        case STEPS:
            steps_idx = constrain(steps_idx + direction, 0, NUM_STEP_LENGTHS - 1);
            break;
        case OPS:
            ops_base = constrain(ops_base + direction, 0, OPS_CV_CTRL);
            break;
        case TREE:
            tree_base = constrain(tree_base + direction, 0, TREE_CV_CTRL);
            break;
        case TR2_MODE:
            tr2_mode = (Tr2Mode) constrain((int)tr2_mode + direction, 0, 3);
            break;
        case OUT2_MODE:
            out2_mode = (Out2Mode) constrain((int)out2_mode + direction, 0, (int)O2_LAST);
            break;
        }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,  4}, steps_idx);
        Pack(data, PackLocation {4,  4}, ops_base);   // 0–8
        Pack(data, PackLocation {8,  3}, tree_base);  // 0–5
        Pack(data, PackLocation {11, 2}, (uint8_t)tr2_mode);
        Pack(data, PackLocation {13, 3}, (uint8_t)out2_mode);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        steps_idx  = constrain((int)Unpack(data, PackLocation {0,  4}), 0, NUM_STEP_LENGTHS - 1);
        ops_base   = constrain((int)Unpack(data, PackLocation {4,  4}), 0, OPS_CV_CTRL);
        tree_base  = constrain((int)Unpack(data, PackLocation {8,  3}), 0, TREE_CV_CTRL);
        tr2_mode   = (Tr2Mode)  constrain((int)Unpack(data, PackLocation {11, 2}), 0, 3);
        out2_mode  = (Out2Mode) constrain((int)Unpack(data, PackLocation {13, 3}), 0, (int)O2_LAST);
        counter    = 0;
    }

protected:
    void SetHelp() {
        //                    "-------" <-- Label size guide
        help[HELP_DIGITAL1] = "Clock";
        help[HELP_DIGITAL2] = tr2_names[(int)tr2_mode];
        help[HELP_CV1]      = (ops_base  == OPS_CV_CTRL)  ? "Ops CV" : "  -  ";
        help[HELP_CV2]      = (tree_base == TREE_CV_CTRL) ? "TreeCV" : "  -  ";
        help[HELP_OUT1]     = "BoolOut";
        help[HELP_OUT2]     = "Out2";
        help[HELP_EXTRA1]   = "Catalan Boolean";
        help[HELP_EXTRA2]   = "Gate Sequencer";
        //                   "---------------------" <-- Extra text size guide
    }

private:
    // ---- Constants ----
    static constexpr int STEP_LENGTHS[]   = {3,4,5,6,7,8,10,12,15,16,20,24,32};
    static constexpr int NUM_STEP_LENGTHS = 13;

    // 8 operator patterns ordered by NAND density (0 NANDs → 3 NANDs)
    static constexpr GateOp opPatterns[NUM_OPS_PATTERNS][3] = {
        {NOR_OP,  NOR_OP,  NOR_OP},   // 0 — OOO — 0 NANDs
        {NAND_OP, NOR_OP,  NOR_OP},   // 1 — AOO
        {NOR_OP,  NAND_OP, NOR_OP},   // 2 — OAO
        {NOR_OP,  NOR_OP,  NAND_OP},  // 3 — OOA — 1 NAND
        {NAND_OP, NAND_OP, NOR_OP},   // 4 — AAO
        {NAND_OP, NOR_OP,  NAND_OP},  // 5 — AOA
        {NOR_OP,  NAND_OP, NAND_OP},  // 6 — OAA — 2 NANDs
        {NAND_OP, NAND_OP, NAND_OP},  // 7 — AAA — 3 NANDs
    };

    static constexpr const char* tr2_names[]  = {"RST","HLD","TrAdv","OpAdv"};
    static constexpr const char* out2_names[] = {"NXT","NOT","AND","OR","XOR","FLP"};

    // ---- Settings ----
    int      cursor;
    int      steps_idx;
    int      ops_base;   // 0–7 direct; 8 = CV ctrl
    int      tree_base;  // 0–4 direct; 5 = CV ctrl
    Tr2Mode  tr2_mode;
    Out2Mode out2_mode;

    // ---- Runtime state ----
    int      counter;
    bool     out1_state;
    bool     out2_state;
    uint32_t hist1;
    uint32_t hist2;

    // ---- Helpers ----

    int stepLength() const {
        return STEP_LENGTHS[steps_idx];
    }

    static bool applyOp(GateOp op, bool x, bool y) {
        return (op == NOR_OP) ? !(x | y) : !(x & y);
    }

    // Evaluate one of the 5 Catalan C₃ parse trees.
    // ops[k] is the operator between the (k+1)th and (k+2)th leaf in A-B-C-D.
    static bool evalTree(int tree, bool A, bool B, bool C, bool D,
                         const GateOp ops[3])
    {
        switch (tree) {
        case 0: return applyOp(ops[0], A, applyOp(ops[1], B, applyOp(ops[2], C, D)));  // A op (B op (C op D))
        case 1: return applyOp(ops[0], A, applyOp(ops[2], applyOp(ops[1], B, C), D));  // A op ((B op C) op D)
        case 2: return applyOp(ops[1], applyOp(ops[0], A, B), applyOp(ops[2], C, D));  // (A op B) op (C op D)
        case 3: return applyOp(ops[2], applyOp(ops[0], A, applyOp(ops[1], B, C)), D);  // (A op (B op C)) op D
        case 4: return applyOp(ops[2], applyOp(ops[1], applyOp(ops[0], A, B), C), D);  // ((A op B) op C) op D
        default: return false;
        }
    }

    // Evaluate the same step using the next tree index — the "adjacent interpretation"
    bool computeAltTree(int tree_mod, int ops_mod,
                        bool A, bool B, bool C, bool D) const {
        int alt_tree = (tree_mod + 1) % NUM_TREES;
        return evalTree(alt_tree, A, B, C, D, opPatterns[ops_mod]);
    }

    bool computeOut2(bool out1, int tree_mod, int ops_mod,
                     bool A, bool B, bool C, bool D) const
    {
        switch (out2_mode) {
        case O2_NEXT:
            return computeAltTree(tree_mod, ops_mod, A, B, C, D);
        case O2_NOT:
            return !out1;
        case O2_AND:
            return out1 && computeAltTree(tree_mod, ops_mod, A, B, C, D);
        case O2_OR:
            return out1 || computeAltTree(tree_mod, ops_mod, A, B, C, D);
        default:
        case O2_XOR:
            return out1 != computeAltTree(tree_mod, ops_mod, A, B, C, D);
        case O2_FLIP: {
            GateOp flipped[3];
            for (int i = 0; i < 3; i++)
                flipped[i] = (opPatterns[ops_mod][i] == NOR_OP) ? NAND_OP : NOR_OP;
            return evalTree(tree_mod, A, B, C, D, flipped);
        }
        }
    }

    // ---- Display ----
    //
    // Layout mirrors TruthCat4 but with 3 operator icons instead of 4.
    //
    //  Context help area (y=0–9): clears header, shows icon help for OPS/TR2/OUT2 cursors
    //  Row 1 (y=15): "Op:" + 3 arrow icons
    //  Row 2 (y=25): step counter / length + "T:" tree index
    //  Row 3 (y=34): TR2 mode + output letter + ":" + OUT2 mode
    //  Row 4 (y=45): OUT1 history (16 dots)
    //  Row 5 (y=55): OUT2 history (16 dots)

    void DrawInterface() {
        if (cursor == OPS || cursor == TR2_MODE || cursor == OUT2_MODE) {
            graphics.clearRect(gfx_offset, 0, 64, 10);
        }
        if (cursor == OPS) {
            gfxIcon(0, 1, UP_ICON);
            gfxPrint(9, 2, "NOR");
            gfxIcon(31, 1, DOWN_ICON);
            gfxPrint(39, 2, "NAND");
        } else if (cursor == TR2_MODE) {
            gfxPrint(0, 2, "Trg input");
        } else if (cursor == OUT2_MODE) {
            gfxPrint(0, 2, "Output 2");
        }

        // Row 1: Ops — 3 arrow icons
        gfxPrint(0, 15, "Op:");
        if (ops_base == OPS_CV_CTRL) {
            gfxPrint("CV");
        } else {
            for (int i = 0; i < 3; i++) {
                gfxIcon(19 + i * 9, 15,
                    opPatterns[ops_base][i] == NOR_OP ? UP_ICON : DOWN_ICON);
            }
        }

        // Row 2: step counter / length (left) + tree index (right)
        const int slen   = stepLength();
        const int slash_x = 12;
        const int len_x   = 19;
        const int tree_x  = 37;
        gfxPrint(pad(10, counter + 1), 25, counter + 1);
        gfxPrint(slash_x, 25, "/");
        gfxPrint(len_x, 25, slen);
        gfxPrint(tree_x, 25, "T:");
        if (tree_base == TREE_CV_CTRL) {
            gfxPrint("CV");
        } else {
            gfxPrint(pad(10, tree_base + 1), tree_base + 1);
        }

        // Row 3: TR2 mode (left) | OUT2 label + mode (right)
        gfxPrint(0, 34, tr2_names[(int)tr2_mode]);
        gfxPrint(30, 34, OutputLabel(1));
        gfxPrint(":");
        gfxPrint(out2_names[(int)out2_mode]);

        // Rows 4–5: history dots
        gfxPrint(0, 45, OutputLabel(0));
        gfxPrint(0, 55, OutputLabel(1));
        for (int i = 0; i < 16; i++) {
            int b1 = (hist1 >> (15 - i)) & 1;
            int b2 = (hist2 >> (15 - i)) & 1;
            int x  = 8 + i * 3;
            if (b1) gfxRect(x, 45, 2, 5);
            if (b2) gfxRect(x, 55, 2, 5);
        }

        // Cursor underlines
        switch (cursor) {
        case OPS:
            gfxCursor(0, 23, ops_base == OPS_CV_CTRL ? 30 : 55);
            break;
        case STEPS:
            gfxCursor(len_x, 33, slen >= 10 ? 13 : 7);
            break;
        case TREE:
            gfxCursor(tree_x, 33, 23);
            break;
        case TR2_MODE:
            gfxCursor(0, 42, 29);
            break;
        case OUT2_MODE:
            gfxCursor(30, 42, 33);
            break;
        }
    }
};
