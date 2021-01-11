// Icons! Made with http://beigemaze.com/bitmap8x8.html
// Retouched with https://bitca--goatama.repl.co/ (Column major MSB Bottom)

#ifndef HS_ICON_SET
#define HS_ICON_SET

const uint8_t VOCT_ICON[8]       = { 0x07, 0x08, 0x27, 0x10, 0x08, 0xe4, 0xa0, 0xe0 };
const uint8_t GATE_ICON[8]       = {0x40, 0x7e, 0x02, 0x02, 0x02, 0x7e, 0x40, 0x40};
const uint8_t SLEW_ICON[8]       = { 0x40, 0x7e, 0x02, 0x02, 0x0c, 0x30, 0x40, 0x40}; 
const uint8_t LENGTH_ICON[8]       = { 0x0a, 0x01, 0x09, 0x22, 0x48, 0x40, 0x28, 0x00 };
//const uint8_t LENGTH_ICON[8]       = { 0x08, 0x00, 0x08, 0x00, 0x08, 0x00, 0x08, 0x00 };
const uint8_t RANGE_ICON[8]       = { 0x42, 0x7e, 0x42, 0x00, 0x7e, 0x7e, 0x00, 0x78 };
const uint8_t PULSES_ICON[8]       = {0x08, 0x00, 0x1c, 0x1c, 0x1c, 0x00, 0x08, 0x00};
const uint8_t OFFSET_ICON[8]       = { 0x08, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x1c, 0x00 };
const uint8_t ROTATE_ICON[8]       = { 0x08, 0x00, 0x08, 0x00, 0x1c, 0x1c, 0x1c, 0x00};
const uint8_t TOSS_ICON[8]       = { 0x0c, 0x12, 0x52, 0x91, 0x91, 0x09, 0x49, 0x06};

const uint8_t METER_ICON[8]      = {0x00,0xff,0x00,0xfc,0x00,0xff,0x00,0xfc};
const uint8_t CLOCK_ICON[8]      = { 0x3c, 0x42, 0x81, 0x8d, 0x91, 0x81, 0x42, 0x3c };
const uint8_t ENV_ICON[8]        = { 0x20, 0x18, 0x04, 0x08, 0x08, 0x10, 0x20, 0x20 };
const uint8_t MOD_ICON[8]        = {0x30,0x08,0x04,0x08,0x10,0x20,0x10,0x0c};
const uint8_t BEND_ICON[8]       = { 0x00, 0x30, 0x3e, 0x82, 0x70, 0x0e, 0x01, 0x00 };
const uint8_t AFTERTOUCH_ICON[8] = {0x00,0x00,0x20,0x42,0xf5,0x48,0x20,0x00};
const uint8_t MIDI_ICON[8]       = {0x3c,0x42,0x91,0x45,0x45,0x91,0x42,0x3c};
const uint8_t CV_ICON[8]         = { 0x00, 0x3e, 0x22, 0x00, 0x1e, 0x20, 0x1e, 0x00 };
const uint8_t TR_ICON[8]         = { 0x00, 0x02, 0x3e, 0x02, 0x00, 0x3e, 0x0a, 0x36 };
const uint8_t SCALE_ICON[8]      = { 0x00, 0x00, 0x0e, 0x8d, 0x61, 0x1e, 0x00, 0x00};
const uint8_t LOCK_ICON[8]       = {0xf8, 0x8e, 0x89, 0xa9, 0x89, 0x8e, 0xf8, 0x00};
// old lock 0x00,0xf8,0xfe,0xf9,0x89,0xf9,0xfe,0xf8
const uint8_t FAVORITE_ICON[8]   = {0x0e,0x11,0x21,0x42,0x42,0x21,0x11,0x0e};
const uint8_t ROTATE_L_ICON[8]   = { 0x0c, 0x1e, 0x3f, 0x0c, 0x0c, 0x1c, 0x78, 0x00 };
const uint8_t ROTATE_R_ICON[8]   = { 0x00, 0x78, 0x1c, 0x0c, 0x0c, 0x3f, 0x1e, 0x0c };
const uint8_t MONITOR_ICON[8]    = {0x1f,0x51,0x51,0x71,0x71,0x51,0x51,0x1f};
const uint8_t AUDITION_ICON[8]   = {0x78,0x68,0x68,0x78,0x48,0x4c,0x4a,0x79};
const uint8_t LINK_ICON[8]       = {0x70,0xd8,0x88,0xda,0x5b,0x11,0x1b,0x0e};
const uint8_t CHECK_OFF_ICON[8]  = {0xff,0x81,0x81,0x81,0x81,0x81,0x81,0xff};
const uint8_t CHECK_ON_ICON[8]   = {0xcb,0x99,0xb1,0xb1,0x99,0x8c,0x86,0xf3};
const uint8_t CHECK_ICON[8]      = {0x08,0x18,0x30,0x30,0x18,0x0c,0x06,0x03};

