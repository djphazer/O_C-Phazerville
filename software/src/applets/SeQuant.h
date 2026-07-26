// Copyright (c) 2026, Dorian Guyot
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

#include <string>
#include <map>

/**
 * This applet is a quantizer sequencer: a set of roots and scales can be selected and stepped through. They then configure a quantizer.
 * 
 * Channel 1: Quantizer
 * TR: Trigger S&H of CV to quantize.
 * CV: The cv to quantize
 * OUT: the quantized CV.
 * Note: The signal from CV is quantized continuously to the current scale until a trigger is received in TR1. It then acts as S&H
 * The output is the quantized voltage
 * 
 * Channel 2: Scale sequencer
 * TR: step through the scales according to one of the modes:
 * - Step one scale forwards
 * - Step one scale backwards
 * - Step back and forth through all scales (changing direction when reaching an end)
 * - Step to a random scale in the sequence (possibly staying in the current one)
 * - Step to a different random scale in the sequence
 * - Select the scale with CV2 (directly or with sample and hold through TR2)
 * 
 * CV: Select the current scale. Works continuously until a trigger is received in TR2.
 * 
 * OUT: Depending on the out mode, the output of the second channel can be a host of things:
 * - Note trigger: sends a trigger when the quantized note changes (both in S&H and continuous mode)
 * - Scale trigger: sends a trigger when the scale changes (both in S&H and continuous mode)
 * - Loop trigger: sends a trigger when the chord progression loops (or when hitting the first chord when stepping randomly)
 * - Root: outputs a voltage corresponding to the root of the current scale
 */

class SeQuant : public HemisphereApplet {
public:

    const char* applet_name() { // Maximum 10 characters
        return "SeQuant";
    }
    const uint8_t* applet_icon() { return PhzIcons::dualQuantizer; }

    void Start() {
        cursor = 0;
        ForEachChannel(ch)
        {
            continuous[ch] = 1;
        }
        std::fill_n(rootNote, MAX_NB_SCALES, 0);
        std::fill_n(scale, MAX_NB_SCALES, OC::Scales::SCALE_SEMI);
    }

    void Controller() {
        if (Clock(LEFT_CH)) {
            continuous[LEFT_CH] = 0; // Turn off continuous mode if there's a clock
            StartADCLag(LEFT_CH);
        }

        if(Clock(RIGHT_CH)){
            continuous[RIGHT_CH] = 0; // Turn off continuous mode if there's a clock
            StartADCLag(RIGHT_CH);
        }
        
        // Melody quantization
        uint32_t previousPitch = currentPitch;
        if (continuous[LEFT_CH] || EndOfADCLag(LEFT_CH)) {
            int32_t pitch = In(LEFT_CH);
            currentPitch = Quantize(quantizerId, pitch);
            Out(LEFT_CH, currentPitch);
        }

        // Scale sequencing
        int previousScaleId = currentScaleId;
        if (continuous[RIGHT_CH]){
            int32_t scale_cv = In(RIGHT_CH);

            // Is there a better way to do this ?
            // I have just printed the values on the screen to find out
            CONSTRAIN(scale_cv, 0, HEMISPHERE_MAX_INPUT_CV - 1);
            currentScaleId = (scale_cv * nbScales) / HEMISPHERE_MAX_INPUT_CV;

            SetRootNote(quantizerId, rootNote[currentScaleId]);
            SetScale(quantizerId, scale[currentScaleId]);
        }else if(EndOfADCLag(RIGHT_CH)){
            switch (tr2Modes[currentTr2ModeId]){
            case TR2Mode::STEP_FOWARDS:{
                currentScaleId = (currentScaleId + 1) % nbScales;
                break;
            }

            case TR2Mode::STEP_BACKWARDS:{
                currentScaleId = (currentScaleId + nbScales - 1) % nbScales;
                break;
            }

            case TR2Mode::STEP_BACK_AND_FORTH:{
                if(step_direction){ // forwards
                    if(currentScaleId == nbScales - 1){
                        currentScaleId = nbScales - 2;
                        step_direction = false;
                    }else{
                        currentScaleId = currentScaleId + 1;
                    }
                }else{ // backwards
                    if(currentScaleId == 0){
                        currentScaleId = 1;
                        step_direction = true;
                    }else{
                        currentScaleId = (currentScaleId + nbScales - 1) % nbScales;
                    }
                }
                break;
            }
            
            case TR2Mode::STEP_RANDOM:{
                srand((unsigned)time(0)); 
                currentScaleId = rand() % nbScales;
                break;
            }

            case TR2Mode::STEP_RANDOM_NO_REPEAT:{
                srand((unsigned)time(0));
                int new_scaleId = -1;
                while(new_scaleId == -1 || new_scaleId == currentScaleId){
                    new_scaleId = rand() % nbScales;
                }
                currentScaleId = new_scaleId;
                break;
            }

            case TR2Mode::CV_SELECT_SCALE:{
                int32_t cv = In(RIGHT_CH);

                // Is there a better way to do this ?
                // I have just printed the values on the screen to find out
                CONSTRAIN(cv, 0, HEMISPHERE_MAX_INPUT_CV - 1);
                currentScaleId = (cv * nbScales) / HEMISPHERE_MAX_INPUT_CV;
                break;
            }
            
            default:
                break;
            }

            SetRootNote(quantizerId, rootNote[currentScaleId]);
            SetScale(quantizerId, scale[currentScaleId]);
        }


        // OUT2
        if(out2Modes[currentOut2ModeId] == Out2Mode::NOTE_TRIGGER){
            GateOut(RIGHT_CH, currentPitch != previousPitch);
        }else if (out2Modes[currentOut2ModeId] == Out2Mode::SCALE_TRIGGER){
            GateOut(RIGHT_CH, currentScaleId != previousScaleId);
        }else if (out2Modes[currentOut2ModeId] == Out2Mode::PROGRESSION_LOOPBACK_TRIGGER){
            GateOut(RIGHT_CH, previousScaleId != 0 && currentScaleId == 0);
        }else if (out2Modes[currentOut2ModeId] == Out2Mode::ROOT){
            int32_t quantized_root = QuantizerLookup(quantizerId, 64);
            Out(RIGHT_CH, quantized_root);
        }

        previousPitch = currentPitch;
        previousScaleId = currentScaleId;
    }

