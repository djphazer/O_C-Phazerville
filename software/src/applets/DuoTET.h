// Copyright (c) 2025, Eris Fairbanks
//
// Original design for a microtonal N-TET duophonic quantizer.
//
// DuoTET is a dual/duophonic microtonal quantizer. It is 
// specifically intended to facilitate fluid exploration
// of xenharmonic terrain for composers who may be acclimated
// to western notions of pitch and harmony, while remaining 
// flexible enough to satisfy users who are comfortable with
// a wider range of tonalities.
//
// DuoTET generates a scale of N notes by stacking alternating
// intervals on top of one another. An offset into the list of
// generated notes can be chosen, at which point that note becomes
// the root of the scale, the note set is normalized with respect
// to that note, and the resulting pitch collection is conditioned
// into a collection of pitch classes, which can be thought of as
// either a chord or a scale depending on which concept is more
// convenient to the composer.
//
// The "Duo" portion of DuoTET refers to the function of its second
// quantizer (B). This quantizer can be configured to operate 
// independent of the first quantizer, but may also be configured to
// add the input of the first quantizer to its own, along with a 
// configurable offset, in order to create duophonic harmonies.
//
// DuoTET is currently unfinished. The trigger inputs are unused and
// should be configurable as sample-and-holds for the two quantizers.
// Additional work includes providing parameters for wrapping the 
// lower and upper bounds of the two quantizers in order to provide
// control over the voicing and register of the two. Transposition of
// the first quantizer should be considered. It would also be
// ideal to be able to modulate the parameters of the two quantizers
// using other applets.

#include "../CVInputMap.h"

#define DUOTET_SCALE_MAX_LEN 32

class DuoTET : public HemisphereApplet {
public:

    const char* applet_name() {
        return "DuoTET";
    }
    const uint8_t* applet_icon() { return PhzIcons::scaleDuet; }

    static int cmpfunc(const void * a, const void * b) {
        return ( *(int16_t*)a - *(int16_t*)b );
    }

    static int wrap(int val, int max) {
        while(val >= max) val -= max;
        while(val < 0) val += max;
        return val;   
    }

    void conditionParams() {
        for(int i=0; i<DUOTET_PARAM_LAST; i++) {
            switch(i) {
                case DUOTET_PARAM_TET:
                    params[i] = constrain(params[i], 1, 63);
                    break;
                case DUOTET_PARAM_INTERVALA:
                    params[i] = constrain(params[i], 0, params[DUOTET_PARAM_TET]);
                    break;
                case DUOTET_PARAM_INTERVALB:
                    params[i] = constrain(params[i], 0, params[DUOTET_PARAM_TET]);
                    break;
                case DUOTET_PARAM_OFFSET:
                    params[i] = constrain(params[i], 0, params[DUOTET_PARAM_SCALELEN]);
                    break;
                case DUOTET_PARAM_SCALELEN:
                    params[i] = constrain(params[i], 1, DUOTET_SCALE_MAX_LEN);
                    break;
                case DUOTET_PARAM_BpA:
                    params[i] = constrain(params[i], 0, 1);
                    break;
                case DUOTET_PARAM_Bp:
                    params[i] = constrain(params[i], -31, 31);
                    break;
                case DUOTET_PARAM_Bu:
                    params[i] = constrain(params[i], -63, 63);
                    break;
                case DUOTET_PARAM_Bl:
                    params[i] = constrain(params[i], -63, 63);
                    break;
                case DUOTET_PARAM_Au:
                    params[i] = constrain(params[i], -63, 63);
                    break;
                case DUOTET_PARAM_Al:
                    params[i] = constrain(params[i], -63, 63);
                    break;
                default:
                    break;
            }
        }
    }

    void genScale() {
        int tet = params[DUOTET_PARAM_TET];
        int intervalA = params[DUOTET_PARAM_INTERVALA];
        int intervalB = params[DUOTET_PARAM_INTERVALB];
        int len = params[DUOTET_PARAM_SCALELEN];
        int offset = params[DUOTET_PARAM_OFFSET];

        int note = 0;
        for(int i = 0; i < len + offset; i++) {
            scale[i] = note;
            note = (note + (i & 0x1 ? intervalB : intervalA)) % tet;
        }
        offset = offset % len;
        int shiftBy = scale[offset];
        for(int i = 0; i < len; i++) {
            scale[i] = wrap(scale[i] - shiftBy, tet);
        }
        qsort(scale, len, sizeof(int), cmpfunc);
    }

