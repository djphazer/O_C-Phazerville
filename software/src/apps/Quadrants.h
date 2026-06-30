// Copyright (c) 2018, Jason Justian
// Copyright (c) 2024, Nicholas J. Michalek
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

#pragma once

// per bank file
static constexpr int QUAD_PRESET_COUNT = 32;
static constexpr int PRESET_FILE_REVISION = 1;

using namespace HS;

void QuadrantSysExHandler();

OC_APP_CLASS(AppQuadrants, TWOCCS("QS"), "Quadrants", "4x Applets"),
  public HSApplication {
public:
  OC_APP_INTERFACE_DECLARE(AppQuadrants, 0);

    void Start() {
        audio_app.Init();

        //bank_filename[16] = "BANK_000.DAT";
        bank_num = 0;
        preset_id = -1;
        queued_preset = -1;
        preset_cursor = 0;

        //HemisphereApplet *active_applet[4]; // Pointers to actual applets
        view_slot[0] = 0;
        view_slot[1] = 0;
        config_cursor = LOAD_PRESET;

        select_mode = -1;
        zoom_slot = HEM_SIDE(0);
        zoom_cursor = 0;
        click_tick = 0;
        first_click = -1;

        mask = 0;
        last_mask = 0;

        Randomizer();
        //SetApplet(HEM_SIDE(0), HS::get_applet_index_by_id(18)); // DualTM
        //SetApplet(HEM_SIDE(1), HS::get_applet_index_by_id(15)); // EuclidX
        //SetApplet(HEM_SIDE(2), HS::get_applet_index_by_id(68)); // DivSeq
        //SetApplet(HEM_SIDE(3), HS::get_applet_index_by_id(71)); // Pigeons
    }

    void Resume() {
        SetBank(bank_num);

        if (preset_id < 0)
          LoadFromPreset(0);

        for (auto& env : HS::env_) env.reset();
    }
    void Suspend() {
        if (preset_id >= 0) {
            if (HS::auto_save_enabled)
              StoreToPreset(preset_id);
            // TODO
            //OnSendSysEx();
        }
    }
    void SetBank(uint8_t id) {
      bank_filename[5] = '0' + char(id / 100);
      bank_filename[6] = '0' + char(id / 10 % 10);
      bank_filename[7] = '0' + char(id % 10);

      bool success = false;
      if (SDcard_Ready) // load from SD card
        success = PhzConfig::load_config(bank_filename, SD);

      if (!success) // fallback load from LFS
        PhzConfig::load_config(bank_filename);

      LoadGlobals();

      // Version flag - for future reconfigurations...
      //  - This can be used to migrate old data after breaking changes to the schema
      //  - Right now, its existence indicates v1.10.1+
      uint64_t data = 0;
      bool version_key_present = PhzConfig::getValue(VERSION_KEY, data);
      int preset_file_revision = version_key_present? data : 0;

      // Data migration to protect against a bug in v1.10
      // This can be removed later, when we're sure no one is still using broken preset files...
      if (!version_key_present) {
        for (size_t i = 0; i < QUAD_PRESET_COUNT; ++i) {
          PhzConfig::deleteKey((i << 11) | (CVMAP_KEY + 1));
          PhzConfig::deleteKey((i << 11) | (TRIGMAP_KEY + 1));
        }
      }
      if (preset_file_revision < 1) {
        // migrations from v1.x to v2.0 go here
        // these are handled elsewhere in Unpack() functions:
        // - CVInputMap sources got remapped
        // - MIDI Map functions got remapped
      }

      // update version key after all data migrations
      PhzConfig::setValue(VERSION_KEY, PRESET_FILE_REVISION);
    }

    // lower 11 bits of PhzConfig KEY
    // Audio applet data keys also use 11 bits, with the upper 3 being non-zero...
    // - watch out for collisions, especially on Preset A (index 0)
    enum PresetDataKeys : uint16_t {
        APPLET_METADATA_KEY = 0, // applet ids
        CLOCK_DATA_KEY = 1,
        GLOBALS_KEY = 2,
        ANCIENT_TRIGMAP_KEY = 3, // from v1.9
        ANCIENT_CVMAP_KEY = 4, // from v1.9
        OUTSKIP_KEY = 5,
        OUTSLEW_KEY = 6,
        OUTATTEN_KEY = 7,
        INSKIP_KEY = 8,

        APPLET_L1_DATA_KEY = 10,
        APPLET_R1_DATA_KEY = 11,
        APPLET_L2_DATA_KEY = 12,
        APPLET_R2_DATA_KEY = 13,

        CVMAP_KEY = 20, // 16-bit CVInputMap, multiple pages

        OLD_TRIGMAP_KEY = 30, // 2 blobs, 16-bit DigitalInputMap
        TRIGMAP_KEY = 32, // 32-bit DigitalInputMap, 4 pages

        // 100s = Globals
        FILTERMASK1_KEY = 100,
        FILTERMASK2_KEY = 101,

        MIDI_GLOBALS_KEY  = 110,
        PRESET_JUMP_KEY = 111,

        MIDI_MAPS_KEY   = 150, // + 0..32

        // 200s = Quantizers
        Q_ENGINE_KEY    = 200, // + slot number (200 - 207)

        // 256 = used by Audio Applets

        // 300-428 = Sequences (aka Patterns)
        SEQUENCES_KEY   = 300, // + blob index

        // More ranges used by Audio Applet data:
        // 512-522
        // 768-778
        // 1024+
        // ... check AudioAppletSubapp to be sure!

        VERSION_KEY = 0xFFFF // 65535
    };

    void DeletePreset(int id) {
        uint16_t preset_key = id << 11;
        // non-global values are all 0-99 in the enum
        for (int i = 0; i < 100; ++i) {
          PhzConfig::deleteKey(preset_key | i);
        }
        // TODO:
        //audio_app.deletePresetData(id);
    }

    void StoreToPreset(int id);
    void store_to_preset(int id) {
        preset_id = id;
        // preset id is upper 5 bits - 32 presets per bank
        uint16_t preset_key = id << 11;

        // clock data
        clock_data = ClockSetup_instance.OnDataRequest();
        PhzConfig::setValue(preset_key | CLOCK_DATA_KEY, clock_data);

        // vague globals
        global_data = ClockSetup_instance.GetGlobals();
        PhzConfig::setValue(preset_key | GLOBALS_KEY, global_data);

        uint64_t data = 0;
        // Input Mappings
        for (size_t i = 0; i < ADC_CHANNEL_COUNT/4; ++i) {
          data = PackPackables(HS::trigmap[i*4], HS::trigmap[i*4+1]);
          PhzConfig::setValue(preset_key | (TRIGMAP_KEY + i*2), data);
          data = PackPackables(HS::trigmap[i*4+2], HS::trigmap[i*4+3]);
          PhzConfig::setValue(preset_key | (TRIGMAP_KEY + i*2 + 1), data);

          data = PackPackables(HS::cvmap[i*4], HS::cvmap[i*4+1], HS::cvmap[i*4+2], HS::cvmap[i*4+3]);
          PhzConfig::setValue(preset_key | (CVMAP_KEY + i), data);
        }
        PhzConfig::deleteKey(preset_key | OLD_TRIGMAP_KEY);
        PhzConfig::deleteKey(preset_key | (OLD_TRIGMAP_KEY + 1));

        data = 0;
        for (size_t i = 0; i < 8; ++i) {
          Pack(data, PackLocation{i*8, 8}, HS::frame.clockinskip[i]);
        }
        PhzConfig::setValue(preset_key | INSKIP_KEY, data);
        data = 0;
        for (size_t i = 0; i < 8; ++i) {
          Pack(data, PackLocation{i*8, 8}, HS::frame.clockoutskip[i]);
        }
        PhzConfig::setValue(preset_key | OUTSKIP_KEY, data);
        data = 0;
        for (size_t i = 0; i < 8; ++i) {
          Pack(data, PackLocation{i*8, 8}, HS::frame.output_slew[i]);
        }
        PhzConfig::setValue(preset_key | OUTSLEW_KEY, data);
        data = 0;
        for (size_t i = 0; i < 8; ++i) {
          Pack(data, PackLocation{i*8, 8}, static_cast<uint8_t>(HS::frame.output_atten[i]));
        }
        PhzConfig::setValue(preset_key | OUTATTEN_KEY, data);

        data = 0;
        for (size_t h = 0; h < APPLET_SLOTS; h++)
        {
            int index = active_applet_index[h];
            Pack(data, PackLocation{h*8,8}, HS::appletIds[index]);

            // applet data
            applet_data[h] = HS::get_applet(index, HEM_SIDE(h))->OnDataRequest();
            PhzConfig::setValue(preset_key | (APPLET_L1_DATA_KEY + h), applet_data[h]);
        }

        // applet ids, and maybe some other stuff?
        PhzConfig::setValue(preset_key | APPLET_METADATA_KEY, data);

        // applet filtering is actually just global
        PhzConfig::setValue(FILTERMASK1_KEY, HS::hidden_applets[0]);
        PhzConfig::setValue(FILTERMASK2_KEY, HS::hidden_applets[1]);

        data = PackPackables(
          HS::frame.MIDIState.pc_channel,
          HS::frame.MIDIState.bend_range,
          midi_thru_disable,
          midi_clkrx_disable,
          midi_clktx_disable,
          midi_msgrx_disable,
          midi_msgtx_disable
        );
        PhzConfig::setValue(MIDI_GLOBALS_KEY, data);

        data = PackPackables(jump_trig_);
        PhzConfig::setValue(PRESET_JUMP_KEY, data);

        // Global quantizer settings
        for (size_t qslot = 0; qslot < QUANT_CHANNEL_COUNT; ++qslot) {
          /*
            // XXX: fine-tuning stuff from Calibr8or that should also be global
            int8_t offset;
            int16_t scale_factor; // precision of 0.01% as an offset from 100%
            int8_t transpose; // in semitones
          */
          auto &q = q_engine[qslot];
          data = PackPackables(
              q.scale,
              q.octave,
              q.root_note,
              q.mask
              );
          PhzConfig::setValue(Q_ENGINE_KEY + qslot, data);
        }

        // Global MIDI Maps
        for (size_t midx = 0; midx < MIDIMAP_MAX; ++midx) {
          data = PackPackables(frame.MIDIState.mapping[midx]);
          PhzConfig::setValue(MIDI_MAPS_KEY + midx, data);
        }

        // User Patterns aka Sequences
        for (size_t i = 0; i < OC::Patterns::PATTERN_USER_COUNT; ++i) {
          data = 0;
          for (size_t step = 0; step < ARRAY_SIZE(OC::Pattern::notes); ++step) {
            Pack(data, PackLocation{(step & 0x3)*16, 16}, (uint16_t)OC::user_patterns[i].notes[step]);
            if ((step & 0x3) == 0x3) {
              PhzConfig::setValue(SEQUENCES_KEY + ((i << 2) | (step >> 2)), data);
              data = 0;
            }
          }
        }

        audio_app.SavePreset(id);

        bool success = false;
        if (SDcard_Ready)
          success = PhzConfig::save_config(bank_filename, SD);
        else
          success = PhzConfig::save_config(bank_filename);

        if (success)
          PokePopup(HS::MESSAGE_POPUP, HS::PRESET_SAVED);
    }

    void LoadFromPreset(int id);
    void load_from_preset(int id) {
        preset_id = id;

        uint16_t preset_key = id << 11;
        uint64_t data;

        // applet ids + misc
        if (!PhzConfig::getValue(preset_key | APPLET_METADATA_KEY, data)) return;
        if (!data) return;

        for (size_t h = 0; h < APPLET_SLOTS; h++)
        {
            int index = HS::get_applet_index_by_id( Unpack(data, PackLocation{h*8, 8}) );

            // applet data
            PhzConfig::getValue(preset_key | (APPLET_L1_DATA_KEY + h), applet_data[h]);
            SetApplet(HEM_SIDE(h), index);
            HS::get_applet(index, HEM_SIDE(h))->OnDataReceive(applet_data[h]);
        }

        // clock data
        if (!PhzConfig::getValue(preset_key | CLOCK_DATA_KEY, clock_data)) return;
        ClockSetup_instance.OnDataReceive(clock_data);
        // if the first key exists, we are assuming the rest are present...

        // vague globals
        PhzConfig::getValue(preset_key | GLOBALS_KEY, global_data);
        ClockSetup_instance.SetGlobals(global_data);

        // Input Mappings
        if (PhzConfig::getValue(preset_key | TRIGMAP_KEY, data)) {
          for (size_t i = 0; i < ADC_CHANNEL_COUNT/2; ++i) {
            UnpackPackables(data, HS::trigmap[i*2], HS::trigmap[i*2+1]);
            if (!PhzConfig::getValue(preset_key | (TRIGMAP_KEY + i+1), data)) break;
          }
        } else if (PhzConfig::getValue(preset_key | OLD_TRIGMAP_KEY, data)) {
          // migrate from v1.x
          uint16_t mapdata[4];
          UnpackPackables(data, mapdata[0], mapdata[1], mapdata[2], mapdata[3]);
          HS::trigmap[0].Unpack(mapdata[0]);
          HS::trigmap[1].Unpack(mapdata[1]);
          HS::trigmap[2].Unpack(mapdata[2]);
          HS::trigmap[3].Unpack(mapdata[3]);
          PhzConfig::getValue(preset_key | (OLD_TRIGMAP_KEY + 1), data);
          UnpackPackables(data, mapdata[0], mapdata[1], mapdata[2], mapdata[3]);
          HS::trigmap[4].Unpack(mapdata[0]);
          HS::trigmap[5].Unpack(mapdata[1]);
          HS::trigmap[6].Unpack(mapdata[2]);
          HS::trigmap[7].Unpack(mapdata[3]);
        }

        if (PhzConfig::getValue(preset_key | CVMAP_KEY, data)) {
          for (size_t i = 0; i < ADC_CHANNEL_COUNT/4; ++i) {
            UnpackPackables(data, HS::cvmap[i*4], HS::cvmap[i*4+1], HS::cvmap[i*4+2], HS::cvmap[i*4+3]);
            if (!PhzConfig::getValue(preset_key | (CVMAP_KEY + i+1), data)) break;
          }
        }

        data = 0;
        PhzConfig::getValue(preset_key | INSKIP_KEY, data);
        for (size_t i = 0; i < 8; ++i) {
          HS::frame.clockinskip[i] = Unpack(data, PackLocation{i*8, 8});
        }

        PhzConfig::getValue(preset_key | OUTSKIP_KEY, data);
        for (size_t i = 0; i < 8; ++i)
        {
          HS::frame.clockoutskip[i] = Unpack(data, PackLocation{i*8, 8});
        }

        PhzConfig::getValue(preset_key | OUTSLEW_KEY, data);
        for (size_t i = 0; i < 8; ++i)
        {
          HS::frame.output_slew[i] = Unpack(data, PackLocation{i*8, 8});
        }

        const bool has_output_atten = PhzConfig::getValue(preset_key | OUTATTEN_KEY, data);
        for (size_t i = 0; i < 8; ++i)
        {
          HS::frame.output_atten[i] = has_output_atten ? Unpack(data, PackLocation{i*8, 8}) : 60;
        }

        //LoadGlobals();

        audio_app.LoadPreset(id);
        PokePopup(PRESET_POPUP);
    }
    void LoadGlobals() {
        // applet filtering
        PhzConfig::getValue(FILTERMASK1_KEY, HS::hidden_applets[0]);
        PhzConfig::getValue(FILTERMASK2_KEY, HS::hidden_applets[1]);

        uint64_t data = 0;
        if (PhzConfig::getValue(MIDI_GLOBALS_KEY, data)) {
          UnpackPackables(data,
              HS::frame.MIDIState.pc_channel,
              HS::frame.MIDIState.bend_range,
              midi_thru_disable,
              midi_clkrx_disable,
              midi_clktx_disable,
              midi_msgrx_disable,
              midi_msgtx_disable
          );
        }
        if (midi_thru_disable & mMaskSerial) MIDI1.turnThruOff();
        else MIDI1.turnThruOn();

        if (PhzConfig::getValue(PRESET_JUMP_KEY, data))
          UnpackPackables(data, jump_trig_);

        // Global quantizer settings
        for (size_t qslot = 0; qslot < QUANT_CHANNEL_COUNT; ++qslot) {
          if (!PhzConfig::getValue(Q_ENGINE_KEY + qslot, data))
              break;
          auto &q = q_engine[qslot];
          UnpackPackables(data,
              q.scale,
              q.octave,
              q.root_note,
              q.mask);
          q.Reconfig();
        }

        // Global MIDI Maps
        for (size_t midx = 0; midx < MIDIMAP_MAX; ++midx) {
          if (!PhzConfig::getValue(MIDI_MAPS_KEY + midx, data))
              break;
          UnpackPackables(data, frame.MIDIState.mapping[midx]);
        }
        frame.MIDIState.UpdateMidiChannelFilter();
        frame.MIDIState.UpdateMaxPolyphony();

        // User Patterns aka Sequences
        for (size_t i = 0; i < OC::Patterns::PATTERN_USER_COUNT; ++i) {
          for (size_t step = 0; step < ARRAY_SIZE(OC::Pattern::notes); ++step) {
            if ((step & 0x3) == 0x0) {
              data = 0;
              if (!PhzConfig::getValue(SEQUENCES_KEY + ((i << 2) | (step >> 2)), data))
                break;
            }
            OC::user_patterns[i].notes[step] = Unpack(data, PackLocation{(step & 0x3)*16, 16});
          }
        }
    }
    void ProcessQueue() {
      if (queued_preset >= 0) {
        LoadFromPreset(queued_preset);
        queued_preset = -1;
      }
    }
    void QueuePresetLoad(int id) {
      if (HS::clock_m.IsRunning()) {
        queued_preset = id;
        HS::clock_m.BeatSync( [this](){ ProcessQueue(); } );
      }
      else
        LoadFromPreset(id);
    }
    void JumpToNextPreset() {
      int next_id = preset_id + 1;
      while (!isValidPreset(next_id) && next_id != preset_id) {
        ++next_id %= QUAD_PRESET_COUNT;
      }
      if (next_id != preset_id)
        QueuePresetLoad(next_id);
    }

    // does not modify the preset, only the current state
    void SetApplet(HEM_SIDE hemisphere, int index) {
        /*noInterrupts();*/
        HemisphereApplet* next_ = HS::get_applet(index, hemisphere);
        HemisphereApplet* old_ = active_applet[hemisphere];
        next_->BaseStart(hemisphere);
        // make sure we've called Start before changing the shared pointer
        active_applet[hemisphere] = next_;
        // unload previous applet after swapping
        if (old_) old_->Unload();
        next_applet_index[hemisphere] = active_applet_index[hemisphere] = index;
        /*interrupts();*/
    }
    void ChangeApplet(HEM_SIDE h, int dir) {
        int index = HS::get_next_applet_index(next_applet_index[h], dir);
        next_applet_index[h] = index;
    }

    void SendMIDIThru(const MIDIMessage &msg, uint8_t exclude_mask) {
      exclude_mask |= midi_thru_disable;
      if (~exclude_mask & mMaskUSBDev) {
        usbMIDI.send(msg.message, msg.data1, msg.data2, msg.channel, 0);
      }
      if (~exclude_mask & mMaskUSBHost) {
        usbHostMIDI[0].send(msg.message, msg.data1, msg.data2, msg.channel);
      }
      if (~exclude_mask & mMaskUSBHost2) {
        usbHostMIDI[1].send(msg.message, msg.data1, msg.data2, msg.channel);
      }
      if (~exclude_mask & mMaskSerial) {
        MIDI1.send((midi::MidiType)msg.message, msg.data1, msg.data2, msg.channel);
      }
    }

    template <typename T1>
    void ProcessMIDI(T1 &device) {
        HS::IOFrame &f = HS::frame;
        int load_slot = -1;

        uint8_t thrumask = 0;
        // who needs types? we have pointers!
        if ((void*)&device == (void*)&usbMIDI) thrumask = mMaskUSBDev;
        if ((void*)&device == (void*)&usbHostMIDI[0]) thrumask = mMaskUSBHost;
        if ((void*)&device == (void*)&usbHostMIDI[1]) thrumask = mMaskUSBHost2;
        if ((void*)&device == (void*)&MIDI1) thrumask = mMaskSerial;
        // Serial-to-Serial Thru still happens in the library, if enabled

        const bool clkrx = ~midi_clkrx_disable & thrumask;
        const bool msgrx = ~midi_msgrx_disable & thrumask;

        while (timeout < 60 && device.read()) {
          const MIDIMessage msg = {
            device.getChannel(),
            device.getType(),
            device.getData1(),
            device.getData2()
          };

          switch (msg.message) {
            case midi::SystemExclusive:
              QuadrantSysExHandler();
              continue;

            case midi::ProgramChange:
              if (msg.channel == f.MIDIState.pc_channel || f.MIDIState.pc_channel == f.MIDIState.PC_OMNI) {
                load_slot = msg.data1;
              }
              break;

            case midi::Clock:
            case midi::Start:
            case midi::Stop:
              if (clkrx) f.MIDIState.ProcessMIDIMsg(msg); // receive it
                break;

            default:
              if (msgrx && (msg.message >> 4) != 0xF) f.MIDIState.ProcessMIDIMsg(msg); // receive it
                break;
          }

          // send it along
          SendMIDIThru(msg, thrumask);
        }
        if (load_slot >= 0 && load_slot < QUAD_PRESET_COUNT) {
            QueuePresetLoad(load_slot);
        }
    }

    void mainloop() {
        timeout = 0;
        // top-level MIDI-to-CV handling - alters frame outputs
        ProcessMIDI(usbMIDI);
        ProcessMIDI(usbHostMIDI[0]);
        ProcessMIDI(usbHostMIDI[1]);
        ProcessMIDI(MIDI1);
    }
    void Controller() {
        // Clock Setup applet handles internal clock duties
        ClockSetup_instance.Controller();
        // ^ this will process the queue and load presets

        // this might be triggered by the internal clock...
        if (jump_trig_.Clock() && !HS::clock_m.auto_reset) {
          JumpToNextPreset();
          // ^ this will sometimes queue a preset load

          // The paradox is we need to process the clock first, in case jump_trig needs it,
          // but then jump_trig will queue another preset load,
          // so we have to process the queue again.
          if (jump_trig_.is_clock()) ProcessQueue();
        }

        // execute Applets
        for (int h = 0; h < APPLET_SLOTS; h++)
        {
            if (HS::clock_m.auto_reset)
                active_applet[h]->Reset();

            active_applet[h]->Controller();
        }
        audio_app.Controller();
        HemisphereApplet::ProcessCursors();
        HS::clock_m.auto_reset = false;
    }

    void DrawFullScreen() const {
      if (select_mode == zoom_slot) {
        showhide_cursor.Scroll(next_applet_index[zoom_slot] - showhide_cursor.cursor_pos());
        DrawAppletList(CursorBlink());
        // dotted screen border during applet select
        gfxFrame(0, 0, 128, 64, true);
        return;
      }

      active_applet[zoom_slot]->BaseView(true, zoom_cursor < 0);
      // Applets 3 and 4 get inverted titles
      if (zoom_slot > 1) gfxInvert(0 + (zoom_slot%2)*64, 0, 63, 10);

      // draw cursors for editing applet select and input maps
      if (zoom_cursor < 0) {
        gfxIcon(64 - 8*(zoom_slot & 1), 1, DOWN_ICON, true);
      } else if (0 == zoom_cursor) {
        if (select_mode != zoom_slot && CursorBlink())
          gfxIcon(64 - 8*(zoom_slot & 1), 1, (zoom_slot & 1)? RIGHT_ICON : LEFT_ICON, true);
      } else if (isEditing) {
        const int x = 64*((zoom_cursor-1)%2);
        const int y = 13 + 10*((zoom_cursor-1)/2);
        gfxInvert(x, y, 19, 9);
        gfxFrame(x, y, 19, 9, true);

        if (zoom_cursor <= 2) {
          // trigmap & clock multiplier
          const int io_chan = zoom_slot * 2 + zoom_cursor - 1;
          const int mult = HS::clock_m.GetMultiply(io_chan);

          graphics.clearRect(x, y - 9, 36, 8);
          graphics.drawBitmap8(x, y - 9, 8, RIGHT_ICON);
          graphics.drawBitmap8(x + 8, y - 9, 8, CLOCK_ICON);
          graphics.setPrintPos(x + 16, y - 9);
          graphics.print((mult >= 0) ? "x" : "/");
          graphics.print((mult >= 0) ? mult : 1 - mult);
        }
        if (zoom_cursor >= 5) {
          gfxIcon(x + 18, y + 1, DOWN_ICON, true);

          graphics.clearRect(0, y + 10, 127, 20);
          gfxPrint(x, y+10, "Slew=");
          gfxPrint(HS::frame.output_slew[zoom_slot*2 + zoom_cursor-5]);
          gfxPrint("%");

          const int att = Atten(HS::frame.output_atten[zoom_slot*2 + zoom_cursor-5]);
          gfxPrint(x, y+20, "Lvl=");
          if (att < 0) gfxPrint("-");
          graphics.printf("%d.%d%%", abs(att) / 10, abs(att) % 10);
        }
      } else {
        if (CursorBlink()) {
          const int x = 18 + 64*((zoom_cursor-1)%2);
          const int y = 14 + 10*((zoom_cursor-1)/2);
          gfxIcon(x, y, LEFT_ICON, true);
        }
      }

      gfxDisplayInputMapEditor();
    }

    void DrawOverview() const {
      active_applet[0]->gfxHeader(0);
      active_applet[1]->gfxHeader(0);
      active_applet[2]->gfxHeader(54);
      active_applet[3]->gfxHeader(54);

      gfxDottedLine(63, 0, 63, 63); // vert
      gfxDottedLine(0, 32, 127, 32); // horiz

      ForAllChannels(applet) {
        ForEachChannel(ch) {
            int length;
            int max_length = 62;
            int in_bar_y = 13 + (applet>>1)*22 + (ch * 10);
            int out_bar_y = 17 + (applet>>1)*22 + (ch * 10);

            // positive values extend bars from left side of screen to the right
            // negative values go from right side to left
            length = ProportionCV(abs(DetentedIn(applet*2 + ch)), max_length);
            if (DetentedIn(applet*2 + ch) < 0)
                active_applet[applet]->gfxFrame(max_length - length, in_bar_y, length, 3);
            else
                active_applet[applet]->gfxFrame(0, in_bar_y, length, 3);

            length = ProportionCV(abs(ViewOut(applet*2 + ch)), max_length);
            if (ViewOut(applet*2 + ch) < 0)
                active_applet[applet]->gfxRect(max_length - length, out_bar_y, length, 3);
            else
                active_applet[applet]->gfxRect(0, out_bar_y, length, 3);
        }
      }
    }

    void View() const {
        bool draw_applets = true;

        if (preset_cursor) {
          DrawPresetSelector();
          draw_applets = false;
        }
        else if (config_page > HIDE_CONFIG) {
          switch(config_page) {
          default:
          case LOADSAVE_POPUP:
            PokePopup(MENU_POPUP);
            // but still draw the applets
            break;

          case MIDI_MAPS_PAGE:
            DrawMidiMaps(config_cursor - MIDIMAP1);
            draw_applets = false;
            break;

          case INPUT_SETTINGS:
            DrawInputMappings();
            draw_applets = false;
            break;

          case QUANTIZER_SETTINGS:
            DrawQuantizerConfig();
            draw_applets = false;
            break;

          case CONFIG_SETTINGS:
            DrawConfigMenu();
            draw_applets = false;
            break;

          case SHOWHIDE_APPLETS:
            DrawAppletList();
            draw_applets = false;
            break;
          }
        }
        if (HS::q_edit)
          PokePopup(QUANTIZER_POPUP);
        else if (HS::midi_edit)
          PokePopup(MIDI_POPUP);

        if (draw_applets) {
          if (view_state == AUDIO_SETUP) {
            audio_app.View();

            draw_applets = false;
          }
        }

        if (draw_applets) {
          if (view_state == APPLET_FULLSCREEN) {
            DrawFullScreen();
          } else if (view_state == OVERVIEW) {
            DrawOverview();
          } else {
            // only two applets visible at a time
            for (int h = 0; h < 2; h++)
            {
                HEM_SIDE slot = HEM_SIDE(h + view_slot[h]*2);
                active_applet[slot]->BaseView();

                // Applets 3 and 4 get inverted titles
                if (slot > 1) gfxInvert(0 + h*64, 0, 63, 10);
            }

            // vertical separator
            graphics.drawLine(63, 0, 63, 63, 2);
          }
        }

        // Clock setup is an overlay
        if (clock_overlay) {
          ClockSetup_instance.View();
        } else {
          ClockSetup_instance.DrawIndicator(view_state == OVERVIEW);
        }

        // Overlay popup window last
        if (OC::CORE::ticks - HS::popup_tick < HEMISPHERE_CURSOR_TICKS * 4) {
          HS::DrawPopup(config_cursor, preset_id, CursorBlink());
        }
    }

    // always act-on-press for encoder
    void DelegateEncoderPush(const UI::Event &event) {
        int h = (event.control == OC::CONTROL_BUTTON_L) ? LEFT_HEMISPHERE : RIGHT_HEMISPHERE;
        HEM_SIDE slot = HEM_SIDE(view_slot[h]*2 + h);

        if (config_page > HIDE_CONFIG || preset_cursor) {
          ConfigButtonPush(h);
          return;
        }
        if (view_state == AUDIO_SETUP) {
          // audio_app.HandleButtonEvent(event);
          return;
        }
        if (view_state == APPLET_FULLSCREEN) {
          switch (zoom_cursor) {
            case -1:
              active_applet[zoom_slot]->OnButtonPress();
              break;

            case 0:
              if (zoom_slot == select_mode) {
                SetApplet(zoom_slot, next_applet_index[zoom_slot]);
                ExitFullScreen();
              } else
                select_mode = zoom_slot;
              break;
            //// 0=select; 1,2=trigmap; 3,4=cvmap; 5,6=outmode
            case 3:
            case 4:
              if (CheckEditInputMapPress(
                    zoom_cursor,
                    IndexedInput(3, cvmap[zoom_slot*2]),
                    IndexedInput(4, cvmap[zoom_slot*2+1])
                  ))
                break;
            case 1:
            case 2:
              if (h == RIGHT_HEMISPHERE && CheckEditInputMapPress(
                    zoom_cursor,
                    IndexedInput(1, trigmap[zoom_slot*2]),
                    IndexedInput(2, trigmap[zoom_slot*2+1])
                  ))
                break;
            case 5:
            case 6:
            default:
              isEditing = !isEditing;
              break;
          }
          return;
        }

        if (view_state == OVERVIEW) {
          // TODO:
          return;
        }

        active_applet[slot]->OnButtonPress();
    }

    const HEM_SIDE ButtonToSlot(const UI::Event &event) {
      if (event.control == OC::CONTROL_BUTTON_X)
        return LEFT2_HEMISPHERE;
      if (event.control == OC::CONTROL_BUTTON_B)
        return RIGHT_HEMISPHERE;
      if (event.control == OC::CONTROL_BUTTON_Y)
        return RIGHT2_HEMISPHERE;

      //if (event.control == OC::CONTROL_BUTTON_A)
      return LEFT_HEMISPHERE; // default
    }

    // returns true if combo detected and button release should be ignored
    bool CheckButtonCombos(const UI::Event &event) {
        HEM_SIDE slot = ButtonToSlot(event);

        // hold Left Enc + (A or B) to toggle view
        if (CheckButtonCombo(OC::CONTROL_BUTTON_L | OC::CONTROL_BUTTON_A) ||
            CheckButtonCombo(OC::CONTROL_BUTTON_L | OC::CONTROL_BUTTON_B)) {
            SwapViewSlot(slot);
            return true;
        }

        // dual press A+B for Clock Setup
        if (CheckButtonCombo(OC::CONTROL_BUTTON_A | OC::CONTROL_BUTTON_B)) {
            clock_overlay = true;
            return true;
        }
        // dual press X+Y for Audio Setup
        if (CheckButtonCombo(OC::CONTROL_BUTTON_X | OC::CONTROL_BUTTON_Y)) {
            view_state = AUDIO_SETUP;
            return true;
        }
        // dual press A+X for Load Preset
        if (CheckButtonCombo(OC::CONTROL_BUTTON_A | OC::CONTROL_BUTTON_X)) {
            ShowPresetSelector();
            return true;
        }

        // dual press B+Y for Input Mapping
        if (CheckButtonCombo(OC::CONTROL_BUTTON_B | OC::CONTROL_BUTTON_Y)) {
            config_page = INPUT_SETTINGS;
            config_cursor = TRIGMAP1;
            return true;
        }

        // cancel clock view
        if (clock_overlay) {
          clock_overlay = false;
          return true;
        }

        // cancel preset select or config screens
        if (config_page || preset_cursor) {
          preset_cursor = 0;
          config_page = HIDE_CONFIG;
          HS::popup_tick = 0;
          return true;
        }

        // cancel other view layers
        if (view_state != APPLETS && view_state != APPLET_FULLSCREEN && view_state != OVERVIEW) {
          view_state = APPLETS;
          return true;
        }

        // A/B/X/Y buttons becomes aux button while editing a param
        if (SlotIsVisible(slot) && active_applet[slot]->EditMode()) {
          active_applet[slot]->AuxButton();
          return true;
        }

        return false;
    }

    void DelegateEncoderMovement(const UI::Event &event) {
        int increment = event.value;
        if (event.mask & (OC::CONTROL_BUTTON_L | OC::CONTROL_BUTTON_R)) {
          // push-and-turn for coarse adjustments
          // XXX: hopefully nothing breaks if event.value is larger than 1 or -1...
          OC::ui.SetButtonIgnoreMask();
          increment *= 10;
        }
        int h = (event.control == OC::CONTROL_ENCODER_L) ? LEFT_HEMISPHERE : RIGHT_HEMISPHERE;
        HEM_SIDE slot = HEM_SIDE(view_slot[h]*2 + h);
        if (HS::q_edit) {
          HS::QEditEncoderMove(h, increment);
          return;
        }
        if (HS::midi_edit) {
          HS::MEditEncoderMove(h, increment);
          return;
        }

        if (clock_overlay) {
          if (h == LEFT_HEMISPHERE)
            ClockSetup_instance.OnLeftEncoderMove(increment);
          else
            ClockSetup_instance.OnEncoderMove(increment);
          return;
        }

        if (config_page > HIDE_CONFIG || preset_cursor) {
            ConfigEncoderAction(h, increment);
            return;
        }
        if (view_state == AUDIO_SETUP) {
          audio_app.HandleEncoderEvent(event);
          return;
        }

        if (view_state == OVERVIEW) {
          // TODO:
          return;
        }

        if (view_state == APPLET_FULLSCREEN) {
            if (select_mode == zoom_slot)
              ChangeApplet(zoom_slot, increment);
            else if (h == LEFT_HEMISPHERE && !isEditing)
              zoom_cursor = (event.value > 0)? 0 : -1;
            else if (zoom_cursor < 0)
              active_applet[zoom_slot]->OnEncoderMove(increment);
            else if (isEditing) { // enc changes value
              switch (zoom_cursor)
              {
                case 1:
                case 2:
                {
                  const int chan = zoom_slot*2 + zoom_cursor - 1;
                  if (h == LEFT_HEMISPHERE) {
                    clock_m.SetMultiply(clock_m.GetMultiply(chan) + increment, chan);
                  } else if (!EditSelectedInputMap(increment))
                    HS::trigmap[chan].ChangeSource(increment);
                  break;
                }
                case 3:
                case 4:
                  if (!EditSelectedInputMap(increment))
                    HS::cvmap[zoom_slot*2 + zoom_cursor - 3].ChangeSource(increment);
                  break;
                case 5:
                case 6:
                  if (h == LEFT_HEMISPHERE)
                    HS::frame.NudgeAtten(zoom_slot*2 + zoom_cursor - 5, increment);
                  else
                    HS::frame.NudgeSlew(zoom_slot*2 + zoom_cursor - 5, increment);
                  break;
                default:
                  isEditing = false;
                  break;
              }
            } else { // enc moves cursor
              zoom_cursor = constrain(zoom_cursor + increment, 0, 6);
              ResetCursor();
            }
        } else if (event.mask & (OC::CONTROL_BUTTON_X | OC::CONTROL_BUTTON_Y)) {
            // hold down X or Y to change applet with encoder
            ChangeApplet(slot, increment);
            SetApplet(slot, next_applet_index[slot]);
        } else {
            active_applet[slot]->OnEncoderMove(increment);
        }
    }

    void SetConfigPageFromCursor() {
      if (config_cursor <= RANDOMIZE_PRESET) config_page = LOADSAVE_POPUP;
      else if (config_cursor < TRIGMAP1) config_page = CONFIG_SETTINGS;
      else if (config_cursor < QUANT1) config_page = INPUT_SETTINGS;
      else if (config_cursor < MIDIMAP1) config_page = QUANTIZER_SETTINGS;
      else if (config_cursor < SHOWHIDELIST) config_page = MIDI_MAPS_PAGE;
      else config_page = LAST_PAGE;
    }
    void ToggleConfigMenu() {
      if (config_page) {
        config_page = HIDE_CONFIG;
      } else {
        SetConfigPageFromCursor();
      }
    }
    void ShowPresetSelector() {
      config_cursor = LOAD_PRESET;
      preset_cursor = preset_id + 1;
    }

    // this toggles the view on a given side
    void SwapViewSlot(int h) {
      // h should be 0 or 1 (left or right)
      //h %= 2;

      view_slot[h] = 1 - view_slot[h];
      // also switch fullscreen to corresponding side/slot
      zoom_slot = HEM_SIDE(view_slot[h]*2 + h);
    }

    bool SlotIsVisible(HEM_SIDE h) {
      if (view_state == APPLET_FULLSCREEN)
        return zoom_slot == h;

      return (view_slot[h % 2] == h / 2);
    }
    // this brings a specific applet into view on the appropriate side
    void SwitchToSlot(HEM_SIDE h) {
      view_slot[h % 2] = h / 2;
      zoom_slot = h;
    }

    void ExitFullScreen() {
      view_state = APPLETS;
      isEditing = false;
      ClearEditInputMap();
      select_mode = -1;
    }
    void SetFullScreen(HEM_SIDE hemisphere) {
      SwitchToSlot(hemisphere);
      view_state = APPLET_FULLSCREEN;
      isEditing = false;
      ClearEditInputMap();
      select_mode = -1;
    }
    void ToggleFullScreen() {
      view_state = (view_state == APPLET_FULLSCREEN) ? APPLETS : APPLET_FULLSCREEN;
    }

protected:
    bool CheckButtonCombo(uint16_t combo) {
        return mask == combo && mask != last_mask;
    }

private:
    char bank_filename[16] = "BANK_000.DAT";
    uint8_t bank_num = 0;
    int queued_preset = -1;
    int preset_cursor = 0;
    HemisphereApplet *active_applet[4]; // Pointers to actual applets
    int active_applet_index[4]; // Indexes to applets
                      // Left side: 0,2
                      // Right side: 1,3
    int next_applet_index[4]; // queued from UI thread, handled by Controller
    uint64_t clock_data, global_data, applet_data[4]; // cache of applet data
    bool view_slot[2] = {0, 0}; // Two applets on each side, only one visible at a time
    int config_cursor = LOAD_PRESET;

    int select_mode = -1;
    HEM_SIDE zoom_slot; // Which of the hemispheres (if any) is in fullscreen/help mode
    int zoom_cursor; // 0=select; 1,2=trigmap; 3,4=cvmap; 5,6=outmode
    uint32_t click_tick; // Measure time between clicks for double-click
    int first_click; // The first button pushed of a double-click set, to see if the same one is pressed

    // Button combos can cause multiple triggers if the buttons are pressed
    // close enough together. Each press will have its own event with both
    // button marked in the mask. So, we track mask history to ensure button
    // state has actually changed between events to register a combo.
    uint16_t mask = 0;
    uint16_t last_mask = 0;

    elapsedMicros timeout = 0;

    // State machine
    enum QuadrantsView {
      APPLETS,
      APPLET_FULLSCREEN,
      OVERVIEW,
      //CONFIG_MENU,
      //PRESET_PICKER,
      //CLOCK_SETUP,
      AUDIO_SETUP,
    };
    QuadrantsView view_state = APPLETS;
    bool clock_overlay = false;

    enum QuadrantsConfigPage {
      HIDE_CONFIG,
      LOADSAVE_POPUP,
      CONFIG_SETTINGS,
      INPUT_SETTINGS,
      QUANTIZER_SETTINGS,
      MIDI_MAPS_PAGE,
      SHOWHIDE_APPLETS,

      LAST_PAGE = SHOWHIDE_APPLETS
    };
    int config_page = HIDE_CONFIG;

    enum QuadrantsConfigCursor {
        DELETE_PRESET,
        LOAD_PRESET, SAVE_PRESET,
        AUTO_SAVE,
        RANDOMIZE_PRESET, // past this point goes full screen

        // General Settings
        TRIG_LENGTH,
        SCREENSAVER_MODE,
        CURSOR_MODE,
        PRESET_JUMP_TRIG,
        MIDI_BEND_RANGE,
        MIDI_PC_CHANNEL,
        AUTO_MIDI,
        MIDI_POLY_MODE,
        MIDI_THRU_SERIAL,
        MIDI_THRU_USBDEV,
        MIDI_THRU_USBHOST1,
        MIDI_THRU_USBHOST2,
        MIDI_CLKRX_SERIAL,
        MIDI_CLKRX_USBDEV,
        MIDI_CLKRX_USBHOST1,
        MIDI_CLKRX_USBHOST2,
        MIDI_CLKTX_SERIAL,
        MIDI_CLKTX_USBDEV,
        MIDI_CLKTX_USBHOST1,
        MIDI_CLKTX_USBHOST2,
        MIDI_MSGRX_SERIAL,
        MIDI_MSGRX_USBDEV,
        MIDI_MSGRX_USBHOST1,
        MIDI_MSGRX_USBHOST2,
        MIDI_MSGTX_SERIAL,
        MIDI_MSGTX_USBDEV,
        MIDI_MSGTX_USBHOST1,
        MIDI_MSGTX_USBHOST2,

        // Input Remapping
        TRIGMAP1, TRIGMAP2, TRIGMAP3, TRIGMAP4,
        CVMAP1, CVMAP2, CVMAP3, CVMAP4,
        TRIGMAP5, TRIGMAP6, TRIGMAP7, TRIGMAP8,
        CVMAP5, CVMAP6, CVMAP7, CVMAP8,

        // Global Quantizers: 4x(Scale, Root, Octave, Mask?)
        QUANT1, QUANT2, QUANT3, QUANT4,
        QUANT5, QUANT6, QUANT7, QUANT8,

        // MIDI Mappings
        MIDIMAP1, MIDIMAP2, MIDIMAP3, MIDIMAP4,
        MIDIMAP5, MIDIMAP6, MIDIMAP7, MIDIMAP8,
        MIDIMAP9, MIDIMAP10, MIDIMAP11, MIDIMAP12,
        MIDIMAP13, MIDIMAP14, MIDIMAP15, MIDIMAP16,
        MIDIMAP17, MIDIMAP18, MIDIMAP19, MIDIMAP20,
        MIDIMAP21, MIDIMAP22, MIDIMAP23, MIDIMAP24,
        MIDIMAP25, MIDIMAP26, MIDIMAP27, MIDIMAP28,
        MIDIMAP29, MIDIMAP30, MIDIMAP31, MIDIMAP32,

        // Applet visibility (dummy position)
        SHOWHIDELIST,

        MAX_CURSOR = MIDIMAP32
    };

    void Randomizer(bool audio = false) {
      static uint8_t cycle = 0;

      if (audio) audio_app.ReInit();

      switch (cycle) {
        case 0:
          // randomize all applets
          for (int ch = 0; ch < APPLET_SLOTS; ++ch) {
            size_t index = random(HEMISPHERE_AVAILABLE_APPLETS);
            SetApplet(HEM_SIDE(ch), index);
#ifdef PEWPEWPEW
            // load random data !!!
            // this will expose critical bugs in data validation ;)
            HS::get_applet(index, ch)->OnDataReceive(uint64_t(random()) << 32 | (uint64_t)random());
#endif
          }
          PokePopup(HS::MESSAGE_POPUP, "Reset/Random");
          break;
        case 3:
          // oops all LFOs
          SetApplet(HEM_SIDE(0), HS::get_applet_index_by_id(7)); // Ebb&LFO
          SetApplet(HEM_SIDE(1), HS::get_applet_index_by_id(89)); // Relabi
          SetApplet(HEM_SIDE(2), HS::get_applet_index_by_id(49)); // VectorLFO
          SetApplet(HEM_SIDE(3), HS::get_applet_index_by_id(49)); // VectorLFO
          PokePopup(HS::MESSAGE_POPUP, "LFOs!");
          break;
        case 2:
          // oops all envelopes
          SetApplet(HEM_SIDE(0), HS::get_applet_index_by_id(8)); // ADSR
          SetApplet(HEM_SIDE(1), HS::get_applet_index_by_id(52)); // VectorEG
          SetApplet(HEM_SIDE(2), HS::get_applet_index_by_id(8)); // ADSR
          SetApplet(HEM_SIDE(3), HS::get_applet_index_by_id(52)); // VectorEG
          PokePopup(HS::MESSAGE_POPUP, "Envelopes!");
          break;
        case 1:
          // oops all quantizers
          SetApplet(HEM_SIDE(0), HS::get_applet_index_by_id(9)); // DualQuant
          SetApplet(HEM_SIDE(1), HS::get_applet_index_by_id(9)); // DualQuant
          SetApplet(HEM_SIDE(2), HS::get_applet_index_by_id(46)); // Squanch
          SetApplet(HEM_SIDE(3), HS::get_applet_index_by_id(46)); // Squanch
          PokePopup(HS::MESSAGE_POPUP, "Quantizers!");
          break;
        case 4:
          // oops all sequencers
          SetApplet(HEM_SIDE(0), HS::get_applet_index_by_id(14)); // SeqX
          SetApplet(HEM_SIDE(1), HS::get_applet_index_by_id(60)); // TB-3PO
          SetApplet(HEM_SIDE(2), HS::get_applet_index_by_id(58)); // Shredder
          SetApplet(HEM_SIDE(3), HS::get_applet_index_by_id(18)); // TwoRings
          PokePopup(HS::MESSAGE_POPUP, "Sequencers!");
          break;
        case 5:
          // oops all clocks
          SetApplet(HEM_SIDE(0), HS::get_applet_index_by_id(6)); // ClockDiv
          SetApplet(HEM_SIDE(1), HS::get_applet_index_by_id(50)); // Metronome
          SetApplet(HEM_SIDE(2), HS::get_applet_index_by_id(72)); // PolyDiv
          SetApplet(HEM_SIDE(3), HS::get_applet_index_by_id(15)); // EuclidX
          PokePopup(HS::MESSAGE_POPUP, "Clocks!");
          break;
      }

      ++cycle %= 6;
    }

    void ConfigEncoderAction(int h, int dir) {
        if (!isEditing && !preset_cursor) {
          if (h == 0) { // change pages
            config_page += dir;
            config_page = constrain(config_page, LOADSAVE_POPUP, LAST_PAGE);

            const int cursorpos[] = { 0, LOAD_PRESET, TRIG_LENGTH, TRIGMAP1, QUANT1, MIDIMAP1, SHOWHIDELIST };
            config_cursor = cursorpos[config_page];
          } else if (config_page == SHOWHIDE_APPLETS) {
            showhide_cursor.Scroll(dir);
          } else { // move cursor
            config_cursor = constrain(config_cursor + dir, LOAD_PRESET, MAX_CURSOR);

            SetConfigPageFromCursor();
          }
          ResetCursor();
          if (config_cursor > RANDOMIZE_PRESET) HS::popup_tick = 0;
          return;
        }

        if (preset_cursor && isEditing) {
          // bank select
          bank_num = constrain(bank_num + dir, 0, 99);
          return;
        }

        switch (config_cursor) {
        case TRIGMAP1:
        case TRIGMAP2:
        case TRIGMAP3:
        case TRIGMAP4:
            if (!EditSelectedInputMap(dir))
              HS::trigmap[config_cursor-TRIGMAP1].ChangeSource(dir);
            break;
        case CVMAP1:
        case CVMAP2:
        case CVMAP3:
        case CVMAP4:
            if (!EditSelectedInputMap(dir))
              HS::cvmap[config_cursor-CVMAP1].ChangeSource(dir);
            break;
        case TRIGMAP5:
        case TRIGMAP6:
        case TRIGMAP7:
        case TRIGMAP8:
            if (!EditSelectedInputMap(dir))
              HS::trigmap[config_cursor-TRIGMAP5 + 4].ChangeSource(dir);
            break;
        case CVMAP5:
        case CVMAP6:
        case CVMAP7:
        case CVMAP8:
            if (!EditSelectedInputMap(dir))
              HS::cvmap[config_cursor-CVMAP5 + 4].ChangeSource(dir);
            break;

        case TRIG_LENGTH:
            HS::trig_length = (uint32_t) constrain( int(HS::trig_length + dir), 1, 127);
            break;
        case PRESET_JUMP_TRIG:
            if (!EditSelectedInputMap(dir))
              jump_trig_.ChangeSource(dir);
            break;
        case MIDI_BEND_RANGE:
            HS::frame.MIDIState.bend_range =
              constrain(HS::frame.MIDIState.bend_range + dir, 0, 36);
            break;
        case MIDI_PC_CHANNEL:
            HS::frame.MIDIState.pc_channel =
              constrain(HS::frame.MIDIState.pc_channel + dir, 0, 17);
            break;
        case MIDI_POLY_MODE:
            HS::frame.MIDIState.poly_mode =
              constrain(HS::frame.MIDIState.poly_mode + dir, 0, HS::MIDIPolyMode::POLY_LAST);
            break;
        case SCREENSAVER_MODE:
            HS::screensaver_mode = constrain(HS::screensaver_mode + dir, 0, SCREENSAVER_MODE_COUNT - 1);
            break;

        case DELETE_PRESET:
        case LOAD_PRESET:
        case SAVE_PRESET:
            if (h == 0) {
              config_cursor = constrain(config_cursor + dir, DELETE_PRESET, SAVE_PRESET);
            } else {
              preset_cursor = constrain(preset_cursor + dir, 1, QUAD_PRESET_COUNT);
            }
            break;
        }
    }
    void ConfigButtonPush(int h) {
      static uint8_t old_bank_num = 0xff;
        if (preset_cursor) {
            // left enc toggles bank select
            if (0 == h || isEditing) {
              isEditing = !isEditing;
              if (!isEditing) {
                if (old_bank_num != bank_num) SetBank(bank_num);
              } else
                old_bank_num = bank_num;

              return;
            }

            // Save or Load on button push
            switch (config_cursor) {
              case DELETE_PRESET:
                DeletePreset(preset_cursor - 1);
                return; // don't close menu
                break;
              case SAVE_PRESET:
                StoreToPreset(preset_cursor - 1);
                break;
              case LOAD_PRESET:
                QueuePresetLoad(preset_cursor - 1);
                break;
            }

            preset_cursor = 0; // deactivate preset selection
            config_page = HIDE_CONFIG;
            view_state = APPLETS;
            isEditing = false;
            return;
        }

        switch (config_cursor) {
          case RANDOMIZE_PRESET:
            // reset input mappings to defaults
            HS::Init();
            Randomizer(true);
            jump_trig_.Clear();
            config_page = HIDE_CONFIG;
            break;

          case DELETE_PRESET:
          case SAVE_PRESET:
          case LOAD_PRESET:
            preset_cursor = preset_id + 1;
            break;

          case AUTO_SAVE:
            HS::auto_save_enabled = !HS::auto_save_enabled;
            break;

          case QUANT1:
          case QUANT2:
          case QUANT3:
          case QUANT4:
          case QUANT5:
          case QUANT6:
          case QUANT7:
          case QUANT8:
            HS::QuantizerEdit(config_cursor - QUANT1);
            break;

          case CVMAP1:
          case CVMAP2:
          case CVMAP3:
          case CVMAP4:
          case CVMAP5:
          case CVMAP6:
          case CVMAP7:
          case CVMAP8:
            if (CheckEditInputMapPress(
                  config_cursor,
                  IndexedInput(CVMAP1, cvmap[0]),
                  IndexedInput(CVMAP2, cvmap[1]),
                  IndexedInput(CVMAP3, cvmap[2]),
                  IndexedInput(CVMAP4, cvmap[3]),
                  IndexedInput(CVMAP5, cvmap[4]),
                  IndexedInput(CVMAP6, cvmap[5]),
                  IndexedInput(CVMAP7, cvmap[6]),
                  IndexedInput(CVMAP8, cvmap[7])
                ))
              break;
          case TRIGMAP1:
          case TRIGMAP2:
          case TRIGMAP3:
          case TRIGMAP4:
          case TRIGMAP5:
          case TRIGMAP6:
          case TRIGMAP7:
          case TRIGMAP8:
            if (CheckEditInputMapPress(
                  config_cursor,
                  IndexedInput(TRIGMAP1, trigmap[0]),
                  IndexedInput(TRIGMAP2, trigmap[1]),
                  IndexedInput(TRIGMAP3, trigmap[2]),
                  IndexedInput(TRIGMAP4, trigmap[3]),
                  IndexedInput(TRIGMAP5, trigmap[4]),
                  IndexedInput(TRIGMAP6, trigmap[5]),
                  IndexedInput(TRIGMAP7, trigmap[6]),
                  IndexedInput(TRIGMAP8, trigmap[7])
                ))
              break;
          case TRIG_LENGTH:
          case MIDI_BEND_RANGE:
          case MIDI_PC_CHANNEL:
          case MIDI_POLY_MODE:
          case SCREENSAVER_MODE:
            isEditing = !isEditing;
            break;

          case PRESET_JUMP_TRIG:
            if (!CheckEditInputMapPress(
                  config_cursor, IndexedInput(PRESET_JUMP_TRIG, jump_trig_)
                ))
              isEditing ^= 1;
            break;

          case CURSOR_MODE:
            HS::cursor_wrap = !HS::cursor_wrap;
            break;

          case AUTO_MIDI:
            HS::frame.autoMIDIOut = !HS::frame.autoMIDIOut;
            break;

          case MIDI_THRU_SERIAL:
            HS::midi_thru_disable ^= mMaskSerial;
            if (midi_thru_disable & mMaskSerial)
              MIDI1.turnThruOff();
            else
              MIDI1.turnThruOn();
            break;
          case MIDI_THRU_USBDEV:
            HS::midi_thru_disable ^= mMaskUSBDev;
            break;
          case MIDI_THRU_USBHOST1:
            HS::midi_thru_disable ^= mMaskUSBHost;
            break;
          case MIDI_THRU_USBHOST2:
            HS::midi_thru_disable ^= mMaskUSBHost2;
            break;

          case MIDI_CLKRX_SERIAL:
            HS::midi_clkrx_disable ^= mMaskSerial;
            break;
          case MIDI_CLKRX_USBDEV:
            HS::midi_clkrx_disable ^= mMaskUSBDev;
            break;
          case MIDI_CLKRX_USBHOST1:
            HS::midi_clkrx_disable ^= mMaskUSBHost;
            break;
          case MIDI_CLKRX_USBHOST2:
            HS::midi_clkrx_disable ^= mMaskUSBHost2;
            break;

          case MIDI_CLKTX_SERIAL:
            HS::midi_clktx_disable ^= mMaskSerial;
            break;
          case MIDI_CLKTX_USBDEV:
            HS::midi_clktx_disable ^= mMaskUSBDev;
            break;
          case MIDI_CLKTX_USBHOST1:
            HS::midi_clktx_disable ^= mMaskUSBHost;
            break;
          case MIDI_CLKTX_USBHOST2:
            HS::midi_clktx_disable ^= mMaskUSBHost2;
            break;

          case MIDI_MSGRX_SERIAL:
            HS::midi_msgrx_disable ^= mMaskSerial;
            break;
          case MIDI_MSGRX_USBDEV:
            HS::midi_msgrx_disable ^= mMaskUSBDev;
            break;
          case MIDI_MSGRX_USBHOST1:
            HS::midi_msgrx_disable ^= mMaskUSBHost;
            break;
          case MIDI_MSGRX_USBHOST2:
            HS::midi_msgrx_disable ^= mMaskUSBHost2;
            break;

          case MIDI_MSGTX_SERIAL:
            HS::midi_msgtx_disable ^= mMaskSerial;
            break;
          case MIDI_MSGTX_USBDEV:
            HS::midi_msgtx_disable ^= mMaskUSBDev;
            break;
          case MIDI_MSGTX_USBHOST1:
            HS::midi_msgtx_disable ^= mMaskUSBHost;
            break;
          case MIDI_MSGTX_USBHOST2:
            HS::midi_msgtx_disable ^= mMaskUSBHost2;
            break;

          case SHOWHIDELIST:
            if (h == 0) // left encoder inverts selection
            {
              HS::hidden_applets[0] = ~HS::hidden_applets[0];
              HS::hidden_applets[1] = ~HS::hidden_applets[1];
            } else // right encoder toggles current
              HS::showhide_applet(showhide_cursor.cursor_pos());
            break;
          default: {
            // I'm not going to paste 32 different MIDI cursor positions so it's just the default :P
            int midx = constrain(config_cursor - MIDIMAP1, 0, 31);
            HS::MidiMapEdit(midx);
            break;
          }
        }
    }

    void DrawInputMappings() const {
        gfxHeader("<  Input Mapping    >");
        gfxIcon(25, 13, TR_ICON); gfxIcon(89, 13, TR_ICON);
        gfxIcon(25, 26, CV_ICON); gfxIcon(89, 26, CV_ICON);
        gfxIcon(25, 39, TR_ICON); gfxIcon(89, 39, TR_ICON);
        gfxIcon(25, 52, CV_ICON); gfxIcon(89, 52, CV_ICON);

        for (int ch=0; ch<4; ++ch) {
          // Physical trigger input mappings
          // Physical CV input mappings
          // Top 2 applets
          gfxPrint(4 + ch*32, 15, HS::trigmap[ch].InputName() );
          gfxPrint(4 + ch*32, 28, HS::cvmap[ch].InputName() );

          // Bottom 2 applets
          gfxPrint(4 + ch*32, 41, HS::trigmap[ch + 4].InputName() );
          gfxPrint(4 + ch*32, 54, HS::cvmap[ch + 4].InputName() );
        }

        gfxDottedLine(63, 11, 63, 63); // vert
        gfxDottedLine(0, 38, 127, 38); // horiz

        // Cursor location is within a 4x4 grid
        const int cur_x = (config_cursor-TRIGMAP1) % 4;
        const int cur_y = (config_cursor-TRIGMAP1) / 4;

        gfxCursor(4 + 32*cur_x, 23 + 13*cur_y, 19);

        gfxDisplayInputMapEditor();
    }
    void DrawQuantizerConfig() const {
        gfxHeader("<  Quantizer Setup  >");

        for (int ch=0; ch<4; ++ch) {
          const int x = 8 + ch*32;

          // 1-4 on top
          gfxPrint(x, 15, "Q");
          gfxPrint(ch + 1);
          gfxLine(x, 23, x + 14, 23);
          gfxLine(x + 14, 13, x + 14, 23);

          const bool upper = config_cursor < QUANT5;
          const int ch_view = upper ? ch : ch + 4;
          auto &q = q_engine[ch_view];

          gfxIcon(x + 3, upper? 25 : 45, upper? UP_BTN_ICON : DOWN_BTN_ICON);

          // Scale
          gfxPrint(x - 3, 30, OC::scale_names_short[ q.scale ]);

          // Root Note + Octave
          gfxPrint(x - 3, 40, OC::Strings::note_names[ q.root_note ]);
          if (q.octave >= 0) gfxPrint("+");
          gfxPrint(q.octave);

          // 5-8 on bottom
          gfxPrint(x, 55, "Q");
          gfxPrint(ch + 5);
          gfxLine(x, 53, x + 14, 53);
          gfxLine(x + 14, 53, x + 14, 63);
        }

        switch (config_cursor) {
        case QUANT1:
        case QUANT2:
        case QUANT3:
        case QUANT4:
          gfxIcon( 32*(config_cursor-QUANT1), 15, RIGHT_ICON);
          break;
        case QUANT5:
        case QUANT6:
        case QUANT7:
        case QUANT8:
          gfxIcon( 32*(config_cursor-QUANT5), 55, RIGHT_ICON);
          break;
        }
    }

    void DrawConfigMenu() const {
        // --- Config Selection
        gfxHeader("< General Settings  >");
        const int NUM_ROWS = TRIGMAP1 - TRIG_LENGTH; // lol weird
        const int SHOW_ROWS = 6; // originally 5
        const int ROW_HEIGHT = 8; // originally 10

        int scroll_top = config_cursor - TRIG_LENGTH - 2;
        CONSTRAIN(scroll_top, 0, NUM_ROWS - SHOW_ROWS);

        // Draw 6 visible rows from scroll_top (scroll_top is a row index)
        for (int i = 0; i < SHOW_ROWS; ++i) {
            int row = scroll_top + i;
            if (row >= NUM_ROWS) break;
            HS::DrawConfigRow(
              row,
              15 + i * ROW_HEIGHT,
              (config_cursor - TRIG_LENGTH) == row,
              EditMode()
            );
        }

        // Scroll arrows
        if (scroll_top > 0)
            gfxIcon(121, 14, UP_ICON);
        if (scroll_top + SHOW_ROWS < NUM_ROWS)
            gfxIcon(121, 56, DOWN_ICON);

        gfxDisplayInputMapEditor();
    }

    bool isValidPreset(int id) const {
      uint64_t data;
      return PhzConfig::getValue(id << 11 | APPLET_METADATA_KEY, data);
    }
    HemisphereApplet* GetApplet(int id, size_t h) const {
        uint64_t data = 0;
        PhzConfig::getValue(id << 11 | APPLET_METADATA_KEY, data);
        int idx = HS::get_applet_index_by_id( Unpack(data, PackLocation{h*8, 8}) );
        return HS::get_applet(idx, HEM_SIDE(h));
    }
    void DrawPresetSelector() const {
        const char * const hdrtxt[] = { "DEL!", "Load", "Save", "???" };
        gfxHeader(hdrtxt[config_cursor]);
        gfxPrint(30, 1, "Preset: Bank# ");
        gfxPrint(bank_num);
        if (!SDcard_Ready) gfxInvert(78, 0, 30, 9);

        // bank select cursor
        if (isEditing) {
          gfxIcon(114, 11, UP_ICON);
          return;
        }

        // Draw preset list
        gfxDottedLine(16, 11, 16, 63);
        int y = 5 + constrain(preset_cursor,1,5)*10;
        gfxIcon(0, y, RIGHT_ICON);
        const int top = constrain(preset_cursor - 4, 1, QUAD_PRESET_COUNT) - 1;
        y = 15;
        for (int i = top; i < QUAD_PRESET_COUNT && i < top + 5; ++i)
        {
            if (i == preset_id)
              gfxIcon(8, y, ZAP_ICON);
            else
              gfxPrint(8, y, OC::Strings::capital_letters[i]);

            if (!isValidPreset(i))
                gfxPrint(18, y, "(empty)");
            else {
                gfxIcon(18, y, GetApplet(i, LEFT_HEMISPHERE)->applet_icon());
                gfxPrint(26, y, GetApplet(i, LEFT_HEMISPHERE)->applet_name());
                gfxPrint(", ");
                gfxPrint(GetApplet(i, RIGHT_HEMISPHERE)->applet_name());
                gfxIcon(120, y, GetApplet(i, RIGHT_HEMISPHERE)->applet_icon(), true);
            }

            y += 10;
        }
    }
};

