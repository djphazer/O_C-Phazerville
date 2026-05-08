#pragma once

#include "../Audio/AudioEffectGlitch.h"

extern "C" uint8_t external_psram_size;

// GlitchApplet — live-input stutter/glitch effect.
//
// Continuously records audio into a 1-second circular buffer. When HOLD is
// gated, the read pointer freezes on a clock-sized slice and loops it in
// Forward, Reverse, Ping-Pong, or Ratchet (MOD) mode. Wet/dry mix blends
// the frozen slice with the live input.
//
// I/O (within Quadrants audio chain):
//   Input  → glitch DSP → wet channel  ┐
//   Input  → dry channel               ┴→ mixer → Output
//
// Parameters:
//   CLOCK_SRC  — clock source for beat tracking (DigitalInputMap)
//   DIV        — slice length as clock division (1/2 … 1/64)
//   HOLD_SRC   — gate input that activates stutter (DigitalInputMap)
//   MODE       — FWD / REV / PNG / MOD (ping-pong / ratchet)
//   MODE_CV    — CV modulates effective mode index
//   RATCHET    — ratchet subdivisions 1–6
//   RATCHET_CV — CV modulates ratchet count
//   BITS       — bit-depth reduction 0–F (0=bypass, F=1-bit), CV-modulatable
//   DECIMATE   — sample-rate reduction 0–F (0=bypass, F=16×), CV-modulatable
//   OFFSET     — slice-capture offset 0–F slices back, CV-modulatable
//   MIX        — wet/dry balance 0–100%, CV-modulatable
//
// AuxButton latches manual hold without a patched gate.
template <AudioChannels Channels>
class GlitchApplet : public HemisphereAudioApplet {
public:
    const char* applet_name() { return "Glitch"; }

    void Start() override {
        for (int ch = 0; ch < Channels; ch++) {
            channels[ch].Start(this, ch, input_stream, output_stream);
        }
        clock_source.source = -2; // default to CLK1
    }

    void Unload() override {
        for (auto& ch : channels) ch.Stop();
        AllowRestart();
    }

    void Controller() override {
        // Track beat period from selected clock source (same pattern as DelayApplet).
        clock_count++;
        if (clock_source.Clock()) {
            clock_base_secs = clock_count / 16666.0f;
            clock_count = 0;
        }

        // Slice length in samples from BPM × division ratio.
        size_t slice_samples = static_cast<size_t>(
            clock_base_secs * DIV_BEATS[div] * AUDIO_SAMPLE_RATE);

        // Gate state: hardware gate OR latched manual hold.
        bool held   = hold_input.Gate() || manual_hold_;
        bool frozen = freeze_input.Gate();

        // CV-modulated mode and ratchet count.
        int eff_mode = constrain(
            (int)mode + (int)roundf(mode_cv.InF() * (NUM_MODES - 1)),
            0, NUM_MODES - 1);
        uint8_t eff_ratchet = (uint8_t)constrain(
            (int)ratchet + (int)roundf(ratchet_cv.InF() * 5.0f),
            1, 6);

        // CV-modulated Bit/Spl/Off (all 0–15).
        uint8_t eff_bits     = (uint8_t)constrain(
            (int)bits_    + (int)roundf(bits_cv.InF()  * 15.0f), 0, 15);
        uint8_t eff_decimate = (uint8_t)constrain(
            (int)decimate_+ (int)roundf(dec_cv.InF()   * 15.0f), 0, 15);
        uint8_t eff_offset   = (uint8_t)constrain(
            (int)offset_  + (int)roundf(off_cv.InF()   * 15.0f), 0, 15);

        // Equal-power wet/dry gains (computed once, set on all channels).
        float dry_gain, wet_gain;
        EqualPowerFade(dry_gain, wet_gain,
            constrain(0.01f * mix + mix_cv.InF(), 0.0f, 1.0f));

        for (int ch = 0; ch < Channels; ch++) {
            channels[ch].glitch_stream.setHold(held);
            channels[ch].glitch_stream.setFreeze(frozen);
            channels[ch].glitch_stream.setSliceSamples(slice_samples);
            channels[ch].glitch_stream.setMode(eff_mode);
            channels[ch].glitch_stream.setRatchet(eff_ratchet);
            channels[ch].glitch_stream.setBits(eff_bits);
            channels[ch].glitch_stream.setDecimate(eff_decimate);
            channels[ch].glitch_stream.setOffset(eff_offset);
            channels[ch].wet_dry_mixer.gain(GlitchChannel::DRY_CH, dry_gain);
            channels[ch].wet_dry_mixer.gain(GlitchChannel::WET_CH, wet_gain);
        }
    }

