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

#ifndef LOGICGATE_H
#define LOGICGATE_H

// 0-7 are inputs, 8-13 are neuron outputs, 14 is ON and 15 is OFF
#define LG_MAX_SOURCE 15

enum LogicGateType {
    NONE,

    // Unary
    NOT,

    // Binary
    AND,
    OR,
    XOR,
    NAND,
    NOR,
    XNOR,
    D_FLIPFLOP,
    T_FLIPFLOP,
    LATCH,

    // Ternary
    TL_NEURON
};

const char* const gate_name[12] = {
    "None", "NOT", "AND", "OR", "XOR", "NAND", "NOR", "XNOR", "D-FF", "T-FF", "Ltch", "TLNe"
};

const char* const source_name[16] = {
    "Dig1", "Dig2", "Dig3", "Dig4",
	"CV 1", "CV 2", "CV 3", "CV 4",
    "Neu1", "Neu2", "Neu3", "Neu4", "Neu5", "Neu6",
	"ON", "OFF"
};

const uint8_t NN_LOGIC_ICON[12][16] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // None
    {0x08,0x08,0x08,0x7f,0x41,0x41,0x22,0x22,0x22,0x14,0x14,0x08,0x08,0x14,0x14,0x08}, // Not
    {0x22,0x22,0x22,0x7f,0x41,0x41,0x41,0x41,0x41,0x41,0x22,0x1c,0x08,0x08,0x08,0x08}, // AND
    {0x22,0x22,0x22,0x55,0x5d,0x41,0x41,0x41,0x41,0x22,0x22,0x1c,0x08,0x08,0x08,0x08}, // OR
    {0x22,0x22,0x7f,0x00,0x63,0x5d,0x41,0x41,0x41,0x22,0x22,0x1c,0x08,0x08,0x08,0x08}, // XOR
    {0x22,0x22,0x22,0x7f,0x41,0x41,0x41,0x41,0x41,0x41,0x22,0x1c,0x08,0x14,0x14,0x08}, // NAND
    {0x22,0x22,0x22,0x55,0x5d,0x41,0x41,0x41,0x41,0x22,0x22,0x1c,0x08,0x14,0x14,0x08}, // NOR
    {0x22,0x22,0x7f,0x00,0x63,0x5d,0x41,0x41,0x41,0x22,0x22,0x1c,0x08,0x14,0x14,0x08}, // XNOR
    {0x12,0xff,0xa9,0xa9,0x91,0x81,0xbd,0xa5,0xa5,0xbd,0x99,0x81,0xff,0x02,0x02,0x02}, // D FlipFlop
    {0x12,0xff,0xa9,0xa9,0x91,0x81,0x85,0x85,0xbd,0x85,0x85,0x81,0xff,0x02,0x02,0x02}, // T FlopFlop
    {0x22,0x22,0x3e,0x22,0x14,0x14,0x08,0x08,0x14,0x14,0x22,0x22,0x22,0x3e,0x02,0x02}, // Latch
    {0x42,0xa5,0xa5,0xc3,0x99,0xa5,0xa5,0x99,0x92,0x7c,0x44,0x82,0x82,0x82,0x44,0x38}, // TLNeuron
};



class LogicGate {
public:
    // State attribute
    bool state; // Current state of the gate
    bool clocked = 0; // State of clock for flipflops
    uint16_t source_state; // Source bitfield, for display purposes

    // General attributes
    int type;
    int source1;
    int source2;
    int source3;

    // Threshold Logic Neuron attributes
    int weight1;
    int weight2;
    int weight3;
    int threshold;

    /* Set the state based on gate type and source valutes */
    bool Calculate(uint16_t source_state_) {
        source_state = source_state_ | (0x01 << 14); // Add the ON state
        bool v1 = source_value(source1);
        bool v2 = source_value(source2);
        bool v3 = source_value(source3);

        switch(type) {
            case LogicGateType::NOT        : state = not_fn(v1); break;
            case LogicGateType::AND        : state = and_fn(v1, v2); break;
            case LogicGateType::OR         : state = or_fn(v1, v2); break;
            case LogicGateType::XOR        : state = xor_fn(v1, v2); break;
            case LogicGateType::NAND       : state = !and_fn(v1, v2); break;
            case LogicGateType::NOR        : state = !or_fn(v1, v2); break;
            case LogicGateType::XNOR       : state = !xor_fn(v1, v2); break;
            case LogicGateType::D_FLIPFLOP : state = d_flipflop_fn(v1, v2); break;
            case LogicGateType::T_FLIPFLOP : state = t_flipflop_fn(v1, v2); break;
            case LogicGateType::LATCH      : state = latch_fn(v1, v2); break;
            case LogicGateType::TL_NEURON  : state = tl_neuron_fn(v1, v2, v3); break;
            default                        : state = 0;
        }

        return state;
    }

    bool SourceValue(byte s) {
        bool v = 0;
        if (s == 0) v = source_value(source1);
        if (s == 1) v = source_value(source2);
        if (s == 2) v = source_value(source3);
        return v;
    }


    /* How many cursor positions does this LogicGate use? */
    byte NumParam() {
        byte max;
        switch (type) {
            case LogicGateType::NONE:
                max = 1; // When there's no type, the only parameter is the type
                break;

            case LogicGateType::NOT:
                max = 2; // Type and 1 source
                break;

            case LogicGateType::TL_NEURON:
                max = 8; // Type, three sources, three weights, and threshold
                break;

            default:
                max = 3; // Most types (binary) have a type and two sources
        }
        return max;
    }