// Drums for DrumMap
const uint8_t BD_ICON[8]         = {0x1c,0xa2,0x41,0x41,0x41,0xa2,0x1c,0x00};
const uint8_t SN_ICON[8]         = {0x3e,0xe5,0x25,0x25,0x25,0xe5,0x3e,0x00};
const uint8_t HH_ICON[8]         = {0x04,0x8a,0x4a,0x79,0x4a,0x8a,0x04,0x00};
const uint8_t BD_HIT_ICON[8]     = {0xf3,0x21,0x80,0xf0,0x80,0x21,0xf3,0x00};
const uint8_t SN_HIT_ICON[8]     = {0xc1,0x04,0xc4,0xc4,0xc4,0x04,0xc1,0x00};
const uint8_t HH_HIT_ICON[8]     = {0x0b,0x91,0x51,0x70,0x51,0x91,0x0b,0x00};

const uint8_t RANDOM_ICON[8]     = {0x7c,0x82,0x8a,0x82,0xa2,0x82,0x7c,0x00};  // A die showing '2'
const uint8_t BURST_ICON[8]      = {0x11,0x92,0x54,0x00,0xd7,0x00,0x54,0x92};
const uint8_t GAUGE_ICON[8]      = {0x38,0x44,0x02,0x32,0x32,0x0a,0x44,0x3a};
const uint8_t CLOSED_ICON[8]     = {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18};
const uint8_t OPEN_ICON[8]       = {0x18,0x0c,0x04,0x06,0x03,0x01,0x00,0x18};

const uint8_t BTN_OFF_ICON[8]    = {0x7e,0x8b,0x91,0x91,0x91,0x8b,0x7e,0x00};
const uint8_t BTN_ON_ICON[8]     = {0x71,0xaa,0xa8,0xab,0xa8,0xaa,0x71,0x00};
const uint8_t ZAP_ICON[8]        = {0x00,0x08,0x9c,0x5e,0x7a,0x39,0x10,0x00};

// Transport
const uint8_t LOOP_ICON[8]       = {0x34,0x64,0x4e,0x4e,0xe4,0xe4,0x4c,0x58};
const uint8_t PLAYONCE_ICON[8]   = {0x10,0x10,0x10,0x10,0x38,0x38,0x10,0x10};
const uint8_t PLAY_ICON[8]       = {0x00,0x7e,0x7e,0x3c,0x3c,0x18,0x18,0x00};
const uint8_t PAUSE_ICON[8]      = {0x00,0x7e,0x7e,0x00,0x00,0x7e,0x7e,0x00};
const uint8_t RESET_ICON[8]      = {0x00,0x7e,0x00,0x00,0x18,0x3c,0x7e,0x00};
const uint8_t RECORD_ICON[8]     = {0x00,0x3c,0x7e,0x7e,0x7e,0x7e,0x3c,0x00};
const uint8_t STOP_ICON[8]       = {0x00,0x7e,0x7e,0x7e,0x7e,0x7e,0x7e,0x00};
const uint8_t EDIT_ICON[8]       = {0xc0,0xb0,0x48,0x44,0x22,0x15,0x0a,0x04};