    void View() {
        const uint8_t nbScalesPage1 = min(nbScales, MAX_SCALES_ON_PAGE_1);
        const uint8_t nbScalesPage2 = max(0, nbScales - MAX_SCALES_ON_PAGE_1);

        const uint8_t nbSelectorsPage1 = 1 + (nbScalesPage1 * 2);
        const uint8_t nbSelectorsPage1and2 = nbSelectorsPage1 + (nbScalesPage2 * 2);

        if(cursor < nbSelectorsPage1){
            DrawScalesPage1(cursor);
        }else if(nbScalesPage2 > 0 && cursor < nbSelectorsPage1and2){
            // Optional second page (depending on number of scales)
            DrawScalesPage2(cursor - nbSelectorsPage1, 0);
        }else{
            DrawSettingsPage(cursor - nbSelectorsPage1and2);
        }
    }

    void DrawFullScreen(){
        const uint8_t nbScalesPage1 = min(nbScales, MAX_SCALES_ON_PAGE_1);
        const uint8_t nbScalesPage2 = max(0, nbScales - MAX_SCALES_ON_PAGE_1);

        const uint8_t nbSelectorsPage1 = 1 + (nbScalesPage1 * 2);
        const uint8_t nbSelectorsPage1and2 = nbSelectorsPage1 + (nbScalesPage2 * 2);

        if(cursor < nbSelectorsPage1and2){
            DrawScalesPage1(cursor);
            if(nbScalesPage2 > 0){
                // Optional second page (depending on number of scales)
                DrawScalesPage2(cursor - nbSelectorsPage1, 64);
            }
        }else{
            DrawSettingsPage(cursor - nbSelectorsPage1and2);
        }
    }