    void UpdateValue(byte cursor, int direction) {
        if (cursor == 0) {
            type = constrain(type + direction, LogicGateType::NONE, LogicGateType::TL_NEURON);
            state = 0; // Reset when changing type
        }
        if (cursor == 1 && type > LogicGateType::NONE) {
            source1 = constrain(source1 + direction, 0, LG_MAX_SOURCE);
        }
        if (cursor == 2 && type >= LogicGateType::AND) {
            source2 = constrain(source2 + direction, 0, LG_MAX_SOURCE);
        }
        if (cursor == 3 && type >= LogicGateType::AND) {
            source3 = constrain(source3 + direction, 0, LG_MAX_SOURCE);
        }
        if (type == LogicGateType::TL_NEURON) {
            if (cursor == 4) weight1 = constrain(weight1 + direction, -9, 9);
            if (cursor == 5) weight2 = constrain(weight2 + direction, -9, 9);
            if (cursor == 6) weight3 = constrain(weight3 + direction, -9, 9);
            if (cursor == 7) threshold = constrain(threshold + direction, -27, 27);
        }
    }

    void PrintParamNameAt(byte x, byte y, byte cursor) {
        if (cursor < 4) { // Other cursors are handled in the left-hand pane
            graphics.setPrintPos(x, y);
            if (cursor == 0) graphics.print("Type");
            if (cursor > 0 && cursor < 4) {
                if (type == LogicGateType::D_FLIPFLOP) {
                    if (cursor == 1) graphics.print("Data");
                    if (cursor == 2) graphics.print("Clock");
                } else if (type == LogicGateType::T_FLIPFLOP) {
                    if (cursor == 1) graphics.print("Toggl");
                    if (cursor == 2) graphics.print("Clock");
                } else if (type == LogicGateType::LATCH) {
                    if (cursor == 1) graphics.print("Reset");
                    if (cursor == 2) graphics.print("Set");
                } else if (type == LogicGateType::TL_NEURON) {
                    graphics.print("Den ");
                    graphics.print(cursor);
                } else {
                    graphics.print("Opd");
                }
            }
        }
    }

    void PrintValueAt(byte x, byte y, byte cursor) {
        if (cursor < 4) {
            graphics.setPrintPos(x, y);
            if (cursor == 0) graphics.print(gate_name[type]);
            if (cursor == 1) graphics.print(source_name[source1]);
            if (cursor == 2) graphics.print(source_name[source2]);
            if (cursor == 3) graphics.print(source_name[source3]);
        }
    }

    void DrawSmallAt(byte x, byte y, bool show_name) {
        if (show_name) {
            graphics.setPrintPos(x, y);
            graphics.print(gate_name[type]);
        }
        graphics.drawBitmap8(x + 4, y + 10, 16, NN_LOGIC_ICON[type]);
    }

    void DrawInputs(byte n) {
        if (type > LogicGateType::NONE) draw_line_from(source1, n);
        if (type >= LogicGateType::AND) draw_line_from(source2, n);
        if (type == LogicGateType::TL_NEURON) draw_line_from(source3, n);
    }

private:
    bool source_value(uint16_t source) {
        uint16_t v = source_state & (0x01 << source);
        return static_cast<bool>(v);
    }

    //////// LOGIC GATE EVALUATION
    bool not_fn(bool a) {return !a;}
    bool and_fn(bool a, bool b) {return a & b;}
    bool or_fn(bool a, bool b) {return a | b;}
    bool xor_fn(bool a, bool b) {return a != b;}
    bool d_flipflop_fn(bool D, bool clock) {
        // Handle clocking
        if (!clock && clocked) clocked = 0;
        if (clock && clocked) clock = 0;
        if (clock && !clocked) clocked = 1;

        if (clock) state = D;
        return state;
    }
    bool t_flipflop_fn(bool T, bool clock) {
        // Handle clocking
        if (!clock && clocked) clocked = 0;
        if (clock && clocked) clock = 0;
        if (clock && !clocked) clocked = 1;

        if (clock && T) state = 1 - state;
        return state;
    }
    bool latch_fn(bool reset, bool set) {
        if (reset) state = 0;
        if (set) state = 1;
        return state;
    }
    bool tl_neuron_fn(bool d1, bool d2, bool d3) {
        int v = (d1 * weight1) + (d2 * weight2) + (d3 * weight3);
        return (v > threshold);
    }

    void draw_line_from(byte source, byte n) {
        byte fx;
        byte fy;
        byte tx = ((n / 2) * 32) + 28;
        byte ty = ((n % 2) * 24) + 29;
        if (source < 8) {
            // Physical input sources
            fx = ((source / 4) * 6) + 4;
            fy = ((source % 4) * 10) + 27;
            graphics.drawLine(fx, fy, tx, ty);
        } else if (source < 14) {
            // Logical output sources
            source -= 8;
            fx = ((source / 2) * 32) + 45;
            fy = ((source % 2) * 24) + 29;
            graphics.drawLine(fx, fy, tx, ty);
        } else {
        		// 14 and 15 are TRUE and FALSE and are not drawn
        }
    }
};


#endif // LOGICGATE_H