    FLASHMEM void View() override {
        if (!channels[0].glitch_stream.IsReady()) {
            gfxPrint(1, 15, "No PSRAM");
            return;
        }

        for (int i = 0; i < 6; ++i) {
            int row = scroll_top + i;
            if (row >= NUM_ROWS) break;
            DrawRow(row, 15 + i * 8);
        }

        if (scroll_top > 0)
            gfxIcon(57, 14, UP_ICON);
        if (scroll_top + 6 < NUM_ROWS)
            gfxIcon(57, 56, DOWN_ICON);

        gfxDisplayInputMapEditor();
    }

    FLASHMEM void DrawRow(int row, int y) {
        switch (row) {
            case 0:
                gfxPos(1, y);
                gfxStartCursor();
                gfxPrint(clock_source);
                gfxEndCursor(cursor == CLOCK_SRC, false, clock_source.InputName());
                gfxStartCursor(30, y);
                gfxPrint(DIV_NAMES[div]);
                gfxEndCursor(cursor == DIV);
                break;
            case 1: // ON: [hold]  FZ:[freeze] — both on one row
                gfxPos(1, y);
                gfxPrint("ON:");
                if (manual_hold_) gfxInvert(1, y, 18, 8);
                gfxStartCursor();
                gfxPrint(hold_input);
                gfxEndCursor(cursor == HOLD_SRC, false, hold_input.InputName());
                gfxPos(31, y);
                gfxPrint("FZ:");
                gfxStartCursor();
                gfxPrint(freeze_input);
                gfxEndCursor(cursor == FREEZE_SRC, false, freeze_input.InputName());
                break;
            case 2:
                gfxPrint(1, y, "Mod:");
                gfxStartCursor();
                gfxPrint(MODE_NAMES[mode]);
                gfxEndCursor(cursor == MODE);
                gfxStartCursor(49, y);
                gfxPrint(mode_cv);
                gfxEndCursor(cursor == MODE_CV, false, mode_cv.InputName());
                break;
            case 3:
                gfxPrint(1, y, "Rch:");
                gfxStartCursor(25, y);
                graphics.printf("%2d", ratchet);
                gfxEndCursor(cursor == RATCHET);
                gfxStartCursor(49, y);
                gfxPrint(ratchet_cv);
                gfxEndCursor(cursor == RATCHET_CV, false, ratchet_cv.InputName());
                break;
            case 4:
                gfxPrint(1, y, "Bit:");
                gfxStartCursor(25, y);
                graphics.printf("%2d", bits_);
                gfxEndCursor(cursor == BITS);
                gfxStartCursor(49, y);
                gfxPrint(bits_cv);
                gfxEndCursor(cursor == BITS_CV, false, bits_cv.InputName());
                break;
            case 5:
                gfxPrint(1, y, "Smp:");
                gfxStartCursor(25, y);
                graphics.printf("%2d", decimate_);
                gfxEndCursor(cursor == DECIMATE);
                gfxStartCursor(49, y);
                gfxPrint(dec_cv);
                gfxEndCursor(cursor == DECIMATE_CV, false, dec_cv.InputName());
                break;
            case 6:
                gfxPrint(1, y, "Off:");
                gfxStartCursor(25, y);
                graphics.printf("%2d", offset_);
                gfxEndCursor(cursor == OFFSET);
                gfxStartCursor(49, y);
                gfxPrint(off_cv);
                gfxEndCursor(cursor == OFFSET_CV, false, off_cv.InputName());
                break;
            case 7:
                gfxPrint(1, y, "Mix:");
                gfxStartCursor(25, y);
                graphics.printf("%3d%%", mix);
                gfxEndCursor(cursor == MIX);
                gfxStartCursor(49, y);
                gfxPrint(mix_cv);
                gfxEndCursor(cursor == MIX_CV, false, mix_cv.InputName());
                break;
        }
    }