    int getScaleNote(int degree) {
        if(degree > 72) degree = 72;
        if(degree < -72) degree = -72;
        int tet = params[DUOTET_PARAM_TET];
        int len = params[DUOTET_PARAM_SCALELEN];
        int octave = 0;
        while(degree < 0) { degree += len; octave--; }
        while(degree >= len) { degree -= len; octave++; }
        return scale[degree] + (octave * tet);
    }

    int getPitchClass(int degree) {
        if(degree > 72) degree = 72;
        if(degree < -72) degree = -72;
        int len = params[DUOTET_PARAM_SCALELEN];
        while(degree < 0) { degree += len; }
        while(degree >= len) { degree -= len; }
        return scale[degree];
    }

    int noteToVoltage(int note) {
        return (1536 * note) / params[DUOTET_PARAM_TET];
    }

    void Start() {
        params[DUOTET_PARAM_TET]        = 19;
        params[DUOTET_PARAM_INTERVALA]  = 5;
        params[DUOTET_PARAM_INTERVALB]  = 6;
        params[DUOTET_PARAM_SCALELEN]   = 7;
        params[DUOTET_PARAM_OFFSET]     = 0;
        params[DUOTET_PARAM_BpA]        = 1;
        params[DUOTET_PARAM_Bp]         = 2;
        params[DUOTET_PARAM_Bu]         = 63;
        params[DUOTET_PARAM_Bl]         = -63;
        params[DUOTET_PARAM_Au]         = 63;
        params[DUOTET_PARAM_Al]         = -63;
        genScale();
    }

    bool cv2note(int16_t& pitch, int cv=0, bool forceUpdate = false, int threshold=8) {
        cv = cv+64;
        int w = cv>>7;
        int f = abs((cv & 0x7F)-64);
        if(f < threshold || forceUpdate) {
            pitch = w;
            return true;
        } else {
            return false;
        }
    }

    int constrainOctave(int note, int l, int u, int o) {
        while(note < l) note += o;
        while(note > u) note -= o;
        return note;
    }

    int getNoteA() {
        int len = processedParams[DUOTET_PARAM_SCALELEN];
        int au = processedParams[DUOTET_PARAM_Au];
        int al = processedParams[DUOTET_PARAM_Al];
        int note = noteA;
        note = constrainOctave(note, al, au, len);
        return note;
    }

    int getNoteB() {
        int len = processedParams[DUOTET_PARAM_SCALELEN];
        int bpa = processedParams[DUOTET_PARAM_BpA];
        int bp = processedParams[DUOTET_PARAM_Bp];
        int bu = processedParams[DUOTET_PARAM_Bu];
        int bl = processedParams[DUOTET_PARAM_Bl];
        int note  = noteB + bp + (bpa > 0 ? noteA : 0);
        note = constrainOctave(note, bl, bu, len);
        return note;
    }

    void Controller() {
        for(int i=0; i<DUOTET_PARAM_LAST; i++) {
            cv2note(cvInValues[i], cv_inputs[i].In());
            processedParams[i] = cvInValues[i] + params[i];
        }
        cv2note(noteA, In(0), forceUpdate); cv2note(noteB, In(1), forceUpdate);
        forceUpdate = false;
        int outA = noteToVoltage(getScaleNote(getNoteA()));
        int outB = noteToVoltage(getScaleNote(getNoteB()));
        Out(0, outA); Out(1, outB);
    }

    void View() {
        int len = params[DUOTET_PARAM_SCALELEN];
        int tet = params[DUOTET_PARAM_TET];
        int yoff = 14;
        int h = 50-4;
        int w = 64;
        int x = w>>1;
        int y = (h>>1)+yoff;

        gfxPrint(x-4*6-1, y-4, DUOTET_PARAM_STR[cursor]);
        gfxStartCursor();
        if(cursor == DUOTET_PARAM_BpA) {
            gfxPrint(x+5-7, y-4, processedParams[cursor] > 0 ? "Y" : "N");
        } else {
            gfxPrint(x+5-7, y-4, processedParams[cursor]);
        }
        if(cursor > DUOTET_PARAM_OFFSET) gfxPrintIcon(cv_inputs[cursor].Icon());
        gfxEndCursor(EditMode());

        for(int i=0; i<tet; i++) {
            gfxPixel(
                (int)(x+sin(2.0*M_PI*i/tet)*((w-5)>>1)),
                (int)(y-cos(2.0*M_PI*i/tet)*(h>>1))
            );
        }

        for(int i=0; i<len; i++) {
            int note = scale[i];
            gfxCircle(
                (int)(x+sin(2.0*M_PI*note/tet)*((w-5)>>1)),
                (int)(y-cos(2.0*M_PI*note/tet)*(h>>1)),
                2
            );
            if(getPitchClass(getNoteA()) == note || getPitchClass(getNoteB()) == note) {
                gfxFrame(
                    (int)(x+sin(2.0*M_PI*note/tet)*((w-5)>>1))-1,
                    (int)(y-cos(2.0*M_PI*note/tet)*(h>>1))-1,
                    3, 3
                );
            }
        }
    }

