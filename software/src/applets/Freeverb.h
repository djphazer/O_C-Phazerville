#include "HemisphereAudioApplet.h"
#include <Audio.h>

class Freeverb : public HemisphereAudioApplet<Mono> {
public:
  const char *applet_name() { return "Freeverb"; }
  void Start() { }
  void Controller() {
    roomsize = InF(0);
    damping = InF(1);
    freeverb.roomsize(roomsize);
    freeverb.damping(damping);
  }
  void View() {
    gfxPrint(0, 15, "Roomsize");
    gfxPrint(0, 25, roomsize, 3);
    gfxPrint(0, 35, "Damping");
    gfxPrint(0, 45, damping, 3);
  }
  void OnButtonPress() {CursorToggle();}
  void OnEncoderMove(int direction) {}
  uint64_t OnDataRequest() {return 0;}
  void OnDataReceive(uint64_t data) {}
protected:
  void SetHelp() {}
private:
  int r = 0;
  int d = 0;
  float roomsize = 0.5f;
  float damping = 0.5f;
  AudioEffectFreeverb freeverb {};
  AudioConnection input_conn{input_stream, freeverb};
  AudioConnection output_conn{freeverb, output_stream};
};
