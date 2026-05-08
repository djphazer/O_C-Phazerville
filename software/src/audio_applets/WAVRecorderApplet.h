/*
 * WAV Recorder applet
 *   Records live audio to SD card as a standard 16-bit PCM .WAV file.
 *   Sits transparently in the signal chain (zero-copy passthrough).
 *
 * A PSRAM ring buffer (1 MB) absorbs SD card wear-levelling stalls so the
 * audio interrupt is never blocked by non-deterministic SD latency.
 *
 * Files are named R001.WAV … R999.WAV in the SD root directory.
 *
 * Slots 1–4 (processor chain).  Supports MONO and STEREO templates.
 * In STEREO mode a single interleaved 2-channel file is written.
 *
 * A static inline SD lock prevents two simultaneous instances (e.g. dual
 * mono) from both opening files at once.
 *
 * AuxButton or the mapped gate input toggles recording.
 */

#pragma once

#include <SD.h>

extern "C" uint8_t external_psram_size;

// Shared across all template specialisations (C++17 inline variable).
inline bool wav_recorder_sd_lock = false;

template <AudioChannels Channels>
class WavRecorderApplet : public HemisphereAudioApplet {
public:

    const char* applet_name() { return "WavRec"; }

    // -----------------------------------------------------------------------
    // Audio framework interface
    // -----------------------------------------------------------------------

    void Start() override {
        // Tap the passthrough into record queues.
        // passthru is used for both InputStream() and OutputStream(), so the
        // audio path is zero-copy; the queues are side-channel sinks only.
        for (int ch = 0; ch < Channels; ch++)
            PatchCable(passthru, ch, record_queue[ch], 0);

        psram_buf = (int8_t*)extmem_calloc(RING_BYTES, 1);
        AllowRestart();
    }

    void Unload() override {
        SyncStop();
        if (psram_buf) {
            extmem_free(psram_buf);
            psram_buf = nullptr;
        }
        AllowRestart();
    }

    void Reset() override {
        rec_cv.Reset();
    }

    // -----------------------------------------------------------------------
    // Controller  (runs in ISR context at ~16 kHz — NO blocking I/O here)
    // -----------------------------------------------------------------------

    void Controller() override {
        if (rec_cv.Clock())
            arm_toggle = true;

        // Blink the '*' in the applet name while recording so the selector
        // header gives live feedback even when this applet isn't in focus.
        title[6] = (state == RECORDING) ? (CursorBlink() ? '*' : ' ') : ' ';

        // Exponential decay for the VU peak meter (~40 ms half-life at 16 kHz)
        if (peak_level > 0)
            peak_level -= (peak_level >> 5) + 1;
    }

    // -----------------------------------------------------------------------
    // mainloop()  (called from the main Arduino loop — SD I/O is safe here)
    // -----------------------------------------------------------------------

    void mainloop() override {
        if (!SDcard_Ready || !psram_buf) return;

        // Scan for next available filename once on first mainloop call
        if (!file_scanned) {
            ScanNextFile();
        }

        // Toggle trigger: arm/stop
        if (arm_toggle) {
            arm_toggle = false;
            if (state == IDLE)
                BeginRecording();
            else if (state == RECORDING)
                state = STOPPING;
        }

        if (state == STARTING)
            DoOpen();

        if (state == RECORDING) {
            DrainQueuesToRing();
            FlushRingToSD(false);
        }

        if (state == STOPPING) {
            DrainQueuesToRing();
            FlushRingToSD(true);
            FinalizeWAV(true);
        }
    }

    // -----------------------------------------------------------------------
    // View  (64×64 px display region)
    // -----------------------------------------------------------------------