// Direction Buttons
const uint8_t UP_BTN_ICON[8]     = {0x00,0x08,0x0c,0x0e,0x0e,0x0c,0x08,0x00};
const uint8_t DOWN_BTN_ICON[8]   = {0x00,0x10,0x30,0x70,0x70,0x30,0x10,0x00};
const uint8_t LEFT_BTN_ICON[8]   = {0x00,0x18,0x3c,0x7e,0x00,0x00,0x00,0x00};
const uint8_t RIGHT_BTN_ICON[8]  = {0x00,0x00,0x00,0x00,0x7e,0x3c,0x18,0x00};
const uint8_t RIGHT_BTN_ICON_UNFILLED[8] = {0x00,0x00,0x00,0x00,0x7e,0x24,0x18,0x00};

const uint8_t UP_ICON[8]     = {0x08,0x0c,0x7e,0x7f,0x7e,0x0c,0x08,0x00};
const uint8_t DOWN_ICON[8]   = {0x10,0x30,0x7e,0xfe,0x7e,0x30,0x10,0x00};
const uint8_t LEFT_ICON[8]   = {0x08,0x1c,0x3e,0x7f,0x1c,0x1c,0x1c,0x00};
const uint8_t RIGHT_ICON[8]  = {0x00,0x1c,0x1c,0x7f,0x3e,0x1c,0x08,0x00};

// Metronome
const uint8_t METRO_L_ICON[8]    = {0xf3,0x8c,0x9a,0xa2,0x82,0x8c,0xf0,0x00};
const uint8_t METRO_R_ICON[8]    = {0xf0,0x8c,0x82,0xa2,0x9a,0x8c,0xf3,0x00};

// Notes
const uint8_t X_NOTE_ICON[8]     = { 0x00, 0x00, 0xa0, 0x40, 0xa0, 0x1f, 0x00, 0x00 };
const uint8_t NOTE_ICON[8]       = { 0x00, 0xe0, 0xe0, 0x7f, 0x02, 0x1c, 0x00, 0x00 };
const uint8_t NOTE2_ICON[8]      = { 0x00, 0x00, 0xe0, 0xa0, 0xff, 0x00, 0x00, 0x00 };
const uint8_t NOTE4_ICON[8]      = { 0x00, 0x00, 0xe0, 0xe0, 0xff, 0x00, 0x00, 0x00 };

// Pigeons
const uint8_t SILENT_PIGEON_ICON[8]       = { 0x3c, 0xc2, 0x01, 0x0d, 0x01, 0x22, 0x54, 0x88 };
const uint8_t SINGING_PIGEON_ICON[8]      = { 0x3c, 0xc2, 0x01, 0x0d, 0x01, 0x02, 0x48, 0x94 };

// Waveform
const uint8_t UP_DOWN_ICON[8]    = {0x00,0x00,0x24,0x66,0xff,0x66,0x24,0x00};
const uint8_t LEFT_RIGHT_ICON[8] = {0x10,0x38,0x7c,0x10,0x10,0x7c,0x38,0x10};
const uint8_t SEGMENT_ICON[8]    = {0xc0,0xc0,0x20,0x10,0x08,0x06,0x06,0x00};
const uint8_t WAVEFORM_ICON[8]   = {0x10, 0x0c, 0x02, 0x0c, 0x70, 0x80, 0x60, 0x10 };

const uint8_t STAIRS_ICON[8]     = {0x00,0x20,0x20,0x38,0x08,0x0e,0x02,0x02};  // Some stairs going up

// Superscript and subscript 1 and 2
const uint8_t SUP_ONE[3]      = {0x0a,0x0f,0x08};
const uint8_t SUB_TWO[3]      = {0x90,0xd0,0xa0};

// Units
const uint8_t HERTZ_ICON[8]      = {0xfe,0x10,0x10,0xfe,0x00,0xc8,0xa8,0x98};
const uint8_t PLUS_ICON[8]       = {0x10,0x18,0x18,0xfe,0x7f,0x18,0x18,0x08};
const uint8_t MINUS_ICON[8]      = {0x10,0x18,0x18,0x18,0x18,0x18,0x18,0x08};

#endif // HS_ICON_SET
