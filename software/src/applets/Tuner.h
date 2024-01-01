// Copyright (c) 2018, Jason Justian
//
// Port of APP_REFS tuner,
// Copyright (c) 2016 Patrick Dowling, 2017 Max Stadler & Tim Churches
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

// Tuner can only work in the right hemisphere because the frequecy input is on
// CV4. However, when the screen is flipped, Tuner can only work in the right
// hemisphere. So there are various checks for the FLIP_180 compile-time option
// in this code.

static constexpr double HEM_TUNER_AaboveMidCtoC0 = 0.03716272234383494188492;

class Tuner : public HemisphereApplet {
public:

    const char* applet_name() {
        return "Tuner";
    }

    void Start() {
        A4_Hz = 440;
#ifdef FLIP_180
        if (hemisphere == 0) {
#else
        if (hemisphere == 1) {
#endif
            FreqMeasure.begin();
        }
        AllowRestart();
    }

    void Controller() {
#ifdef FLIP_180
        if (hemisphere == 0 && FreqMeasure.available())
#else
        if (hemisphere == 1 && FreqMeasure.available())
#endif
        {
            // average several readings together
            freq_sum_ = freq_sum_ + FreqMeasure.read();
            freq_count_ = freq_count_ + 1;

            if (milliseconds_since_last_freq_ > 750) {
                frequency_ = FreqMeasure.countToFrequency(freq_sum_ / freq_count_);
                freq_sum_ = 0;
                freq_count_ = 0;
                milliseconds_since_last_freq_ = 0;
            }
        } else if (milliseconds_since_last_freq_ > 100000) {
            frequency_ = 0.0f;
        }
    }

    void View() {
#ifdef FLIP_180
        if (hemisphere == 0) DrawTuner();
#else
        if (hemisphere == 1) DrawTuner();
#endif
        else DrawWarning();
    }

    void OnButtonPress() {
        Start();
    }

    void OnEncoderMove(int direction) {
        A4_Hz = constrain(A4_Hz + direction, 400, 500);
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,16}, A4_Hz);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        A4_Hz = Unpack(data, PackLocation {0,16});
    }

protected:
    void SetHelp() {
#ifdef FLIP_180
        if (hemisphere == 0) {
            help[HEMISPHERE_HELP_DIGITALS] = "1=Input";
#else
        if (hemisphere == 1) {
            help[HEMISPHERE_HELP_DIGITALS] = "2=Input";
#endif
            help[HEMISPHERE_HELP_CVS]      = "";
            help[HEMISPHERE_HELP_OUTS]     = "";
            help[HEMISPHERE_HELP_ENCODER]  = "A4 Hz P=Reset";
        } else {
            help[HEMISPHERE_HELP_DIGITALS] = "Due to hardware";
            help[HEMISPHERE_HELP_CVS]      = "constraints, the";
            help[HEMISPHERE_HELP_OUTS]     = "Tuner must run in";
#ifdef FLIP_180
            help[HEMISPHERE_HELP_ENCODER]  = "left hemisphere";
#else
            help[HEMISPHERE_HELP_ENCODER]  = "right hemisphere";
#endif
        }
    }
    
private:
    // Port from References
    double freq_sum_;
    uint32_t freq_count_;
    float frequency_ ;
    elapsedMillis milliseconds_since_last_freq_;
    int A4_Hz; // Tuning reference

    void DrawTuner() {
        float frequency_ = get_frequency() ;
        float c0_freq_ = get_C0_freq() ;

        int32_t deviation = round(12000.0 * log2f(frequency_ / c0_freq_)) + 500;
        int8_t octave = deviation / 12000;
        int8_t note = (deviation - (octave * 12000)) / 1000;
        note = constrain(note, 0, 12);
        int32_t residual = ((deviation - ((octave - 1) * 12000)) % 1000) - 500;

        if (frequency_ > 0.0) {
            gfxPrint(20, 30, OC::Strings::note_names[note]);
            gfxPrint(" ");
            gfxPrint(octave);
            if (residual < 10 && residual > -10) gfxInvert(1, 28, 62, 11);
            else {
                // Don't show the residual if the tuning is perfect
                if (residual >= 0) {
                    gfxPrint(22, 38, "+");
                    gfxPrint(residual / 10);
                } else {
                    gfxPrint(22, 38, residual / 10);
                }
                gfxPrint("c");
            }

            // Draw some sort of indicator
            if (residual > 10) {
                uint8_t n = residual / 100;
                for (uint8_t i = 0; i < (n + 1); i++)
                {
                    gfxPrint(48 + (i * 2), 38, "<");
                }
            } else if (residual < -10) {
                uint8_t n = -(residual / 100);
                for (uint8_t i = 0; i < (n + 1); i++)
                {
                    gfxPrint(10 - (i * 2), 38, ">");
                }

            }

            // Draw frequency
            const int f = int(floor(frequency_ * 100));
            const int value = f / 100;
            const int cents = f % 100;
            gfxPrint(6 + pad(10000, value), 54, value);
            gfxPrint(".");
            if (cents < 10) gfxPrint("0");
            gfxPrint(cents);
        }

        gfxPrint(1, 15, "A4= ");
        gfxPrint(A4_Hz);
        gfxPrint(" Hz");
        gfxCursor(25, 23, 36);
    }
    
#ifdef FLIP_180
    void DrawWarning() {
        gfxPrint(1, 15, "Tuner goes");
        gfxPrint(1, 25, "in left");
        gfxPrint(1, 35, "hemisphere");
        gfxPrint(1, 45, "<--");
    }
#else
    void DrawWarning() {
        gfxPrint(1, 15, "Tuner goes");
        gfxPrint(1, 25, "in right");
        gfxPrint(1, 35, "hemisphere");
        gfxPrint(1, 45, "       -->");
    }
#endif

    float get_frequency() {return frequency_;}
    
    float get_C0_freq() {
        return(static_cast<float>(A4_Hz * HEM_TUNER_AaboveMidCtoC0));
    }
};