    int NudgeScaleLocal(int scale, int dir) {
      const int max = OC::Scales::NUM_SCALES;
      scale += dir;
      if (scale >= max) scale = 0;
      if (scale < 0) scale = max - 1;
      return scale;
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, 1 + (nbScales * 2) + 5 - 1);
            return;
        }

        if(cursor == 0){
            nbScales += direction;
            CONSTRAIN(nbScales, MIN_NB_SCALES, MAX_NB_SCALES);
        }else if(cursor <= nbScales * 2){
            // cursor relative to the scale section of the UI
            // I.e. 0 for the forst scale to n-1 for the n-th scale
            int8_t scaleCursor = cursor - 1; 

            // Each scale slot uses two cursor positions
            int8_t scaleSlotId = scaleCursor / 2;

            if (scaleCursor %2 == 0) {
                // Root selection
                int nextRootNote = rootNote[scaleSlotId] + direction;
                CONSTRAIN(nextRootNote, 0, 11);
                rootNote[scaleSlotId] = nextRootNote;
            } else {
                // Scale selection
                scale[scaleSlotId] = NudgeScaleLocal(scale[scaleSlotId], direction);
            }
        }else if (cursor == 1 + (nbScales * 2) + 2){
            quantizerId = quantizerId + direction;
            CONSTRAIN(quantizerId, 1, QUANT_CHANNEL_COUNT);
        }else if (cursor == 1 + (nbScales * 2) + 3){
            currentOut2ModeId = currentOut2ModeId + direction;
            CONSTRAIN(currentOut2ModeId, 0, NB_OUT2_MODES - 1);
        }else if (cursor == 1 + (nbScales * 2) + 4){
            currentTr2ModeId = currentTr2ModeId + direction;
            CONSTRAIN(currentTr2ModeId, 0, tr2Modes.size() - 1);
        }
    }

    void OnButtonPress() {
        if (cursor == 1 + (nbScales * 2)){
            continuous[LEFT_CH] = !continuous[LEFT_CH];
        } else if (cursor == 1 + (nbScales * 2) + 1){
            continuous[RIGHT_CH] = !continuous[RIGHT_CH];
        }else{
            CursorToggle();
        }
    }

    // Data layout
    // Root note:   4 x 4 = 16 bits
    // Scale:       4 x 8 = 32 bits
    // ch1 gated?           1 bit
    // ch2 gated?           1 bit
    // out2 mode            4 bits
    // step mode            3 bits
    // nb of scales         2 bits
    // quantizer id         3 bits

    uint64_t OnDataRequest() {
        if(MAX_NB_SCALES <= 4){
            uint64_t data = 0;
            for (size_t i = 0; i < MAX_NB_SCALES; i++)
            {
                Pack(data, PackLocation {i*4,4}, rootNote[i]);
            }
            for (size_t i = 0; i < MAX_NB_SCALES; i++)
            {
                Pack(data, PackLocation {16 + i * 8, 8}, scale[i]);
            }
            Pack(data, PackLocation {48, 1}, continuous[LEFT_CH]);
            Pack(data, PackLocation {49, 1}, continuous[RIGHT_CH]);
            Pack(data, PackLocation {50, 4}, currentOut2ModeId);
            Pack(data, PackLocation {54, 3}, currentTr2ModeId);
            // nbScales is 1-4, encoded as 0-3
            Pack(data, PackLocation {57, 2}, nbScales - 1);
            Pack(data, PackLocation {59, 3}, quantizerId);
            return data;
        }else{
            return 0;
        }
    }

    void OnDataReceive(uint64_t data) {
        if(MAX_NB_SCALES <= 4){
            for (size_t i = 0; i < MAX_NB_SCALES; i++)
            {
                rootNote[i] = Unpack(data, PackLocation {i*4,4});
                CONSTRAIN(rootNote[i], 0, 11);
            }
            for (size_t i = 0; i < MAX_NB_SCALES; i++)
            {
                scale[i] = Unpack(data, PackLocation {16 + i * 8, 8});
                CONSTRAIN(scale[i], 0, OC::Scales::NUM_SCALES);
            }

            // remark: no need to constrain booleans over 1 bit
            continuous[LEFT_CH] = Unpack(data, PackLocation {48, 1});
            continuous[RIGHT_CH] = Unpack(data, PackLocation {49, 1});
            
            currentOut2ModeId = Unpack(data, PackLocation {50, 4});
            CONSTRAIN(currentOut2ModeId, 0, NB_OUT2_MODES);

            currentTr2ModeId = Unpack(data, PackLocation {54, 3});
            CONSTRAIN(currentTr2ModeId, 0, NB_TR2_MODES);

             // nbScales is 1-4, encoded as 0-3
            nbScales = Unpack(data, PackLocation {57, 2}) + 1;
            CONSTRAIN(nbScales, MIN_NB_SCALES, MAX_NB_SCALES);

            quantizerId = Unpack(data, PackLocation {59, 3});
            CONSTRAIN(quantizerId, 0 ,QUANT_CHANNEL_COUNT);
        }
    }

protected:
  void SetHelp() {
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "Note Ck";
    help[HELP_DIGITAL2] = "Chrd Ck";
    help[HELP_CV1]      = "Note CV";
    help[HELP_CV2]      = "Chrd CV";
    help[HELP_OUT1]     = "Pitch 1";
    help[HELP_OUT2]     = "Func";

    //                  "---------------------" <-- Extra text size guide
    help[HELP_EXTRA1] = "Set: Root / Scale";
    help[HELP_EXTRA2] = "     per step";
  }

