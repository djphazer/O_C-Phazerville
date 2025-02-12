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
                    params[i] = constrain(params[i], 1, 255);
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
                case DUOTET_PARAM_OCTAVE:
                    params[i] = constrain(params[i], -8, 8);
                    break;
                case DUOTET_PARAM_BpA:
                    params[i] = constrain(params[i], 0, 1);
                    break;
                case DUOTET_PARAM_Bp:
                    params[i] = constrain(params[i], -127, 127);
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
        int octave = params[DUOTET_PARAM_OCTAVE];
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
        params[DUOTET_PARAM_OCTAVE]     = 0;
        params[DUOTET_PARAM_BpA]        = 1;
        params[DUOTET_PARAM_Bp]         = 2;
        genScale();
    }

    void cv2note(int& pitch, int cv=0, bool forceUpdate = false, int threshold=8) {
        cv = cv+64;
        int w = cv>>7;
        int f = abs((cv & 0x7F)-64);
        if(f < threshold || forceUpdate) pitch = w;
    }

    int getNoteB() {
        int bpa = params[DUOTET_PARAM_BpA];
        int bp = params[DUOTET_PARAM_Bp];
        return noteB + bp + (bpa > 0 ? noteA : 0);
    }

    void Controller() {
        cv2note(noteA, In(0), forceUpdate); cv2note(noteB, In(1), forceUpdate);
        forceUpdate = false;
        int outA = noteToVoltage(getScaleNote(noteA));
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
        int r = h>>1;

        gfxPrint(x-3*6-1, y-4, DUOTET_PARAM_STR[cursor]);
        if(cursor == DUOTET_PARAM_BpA) {
            gfxPrint(x+5, y-4, params[cursor] > 0 ? "Y" : "N");
        } else {
            gfxPrint(x+5, y-4, params[cursor]);
        }

        if(EditMode()) {
            gfxInvert(x+5, y-5, 6*2+1, 10);
        }

        for(int i=0; i<tet; i++) {
            gfxPixel(
                (int)(x+sin(2.0*M_PI*i/tet)*r),
                (int)(y-cos(2.0*M_PI*i/tet)*r)
            );
        }

        for(int i=0; i<len; i++) {
            int note = scale[i];
            gfxCircle(
                (int)(x+sin(2.0*M_PI*note/tet)*r),
                (int)(y-cos(2.0*M_PI*note/tet)*r),
                2
            );
            if(getPitchClass(noteA) == note || getPitchClass(getNoteB()) == note) {
                gfxFrame(
                    (int)(x+sin(2.0*M_PI*note/tet)*r)-1,
                    (int)(y-cos(2.0*M_PI*note/tet)*r)-1,
                    3, 3
                );
            }
        }
    }

    // void OnButtonPress() {}

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
        for(uint32_t i=0; i<DUOTET_PARAM_LAST; i++) {
            int param = params[i];
            if(i == DUOTET_PARAM_OCTAVE) param += 8;
            if(i == DUOTET_PARAM_Bp) param += 127;
            Pack(data, PackLocation {i*8,8}, param);
        }
        return data;
    }

    void OnDataReceive(uint64_t data) {
        for(uint32_t i=0; i<DUOTET_PARAM_LAST; i++) {
            params[i] = Unpack(data, PackLocation {i*8,8});
            if(i == DUOTET_PARAM_OCTAVE) params[i] -= 8;
            if(i == DUOTET_PARAM_Bp) params[i] -= 127;
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
        DUOTET_PARAM_OCTAVE,
        DUOTET_PARAM_BpA,
        DUOTET_PARAM_Bp,
        DUOTET_PARAM_LAST
    } DUOTET_PARAM;

    static constexpr const char* const DUOTET_PARAM_STR[DUOTET_PARAM_LAST] = {
        "TET:",
        " iA:",
        " iB:",
        "len:",
        "off:",
        "oct:",
        "B+A:",
        " B+:"
    };

    int scale[DUOTET_SCALE_MAX_LEN];

    int noteA = 0;
    int noteB = 0;

    int cursor = 0;
    int params[DUOTET_PARAM_LAST];
    int mode = DUOTET_MODE_ADD;

    bool forceUpdate = false;
};