    void View() override {
        if (!SDcard_Ready) {
            gfxPrint(4, 35, "NO SD CARD!");
            return;
        }
        if (!psram_buf) {
            gfxPrint(4, 35, "No PSRAM");
            return;
        }

        const bool is_recording = (state == RECORDING || state == STOPPING);

        // Row 1 (y=15): state label + trigger input selector
        gfxPrint(1, 15, is_recording ? "REC" : "IDLE");
        if (is_recording)
            gfxInvert(1, 14, 18, 9);

        gfxStartCursor(38, 15);
        gfxPrint(rec_cv);
        gfxEndCursor(cursor == REC_CV, false, rec_cv.InputName(), "RecTrig");

        // Row 2 (y=25): filename (current or next)
        {
            char fname[9];
            if (is_recording)
                memcpy(fname, cur_filename, 9);
            else
                BuildFilename(fname, next_file_num);
            gfxPrint(1, 25, fname);
        }

        // Row 3 (y=35): timer when recording; help text or warning when idle
        if (is_recording) {
            uint32_t ms  = RecMillis();
            uint32_t sec = ms / 1000; ms %= 1000;
            uint32_t min = sec / 60;  sec %= 60;
            gfxPos(1, 35);
            graphics.printf("%02lu:%02lu.%03lu", min, sec, ms);
            // Row 4 (y=44): overflow warning during recording
            if (overflow_flag)
                gfxPrint(1, 44, "OVERFLOW!");
        } else {
            // Idle: show warnings if present, otherwise usage hint
            if (overflow_flag) {
                gfxPrint(1, 35, "OVERFLOW!");
            } else if (wav_recorder_sd_lock) {
                gfxPrint(1, 35, "LOCKED");
            } else {
                gfxPrint(1, 35, "AUX/CV:");
                gfxPrint(1, 44, "rec/stop");
            }
        }

        // VU bar at y=54 (8 px tall, 64 px wide)
        if (is_recording && peak_level > 0) {
            int bar_w = (int)((uint32_t)peak_level * 62 / 32767) + 1;
            gfxRect(0, 54, bar_w, 8);
            // Clip indicator: last 6 px inverted if near full scale
            if (peak_level > 30000)
                gfxInvert(58, 54, 6, 8);
        }

        gfxDisplayInputMapEditor();
    }

    // -----------------------------------------------------------------------
    // Input handling
    // -----------------------------------------------------------------------

    void AuxButton() override {
        arm_toggle = true;
    }

    void OnButtonPress() override {
        if (CheckEditInputMapPress(cursor,
                IndexedInput(REC_CV, rec_cv)
            )) return;
        CursorToggle();
    }

    void OnEncoderMove(int direction) override {
        if (!EditMode()) {
            MoveCursor(cursor, direction, NUM_PARAMS - 1);
            return;
        }
        if (EditSelectedInputMap(direction)) return;
        if (cursor == REC_CV)
            rec_cv.ChangeSource(direction);
    }

    // -----------------------------------------------------------------------
    // Persistence
    // -----------------------------------------------------------------------

    void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) override {
        SyncStop();
        data[0] = PackPackables(rec_cv);
    }

    void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) override {
        SyncStop();
        UnpackPackables(data[0], rec_cv);
    }

    // -----------------------------------------------------------------------
    // Audio routing
    // -----------------------------------------------------------------------

    AudioStream* InputStream()  override { return &passthru; }
    AudioStream* OutputStream() override { return &passthru; }

protected:
    void SetHelp() override {}

