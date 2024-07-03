// (c) 2018, Jason Justian (chysn), Beize Maze
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

// struct for the global storage of many Turing Machines.
//
// See OC_apps.ino, where an array of Turing Machines has been added
// to the GlobalSettings struct; and then save_global_settings() and Init()
// handle transport to and from the EEPROM. This does a couple things: First,
// it keeps me from having to declare lots of settings in the Enigma app, which
// would be pain. Second, it allows me to use these user Turing Machines in the
// Hemisphere TM applet, because they'll be in HS::user_turing_machines[].
//
// For purposes of data storge, a "TuringMachine" has two data points, a register
// and a length. I decided against making probability an inherent property of
// a Turing Machine so that this can be determined within the application.
//
// For actual usage, a TuringMachineState should be instantiated, which consumes
// and processes a TuringMachine for the purposes of playback.

#ifndef TURINGMACHINE_H
#define TURINGMACHINE_H

namespace HS {

const byte TURING_MACHINE_COUNT = 40; // Five banks of eight

struct TuringMachine {
    uint16_t reg; // 16-bit shift register containing data
    byte len; // Length of this machine, in steps. 0 indicates an uninitialized TM
    bool favorite; // Basically locks this Turing Machine

    static void SetName(char *n, byte ix) {
        n[0] = 'A' + (ix / 8);
        n[1] = '-';
        n[2] = '1'  + (ix % 8);
        n[3] = '\0';
    }
};

DMAMEM TuringMachine user_turing_machines[TURING_MACHINE_COUNT];

}; // namespace HS

#endif // TURINGMACHINE_H