void QuadrantSysExHandler() {
  // TODO
}

////////////////////////////////////////////////////////////////////////////////
//// O_C App Functions
////////////////////////////////////////////////////////////////////////////////

// App stubs
void AppQuadrants::Init() {
    BaseStart();
}

size_t AppQuadrants::SaveAppData(util::StreamBufferWriter &stream_buffer) const {
  return 0;
}
size_t AppQuadrants::RestoreAppData(util::StreamBufferReader &stream_buffer) {
  return 0;
}

void AppQuadrants::Process(OC::IOFrame *ioframe) {
  BaseController(ioframe);
}
void AppQuadrants::GetIOConfig(OC::IOConfig &ioconfig) const
{
  using namespace OC;
  ioconfig.digital_inputs[DIGITAL_INPUT_1].set("TR1");
  ioconfig.digital_inputs[DIGITAL_INPUT_2].set("TR2");
  ioconfig.digital_inputs[DIGITAL_INPUT_3].set("TR3");
  ioconfig.digital_inputs[DIGITAL_INPUT_4].set("TR4");

  ioconfig.cv[0].set("CV1");
  ioconfig.cv[1].set("CV2");
  ioconfig.cv[2].set("CV3");
  ioconfig.cv[3].set("CV4");
  ioconfig.cv[4].set("CV5");
  ioconfig.cv[5].set("CV6");
  ioconfig.cv[6].set("CV7");
  ioconfig.cv[7].set("CV8");

  ioconfig.outputs[0].set("Out A", OUTPUT_MODE_PITCH);
  ioconfig.outputs[1].set("Out B", OUTPUT_MODE_PITCH);
  ioconfig.outputs[2].set("Out C", OUTPUT_MODE_PITCH);
  ioconfig.outputs[3].set("Out D", OUTPUT_MODE_PITCH);
  ioconfig.outputs[4].set("Out E", OUTPUT_MODE_PITCH);
  ioconfig.outputs[5].set("Out F", OUTPUT_MODE_PITCH);
  ioconfig.outputs[6].set("Out G", OUTPUT_MODE_PITCH);
  ioconfig.outputs[7].set("Out H", OUTPUT_MODE_PITCH);
}