private:
    // -----------------------------------------------------------------------
    // Constants
    // -----------------------------------------------------------------------

    static const size_t RING_BYTES      = 1024 * 1024; // 1 MB PSRAM ring buffer
    static const size_t SD_WRITE_CHUNK  = 4096;         // 4 KB per SD write call

    static const uint32_t WAV_SAMPLE_RATE = (uint32_t)AUDIO_SAMPLE_RATE_EXACT; // 48000 on T4.1
    static const uint16_t WAV_BITS        = 16;

    enum WAVCursor : int { REC_CV = 0, NUM_PARAMS };
    enum RecState  : uint8_t { IDLE, STARTING, RECORDING, STOPPING };

    // -----------------------------------------------------------------------
    // Audio objects
    // -----------------------------------------------------------------------

    AudioPassthrough<Channels> passthru;           // shared I/O node
    AudioRecordQueue           record_queue[Channels]; // one sink per channel

    // -----------------------------------------------------------------------
    // PSRAM ring buffer
    // -----------------------------------------------------------------------

    int8_t* psram_buf      = nullptr;
    size_t  ring_write_pos = 0;
    size_t  ring_read_pos  = 0;
    size_t  ring_fill      = 0;

    // -----------------------------------------------------------------------
    // File state
    // -----------------------------------------------------------------------

    File     rec_file;
    uint32_t bytes_written = 0;
    uint8_t  next_file_num = 1;
    bool     file_scanned  = false;
    char     cur_filename[9] = "R000.WAV";

    // -----------------------------------------------------------------------
    // UI / control state
    // -----------------------------------------------------------------------

    RecState rec_state_val = IDLE; // backing store; access via state property
    RecState state         = IDLE;
    DigitalInputMap rec_cv;
    int      cursor        = 0;

    char     title[8]      = "WavRec "; // [6] is live status: '*' or ' '
    bool     arm_toggle    = false;
    uint16_t peak_level    = 0;
    bool     overflow_flag = false;

    // -----------------------------------------------------------------------
    // Filename helpers
    // -----------------------------------------------------------------------

    static void BuildFilename(char* buf, uint8_t num) {
        buf[0] = 'R';
        buf[1] = '0' + (num / 100);
        buf[2] = '0' + (num / 10 % 10);
        buf[3] = '0' + (num % 10);
        buf[4] = '.'; buf[5] = 'W'; buf[6] = 'A'; buf[7] = 'V'; buf[8] = '\0';
    }

    void ScanNextFile() {
        char fname[9];
        next_file_num = 1;
        while (next_file_num <= 999) {
            BuildFilename(fname, next_file_num);
            if (!SD.exists(fname)) break;
            ++next_file_num;
        }
        file_scanned = true;
    }

    // -----------------------------------------------------------------------
    // WAV header
    // -----------------------------------------------------------------------

    void WriteWAVHeader(File& f) {
        const uint16_t numCh      = (uint16_t)Channels;
        const uint32_t byteRate   = WAV_SAMPLE_RATE * numCh * (WAV_BITS / 8);
        const uint16_t blockAlign = numCh * (WAV_BITS / 8);

        // All writes are explicit little-endian to be portable
        auto w32 = [&](uint32_t v) {
            uint8_t b[4] = {(uint8_t)v, (uint8_t)(v>>8),
                            (uint8_t)(v>>16), (uint8_t)(v>>24)};
            f.write(b, 4);
        };
        auto w16 = [&](uint16_t v) {
            uint8_t b[2] = {(uint8_t)v, (uint8_t)(v>>8)};
            f.write(b, 2);
        };

        f.write((const uint8_t*)"RIFF", 4);
        w32(0);                          // placeholder: riff_size = data + 36
        f.write((const uint8_t*)"WAVE", 4);
        f.write((const uint8_t*)"fmt ", 4);
        w32(16);                         // fmt chunk size (PCM)
        w16(1);                          // audio format: PCM
        w16(numCh);
        w32(WAV_SAMPLE_RATE);
        w32(byteRate);
        w16(blockAlign);
        w16(WAV_BITS);
        f.write((const uint8_t*)"data", 4);
        w32(0);                          // placeholder: data chunk size
    }

    void PatchWAVHeader(File& f) {
        // Patch riff_size (offset 4): total file size − 8
        uint32_t riff_size = bytes_written + 36;
        uint8_t b[4];
        auto to_le = [](uint8_t* b, uint32_t v) {
            b[0]=(uint8_t)v; b[1]=(uint8_t)(v>>8);
            b[2]=(uint8_t)(v>>16); b[3]=(uint8_t)(v>>24);
        };
        to_le(b, riff_size);
        f.seek(4); f.write(b, 4);

        // Patch data chunk size (offset 40)
        to_le(b, bytes_written);
        f.seek(40); f.write(b, 4);
    }

    // -----------------------------------------------------------------------
    // Recording state machine helpers
    // -----------------------------------------------------------------------

    // Synchronous stop — safe to call from any main-loop context (not ISR).
    // Used by Unload(), OnDataRequest(), and OnDataReceive().
    void SyncStop() {
        if (state == RECORDING || state == STOPPING) {
            if (psram_buf) {
                DrainQueuesToRing();
                FlushRingToSD(true);
            }
            FinalizeWAV(/*close=*/true);
        } else if (state == STARTING) {
            wav_recorder_sd_lock = false;
        }
        state = IDLE;
    }

    void BeginRecording() {
        if (wav_recorder_sd_lock) return; // another instance is active
        wav_recorder_sd_lock = true;
        state = STARTING;
    }

    void DoOpen() {
        if (!file_scanned) ScanNextFile();
        if (next_file_num > 999) {
            // No free filename slots
            wav_recorder_sd_lock = false;
            state = IDLE;
            return;
        }

        BuildFilename(cur_filename, next_file_num);
        rec_file = SD.open(cur_filename, FILE_WRITE_BEGIN);
        if (!rec_file) {
            wav_recorder_sd_lock = false;
            state = IDLE;
            return;
        }

        WriteWAVHeader(rec_file);
        bytes_written  = 0;
        ring_write_pos = ring_read_pos = ring_fill = 0;
        overflow_flag  = false;

        for (int ch = 0; ch < Channels; ch++) {
            record_queue[ch].clear();
            record_queue[ch].begin();
        }
        state = RECORDING;
    }

    // -----------------------------------------------------------------------
    // Ring buffer
    // -----------------------------------------------------------------------

    // Returns false (and sets overflow_flag) if the ring is full.
    bool RingWrite(const void* src, size_t len) {
        if (ring_fill + len > RING_BYTES) {
            overflow_flag = true;
            return false;
        }
        const uint8_t* s   = (const uint8_t*)src;
        uint8_t*       dst = (uint8_t*)psram_buf;
        size_t avail = RING_BYTES - ring_write_pos;

        if (len <= avail) {
            memcpy(dst + ring_write_pos, s, len);
            ring_write_pos += len;
            if (ring_write_pos == RING_BYTES) ring_write_pos = 0;
        } else {
            memcpy(dst + ring_write_pos, s, avail);
            memcpy(dst, s + avail, len - avail);
            ring_write_pos = len - avail;
        }
        ring_fill += len;
        return true;
    }

    // -----------------------------------------------------------------------
    // Producer: AudioRecordQueue → PSRAM ring
    // -----------------------------------------------------------------------

    void DrainQueuesToRing() {
        // Use a fixed-size scratch buffer (worst case = 2-channel block).
        // For MONO (Channels==1): the inner ch loop writes index i*1+0 = i,
        //   producing a flat mono array — no interleaving overhead.
        // For STEREO (Channels==2): produces L[0],R[0],L[1],R[1],…
        int16_t scratch[AUDIO_BLOCK_SAMPLES * 2];

        while (QueuesAvailable()) {
            // Read one block from every channel simultaneously
            int16_t* bufs[2] = {nullptr, nullptr};
            for (int ch = 0; ch < Channels; ch++)
                bufs[ch] = record_queue[ch].readBuffer();

            // Interleave (or copy for mono) + track peak
            for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
                for (int ch = 0; ch < Channels; ch++) {
                    int16_t s = bufs[ch][i];
                    scratch[i * Channels + ch] = s;
                    uint16_t a = (uint16_t)(s < 0 ? -s : s);
                    if (a > peak_level) peak_level = a;
                }
            }

            for (int ch = 0; ch < Channels; ch++)
                record_queue[ch].freeBuffer();

            RingWrite(scratch, (size_t)AUDIO_BLOCK_SAMPLES * Channels * sizeof(int16_t));
        }
    }

    bool QueuesAvailable() const {
        for (int ch = 0; ch < Channels; ch++)
            if (!record_queue[ch].available()) return false;
        return true;
    }

    // -----------------------------------------------------------------------
    // Consumer: PSRAM ring → SD card
    // -----------------------------------------------------------------------

    // flush_all=true: write everything remaining (used during STOPPING).
    // flush_all=false: write only complete SD_WRITE_CHUNK blocks.
    void FlushRingToSD(bool flush_all) {
        size_t threshold = flush_all ? 1 : SD_WRITE_CHUNK;
        uint8_t* src = (uint8_t*)psram_buf;

        while (ring_fill >= threshold) {
            size_t to_write = (ring_fill < SD_WRITE_CHUNK) ? ring_fill : SD_WRITE_CHUNK;
            size_t avail    = RING_BYTES - ring_read_pos;

            if (to_write <= avail) {
                rec_file.write(src + ring_read_pos, to_write);
                ring_read_pos += to_write;
                if (ring_read_pos == RING_BYTES) ring_read_pos = 0;
            } else {
                rec_file.write(src + ring_read_pos, avail);
                rec_file.write(src, to_write - avail);
                ring_read_pos = to_write - avail;
            }
            bytes_written += to_write;
            ring_fill     -= to_write;
        }
    }

    // -----------------------------------------------------------------------
    // Stop and finalise
    // -----------------------------------------------------------------------

    void FinalizeWAV(bool do_close) {
        PatchWAVHeader(rec_file);
        if (do_close) rec_file.close();

        for (int ch = 0; ch < Channels; ch++)
            record_queue[ch].end();

        wav_recorder_sd_lock = false;
        ++next_file_num; // advance counter; no need to rescan
        state = IDLE;
    }

    // -----------------------------------------------------------------------
    // Timer display
    // -----------------------------------------------------------------------

    uint32_t RecMillis() const {
        // bytes_written / bytes_per_second * 1000
        const uint32_t bps = WAV_SAMPLE_RATE * (uint32_t)Channels * sizeof(int16_t);
        return (uint32_t)((uint64_t)bytes_written * 1000 / bps);
    }
};
