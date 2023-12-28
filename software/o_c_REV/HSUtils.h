#pragma once

// misc. utility functions extracted from Hemisphere
// -NJM

// Simulated fixed floats by multiplying and dividing by powers of 2
#ifndef int2simfloat
#define int2simfloat(x) (x << 14)
#define simfloat2int(x) (x >> 14)
typedef int32_t simfloat;
#endif

//////////////// Calculation methods
////////////////////////////////////////////////////////////////////////////////

int Proportion(int numerator, int denominator, int max_value);
int ProportionCV(int cv_value, int max_pixels);

// Specifies where data goes in flash storage for each selcted applet, and how big it is
typedef struct PackLocation {
    size_t location;
    size_t size;
} PackLocation;
void Pack(uint64_t &data, PackLocation p, uint64_t value);
int Unpack(uint64_t data, PackLocation p);

void gfxPos(int x, int y);
void gfxPrint(int x, int y, const char *str);
void gfxPrint(int x, int y, int num);
void gfxPrint(const char *str);
void gfxPrint(int num);
void gfxPrint(int x_adv, int num);
void gfxPrintVoltage(int cv);
void gfxPixel(int x, int y);
void gfxFrame(int x, int y, int w, int h);
void gfxRect(int x, int y, int w, int h);
void gfxInvert(int x, int y, int w, int h);
void gfxLine(int x, int y, int x2, int y2);
void gfxDottedLine(int x, int y, int x2, int y2, uint8_t p = 2);
void gfxCircle(int x, int y, int r);
void gfxBitmap(int x, int y, int w, const uint8_t *data);
void gfxIcon(int x, int y, const uint8_t *data);
uint8_t pad(int range, int number);

namespace HS {

extern braids::Quantizer quantizer[4]; // global shared quantizers
extern int quant_scale[4];
extern int root_note[4];

extern int octave_max;

extern int select_mode;
extern bool cursor_wrap;

extern bool auto_save_enabled;
extern int trigger_mapping[4];
extern uint8_t trig_length;
extern uint8_t screensaver_mode;

}
