// VACVMap.h
#pragma once
#include "HSIOFrame.h"
#include "Audio/VirtualAudioCV.h"
#include "Audio/VACVRegistry.h"

struct VACVMap {
  // 0=None; 1..N = VA1..VAN (1-based for UI)
  int8_t   source   = 0;
  uint16_t owner  = 0;

  // ---- scaling constants/helpers --------------------------------------------
  static constexpr float VACV_VMIN  = -3.0f;
  static constexpr float VACV_VMAX  =  6.0f;
  static constexpr float VACV_VSPAN = (VACV_VMAX - VACV_VMIN); // 9.0
  static constexpr int   HEM_UNITS_PER_V = (12 << 7);          // 1536

  // Keep the caller’s initial channel in constructor; claim later when we have an owner.
  VACVMap(int8_t init_source = 0, uint16_t init_owner = 0)
    : source(init_source), owner(0) {
    if (init_owner) AttachOwner(init_owner);  // this will Claim(source)
  }

  // RAII safety if these ever live beyond an applet
  ~VACVMap() { DetachOwner(); }

  // Editor compatibility (Hemisphere expects these exact names)
  char const* InputName() const { return Name(); }

  void AttachOwner(uint16_t owner_id) {
    if (!owner_id || owner == owner_id) return; // don't change anything if owner id 0 or same as current
    if (owner && !IsNone()) VACVRegistry::I().release(Channel0(), owner);
    owner = owner_id;
    // IMPORTANT: claim whatever is currently selected (including ctor preselect)
    if (!IsNone()) Claim(source);
  }

  void DetachOwner() {
    if (owner && !IsNone()) VACVRegistry::I().release(Channel0(), owner);
    owner = 0;
    source = 0;
  }

  bool IsNone()   const { return source <= 0 || source > HS::VACV_CHANNEL_COUNT; }
  int  Channel01() const { return IsNone() ? 0 : source; } // for UI strings
  int  Channel0()  const { return IsNone() ? -1 : (source - 1); } // zero based index, -1 if none, used for accessing source as a public variable

  // Convenience: selected channel as 0-based index, or -1 if none. This way we can access the channel in a more friendly manner to users
  int SelectedChannel() const { return Channel0(); }

  // bool Claim(int new_source) {
  //   if (!owner) return false;                   // must have an owner first
  //   int c = constrain(new_source, 0, HS::VACV_CHANNEL_COUNT);
  //   if (c == source) return true;                 // no change
  //   // release old
  //   if (!IsNone()) VACVRegistry::I().release(Channel0(), owner);
  //   source = c;
  //   if (IsNone()) return true;                  // None
  //   if (VACVRegistry::I().claim(Channel0(), owner)) return true;
  //   source = 0;                                   // failed, revert to None
  //   return false;
  // }
  bool Claim(int new_source) {
    if (!owner) return false;                                                   // Selecting a real channel requires a valid owner.
    if (source == (new_source = constrain(new_source, 0, HS::VACV_CHANNEL_COUNT))) return true;    // Check old source against constrained new source, to see if it goes out of bounds or is same.

    if (new_source == 0) {                                                      // Always allow selecting None ("-"), regardless of owner state.
      if (owner && !IsNone()) VACVRegistry::I().release(Channel0(), owner);
      source = 0;
      return true;
    }

    if (!IsNone()) VACVRegistry::I().release(Channel0(), owner);                // Release previous claim (if any), then try to claim the new one.
    source = new_source;
    if (VACVRegistry::I().claim(Channel0(), owner)) return true;

    // Failed to claim → revert to None
    source = 0;
    return false;
  }

  // Step in +/- direction; if target is taken by someone else, hop to next free.
  void ChangeSource(int dir) {
    if (dir == 0) return;
    if (HS::VACV_CHANNEL_COUNT == 0) { source = 0; return; }
    int target = constrain(source + dir, 0, HS::VACV_CHANNEL_COUNT);
    // Let Claim() decide — it allows same-owner collisions
    if (Claim(target)) return;
    int free0 = VACVRegistry::I().findFree(target - 1, dir);
    Claim(free0 < 0 ? 0 : (free0 + 1));
  }

  // Scaling Helpers -------------------
  int round_to_int(float x) {
    return (x >= 0.f) ? (int)(x + 0.5f) : (int)(x - 0.5f);
  }
  float volts_from_norm(float n01) {
    return VACV_VMIN + n01 * VACV_VSPAN;                       // [-3..+6]
  }
  float norm_from_volts(float v) {
    return (v - VACV_VMIN) / VACV_VSPAN;                       // [0..1]
  }
  int raw_from_volts(float v) {
    return round_to_int(v * (float)HEM_UNITS_PER_V);           // signed raw
  }
  float volts_from_raw(int raw) {
    return (float)raw / (float)HEM_UNITS_PER_V;                // volts
  }

  // Write helpers ----------------------
  // Writes the raw (it's a control value, so -3V really represents 0 here)
  void WriteRaw(float n01) const {
    if (IsNone()) return;
    if (n01 < 0.f) n01 = 0.f; else if (n01 > 1.f) n01 = 1.f;
    VirtualAudioCV::set(Channel0(), n01);                      // store 0..1
  }

  void WriteRawInScale(int raw) const {                        // signed raw (−4608..+9216)
    float v   = volts_from_raw(raw);                           // → volts (−3..+6 typical)
    float n01 = norm_from_volts(v);                            // → 0..1
    if (n01 < 0.f) n01 = 0.f; else if (n01 > 1.f) n01 = 1.f;
    WriteRaw(n01);
  }

  void WriteVolts(float v, float vmin = VACV_VMIN, float vmax = VACV_VMAX) const {
    if (IsNone() || vmax <= vmin) return;
    // Clamp to stated range, then map to 0..1 and store
    if (v < vmin) v = vmin; else if (v > vmax) v = vmax;
    WriteRaw((v - vmin) / (vmax - vmin));
  }

  //Read Helpers
  // Return the current VACV value as a normalized 0..1 float. Returns 0 if None.
  float ReadNorm() const {
    if (IsNone()) return 0.0f;
    return VirtualAudioCV::read(Channel0());                   // 0..1
  }

  int ReadRaw() const {                                        // signed raw domain
    float v = volts_from_norm(ReadNorm());                     // −3..+6
    return raw_from_volts(v);                                  // −4608..+9216
  }

  float ReadVolts(float vmin = VACV_VMIN, float vmax = VACV_VMAX) const {
    float v = volts_from_norm(ReadNorm());                     // −3..+6
    // honor the caller’s display range, but don’t warp the storage
    if (v < vmin) v = vmin; else if (v > vmax) v = vmax;
    return v;
  }

  const char* Name() const {
    static char s[6];
    if (IsNone()) return "—";
    snprintf(s, sizeof(s), "VA%d", source);
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

  // pack/unpack (persist source only; ownership is runtime)
  uint16_t Pack() const { return (uint16_t)(source & 0xFF); }
  void Unpack(uint16_t d) { Claim((int8_t)(d & 0xFF)); }
};