    // AuxButton latches/unlatches manual hold for performance without a patch.
    FLASHMEM void AuxButton() override {
        manual_hold_ ^= 1;
        CancelEdit();
    }

    FLASHMEM void OnButtonPress() override {
        if (CheckEditInputMapPress(
                cursor,
                IndexedInput(CLOCK_SRC,   clock_source),
                IndexedInput(HOLD_SRC,    hold_input),
                IndexedInput(MODE_CV,     mode_cv),
                IndexedInput(RATCHET_CV,  ratchet_cv),
                IndexedInput(BITS_CV,     bits_cv),
                IndexedInput(DECIMATE_CV, dec_cv),
                IndexedInput(OFFSET_CV,   off_cv),
                IndexedInput(MIX_CV,      mix_cv)
            ))
            return;
        CursorToggle();
    }

    FLASHMEM void OnEncoderMove(int direction) override {
        if (!EditMode()) {
            cursor = (Cursor)constrain(cursor + direction, 0, CURSOR_LENGTH - 1);
            int row = cursorToRow(cursor);
            if (row < scroll_top) scroll_top = row;
            else if (row >= scroll_top + 6) scroll_top = row - 5;
            scroll_top = constrain(scroll_top, 0, NUM_ROWS - 6);
            return;
        }
        if (EditSelectedInputMap(direction)) return;

        switch (cursor) {
            case CLOCK_SRC:   clock_source.ChangeSource(direction); break;
            case DIV:         div = constrain(div + direction, 0, NUM_DIVS - 1); break;
            case HOLD_SRC:    hold_input.ChangeSource(direction); break;
            case FREEZE_SRC:  freeze_input.ChangeSource(direction); break;
            case MODE:        mode = constrain(mode + direction, 0, NUM_MODES - 1); break;
            case MODE_CV:     mode_cv.ChangeSource(direction); break;
            case RATCHET:     ratchet = constrain(ratchet + direction, 1, 6); break;
            case RATCHET_CV:  ratchet_cv.ChangeSource(direction); break;
            case BITS:        bits_    = (uint8_t)constrain((int)bits_    + direction, 0, 15); break;
            case BITS_CV:     bits_cv.ChangeSource(direction); break;
            case DECIMATE:    decimate_= (uint8_t)constrain((int)decimate_+ direction, 0, 15); break;
            case DECIMATE_CV: dec_cv.ChangeSource(direction); break;
            case OFFSET:      offset_  = (uint8_t)constrain((int)offset_  + direction, 0, 15); break;
            case OFFSET_CV:   off_cv.ChangeSource(direction); break;
            case MIX:         mix = constrain(mix + direction, 0, 100); break;
            case MIX_CV:      mix_cv.ChangeSource(direction); break;
            default: break;
        }
    }

#define GLITCH_PARAMS  pack<3>(div), pack<2>(mode), pack<3>(ratchet), mix, \
                       pack<4>(bits_), pack<4>(decimate_), pack<4>(offset_)
    FLASHMEM void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) override {
        data[0] = PackPackables(GLITCH_PARAMS);
        data[1] = PackPackables(clock_source, hold_input, freeze_input, mix_cv);
        data[2] = PackPackables(mode_cv, ratchet_cv, bits_cv, dec_cv); // 4×16=64 bits
        data[3] = PackPackables(off_cv);
    }

    FLASHMEM void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) override {
        UnpackPackables(data[0], GLITCH_PARAMS);
        UnpackPackables(data[1], clock_source, hold_input, freeze_input, mix_cv);
        UnpackPackables(data[2], mode_cv, ratchet_cv, bits_cv, dec_cv);
        UnpackPackables(data[3], off_cv);
    }
#undef GLITCH_PARAMS

    AudioStream* InputStream()  override { return &input_stream; }
    AudioStream* OutputStream() override { return &output_stream; }

protected:
    FLASHMEM void SetHelp() override {}

