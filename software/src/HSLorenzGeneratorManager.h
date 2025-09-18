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

// Lorenz Generator Manager
// It seemed like LowerRenz was crashing when two instances of it were running,
// possibly because it was burdened with processing two Lorenz generators at
// the same time. So this class creates a singleton instance to allow two
// hemispheres to share a single Lorenz generator.

#define LORENZ_PROCESS_TICKS 16

class LorenzGeneratorManager {
    static LorenzGeneratorManager *instance;
    uint32_t freq[2]; // Frequency per hemisphere
    bool reset[2]; // Reset per hemisphere
    uint32_t last_process_tick;
    streams::LorenzGenerator lorenz;

    LorenzGeneratorManager() {
        lorenz.Init(0);
        lorenz.Init(1);
        lorenz.set_out_a(streams::LORENZ_OUTPUT_X1);
        lorenz.set_out_b(streams::LORENZ_OUTPUT_Y1);
        lorenz.set_out_c(streams::LORENZ_OUTPUT_X2);
        lorenz.set_out_d(streams::LORENZ_OUTPUT_Y2);
        last_process_tick = 0;
    }

public:
    static LorenzGeneratorManager *get() {
        if (!instance) instance = new LorenzGeneratorManager;
        return instance;
    }

    int GetOut(int out) {
        return lorenz.dac_code(out);
    }

    void SetRho(bool hemisphere, int16_t rho) {
        if (hemisphere == 0) lorenz.set_rho1(rho);
        else lorenz.set_rho2(rho);
    }

    void SetFreq(bool hemisphere, uint32_t freq_) {
        freq[hemisphere] = freq_;
    }

    void Reset(bool hemisphere) {
        reset[hemisphere] = 1;
    }

    void Process() {
        if (OC::CORE::ticks - last_process_tick >= LORENZ_PROCESS_TICKS) {
            last_process_tick = OC::CORE::ticks;
            lorenz.Process(freq[0], freq[1], reset[0], reset[1], 2, 2);
            reset[0] = 0;
            reset[1] = 0;
        }
    }
};

LorenzGeneratorManager *LorenzGeneratorManager::instance = 0;
