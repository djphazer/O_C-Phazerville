// Copyright (c) 2022, Korbinian Schreiber
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

#ifndef TINY_DSP
#define TINY_DSP

namespace TDSP {

// Constant multiplier for precision divisions
const int16_t M = 2048;
const int16_t QMAX = M;

// CF = 1/(2*pi*dt) for cHz
// sample rate dt = 60 u
const int32_t CF = 265258;

class FilterLP {
public:
    void set_cfreq(int32_t cfreq) {
        alpha = (cfreq * M) / (CF + cfreq);
    }

    int16_t filter(int16_t signal) {
        // cfreq is in cHz
        // alpha = 2*pi*cfreq*dt/100/(1 + 2*pi*cfreq*dt/100)
        // alpha = CF*cfreq/(1+ CF*cfreq)
        y = (alpha*signal) + (M - alpha)*y;
        y /= M;
        return y;
    }

    int16_t filter(int16_t signal, int32_t cfreq) {
        set_cfreq(cfreq);
        return filter(signal);
    }

private:
    int32_t y;
    int32_t alpha;
    const int16_t M = TDSP::M;
    const int32_t CF = TDSP::CF;
};

class FilterStateVariable {
    /*
    State Variable Filter (Chamberlin version)
    The code is based on
    https://www.musicdsp.org/en/latest/Filters/142-state-variable-filter-chamberlin-version.html
    which, in turn, seems to be based on
    Hal Chamberlin, “Musical Applications of Microprocessors,” 2nd Ed,
    Hayden Book Company 1985. pp 490-492.
    */
public:
    void feed(int16_t signal) {
        lp += (int32_t)(ft*bp)/M;
        hp = signal - lp - (int32_t)(qi*bp)/M;
        bp += (int32_t)(ft*hp)/M;
        no = hp + lp;
    }

    void set_cfreq(int32_t cfreq) {
        // We use a first order Tailor approximation here. -> Deviations close to
        // Nyquist.
        ft = (M*cfreq)/CF;
    }

    void set_q(int16_t _q) {
        qi = _q > M ? 0 : M - _q;
    }

    void feed(int16_t signal, int32_t cfreq, int16_t _q) {
        set_cfreq(cfreq);
        set_q(_q);
        feed(signal);
    }

    int16_t get_lp(void) {return lp;}
    int16_t get_bp(void) {return bp;}
    int16_t get_hp(void) {return hp;}
    int16_t get_no(void) {return no;}

private:
    int16_t lp;
    int16_t bp;
    int16_t hp;
    int16_t no;  // notch

    int32_t ft;
    int16_t qi;  // inverse q

    const int16_t M = TDSP::M;
    const int32_t CF = TDSP::CF;
};

}
#endif // TINY_DSP