private:
    static const uint8_t NUM_DIVS  = 8;
    static const uint8_t NUM_MODES = 4;
    static const int     NUM_ROWS  = 8;
    static const uint8_t MODE_RATCHET = AudioEffectGlitch::MODE_RATCHET;

    static constexpr const char* DIV_NAMES[] = {
        "1/2", "1/3", "1/4", "1/6", "1/8", "1/16", "1/32", "1/64"
    };
    static constexpr float DIV_BEATS[] = {
        2.0f, 4.0f/3.0f, 1.0f, 2.0f/3.0f, 0.5f, 0.25f, 0.125f, 0.0625f
    };
    static constexpr const char* MODE_NAMES[] = { "FWD", "REV", "PNG", "RAT" };

    enum Cursor : int8_t {
        CLOCK_SRC = 0,
        DIV,
        HOLD_SRC,
        FREEZE_SRC,
        MODE,
        MODE_CV,
        RATCHET,
        RATCHET_CV,
        BITS,
        BITS_CV,
        DECIMATE,
        DECIMATE_CV,
        OFFSET,
        OFFSET_CV,
        MIX,
        MIX_CV,
        CURSOR_LENGTH,
    };

    Cursor cursor     = DIV;
    int8_t scroll_top = 0;

    int8_t cursorToRow(int8_t c) const {
        if (c <= DIV)         return 0;
        if (c <= FREEZE_SRC)  return 1; // HOLD_SRC and FREEZE_SRC share row 1
        if (c <= MODE_CV)     return 2;
        if (c <= RATCHET_CV)  return 3;
        if (c <= BITS_CV)     return 4;
        if (c <= DECIMATE_CV) return 5;
        if (c <= OFFSET_CV)   return 6;
        return 7;
    }

    // Parameters
    DigitalInputMap clock_source;
    uint8_t  div     = 5;   // default 1/16
    DigitalInputMap hold_input;
    DigitalInputMap freeze_input;
    uint8_t  mode    = 0;   // 0=FWD, 1=REV, 2=PING, 3=RAT
    CVInputMap mode_cv;
    uint8_t  ratchet = 2;   // 1–6 subdivisions
    CVInputMap ratchet_cv;
    uint8_t  bits_    = 0;  // 0=bypass(16-bit) … F=1-bit
    CVInputMap bits_cv;
    uint8_t  decimate_= 0;  // 0=bypass … F=16× sample hold
    CVInputMap dec_cv;
    uint8_t  offset_  = 0;  // 0–F slices back on hold-rise
    CVInputMap off_cv;
    int8_t   mix     = 100; // 0–100% wet
    CVInputMap mix_cv;

    bool manual_hold_ = false;

    // Clock period tracking (same approach as DelayApplet)
    uint32_t clock_count     = 0;
    float    clock_base_secs = 0.5f; // 120 BPM default until first clock tick

    // Per-channel DSP struct (mirrors DelayChannel pattern).
    struct GlitchChannel {
        static const uint8_t DRY_CH = 0;
        static const uint8_t WET_CH = 1;

        AudioEffectGlitch glitch_stream;
        AudioMixer<2>     wet_dry_mixer;

        GlitchChannel()
            : glitch_stream(
                external_psram_size
                    ? AudioEffectGlitch::GLITCH_BUFFER_SAMPLES
                    : AudioEffectGlitch::GLITCH_BUFFER_SAMPLES / 16)
        {}

        void Start(HemisphereAudioApplet* owner, int ch,
                   AudioStream& input, AudioStream& output) {
            glitch_stream.Acquire();
            owner->PatchCable(input,        ch,     glitch_stream,   0);
            owner->PatchCable(input,        ch,     wet_dry_mixer,   DRY_CH);
            owner->PatchCable(glitch_stream, 0,     wet_dry_mixer,   WET_CH);
            owner->PatchCable(wet_dry_mixer, 0,     output,          ch);
        }

        void Stop() { glitch_stream.Release(); }
    } channels[Channels];

    AudioPassthrough<Channels> input_stream;
    AudioPassthrough<Channels> output_stream;
};

// Out-of-line definitions for static constexpr members (C++14 ODR requirement).
template <AudioChannels Channels>
constexpr const char* GlitchApplet<Channels>::DIV_NAMES[];
template <AudioChannels Channels>
constexpr float GlitchApplet<Channels>::DIV_BEATS[];
template <AudioChannels Channels>
constexpr const char* GlitchApplet<Channels>::MODE_NAMES[];
