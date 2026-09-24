#pragma once
#include <stdint.h>

#include "Bus200eMaster.h"

namespace OC {
namespace PresetBus {

struct Stats {
  uint32_t isr_count;
  uint32_t starts;
  uint32_t stops;
  uint32_t bytes;
  uint32_t ring_ovf;
  uint32_t query_replies;
  uint32_t query_retries;
  uint32_t midi_rx;
  uint32_t midi_rx_ovf;
  uint32_t midi_tx;
  uint32_t midi_tx_drop;
  uint32_t midi_tx_merged;
  uint32_t ring_hw;
  uint32_t midi_rx_hw;
  uint32_t midi_tx_hw;
  uint32_t bus_stuck;
  uint32_t bus_recovered;
};

#if defined(ARDUINO_TEENSY41) && defined(PRESET_BUS)

void Init();
void Task();

bool Enabled();
bool RemoteEnabled();
void SetModuleAddress(uint8_t a);
void SetModuleAddressRuntime(uint8_t a);
uint8_t ModuleAddress();

void BroadcastSave(uint8_t slot);
void BroadcastRecall(uint8_t slot);
bool BroadcastQueued();

bool WpmPresent();

int CardServeEnable(bool on);
bool CardServing();

int MasterBackup(uint8_t mod_addr);
int MasterRestore(uint8_t mod_addr);
Bus200eMasterState MasterState();
Bus200eMasterError MasterError();
bool MasterTransferring();
uint8_t *MasterCardImage();
void MasterReset();
void DumpCard();

int MasterQuery(uint8_t mod_addr);
bool QueryReplyReady();
uint8_t MasterQueryVersion(uint8_t *out, uint8_t cap);
Bus200eQueryState MasterQueryState();
Bus200eMasterError MasterQueryError();
void MasterQueryReset();

void MasterQuerySetQuiet(bool on);
bool MasterQueryQuiet();

void QueueMidiTx(uint8_t type, uint8_t channel, uint8_t d1, uint8_t d2);
bool ReadMidiRx(uint8_t &status, uint8_t &d1, uint8_t &d2);
bool LoadAckSeenSince(uint8_t addr, uint32_t ms_ago);
int LoadAckCountSince(uint32_t since_ms);
const Stats &GetStats();
void DebugDump();
void SetVerbose(bool on);
bool Verbose();

#else

inline void Init() {}
inline void Task() {}
inline bool Enabled() { return false; }
inline bool RemoteEnabled() { return false; }
inline void SetModuleAddress(uint8_t) {}
inline void SetModuleAddressRuntime(uint8_t) {}
inline uint8_t ModuleAddress() { return 0; }
inline void QueueMidiTx(uint8_t, uint8_t, uint8_t, uint8_t) {}
inline bool ReadMidiRx(uint8_t &, uint8_t &, uint8_t &) { return false; }
inline void BroadcastSave(uint8_t) {}
inline void BroadcastRecall(uint8_t) {}
inline bool BroadcastQueued() { return false; }
inline bool WpmPresent() { return false; }
inline int CardServeEnable(bool) { return -1; }
inline bool CardServing() { return false; }
inline int MasterBackup(uint8_t) { return -1; }
inline int MasterRestore(uint8_t) { return -1; }
inline Bus200eMasterState MasterState() { return BUS200E_MASTER_IDLE; }
inline Bus200eMasterError MasterError() { return BUS200E_MASTER_ERR_NONE; }
inline bool MasterTransferring() { return false; }
inline uint8_t *MasterCardImage() { return nullptr; }
inline void MasterReset() {}
inline void DumpCard() {}
inline int MasterQuery(uint8_t) { return -1; }
inline bool QueryReplyReady() { return false; }
inline uint8_t MasterQueryVersion(uint8_t *, uint8_t) { return 0; }
inline Bus200eQueryState MasterQueryState() { return BUS200E_QUERY_IDLE; }
inline Bus200eMasterError MasterQueryError() { return BUS200E_MASTER_ERR_NONE; }
inline void MasterQueryReset() {}
inline void MasterQuerySetQuiet(bool) {}
inline bool MasterQueryQuiet() { return false; }
inline bool LoadAckSeenSince(uint8_t, uint32_t) { return false; }
inline int LoadAckCountSince(uint32_t) { return 0; }
inline const Stats &GetStats() { static Stats s = {}; return s; }
inline void DebugDump() {}
inline void SetVerbose(bool) {}
inline bool Verbose() { return false; }

#endif

}
}
