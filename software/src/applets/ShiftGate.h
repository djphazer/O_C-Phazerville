class ShiftGate : public HemisphereApplet {
public:

    const char* applet_name() {
        return "ShiftGate";
    }
    const uint8_t* applet_icon() { return PhzIcons::shiftGate; }

    void Start() {
        ForEachChannel(ch)
        {
            length[ch] = 4;
            trigger[ch] = ch;
            reg[ch] = random(0, 0xffff);
        }
    }

    void Controller() {
        if (Clock(0)) StartADCLag();

        if (EndOfADCLag()) {
            ForEachChannel(ch)
            {
                // Grab the bit that's about to be shifted away
                int last = (reg[ch] >> (length[ch] - 1)) & 0x01;

                if (!Clock(1)) { // Digital 2 freezes the buffer
                    // XOR the incoming one-bit data with the high bit to get a new value
                    bool data = In(ch) > HEMISPHERE_3V_CV ? 0x01 : 0x00;
                    last = (data != last);
                }

                // Shift left, then potentially add the bit from the other side
                reg[ch] = (reg[ch] << 1) + last;

                bool clock = reg[ch] & 0x01;
                if (trigger[ch]) {
                    if (clock) ClockOut(ch);
                }
                else GateOut(ch, clock);
            }
        }
    }

    void View() {
        DrawInterface();
    }

    // void OnButtonPress() { }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, 3);
            return;
        }

        byte ch = cursor > 1 ? 1 : 0;
        byte c = cursor > 1 ? cursor - 2 : cursor;
        if (c == 0) length[ch] = constrain(length[ch] + direction, 1, 16);
        if (c == 1) trigger[ch] = 1 - trigger[ch];
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,4}, length[0] - 1);
        Pack(data, PackLocation {4,4}, length[1] - 1);
        Pack(data, PackLocation {8,1}, trigger[0]);
        Pack(data, PackLocation {9,1}, trigger[1]);
        Pack(data, PackLocation {16,16}, reg[0]);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        length[0] = Unpack(data, PackLocation {0,4}) + 1;
        length[1] = Unpack(data, PackLocation {4,4}) + 1;
        trigger[0] = Unpack(data, PackLocation {8,1});
        trigger[1] = Unpack(data, PackLocation {9,1});
        reg[0] = Unpack(data, PackLocation {16,16});
    }

protected:
    void SetHelp() {
        //                    "-------" <-- Label size guide
        help[HELP_DIGITAL1] = "Clock";
        help[HELP_DIGITAL2] = "Freeze";
        help[HELP_CV1]      = "Flip0 1";
        help[HELP_CV2]      = "Flip0 2";
        help[HELP_OUT1]     = trigger[0] ? "Trigger" : "Gate";
        help[HELP_OUT2]     = trigger[1] ? "Trigger" : "Gate";
        help[HELP_EXTRA1] = "";
        help[HELP_EXTRA2] = "";
       //                   "---------------------" <-- Extra text size guide
    }

private:
    int cursor; // 0=Length, 1=Trigger/Gate
    uint16_t reg[2]; // Registers
    
    // Settings
    int8_t length[2]; // 1-16
    bool trigger[2]; // 0=Gate, 1=Trigger

    void DrawInterface() {
        gfxIcon(1, 14, LOOP_ICON);
        gfxIcon(35, 14, LOOP_ICON);
        gfxPrint(12 + pad(10, length[0]), 15, length[0]);
        gfxPrint(45 + pad(10, length[1]), 15, length[1]);

        gfxIcon(1, 25, X_NOTE_ICON);
        gfxIcon(35, 25, X_NOTE_ICON);
        gfxPrint(12, 25, trigger[0] ? "Trg" : "Gte");
        gfxPrint(45, 25, trigger[1] ? "Trg" : "Gte");

        byte x = cursor > 1 ? 1 : 0;
        byte y = cursor > 1 ? cursor - 2 : cursor;
        gfxCursor(12 + (33 * x), 23 + (10 * y), 18);

        // Register display
        ForEachChannel(ch)
        {
            for (int b = 0; b < 16; b++)
            {
                byte x = (31 - (b * 2)) + (32 * ch);
                if ((reg[ch] >> b) & 0x01) gfxLine(x, 45 + (ch * 5), x, 50 + (ch * 5));
                if (b == 0 && (reg[ch] & 0x01)) gfxLine(x, 40, x, 60);
            }
        }
    }
};
