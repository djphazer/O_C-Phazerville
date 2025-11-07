// Copyright (c) 2024, Samuel Burt
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



class RelabiManager {
    static RelabiManager *instance;

    int lfo1;
    int lfo2;
    int lfo3;
    int lfo4;
    uint8_t leftOn;
    uint8_t rightOn;
    bool linked;
    uint32_t registered[2];
    uint32_t lastRegistered[2];
    uint8_t hemRelabi;
    

    RelabiManager() {
        leftOn = 0;
        rightOn = 0;
        linked = false;
        registered[LEFT_HEMISPHERE] = 0;
        registered[RIGHT_HEMISPHERE] = 0;
    }

public:

    static RelabiManager *get() {
        if (!instance) instance = new RelabiManager;
        return instance;
    }

    void RegisterRelabi(bool hemisphere) {
        hemRelabi = hemisphere;
        registered[hemisphere] = OC::CORE::ticks;
    }

    bool IsLinked() {
        uint32_t t = OC::CORE::ticks;
        return ((t - registered[LEFT_HEMISPHERE] < 160)
            && (t - registered[RIGHT_HEMISPHERE] < 160));
    }

    void WriteValues(int value1, int value2, int value3, int value4) {
        // Update individual variables
        lfo1 = value1;
        lfo2 = value2;
        lfo3 = value3;
        lfo4 = value4;
    }

    void ReadValues(int &value1, int &value2, int &value3, int &value4) const {
        // Read values into the referenced variables
        value1 = lfo1;
        value2 = lfo2;
        value3 = lfo3;
        value4 = lfo4;
    }
};

RelabiManager *RelabiManager::instance = 0;

