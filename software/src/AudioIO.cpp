#ifdef ARDUINO_TEENSY41

#include "AudioIO.h"
#include "AudioStream.h"
#include "extern/f32/AudioStream_F32.h"
#include "extern/f32/AudioConvert_F32.h"
#include "extern/f32/AudioMixer_F32.h"
#include "extern/f32/input_i2s2_F32.h"
#include "extern/f32/output_i2s2_F32.h"
#include "Audio/AudioMixer.h"
#include "Audio/AudioPassthrough.h"
#include "Audio/USB_F32.h"
#include "PhzConfig.h"
#include "usb_desc.h"

namespace OC {
  namespace AudioIO {
    // The codec path runs float32 with 32-bit I2S frames on the wire,
    // preserving full 24-bit converter resolution. The applet bus is still
    // int16; converters bridge at the hardware boundary until applets are
    // ported to F32.
    AudioInputI2S2_F32 input_i2s;
    AudioConvert_F32toI16 conv_in[2];
    AudioPassthrough<2> input_route;
    AudioConnection_F32 conn_in_f32L{input_i2s, 0, conv_in[0], 0};
    AudioConnection_F32 conn_in_f32R{input_i2s, 1, conv_in[1], 0};
    AudioConnection conn_in_i16L{conv_in[0], 0, input_route, 0};
    AudioConnection conn_in_i16R{conv_in[1], 0, input_route, 1};

    AudioOutputI2S2_F32* output_stream = nullptr;
    AudioConvert_I16toF32* conv_out[2] = {nullptr, nullptr};
    AudioPassthrough<2> output_route;

#ifdef AUDIO_INTERFACE
#if AUDIO_SUBSLOT_SIZE == 3
    // 24-bit USB: float bridge end to end. Codec input goes to the host
    // untruncated (F32 direct); host playback reaches the codec through an
    // F32 monitor mix; the int16 applet bus sees USB-in through a facade.
    AudioInputUSB_F32 input_usb;
    AudioConvert_F32toI16 conv_usb_in[2];
    AudioPassthrough<2> usb_in_route;
    AudioConnection_F32 conn_usb_f32L{input_usb, 0, conv_usb_in[0], 0};
    AudioConnection_F32 conn_usb_f32R{input_usb, 1, conv_usb_in[1], 0};
    AudioConnection conn_usb_i16L{conv_usb_in[0], 0, usb_in_route, 0};
    AudioConnection conn_usb_i16R{conv_usb_in[1], 0, usb_in_route, 1};

    AudioOutputUSB_F32 output_usb;
    AudioConnection_F32 out_conn_usbL2{input_i2s, 0, output_usb, 2};
    AudioConnection_F32 out_conn_usbR2{input_i2s, 1, output_usb, 3};
    // engine -> host (ch 0,1) and the monitor mix are wired lazily in
    // OutputStream(), after the applet chain exists
    AudioMixer4_F32* usbmix_f32[2] = {nullptr, nullptr};
#else
    // 16-bit USB: original int16 graph
    AudioInputUSB input_usb __attribute__((init_priority(101)));
    AudioMixer<2> usbmix[2];
    AudioOutputUSB output_usb __attribute__((init_priority(101)));

    AudioConnection out_conn_usbL{output_route, 0, output_usb, 0};
    AudioConnection out_conn_usbR{output_route, 1, output_usb, 1};
    AudioConnection out_conn_usbL2{input_route, 0, output_usb, 2};
    AudioConnection out_conn_usbR2{input_route, 1, output_usb, 3};

    AudioConnection in_conn_usbL{input_usb, 2, usbmix[0], 0};
    AudioConnection in_conn_usbR{input_usb, 3, usbmix[1], 0};
    AudioConnection in_conn_mixL{output_route, 0, usbmix[0], 1};
    AudioConnection in_conn_mixR{output_route, 1, usbmix[1], 1};
#endif
#endif

    AudioConnection out_conn[2];

    AudioStream& InputStream(int interface) {
      switch (interface) {
        default:
        case 0:
          return input_route;
#ifdef AUDIO_INTERFACE
        case 1:
#if AUDIO_SUBSLOT_SIZE == 3
          return usb_in_route;
#else
          return input_usb;
#endif
#endif
      }
    }

    AudioStream& OutputStream() {
      // The output stream should be created after all other streams have been
      // created or we get an extra 3ms of latency. Hence, we initialize it
      // lazily. This is pretty hacky; if someone references this before all
      // other streams have been created it won't work. But it was the simplest
      // fix for now. The int16->F32 output converters (and the F32 monitor
      // mixers in 24-bit USB mode) are created here for the same reason: they
      // must update after the applet chain feeding them.
      if (output_stream == nullptr) {
        conv_out[0] = new AudioConvert_I16toF32();
        conv_out[1] = new AudioConvert_I16toF32();
        output_stream = new AudioOutputI2S2_F32();
#if defined(AUDIO_INTERFACE) && AUDIO_SUBSLOT_SIZE == 3
        // engine out (int16 bus) -> F32
        out_conn[0].connect(output_route, 0, *conv_out[0], 0);
        out_conn[1].connect(output_route, 1, *conv_out[1], 0);
        // engine out -> host (int16-limited until applets are F32)
        new AudioConnection_F32(*conv_out[0], 0, output_usb, 0);
        new AudioConnection_F32(*conv_out[1], 0, output_usb, 1);
        // monitor mix: host playback (ch 2,3) + engine out -> codec
        usbmix_f32[0] = new AudioMixer4_F32();
        usbmix_f32[1] = new AudioMixer4_F32();
        new AudioConnection_F32(input_usb, 2, *usbmix_f32[0], 0);
        new AudioConnection_F32(input_usb, 3, *usbmix_f32[1], 0);
        new AudioConnection_F32(*conv_out[0], 0, *usbmix_f32[0], 1);
        new AudioConnection_F32(*conv_out[1], 0, *usbmix_f32[1], 1);
        new AudioConnection_F32(*usbmix_f32[0], 0, *output_stream, 0);
        new AudioConnection_F32(*usbmix_f32[1], 0, *output_stream, 1);
#elif defined(AUDIO_INTERFACE)
        // 16-bit USB: the int16 monitor mix feeds the F32 codec output
        out_conn[0].connect(usbmix[0], 0, *conv_out[0], 0);
        out_conn[1].connect(usbmix[1], 0, *conv_out[1], 0);
        usbmix[0].gain(0, 1.0f);
        usbmix[0].gain(1, 1.0f);
        usbmix[1].gain(0, 1.0f);
        usbmix[1].gain(1, 1.0f);
        new AudioConnection_F32(*conv_out[0], 0, *output_stream, 0);
        new AudioConnection_F32(*conv_out[1], 0, *output_stream, 1);
#else
        out_conn[0].connect(output_route, 0, *conv_out[0], 0);
        out_conn[1].connect(output_route, 1, *conv_out[1], 0);
        new AudioConnection_F32(*conv_out[0], 0, *output_stream, 0);
        new AudioConnection_F32(*conv_out[1], 0, *output_stream, 1);
#endif
      }
      return output_route;
    }

    void Init() {
      AudioMemory(AUDIO_MEMORY);
      AudioMemory_F32(F32_AUDIO_MEMORY);
    }
  }
}
#endif