    // void OnButtonPress() {}

    void AuxButton() {
        cv_inputs[cursor].RotateSource(1);
    }

    void OnEncoderMove(int direction) {
        if(EditMode()) {
            params[cursor] += direction;
            if(direction != 0) conditionParams();
            genScale();
            forceUpdate = true;
        } else {
            MoveCursor(cursor, direction, DUOTET_PARAM_LAST-1);
        }
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        uint32_t offset = 0;
        for(uint32_t i=0; i<DUOTET_PARAM_LAST; i++) {
            int param = params[i];
            // if(i == DUOTET_PARAM_OCTAVE) param += 8;
            if( i == DUOTET_PARAM_Bp || \
                i == DUOTET_PARAM_Bu || \
                i == DUOTET_PARAM_Bl || \
                i == DUOTET_PARAM_Au || \
                i == DUOTET_PARAM_Al)
            {
                param += (1<<(DUOTET_PARAM_BITS[i]-1))-1;
            }
            Pack(data, PackLocation {offset, DUOTET_PARAM_BITS[i]}, param);
            offset += DUOTET_PARAM_BITS[i];
        }
        return data;
    }

    void OnDataReceive(uint64_t data) {
        uint32_t offset = 0;
        for(uint32_t i=0; i<DUOTET_PARAM_LAST; i++) {
            params[i] = Unpack(data, PackLocation {offset, DUOTET_PARAM_BITS[i]});
            if( i == DUOTET_PARAM_Bp || \
                i == DUOTET_PARAM_Bu || \
                i == DUOTET_PARAM_Bl || \
                i == DUOTET_PARAM_Au || \
                i == DUOTET_PARAM_Al)
            {
                params[i] -= (1<<(DUOTET_PARAM_BITS[i]-1))-1;
            }
            offset += DUOTET_PARAM_BITS[i];
        }
        genScale();
        forceUpdate = true;
    }

protected:

  void SetHelp() {
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "Clock 1";
    help[HELP_DIGITAL2] = "Clock 2";
    help[HELP_CV1]      = "CV Ch1";
    help[HELP_CV2]      = "CV Ch2";
    help[HELP_OUT1]     = "Pitch 1";
    help[HELP_OUT2]     = "Pitch 2";
    help[HELP_EXTRA1]   = "";
    help[HELP_EXTRA2]   = "";
    //                  "---------------------" <-- Extra text size guide
  }

private:

    typedef enum {
        DUOTET_MODE_ADD,
        DUOTET_MODE_INDEPENDENT
    } DUOTET_MODE;

    typedef enum {
        DUOTET_PARAM_TET,
        DUOTET_PARAM_INTERVALA,
        DUOTET_PARAM_INTERVALB,
        DUOTET_PARAM_SCALELEN,
        DUOTET_PARAM_OFFSET,
        // DUOTET_PARAM_OCTAVE,
        DUOTET_PARAM_BpA,
        DUOTET_PARAM_Bp,
        DUOTET_PARAM_Bu,
        DUOTET_PARAM_Bl,
        DUOTET_PARAM_Au,
        DUOTET_PARAM_Al,
        DUOTET_PARAM_LAST
    } DUOTET_PARAM;

    static constexpr const char* const DUOTET_PARAM_STR[DUOTET_PARAM_LAST] = {
        "TET:",
        " iA:",
        " iB:",
        "len:",
        "off:",
        // "oct:",
        "B+A:",
        " B+:",
        " B^:",
        " Bv:",
        " A^:",
        " Av:",
    };

    uint8_t DUOTET_PARAM_BITS[DUOTET_PARAM_LAST] = {
        6,
        6,
        6,
        5,
        5,
        // 4,
        1,
        7,
        7,
        7,
        7,
        7
    };

    int scale[DUOTET_SCALE_MAX_LEN];

    int16_t noteA = 0;
    int16_t noteB = 0;

    int cursor = 0;
    int16_t params[DUOTET_PARAM_LAST];
    int16_t processedParams[DUOTET_PARAM_LAST];
    int16_t cvInValues[DUOTET_PARAM_LAST];
    int mode = DUOTET_MODE_ADD;

    CVInputMap cv_inputs[DUOTET_PARAM_LAST];

    bool forceUpdate = false;
};
