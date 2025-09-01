// VACVMap.h
#pragma once
#include "HSIOFrame.h"
#include "Audio/VirtualAudioCV.h"
#include "Audio/VACVRegistry.h"

struct VACVMap {
  // 0=None; 1..N = VA1..VAN (1-based for UI)
  int8_t   ch01   = 0;
  uint16_t owner  = 0;

  // Keep the caller’s initial channel in constructor; claim later when we have an owner.
  VACVMap(int8_t init_ch01 = 0, uint16_t init_owner = 0)
    : ch01(init_ch01), owner(0) {
    if (init_owner) AttachOwner(init_owner);  // this will Claim(ch01)
  }

  // RAII safety if these ever live beyond an applet
  ~VACVMap() { DetachOwner(); }

  // Editor compatibility (Hemisphere expects these exact names)
  char const* InputName() const { return Name(); }

  void AttachOwner(uint16_t owner_id) {
    if (!owner_id || owner == owner_id) return;
    if (owner && !IsNone()) VACVRegistry::I().release(Channel0(), owner);
    owner = owner_id;
    // IMPORTANT: claim whatever is currently selected (including ctor preselect)
    if (!IsNone()) Claim(ch01);
  }

  void DetachOwner() {
    if (owner && !IsNone()) VACVRegistry::I().release(Channel0(), owner);
    owner = 0;
    ch01  = 0;
  }

  bool IsNone()   const { return ch01 <= 0 || ch01 > HS::VACV_CHANNEL_COUNT; }
  int  Channel01() const { return IsNone() ? 0 : ch01; } // for UI strings
  int  Channel0()  const { return IsNone() ? -1 : (ch01 - 1); } // zero based index, -1 if none, used for accessing ch01 as a public variable

  // Convenience: selected channel as 0-based index, or -1 if none. This way we can access the channel in a more friendly manner to users
  int SelectedChannel() const { return Channel0(); }

  bool Claim(int new_ch01) {
    if (!owner) return false;                   // must have an owner first
    int c = constrain(new_ch01, 0, HS::VACV_CHANNEL_COUNT);
    if (c == ch01) return true;                 // no change
    // release old
    if (!IsNone()) VACVRegistry::I().release(Channel0(), owner);
    ch01 = c;
    if (IsNone()) return true;                  // None
    if (VACVRegistry::I().claim(Channel0(), owner)) return true;
    ch01 = 0;                                   // failed, revert to None
    return false;
  }

  // Step in +/- direction; if target is taken by someone else, hop to next free.
  void ChangeSource(int dir) {
    if (dir == 0) return;
    if (HS::VACV_CHANNEL_COUNT == 0) { ch01 = 0; return; }
    int target = ch01 + (dir > 0 ? 1 : -1);
    target = constrain(target, 0, HS::VACV_CHANNEL_COUNT);
    if (target == 0) { Claim(0); return; }
    // Let Claim() decide — it allows same-owner collisions
    if (Claim(target)) return;
    int free0 = VACVRegistry::I().findFree(target - 1, dir);
    Claim(free0 < 0 ? 0 : (free0 + 1));
  }

  // Optional: wrap around instead of clamping
  void Rotate(int dir) {
    if (dir == 0) return;
    if (HS::VACV_CHANNEL_COUNT == 0) { ch01 = 0; return; }
    int next = ( (ch01==0?0:(ch01-1)) + (dir>0?1:-1) + HS::VACV_CHANNEL_COUNT + 1 )
               % (HS::VACV_CHANNEL_COUNT + 1);   // in [0..N]
    if (Claim(next)) return;
    int free0 = VACVRegistry::I().findFree((next?next-1:0), dir);
    Claim(free0 < 0 ? 0 : (free0 + 1));
  }

  // Write helpers (CVInputMap::RawIn() already scales VA 0..1 -> raw)
  void WriteNorm(float n01) const {
    if (IsNone()) return;
    if (n01 < 0.f) n01 = 0.f; else if (n01 > 1.f) n01 = 1.f;
    VirtualAudioCV::set(Channel0(), n01);
  }

  // expects a constrained voltage input for range of the O_C
  void WriteVolts(float v, float vmin=-3.f, float vmax=6.f) const { 
    if (IsNone() || vmax <= vmin) return;
    WriteNorm((v - vmin) / (vmax - vmin));
  }
  void WriteRawInScale(int raw) const { // 0..HEMISPHERE_MAX_INPUT_CV
    WriteNorm((float)raw / (float)HEMISPHERE_MAX_INPUT_CV);
  }

  //Read Helpers
  // Return the current VACV value as a normalized 0..1 float. Returns 0 if None.
  float ReadNorm() const {
    if (IsNone()) return 0.0f;
    return VirtualAudioCV::read(Channel0());
  }

  // Return the current VACV value scaled to volts in [vmin..vmax].
  float ReadVolts(float vmin = -3.0f, float vmax = 6.0f) const {
    if (vmax <= vmin) return vmin;
    return vmin + ReadNorm() * (vmax - vmin);
  }

  // Return the current VACV value scaled to the integer raw-in domain
  // (0 .. HEMISPHERE_MAX_INPUT_CV). Rounds to nearest int. Returns 0 if None.
  int ReadRaw() const {
    if (IsNone()) return 0;
    return (int) (ReadNorm() * (float)HEMISPHERE_MAX_INPUT_CV + 0.5f);
  }

  const char* Name() const {
    static char s[6];
    if (IsNone()) return "—";
    snprintf(s, sizeof(s), "VA%d", ch01);
    return s;
  }

  uint8_t const* Icon() const {
    // 0 = None uses the "empty" slot like CVInputMap; otherwise slot after DACs
    const int src = IsNone() ? 0 : (FirstSource() + Channel0());
    return PARAM_MAP_ICONS + 8 * src;
  }

  // CVInputMap interop (so the same 'source' space is used)
  static constexpr int FirstSource() { return ADC_CHANNEL_LAST + DAC_CHANNEL_LAST + 1; }
  static constexpr int LastSource()  { return ADC_CHANNEL_LAST + DAC_CHANNEL_LAST + HS::VACV_CHANNEL_COUNT; }
  int  ToCVInputSource() const { return IsNone() ? 0 : (FirstSource() + Channel0()); }
  void FromCVInputSource(int src) {
    if (src >= FirstSource() && src <= LastSource()) Claim(1 + (src - FirstSource()));
    else Claim(0);
  }

  // pack/unpack (persist ch01 only; ownership is runtime)
  uint16_t Pack() const { return (uint16_t)(ch01 & 0xFF); }
  void Unpack(uint16_t d) { Claim((int8_t)(d & 0xFF)); }
};