private:
    static const uint8_t MAX_SCALES_ON_PAGE_1 = 4;

    // Each channel starts as continuous and becomes clocked when a clock is
    // received
    bool continuous[2];
    int8_t cursor;

    // The minimal/maximal number of scales in the progression
    static const int8_t MIN_NB_SCALES = 2;
    static const int8_t MAX_NB_SCALES = 4;
    // This can not be set to a value above 4, as the data would not fit inside
    // 64 bits anymore. If it is set above 4, the applet will work, but not the
    // saving of presets
    static_assert(MAX_NB_SCALES <= 4, 
        "The max number of scales can't be more than 4, or data won't fit into"
        " 64 bits. If this assert is removed, the app works with up to 8 "
        "scales, but the saving won't work.");

    static const int LEFT_CH = 0;
    static const int RIGHT_CH = 1;

    int quantizerId = 1;

    static const uint8_t NB_OUT2_MODES = 4;
    enum class Out2Mode : uint8_t {
        // If no note trigger is used, outputs a trigger when the note changes
        // If a note trigger is used, otuputs a copy of the trigger
        NOTE_TRIGGER,
        // If no scale trigger is used, outputs a trigger when the scale changes
        // If a scale trigger is used, outputs a copy of the scale trigger
        SCALE_TRIGGER,
        // Outputs a trigger when the scale progression loops back to the start
        PROGRESSION_LOOPBACK_TRIGGER,
        // Outputs the root note of the current scale (this doubles as a 
        // classic sequencer for the bass for example)
        ROOT
    };
    // an array makes cycling through easier
    const Out2Mode out2Modes[NB_OUT2_MODES] = {
        Out2Mode::NOTE_TRIGGER, 
        Out2Mode::SCALE_TRIGGER, 
        Out2Mode::PROGRESSION_LOOPBACK_TRIGGER, 
        Out2Mode::ROOT
    };
    const std::map<Out2Mode, const std::string> out2ModeLabel = {
        {Out2Mode::NOTE_TRIGGER, "nt_tg"}, 
        {Out2Mode::SCALE_TRIGGER, "sc_tg"}, 
        {Out2Mode::PROGRESSION_LOOPBACK_TRIGGER, "oo_tg"}, 
        {Out2Mode::ROOT, "root"}
    };

    int currentOut2ModeId = 2;


    // Behaviour of the app when receiving a trigger on the second channel
    static const uint8_t NB_TR2_MODES = 6;
    enum class TR2Mode : uint8_t {
        // Go one step forward in the scale sequence
        STEP_FOWARDS,
        // Same but in the other direction
        STEP_BACKWARDS,
        // Step back and forth
        STEP_BACK_AND_FORTH,
        // Jump to a random scale
        STEP_RANDOM,
        // Jump to a random scale different than the current one
        STEP_RANDOM_NO_REPEAT,
        // Select scale via CVIN2 now.
        CV_SELECT_SCALE
    };

    const std::array<TR2Mode, NB_TR2_MODES> tr2Modes = {
        TR2Mode::STEP_FOWARDS,
        TR2Mode::STEP_BACKWARDS,
        TR2Mode::STEP_BACK_AND_FORTH,
        TR2Mode::STEP_RANDOM,
        TR2Mode::STEP_RANDOM_NO_REPEAT,
        TR2Mode::CV_SELECT_SCALE
    };

    const std::map<TR2Mode, const std::string> tr2ModeLabel = {
        {TR2Mode::STEP_FOWARDS, "step>"},
        {TR2Mode::STEP_BACKWARDS, "step<"},
        {TR2Mode::STEP_BACK_AND_FORTH, "step<>"},
        {TR2Mode::STEP_RANDOM, "step?"},
        {TR2Mode::STEP_RANDOM_NO_REPEAT, "step?!"},
        {TR2Mode::CV_SELECT_SCALE, "cv"}
    };

    // true is forwards (used when in forwards/backwards mode)
    bool step_direction = true;

    // an array makes cycling through easier
    int currentTr2ModeId = 0;
    
    // The number of scales in the progression
    int nbScales = 4;

    // Root notes of the progression
    int rootNote[MAX_NB_SCALES];

    // Scales of the progression
    int scale[MAX_NB_SCALES];

    // Current scale in the progression
    int currentScaleId = 0;

    // Previous pitch, used tosend a trigger when the pitch changes
    uint32_t currentPitch = -1;
    int previousRoot = -1;
    int previousScale = -1;

    /**
     * Draws the selector for the number of scales and the first page of them.
     * @param pageCursor A local offset cursor for this page
     */
    void DrawScalesPage1(int pageCursor){
        // Selector for number of scales
        gfxPrint(0, 15, "Scales:");
        gfxPrint(45, 15, nbScales);
        if(pageCursor == 0){
            gfxCursor(45, 23, 12, "nb scales");
        }

        // First page of scales
        static const int scalesPosY = 25;
        const int lastScale = min(nbScales, 4);
        DrawRootsAndScalesWidget(0, scalesPosY, pageCursor - 1, 0, lastScale);
    }

    /**
     * Draws the second page of scales. 
     * Does not check if it is necessary !
     * @param pageCursor A local offset cursor for this page
     * @param posX The x-position of the page. Useful to display two pages side by side.
     */
    void DrawScalesPage2(int pageCursor, int posX){
        static const int scalesPosY = 15;
        const int lastScale = min(nbScales, 8);
        DrawRootsAndScalesWidget(posX, scalesPosY, pageCursor, 4, lastScale);
    }

    /**
     * Draws pairs of root + scale according to the provided indices and the 
     * given y position
     * @param posY The y position of this widget
     * @param widgetCursor A local offset cursor for this widget
     * @param fromScaleId The id of the first scale to draw
     * @param toScaleId The id of the last sale to draw
     */
    void DrawRootsAndScalesWidget(
        int posX, 
        int posY, 
        int widgetCursor, 
        int fromScaleId, 
        int toScaleId){
        for (int i = fromScaleId; i < toScaleId; i++){
            const uint8_t y = posY + (i - fromScaleId) * 10;

            // Note icon
            if(i == currentScaleId){
                gfxBitmap(posX, y, 8, NOTE_ICON);
            }

            // Root
            gfxPrint(posX + 15, y, OC::Strings::note_names_unpadded[rootNote[i]]);

            // Scale
            gfxPrint(posX + 31, y, OC::scale_names_short[scale[i]]);
        }

        // Cursor for the roots and scales
        // The cursor might not be for this widget
        const uint8_t lastCursorIndex = ((toScaleId + 1) - fromScaleId) * 2;
        if(widgetCursor < 0 || widgetCursor > lastCursorIndex){
            return;
        }else{
            const uint8_t scaleRow = widgetCursor / 2;
            if(widgetCursor % 2 == 0){
                // Root cursor
                gfxCursor(posX + 15, (posY + 8) + scaleRow * 10, 12, "Root");
            }else{
                // Scale cursor
                gfxCursor(posX + 31,  (posY + 8) + scaleRow * 10, 25, "Scale");
            }
        }
    }

    /**
     * Draws the settings page
     * @param pageCursor A local offset cursor for this page
     */
    void DrawSettingsPage(int pageCursor){
        // Toggle for the left clock input
        gfxBitmap(0, 15, 8, NOTE_ICON);
        gfxBitmap(8, 15, 8, GATE_ICON);
        gfxBitmap(17, 15, 8, continuous[LEFT_CH]?CHECK_OFF_ICON:CHECK_ON_ICON);

        // Toggle for the right clock input
        gfxBitmap(31, 13, 8, NOTE_ICON);
        gfxBitmap(30, 17, 8, NOTE4_ICON);
        gfxBitmap(39, 15, 8, GATE_ICON);
        gfxBitmap(48, 15, 8, continuous[RIGHT_CH]?CHECK_OFF_ICON:CHECK_ON_ICON);

        gfxPrint(0, 25, "Q:");
        gfxPrint(16, 25, quantizerId);
        
        // B: output options
        gfxPrint(0, 35, OutputLabel(1));
        gfxPrint(":");
        gfxPrint(12, 35, out2ModeLabel.at(out2Modes[currentOut2ModeId]).c_str());

        // TR2 output options
        gfxPrint(0, 45, "TR2:");
        gfxPrint(8 * 3, 45, tr2ModeLabel.at(tr2Modes[currentTr2ModeId]).c_str());

        // Draw cursor
        if(pageCursor == 0){
            gfxCursor(17, 25, 8, "TR1 clk");
        }else if (pageCursor == 1){
            gfxCursor(48, 25, 8, "TR2 clk");
        }else if (pageCursor == 2){
            gfxCursor(16, 33, 8, "Quant engine");
        }else if (pageCursor == 3){
            gfxCursor(12, 43, 7*5, "CV2 mode");
        }else if (pageCursor == 4){
            gfxCursor(8*3, 53, 7*5, "TR2 mode");
        }
    }
};