void AppQuadrants::HandleAppEvent(OC::AppEvent event) {
    switch (event) {
    case OC::APP_EVENT_RESUME:
        Resume();
        break;

    case OC::APP_EVENT_SCREENSAVER_ON:
    case OC::APP_EVENT_SUSPEND:
        Suspend();
        break;

    default: break;
    }
}

void AppQuadrants::Loop() {
    mainloop();
    audio_app.mainloop();
}

FLASHMEM
void AppQuadrants::DrawMenu() const {
    View();
}

void AppQuadrants::DrawScreensaver() const {
    switch (HS::screensaver_mode) {
    case SCREEN_ZIPS:
    case SCREEN_STARS:
    case SCREEN_ZAPS:
        ZapScreensaver(screensaver_mode - SCREEN_ZAPS);
        break;
    case SCREEN_SCOPE: // output scope
        OC::scope_render();
        break;
    case SCREEN_METERS:
        BaseScreensaver(true); // show note names
        break;
    case SCREEN_BEATS:
        BeatCounterScreensaver();
        break;
    default: break; // blank screen
    }
}
void AppQuadrants::DrawDebugInfo() const {
  // TODO:
}

FLASHMEM
void AppQuadrants::HandleButtonEvent(const UI::Event &event) {
    last_mask = mask;
    mask = event.mask;
    /*SERIAL_PRINTLN(*/
    /*  "mask=%d type=%d value=%d control=%d last_mask=%d",*/
    /*  event.mask, event.type, event.value, event.control,*/
    /*  last_mask*/
    /*);*/

    if (AUDIO_SETUP == view_state && !clock_overlay && !preset_cursor) {
      if (CheckButtonCombo(OC::CONTROL_BUTTON_A | OC::CONTROL_BUTTON_B)) {
        clock_overlay = true;
        //view_state = APPLETS;
        return;
      }
      // dual press A+X for Load Preset
      if (CheckButtonCombo(OC::CONTROL_BUTTON_A | OC::CONTROL_BUTTON_X)) {
        ShowPresetSelector();
        return;
      }
      if ((event.control == OC::CONTROL_BUTTON_L
           || event.control == OC::CONTROL_BUTTON_R)) {
        audio_app.HandleEncoderButtonEvent(event);
        return;
      }
      if (event.control == OC::CONTROL_BUTTON_X
          || event.control == OC::CONTROL_BUTTON_Y
          || event.control == OC::CONTROL_BUTTON_A
          || event.control == OC::CONTROL_BUTTON_B) {
        if (audio_app.HandleButtonEvent(event))
          view_state = APPLETS;
        return;
      }
    }

    if (CheckButtonCombo(OC::CONTROL_BUTTON_A | OC::CONTROL_BUTTON_Y) ||
        CheckButtonCombo(OC::CONTROL_BUTTON_X | OC::CONTROL_BUTTON_B)) {
      view_state = OVERVIEW;
      OC::ui.SetButtonIgnoreMask();
      return;
    }

    switch (event.type) {
    case UI::EVENT_BUTTON_DOWN:

      // Quantizer popup editor intercepts everything on-press
      if (HS::q_edit) {
        if (event.control == OC::CONTROL_BUTTON_B)
          HS::NudgeOctave(HS::qview, 1);
        else if (event.control == OC::CONTROL_BUTTON_A)
          HS::NudgeOctave(HS::qview, -1);
        else {
          HS::q_edit = 0;
          HS::popup_tick = 0;
          select_mode = -1;
        }

        OC::ui.SetButtonIgnoreMask();
        break;
      }
      if (HS::midi_edit) {
        if (event.control == OC::CONTROL_BUTTON_A) {
          mview = constrain(mview - 1, 0, MIDIMAP_MAX-1);
          config_cursor = MIDIMAP1 + mview;
        } else if (event.control == OC::CONTROL_BUTTON_B) {
          mview = constrain(mview + 1, 0, MIDIMAP_MAX-1);
          config_cursor = MIDIMAP1 + mview;
        } else if (event.control == OC::CONTROL_BUTTON_Z) {
          frame.MIDIState.mapping[mview].AutoLearn();
        } else {
          HS::midi_edit = 0;
          HS::popup_tick = 0;
          select_mode = -1;
        }
        OC::ui.SetButtonIgnoreMask();
        break;
      }

      if (event.control == OC::CONTROL_BUTTON_L ||
          event.control == OC::CONTROL_BUTTON_R)
      {
          if (clock_overlay) {
            ClockSetup_instance.OnButtonPress();
          } else
            DelegateEncoderPush(event);
          // ignore long-press to prevent Main Menu >:)
          //OC::ui.SetButtonIgnoreMask();
      } else if (
        event.control == OC::CONTROL_BUTTON_A ||
        event.control == OC::CONTROL_BUTTON_B ||
        event.control == OC::CONTROL_BUTTON_X ||
        event.control == OC::CONTROL_BUTTON_Y)
      {
          if (CheckButtonCombos(event)) {
            select_mode = -1;
            isEditing = false;
            ClearEditInputMap();
            OC::ui.SetButtonIgnoreMask(); // ignore release and long-press
          } else {
            HEM_SIDE slot = ButtonToSlot(event);
            if (OC::CORE::ticks - click_tick < HEMISPHERE_DOUBLE_CLICK_TIME
                && (slot == first_click))
            {
                // This is a double-click on one button.
                SetFullScreen(slot);
                click_tick = 0;
                OC::ui.SetButtonIgnoreMask(); // ignore button release
                return;
            }

            // -- Single click
            // If a help screen is already selected, and the button is for
            // the opposite one, go to the other help screen
            if (view_state == APPLET_FULLSCREEN) {
                if (zoom_slot != slot) SetFullScreen(slot);
                else ExitFullScreen(); // Exit help screen if same button is clicked
                OC::ui.SetButtonIgnoreMask(); // ignore release
            }
            if (view_state == OVERVIEW) {
              // TODO: what do A/B/X/Y do in the Overview? modifiers?
            }

            // mark this single click
            click_tick = OC::CORE::ticks;
            first_click = slot;
          }
      }

      break;

    case UI::EVENT_BUTTON_PRESS:
      // A/B/X/Y switch to corresponding applet on release
      if (event.control == OC::CONTROL_BUTTON_A ||
          event.control == OC::CONTROL_BUTTON_B ||
          event.control == OC::CONTROL_BUTTON_X ||
          event.control == OC::CONTROL_BUTTON_Y)
      {
        HEM_SIDE slot = ButtonToSlot(event);
        if (view_state == APPLET_FULLSCREEN && slot == zoom_slot)
          view_state = APPLETS;

        if (view_state == OVERVIEW) {
          // jump to applet view on release
          SetFullScreen(slot);
          zoom_cursor = -1; // applet UI, not config
        } else
          SwitchToSlot(slot);
      }
      // fall thru here
    case UI::EVENT_BUTTON_LONG_RELEASE:
      // Z-button acts on release, either short or long
      if (event.control == OC::CONTROL_BUTTON_Z) {
        ToggleClockRun();
      }

      // ignore all other button release events
      break;

    case UI::EVENT_BUTTON_LONG_PRESS:
      if (event.control == OC::CONTROL_BUTTON_B) ToggleConfigMenu();
      if (event.control == OC::CONTROL_BUTTON_Z) {
        // TODO: show a menu when Z is held long enough
        // - encoder can change selected Z action
      }
      break;

    default: break;
    }
}

FLASHMEM
void AppQuadrants::HandleEncoderEvent(const UI::Event &event) {
    DelegateEncoderMovement(event);
}

FLASHMEM
void AppQuadrants::StoreToPreset(int id) {
  store_to_preset(id);
}
FLASHMEM
void AppQuadrants::LoadFromPreset(int id) {
  load_from_preset(id);
}
