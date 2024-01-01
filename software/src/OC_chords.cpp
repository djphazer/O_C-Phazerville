#include "OC_chords.h"
#include "OC_chords_presets.h"

namespace OC {

    Chord user_chords[Chords::CHORDS_USER_LAST];

    /*static*/
    // 
    void Chords::Init() {
      for (size_t i = 0; i < OC::Chords::CHORDS_USER_LAST; ++i)
        memcpy(&user_chords[i], &OC::chords[0], sizeof(Chord));
    }

    const Chord &Chords::GetChord(int index, int progression) {

       uint8_t _index = index + progression * Chords::NUM_CHORDS;
       if (_index < CHORDS_USER_LAST) 
        return user_chords[_index];
       else
        return user_chords[0x0];
    }
} // namespace OC
