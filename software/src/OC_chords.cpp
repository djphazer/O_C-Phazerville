#include "OC_chords.h"
#include "OC_chords_presets.h"

namespace OC {

    DMAMEM Chord user_chords[Chords::CHORDS_USER_LAST];

    /*static*/
    void Chords::Init() {
      for (size_t i = 0; i < OC::Chords::CHORDS_USER_LAST; ++i)
        memcpy(&user_chords[i], &OC::chords[0], sizeof(Chord));
    }

    /*static*/
    void Chords::Validate() {
      // protecc from garbage EEPROM data
      for (size_t i = 0; i < CHORDS_USER_LAST; ++i) {
        CONSTRAIN(user_chords[i].quality, 0, Chords::CHORDS_QUALITY_LAST - 1);
        CONSTRAIN(user_chords[i].inversion, 0, Chords::CHORDS_INVERSION_LAST - 1);
        CONSTRAIN(user_chords[i].voicing, 0, Chords::CHORDS_VOICING_LAST - 1);
        CONSTRAIN(user_chords[i].base_note, 0, 16);
        CONSTRAIN(user_chords[i].octave, -4, 4);
      }
    }

    const Chord &Chords::GetChord(int index, int progression) {

       uint8_t _index = index + progression * Chords::NUM_CHORDS;
       if (_index < CHORDS_USER_LAST) 
        return user_chords[_index];
       else
        return user_chords[0x0];
    }
} // namespace OC
