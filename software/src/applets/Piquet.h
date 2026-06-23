// Copyright (c) 2026
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

#ifndef _HEM_PIQUET_APPLET_H_
#define _HEM_PIQUET_APPLET_H_

class Piquet : public HemisphereApplet {
public:
    static constexpr int NUM_CHANNELS = 2;
    static constexpr int NUM_STAGES = 4;
    static constexpr int STAGE_MAX_VALUE = 255;
    static constexpr int DISPLAY_HEIGHT = 30;
    static constexpr uint8_t MAX_TRIGGER_QUEUE = 4;
    static constexpr int SHAPE_LUT_SIZE = 256; // Pre-computed shape curve resolution
    static constexpr uint16_t SHAPE_LUT_MAX = 65535;
    // Pre-computed exponential shape LUT (full uint16_t range)
    static constexpr uint16_t kShapeExponential[SHAPE_LUT_SIZE] = {
      0, 1, 4, 9, 16, 25, 36, 49, 65, 82, 101, 122, 145, 170, 198, 227,
      258, 291, 327, 364, 403, 444, 488, 533, 581, 630, 681, 735, 790, 848, 907, 969,
      1032, 1098, 1165, 1235, 1306, 1380, 1455, 1533, 1613, 1694, 1778, 1864, 1951, 2041, 2133, 2226,
      2322, 2420, 2520, 2621, 2725, 2831, 2939, 3049, 3161, 3274, 3390, 3508, 3628, 3750, 3874, 4000,
      4128, 4258, 4390, 4524, 4660, 4798, 4938, 5081, 5225, 5371, 5519, 5669, 5821, 5976, 6132, 6290,
      6450, 6612, 6777, 6943, 7111, 7282, 7454, 7628, 7805, 7983, 8164, 8346, 8530, 8717, 8905, 9096,
      9288, 9483, 9679, 9878, 10078, 10281, 10486, 10692, 10901, 11111, 11324, 11539, 11755, 11974, 12195, 12418,
      12642, 12869, 13098, 13329, 13562, 13796, 14033, 14272, 14513, 14756, 15001, 15248, 15497, 15748, 16001, 16256,
      16513, 16772, 17033, 17296, 17561, 17828, 18097, 18368, 18641, 18916, 19193, 19473, 19754, 20037, 20322, 20609,
      20899, 21190, 21483, 21778, 22076, 22375, 22676, 22980, 23285, 23593, 23902, 24213, 24527, 24842, 25160, 25479,
      25801, 26124, 26450, 26777, 27107, 27439, 27772, 28108, 28445, 28785, 29127, 29470, 29816, 30164, 30513, 30865,
      31219, 31575, 31933, 32292, 32654, 33018, 33384, 33752, 34122, 34493, 34867, 35243, 35621, 36001, 36383, 36767,
      37153, 37541, 37931, 38323, 38717, 39113, 39511, 39912, 40314, 40718, 41124, 41532, 41942, 42355, 42769, 43185,
      43603, 44024, 44446, 44870, 45297, 45725, 46155, 46588, 47022, 47458, 47897, 48337, 48780, 49224, 49671, 50119,
      50570, 51022, 51477, 51933, 52392, 52852, 53315, 53780, 54246, 54715, 55185, 55658, 56133, 56610, 57088, 57569,
      58052, 58537, 59023, 59512, 60003, 60496, 60991, 61488, 61986, 62487, 62990, 63495, 64002, 64511, 65022, 65535,
    };

    // Pre-computed quartic shape LUT (softened to rise earlier, full uint16_t range)
    static constexpr uint16_t kShapeQuartic[SHAPE_LUT_SIZE] = {
      0, 2, 8, 18, 30, 45, 64, 85, 108, 135, 164, 195, 230, 266, 305, 347,
      391, 437, 486, 537, 591, 646, 704, 765, 827, 892, 960, 1029, 1101, 1174, 1250, 1329,
      1409, 1492, 1576, 1663, 1752, 1843, 1936, 2032, 2129, 2229, 2330, 2434, 2540, 2647, 2757, 2869,
      2983, 3099, 3217, 3337, 3459, 3583, 3709, 3837, 3968, 4100, 4234, 4370, 4508, 4648, 4790, 4933,
      5079, 5227, 5377, 5529, 5682, 5838, 5995, 6155, 6316, 6479, 6644, 6811, 6980, 7151, 7324, 7499,
      7675, 7854, 8034, 8216, 8400, 8586, 8774, 8964, 9155, 9349, 9544, 9741, 9940, 10141, 10343, 10548,
      10754, 10962, 11172, 11384, 11598, 11813, 12031, 12250, 12471, 12693, 12918, 13144, 13372, 13602, 13834, 14068,
      14303, 14540, 14779, 15020, 15262, 15507, 15753, 16001, 16250, 16502, 16755, 17010, 17266, 17525, 17785, 18047,
      18311, 18577, 18844, 19113, 19384, 19656, 19930, 20207, 20484, 20764, 21045, 21328, 21613, 21899, 22187, 22477,
      22769, 23062, 23358, 23654, 23953, 24253, 24555, 24859, 25164, 25471, 25780, 26091, 26403, 26717, 27033, 27350,
      27669, 27990, 28312, 28637, 28962, 29290, 29619, 29950, 30283, 30617, 30953, 31291, 31630, 31971, 32314, 32658,
      33004, 33352, 33702, 34053, 34406, 34760, 35116, 35474, 35833, 36194, 36557, 36922, 37288, 37655, 38025, 38396,
      38769, 39143, 39519, 39897, 40276, 40657, 41040, 41424, 41810, 42197, 42587, 42978, 43370, 43764, 44160, 44557,
      44956, 45357, 45759, 46163, 46569, 46976, 47385, 47795, 48207, 48621, 49036, 49453, 49872, 50292, 50714, 51137,
      51562, 51989, 52417, 52847, 53279, 53712, 54147, 54583, 55021, 55460, 55901, 56344, 56789, 57235, 57682, 58131,
      58582, 59034, 59488, 59944, 60401, 60860, 61320, 61782, 62246, 62711, 63178, 63646, 64116, 64587, 65060, 65535,
    };

    // Pre-computed sine shape LUT (full uint16_t range)
    static constexpr uint16_t kShapeSine[SHAPE_LUT_SIZE] = {
      0, 404, 807, 1211, 1615, 2018, 2422, 2825, 3228, 3631, 4034, 4437, 4840, 5242, 5645, 6047,
      6449, 6850, 7252, 7653, 8053, 8454, 8854, 9254, 9653, 10053, 10451, 10850, 11247, 11645, 12042, 12439,
      12835, 13230, 13625, 14020, 14414, 14808, 15201, 15593, 15985, 16376, 16767, 17157, 17546, 17935, 18322, 18710,
      19096, 19482, 19867, 20251, 20635, 21018, 21400, 21781, 22161, 22541, 22919, 23297, 23674, 24050, 24425, 24799,
      25172, 25545, 25916, 26286, 26655, 27024, 27391, 27757, 28122, 28487, 28850, 29211, 29572, 29932, 30291, 30648,
      31004, 31359, 31713, 32066, 32417, 32767, 33116, 33464, 33811, 34156, 34500, 34842, 35184, 35523, 35862, 36199,
      36535, 36870, 37203, 37534, 37864, 38193, 38521, 38846, 39171, 39494, 39815, 40135, 40453, 40770, 41085, 41399,
      41711, 42022, 42331, 42638, 42944, 43248, 43551, 43851, 44151, 44448, 44744, 45038, 45330, 45621, 45910, 46197,
      46483, 46766, 47048, 47328, 47607, 47883, 48158, 48431, 48702, 48971, 49239, 49504, 49768, 50029, 50289, 50547,
      50803, 51057, 51309, 51559, 51808, 52054, 52298, 52540, 52781, 53019, 53255, 53489, 53722, 53952, 54180, 54406,
      54630, 54852, 55072, 55290, 55505, 55719, 55930, 56140, 56347, 56552, 56755, 56956, 57154, 57351, 57545, 57737,
      57927, 58115, 58300, 58483, 58665, 58843, 59020, 59194, 59366, 59536, 59704, 59869, 60032, 60193, 60352, 60508,
      60662, 60813, 60963, 61110, 61254, 61397, 61537, 61674, 61810, 61943, 62073, 62202, 62327, 62451, 62572, 62691,
      62808, 62922, 63033, 63143, 63249, 63354, 63456, 63556, 63653, 63748, 63840, 63930, 64018, 64103, 64186, 64266,
      64344, 64419, 64492, 64563, 64631, 64696, 64759, 64820, 64878, 64934, 64987, 65038, 65087, 65133, 65176, 65217,
      65255, 65291, 65325, 65356, 65385, 65411, 65434, 65455, 65474, 65490, 65504, 65515, 65524, 65530, 65534, 65535,
    };

    // Pre-computed big dipper shape LUT (full uint16_t range)
    static constexpr uint16_t kShapeBigDipper[SHAPE_LUT_SIZE] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 37, 107, 184, 265, 353, 447, 546, 652, 763, 881, 1005, 1136, 1273,
      1417, 1567, 1724, 1888, 2059, 2236, 2420, 2612, 2810, 3015, 3228, 3447, 3674, 3908, 4149, 4397,
      4652, 4914, 5184, 5461, 5744, 6035, 6333, 6638, 6951, 7270, 7596, 7929, 8269, 8616, 8969, 9330,
      9697, 10070, 10450, 10837, 11230, 11629, 12035, 12446, 12864, 13287, 13716, 14151, 14592, 15038, 15490, 15946,
      16408, 16875, 17347, 17823, 18305, 18790, 19280, 19775, 20273, 20775, 21281, 21791, 22304, 22820, 23340, 23862,
      24387, 24916, 25446, 25979, 26514, 27051, 27590, 28130, 28672, 29216, 29760, 30306, 30852, 31399, 31946, 32494,
      33041, 33589, 34136, 34683, 35229, 35775, 36319, 36863, 37405, 37945, 38484, 39021, 39556, 40089, 40619, 41148,
      41673, 42195, 42715, 43231, 43744, 44254, 44760, 45262, 45760, 46255, 46745, 47230, 47712, 48188, 48660, 49127,
      49589, 50045, 50497, 50943, 51384, 51819, 52248, 52671, 53089, 53500, 53906, 54305, 54698, 55085, 55465, 55838,
      56205, 56566, 56919, 57266, 57606, 57939, 58265, 58584, 58897, 59202, 59500, 59791, 60074, 60351, 60621, 60883,
      61138, 61386, 61627, 61861, 62088, 62307, 62520, 62725, 62923, 63115, 63299, 63476, 63647, 63811, 63968, 64118,
      64262, 64399, 64530, 64654, 64772, 64883, 64989, 65088, 65182, 65270, 65351, 65428, 65498, 65535, 65535, 65535,
      65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535,
      65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535,
    };

    // Pre-computed plateau shape LUT (full uint16_t range)
    static constexpr uint16_t kShapePlateau[SHAPE_LUT_SIZE] = {
      0, 24, 95, 211, 371, 573, 816, 1098, 1419, 1775, 2166, 2590, 3046, 3532, 4047, 4589,
      5156, 5748, 6362, 6997, 7652, 8324, 9014, 9718, 10436, 11165, 11905, 12654, 13411, 14173, 14940, 15709,
      16480, 17251, 18020, 18785, 19546, 20301, 21048, 21786, 22512, 23227, 23928, 24613, 25281, 25931, 26561, 27170,
      27756, 28317, 28852, 29360, 29838, 30286, 30703, 31085, 31432, 31743, 32016, 32249, 32441, 32590, 32695, 32754,
      32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768,
      32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768,
      32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768,
      32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768,
      32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768,
      32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768,
      32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768,
      32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768,
      32781, 32840, 32945, 33094, 33286, 33519, 33792, 34103, 34450, 34832, 35249, 35697, 36175, 36683, 37218, 37779,
      38365, 38974, 39604, 40254, 40922, 41607, 42308, 43023, 43749, 44487, 45234, 45989, 46750, 47515, 48284, 49055,
      49826, 50595, 51362, 52124, 52881, 53630, 54370, 55099, 55817, 56521, 57211, 57883, 58538, 59173, 59787, 60379,
      60946, 61488, 62003, 62489, 62945, 63369, 63760, 64116, 64437, 64719, 64962, 65164, 65324, 65440, 65511, 65535,
    };

    // Pre-computed sinefold shape LUT (full uint16_t range)
    static constexpr uint16_t kShapeSinefold[SHAPE_LUT_SIZE] = {
      0, 262, 533, 814, 1104, 1403, 1711, 2028, 2353, 2686, 3027, 3375, 3730, 4091, 4457, 4829,
      5204, 5584, 5967, 6352, 6739, 7127, 7515, 7903, 8289, 8674, 9055, 9433, 9807, 10176, 10539, 10895,
      11245, 11586, 11919, 12243, 12557, 12861, 13154, 13436, 13706, 13964, 14210, 14443, 14662, 14869, 15062, 15241,
      15407, 15560, 15699, 15824, 15936, 16035, 16122, 16196, 16258, 16308, 16347, 16375, 16394, 16402, 16403, 16395,
      16379, 16357, 16330, 16297, 16261, 16221, 16180, 16137, 16093, 16051, 16010, 15971, 15937, 15906, 15882, 15864,
      15854, 15852, 15859, 15877, 15906, 15947, 16001, 16068, 16150, 16247, 16359, 16488, 16633, 16796, 16977, 17176,
      17394, 17630, 17886, 18161, 18455, 18768, 19101, 19453, 19824, 20214, 20623, 21050, 21495, 21957, 22436, 22932,
      23443, 23970, 24511, 25065, 25632, 26211, 26802, 27402, 28011, 28629, 29253, 29884, 30520, 31159, 31801, 32445,
      33090, 33734, 34376, 35015, 35651, 36282, 36906, 37524, 38133, 38733, 39324, 39903, 40470, 41024, 41565, 42092,
      42603, 43099, 43578, 44040, 44485, 44912, 45321, 45711, 46082, 46434, 46767, 47080, 47374, 47649, 47905, 48141,
      48359, 48558, 48739, 48902, 49047, 49176, 49288, 49385, 49467, 49534, 49588, 49629, 49658, 49676, 49683, 49681,
      49671, 49653, 49629, 49598, 49564, 49525, 49484, 49442, 49398, 49355, 49314, 49274, 49238, 49205, 49178, 49156,
      49140, 49132, 49133, 49141, 49160, 49188, 49227, 49277, 49339, 49413, 49500, 49599, 49711, 49836, 49975, 50128,
      50294, 50473, 50666, 50873, 51092, 51325, 51571, 51829, 52099, 52381, 52674, 52978, 53292, 53616, 53949, 54290,
      54640, 54996, 55359, 55728, 56102, 56480, 56861, 57246, 57632, 58020, 58408, 58796, 59183, 59568, 59951, 60331,
      60706, 61078, 61444, 61805, 62160, 62508, 62849, 63182, 63507, 63824, 64132, 64431, 64721, 65002, 65273, 65535,
    };

    inline static uint16_t lookup_shape_u16(uint16_t phase_0_1024, const uint16_t* lut) {
      // phase_0_1024 is 0-1024, map to LUT index 0-255, return full uint16_t range
      int idx = (phase_0_1024 * SHAPE_LUT_SIZE) >> 10;
      if (idx >= SHAPE_LUT_SIZE) idx = SHAPE_LUT_SIZE - 1;
      return lut[idx];
    }

    inline static uint16_t lookup_shape_inverted_u16(uint16_t phase_0_1024, const uint16_t* lut) {
      // Compute inverted shape: 65535 - lut[idx]
      int idx = (phase_0_1024 * SHAPE_LUT_SIZE) >> 10;
      if (idx >= SHAPE_LUT_SIZE) idx = SHAPE_LUT_SIZE - 1;
      return SHAPE_LUT_MAX - lut[idx];
    }

    inline static uint16_t lookup_shape_scaled_u16(uint16_t phase_0_1024, const uint16_t* lut, int depth) {
      // Scale dipper depth around the linear ramp so derived dippers stay anchored at 0 and 1.
      int idx = (phase_0_1024 * SHAPE_LUT_SIZE) >> 10;
      if (idx >= SHAPE_LUT_SIZE) idx = SHAPE_LUT_SIZE - 1;
      const int32_t linear = (static_cast<uint32_t>(phase_0_1024) * SHAPE_LUT_MAX + 512) >> 10;
      const int32_t offset = static_cast<int32_t>(lut[idx]) - linear;
      const int32_t scaled = linear + ((offset * depth + 512) >> 10);
      return static_cast<uint16_t>(constrain(scaled, 0, static_cast<int32_t>(SHAPE_LUT_MAX)));
    }

    enum EnvType : uint8_t {
      ENV_AD,
      ENV_AR,
      ENV_ASR,
      ENV_ADSR,
      ENV_LOOP_AD,
      ENV_LOOP_AR,
      ENV_TYPE_LAST
    };

    enum Stage : int8_t {
      STAGE_ATTACK,
      STAGE_DECAY,
      STAGE_SUSTAIN,
      STAGE_RELEASE,
      STAGE_IDLE = -1
    };

    enum DelayMode : uint8_t {
      DELAY_OFF,
      DELAY_REPLACE,
      DELAY_QUEUE,
      DELAY_MODE_LAST
    };

    enum CVMap : uint8_t {
      CVMAP_NONE,
      CVMAP_ATTACK,
      CVMAP_DECAY,
      CVMAP_SUSTAIN,
      CVMAP_RELEASE,
      CVMAP_ADR,
      CVMAP_LEVEL,
      CVMAP_LAST
    };

    enum ShapeType : uint8_t {
      SHAPE_LINEAR,
      SHAPE_EXPONENTIAL,
      SHAPE_QUARTIC,
      SHAPE_SINE,
      SHAPE_PLATEAU,
      SHAPE_CLIFF,
      SHAPE_GATE,
      SHAPE_BIG_DIPPER,
      SHAPE_MEDIUM_DIPPER,
      SHAPE_LITTLE_DIPPER,
      SHAPE_SINEFOLD,
      SHAPE_LAST
    };

    enum Cursor : uint8_t {
      CURSOR_CHANNEL,
      CURSOR_TYPE,
      CURSOR_ATTACK,
      CURSOR_ATTACK_SHAPE,
      CURSOR_DECAY,
      CURSOR_DECAY_SHAPE,
      CURSOR_SUSTAIN,
      CURSOR_RELEASE,
      CURSOR_RELEASE_SHAPE,
      CURSOR_DELAY,
      CURSOR_DELAY_MODE,
      CURSOR_TRIG_SOURCE,
      CURSOR_CV_MAP,
      CURSOR_LEVEL,
      CURSOR_INVERT,
      CURSOR_LAST
    };

    struct TriggerQueue {
      int32_t ticks[MAX_TRIGGER_QUEUE];
      uint8_t count;

      void Reset() {
        count = 0;
        for (uint8_t i = 0; i < MAX_TRIGGER_QUEUE; ++i) ticks[i] = 0;
      }

      void Add(int32_t delay_ticks, bool queue_mode) {
        if (delay_ticks <= 0) return;
        if (!queue_mode) {
          count = 1;
          ticks[0] = delay_ticks;
          return;
        }
        if (count < MAX_TRIGGER_QUEUE) {
          ticks[count++] = delay_ticks;
          return;
        }
        for (uint8_t i = 1; i < MAX_TRIGGER_QUEUE; ++i) ticks[i - 1] = ticks[i];
        ticks[MAX_TRIGGER_QUEUE - 1] = delay_ticks;
      }

      bool TickReady() {
        if (!count) return false;
        for (uint8_t i = 0; i < count; ++i) --ticks[i];
        if (ticks[0] <= 0) {
          for (uint8_t i = 1; i < count; ++i) ticks[i - 1] = ticks[i];
          --count;
          return true;
        }
        return false;
      }
    };

    struct Channel {
      uint8_t type;
      uint8_t stage_value[NUM_STAGES];
      uint8_t delay_ms;
      uint8_t delay_mode;
      uint8_t trigger_source;
      uint8_t cv_map;
      uint8_t level;
      uint8_t attack_shape;
      uint8_t decay_shape;
      uint8_t release_shape;
      bool inverted;

      Stage stage;
      int32_t stage_ticks;
      bool gate_prev;
      simfloat amplitude;
      int segment_start_cv;
      TriggerQueue trigger_queue;

      void Init(uint8_t ch) {
        type = (ch == 0) ? ENV_ADSR : ENV_ASR;
        stage_value[STAGE_ATTACK] = 28 + ch * 8;
        stage_value[STAGE_DECAY] = 52;
        stage_value[STAGE_SUSTAIN] = 160;
        stage_value[STAGE_RELEASE] = 44 + ch * 8;
        delay_ms = 0;
        delay_mode = DELAY_OFF;
        trigger_source = ch;
        cv_map = CVMAP_NONE;
        level = 255;
        attack_shape = SHAPE_QUARTIC;
        decay_shape = SHAPE_EXPONENTIAL;
        release_shape = SHAPE_EXPONENTIAL;
        inverted = false;

        stage = STAGE_IDLE;
        stage_ticks = 0;
        gate_prev = false;
        amplitude = 0;
        segment_start_cv = 0;
        trigger_queue.Reset();
      }

      bool is_looping() const {
        return type == ENV_LOOP_AD || type == ENV_LOOP_AR;
      }

      bool has_decay() const {
        return type == ENV_AD || type == ENV_ADSR || type == ENV_LOOP_AD;
      }

      bool has_sustain_hold() const {
        return type == ENV_ASR || type == ENV_ADSR;
      }

      bool is_ar_family() const {
        return type == ENV_AR || type == ENV_LOOP_AR;
      }
    };

    const char* applet_name() {
      return "Piquet";
    }

    const uint8_t* applet_icon() {
      return PhzIcons::ADSR_EG;
    }

    void Start() {
      selected_channel = 0;
      cursor = CURSOR_ATTACK;
      ForEachChannel(ch) channels[ch].Init(ch);
    }

    void Controller() {
      ForEachChannel(ch) {
        Channel &c = channels[ch];
        const int cv_in = DetentedIn(ch);
        const bool gate = read_gate_source(c.trigger_source);
        const bool gate_rise = gate && !c.gate_prev;
        const bool gate_fall = !gate && c.gate_prev;
        c.gate_prev = gate;

        if (gate_rise) {
          const int32_t dticks = static_cast<int32_t>(c.delay_ms) * 17;
          if (c.delay_mode == DELAY_OFF || dticks <= 0) {
            c.trigger_queue.Reset();
            begin_attack(c);
          } else {
            c.trigger_queue.Add(dticks, c.delay_mode == DELAY_QUEUE);
          }
        }

        if (c.trigger_queue.TickReady()) begin_attack(c);

        process_envelope(c, gate, gate_fall, cv_in);
        Out(ch, apply_level_and_polarity(c, cv_in));
      }
    }

    void View() {
      DrawIndicators();
      DrawEnvelope();
      DrawParamRow();
    }

    void OnEncoderMove(int direction) {
      if (!EditMode()) {
        cursor = constrain(cursor + direction, 0, CURSOR_LAST - 1);
        return;
      }

      if (cursor == CURSOR_CHANNEL) {
        selected_channel = constrain(selected_channel + direction, 0, NUM_CHANNELS - 1);
        return;
      }

      Channel &c = channels[selected_channel];
      switch (cursor) {
        case CURSOR_TYPE:
          c.type = constrain(c.type + direction, 0, ENV_TYPE_LAST - 1);
          break;
        case CURSOR_ATTACK:
          c.stage_value[STAGE_ATTACK] = constrain(c.stage_value[STAGE_ATTACK] + direction, 1, STAGE_MAX_VALUE);
          break;
        case CURSOR_ATTACK_SHAPE:
          c.attack_shape = constrain(c.attack_shape + direction, 0, SHAPE_LAST - 1);
          break;
        case CURSOR_DECAY:
          c.stage_value[STAGE_DECAY] = constrain(c.stage_value[STAGE_DECAY] + direction, 1, STAGE_MAX_VALUE);
          break;
        case CURSOR_DECAY_SHAPE:
          c.decay_shape = constrain(c.decay_shape + direction, 0, SHAPE_LAST - 1);
          break;
        case CURSOR_SUSTAIN:
          c.stage_value[STAGE_SUSTAIN] = constrain(c.stage_value[STAGE_SUSTAIN] + direction, 0, STAGE_MAX_VALUE);
          break;
        case CURSOR_RELEASE:
          c.stage_value[STAGE_RELEASE] = constrain(c.stage_value[STAGE_RELEASE] + direction, 1, STAGE_MAX_VALUE);
          break;
        case CURSOR_RELEASE_SHAPE:
          c.release_shape = constrain(c.release_shape + direction, 0, SHAPE_LAST - 1);
          break;
        case CURSOR_DELAY:
          c.delay_ms = constrain(c.delay_ms + direction, 0, 255);
          break;
        case CURSOR_DELAY_MODE:
          c.delay_mode = constrain(c.delay_mode + direction, 0, DELAY_MODE_LAST - 1);
          break;
        case CURSOR_TRIG_SOURCE:
          c.trigger_source = constrain(c.trigger_source + direction, 0, 1);
          break;
        case CURSOR_CV_MAP:
          c.cv_map = constrain(c.cv_map + direction, 0, CVMAP_LAST - 1);
          break;
        case CURSOR_LEVEL:
          c.level = constrain(c.level + direction, 0, 255);
          break;
        case CURSOR_INVERT:
          if (direction) c.inverted = !c.inverted;
          break;
        default:
          break;
      }
    }

    void OnButtonPress() {
      CursorToggle();
    }

    void AuxButton() {
      selected_channel = 1 - selected_channel;
      cursor = CURSOR_CHANNEL;
      if (EditMode()) CursorToggle();
    }

    uint64_t OnDataRequest() {
      uint64_t data_ch0 = PackChannel(channels[0]);
      uint64_t data_ch1 = PackChannel(channels[1]);
      SetData(0, data_ch1);

      // Pack and save shapes for both channels
      uint16_t shapes_ch0 = channels[0].attack_shape | (channels[0].decay_shape << 4) | (channels[0].release_shape << 8);
      uint16_t shapes_ch1 = channels[1].attack_shape | (channels[1].decay_shape << 4) | (channels[1].release_shape << 8);
      SetData(1, shapes_ch0);
      SetData(2, shapes_ch1);

      return data_ch0;
    }

    void OnDataReceive(uint64_t data) {
      if (!data) {
        Start();
        return;
      }

      unpack_channel(channels[0], data);

      uint64_t data_ch1 = 0;
      if (GetData(0, data_ch1) && data_ch1) {
        unpack_channel(channels[1], data_ch1);
      } else {
        channels[1].Init(1);
      }

      // Force deterministic defaults first; only override when aux storage exists.
      channels[0].attack_shape = SHAPE_QUARTIC;
      channels[0].decay_shape = SHAPE_EXPONENTIAL;
      channels[0].release_shape = SHAPE_EXPONENTIAL;
      channels[1].attack_shape = SHAPE_QUARTIC;
      channels[1].decay_shape = SHAPE_EXPONENTIAL;
      channels[1].release_shape = SHAPE_EXPONENTIAL;

      // Load shapes from auxiliary storage
      uint16_t shapes_ch0 = 0;
      PhzConfig::VALUE shapes_ch0_data = 0;
      if (GetData(1, shapes_ch0_data)) {
        shapes_ch0 = static_cast<uint16_t>(shapes_ch0_data & 0x0FFFu);
        channels[0].attack_shape = constrain((shapes_ch0 & 0x0F), 0, SHAPE_LAST - 1);
        channels[0].decay_shape = constrain(((shapes_ch0 >> 4) & 0x0F), 0, SHAPE_LAST - 1);
        channels[0].release_shape = constrain(((shapes_ch0 >> 8) & 0x0F), 0, SHAPE_LAST - 1);
      }

      uint16_t shapes_ch1 = 0;
      PhzConfig::VALUE shapes_ch1_data = 0;
      if (GetData(2, shapes_ch1_data)) {
        shapes_ch1 = static_cast<uint16_t>(shapes_ch1_data & 0x0FFFu);
        channels[1].attack_shape = constrain((shapes_ch1 & 0x0F), 0, SHAPE_LAST - 1);
        channels[1].decay_shape = constrain(((shapes_ch1 >> 4) & 0x0F), 0, SHAPE_LAST - 1);
        channels[1].release_shape = constrain(((shapes_ch1 >> 8) & 0x0F), 0, SHAPE_LAST - 1);
      }

      channels[0].stage = STAGE_IDLE;
      channels[0].stage_ticks = 0;
      channels[0].amplitude = 0;
      channels[0].segment_start_cv = 0;
      channels[0].gate_prev = false;
      channels[0].trigger_queue.Reset();

      channels[1].stage = STAGE_IDLE;
      channels[1].stage_ticks = 0;
      channels[1].amplitude = 0;
      channels[1].segment_start_cv = 0;
      channels[1].gate_prev = false;
      channels[1].trigger_queue.Reset();
    }

protected:
    void SetHelp() {
      help[HELP_DIGITAL1] = "Trig 1";
      help[HELP_DIGITAL2] = "Trig 2";
      help[HELP_CV1] = "CV Mod1";
      help[HELP_CV2] = "CV Mod2";
      help[HELP_OUT1] = "Env 1";
      help[HELP_OUT2] = "Env 2";
      help[HELP_EXTRA1] = "Btn: Edit";
      help[HELP_EXTRA2] = "Aux: Chan";
    }

private:
    int selected_channel = 0;
    int cursor = CURSOR_ATTACK;
    Channel channels[NUM_CHANNELS];

    static int StageTicks(int value) {
      return kShapeExponential[constrain(value, 0, STAGE_MAX_VALUE)] >> 1;
    }

    bool read_gate_source(uint8_t source) const {
      return Gate(constrain(source, 0, 1));
    }

    void begin_attack(Channel &c) {
      set_stage(c, STAGE_ATTACK);
    }

    void set_stage(Channel &c, Stage stage) {
      c.stage = stage;
      c.stage_ticks = 0;
      c.segment_start_cv = simfloat2int(c.amplitude);
    }

    int cv_mod(const Channel &c, int stage, int mod) {
      if (mod == 0) return 0;
      if (c.cv_map == CVMAP_ADR && (stage == STAGE_ATTACK || stage == STAGE_DECAY || stage == STAGE_RELEASE)) return mod;
      if (c.cv_map == CVMAP_ATTACK && stage == STAGE_ATTACK) return mod;
      if (c.cv_map == CVMAP_DECAY && stage == STAGE_DECAY) return mod;
      if (c.cv_map == CVMAP_SUSTAIN && stage == STAGE_SUSTAIN) return mod;
      if (c.cv_map == CVMAP_RELEASE && stage == STAGE_RELEASE) return mod;
      return 0;
    }

    void process_envelope(Channel &c, bool gate, bool gate_fall, int cv_in) {
      const int base_mod = (c.cv_map == CVMAP_NONE || c.cv_map == CVMAP_LEVEL)
        ? 0
        : Proportion(cv_in, HEMISPHERE_MAX_INPUT_CV, STAGE_MAX_VALUE / 2);

      if (c.stage == STAGE_IDLE) {
        if (!c.is_looping()) c.amplitude = 0;
        return;
      }

      if (gate_fall && (c.type == ENV_AR || c.type == ENV_ASR || c.type == ENV_ADSR || c.type == ENV_LOOP_AR)) {
        set_stage(c, STAGE_RELEASE);
      }

      ++c.stage_ticks;

      if (c.stage == STAGE_ATTACK) {
        int attack = constrain(c.stage_value[STAGE_ATTACK] + cv_mod(c, STAGE_ATTACK, base_mod), 1, STAGE_MAX_VALUE);
        if (run_segment(c, HEMISPHERE_MAX_CV, StageTicks(attack))) {
          if (c.has_decay()) {
            set_stage(c, STAGE_DECAY);
          } else if (c.has_sustain_hold()) {
            set_stage(c, STAGE_SUSTAIN);
          } else if (c.is_ar_family()) {
            set_stage(c, gate ? STAGE_SUSTAIN : STAGE_RELEASE);
          } else {
            set_stage(c, STAGE_RELEASE);
          }
        }
        return;
      }

      if (c.stage == STAGE_DECAY) {
        int decay = constrain(c.stage_value[STAGE_DECAY] + cv_mod(c, STAGE_DECAY, base_mod), 1, STAGE_MAX_VALUE);
        const int sustain_cv = constrain(c.stage_value[STAGE_SUSTAIN] + cv_mod(c, STAGE_SUSTAIN, base_mod), 0, STAGE_MAX_VALUE);
        const int target = Proportion(sustain_cv, STAGE_MAX_VALUE, HEMISPHERE_MAX_CV);
        if (run_segment(c, target, StageTicks(decay))) {
          if (c.type == ENV_AD || c.type == ENV_LOOP_AD) {
            set_stage(c, STAGE_RELEASE);
          } else {
            set_stage(c, STAGE_SUSTAIN);
          }
        }
        return;
      }

      if (c.stage == STAGE_SUSTAIN) {
        if (c.type == ENV_ASR || c.type == ENV_ADSR || c.type == ENV_AR || c.type == ENV_LOOP_AR) {
          if (c.type == ENV_AR || c.type == ENV_LOOP_AR) {
            c.amplitude = int2simfloat(HEMISPHERE_MAX_CV);
          } else {
            const int sustain_cv = constrain(c.stage_value[STAGE_SUSTAIN] + cv_mod(c, STAGE_SUSTAIN, base_mod), 0, STAGE_MAX_VALUE);
            c.amplitude = int2simfloat(Proportion(sustain_cv, STAGE_MAX_VALUE, HEMISPHERE_MAX_CV));
          }
          if (!gate && (c.type == ENV_ASR || c.type == ENV_ADSR || c.type == ENV_AR || c.type == ENV_LOOP_AR)) {
            set_stage(c, STAGE_RELEASE);
          }
        }
        return;
      }

      if (c.stage == STAGE_RELEASE) {
        int release = constrain(c.stage_value[STAGE_RELEASE] + cv_mod(c, STAGE_RELEASE, base_mod), 1, STAGE_MAX_VALUE);
        if (run_segment(c, 0, StageTicks(release) * 2)) {
          set_stage(c, c.is_looping() ? STAGE_ATTACK : STAGE_IDLE);
        }
      }
    }

    // Apply shape transformation to 10-bit phase (0..1024) and return u16 phase (0..65535)
    uint16_t apply_shape_u16(uint16_t phase_0_1024, uint8_t shape) {
      const uint16_t p = constrain(phase_0_1024, 0, 1024);

      switch (shape) {
        case SHAPE_LINEAR:
          // Convert 10-bit phase to 16-bit phase with rounding.
          return (static_cast<uint32_t>(p) * SHAPE_LUT_MAX + 512) >> 10;
        case SHAPE_EXPONENTIAL:
          return lookup_shape_u16(p, kShapeExponential);
        case SHAPE_QUARTIC:
          return lookup_shape_u16(p, kShapeQuartic);
        case SHAPE_SINE:
          return lookup_shape_u16(p, kShapeSine);
        case SHAPE_PLATEAU:
          return lookup_shape_u16(p, kShapePlateau);
        case SHAPE_CLIFF:
          // Computed by inverting PLATEAU (fast start → slow end)
          return lookup_shape_inverted_u16(p, kShapePlateau);
        case SHAPE_GATE:
          // Gate: hold low then jump
          return (p < 768) ? 3276 : SHAPE_LUT_MAX;
        case SHAPE_BIG_DIPPER:
          return lookup_shape_u16(p, kShapeBigDipper);
        case SHAPE_MEDIUM_DIPPER:
          // Derived from BIG_DIPPER with 50% depth scaling
          return lookup_shape_scaled_u16(p, kShapeBigDipper, 512);
        case SHAPE_LITTLE_DIPPER:
          // Derived from BIG_DIPPER with 25% depth scaling
          return lookup_shape_scaled_u16(p, kShapeBigDipper, 256);
        case SHAPE_SINEFOLD:
          return lookup_shape_u16(p, kShapeSinefold);
        default:
          return (static_cast<uint32_t>(p) * SHAPE_LUT_MAX + 512) >> 10;
      }
    }

    bool run_segment(Channel &c, int target_cv, int total_ticks) {
      if (total_ticks < 1) total_ticks = 1;
      const int ticks_remaining = total_ticks - c.stage_ticks;
      if (ticks_remaining <= 0) {
        c.amplitude = int2simfloat(constrain(target_cv, 0, HEMISPHERE_MAX_CV));
        return true;
      }

      // Normalized segment phase in 10-bit fixed-point (0..1024).
      const uint16_t phase_0_1024 = constrain((static_cast<uint32_t>(c.stage_ticks) * 1024u) / total_ticks, 0u, 1024u);

      uint16_t shaped_phase_u16 = (static_cast<uint32_t>(phase_0_1024) * SHAPE_LUT_MAX + 512) >> 10;
      if (c.stage == STAGE_ATTACK) {
        shaped_phase_u16 = apply_shape_u16(phase_0_1024, c.attack_shape);
      } else if (c.stage == STAGE_DECAY) {
        shaped_phase_u16 = apply_shape_u16(phase_0_1024, c.decay_shape);
      } else if (c.stage == STAGE_RELEASE) {
        shaped_phase_u16 = apply_shape_u16(phase_0_1024, c.release_shape);
      }

      const int32_t start = c.segment_start_cv;
      const int32_t delta = static_cast<int32_t>(target_cv) - start;
      const int32_t abs_delta = (delta >= 0) ? delta : -delta;
      // Interpolate in Q16: shaped_phase_u16 is 0..65535. Using >>16 gives a fast /65536
      // approximation; end-of-stage and <=1 CV snap logic below ensures exact landing.
      const int32_t interp = (abs_delta * static_cast<int32_t>(shaped_phase_u16) + (1 << 15)) >> 16;
      int32_t amp_cv = (delta >= 0) ? (start + interp) : (start - interp);
      amp_cv = constrain(amp_cv, 0, HEMISPHERE_MAX_CV);
      c.amplitude = int2simfloat(amp_cv);

      if (c.stage_ticks >= total_ticks || abs(target_cv - amp_cv) <= 1) {
        c.amplitude = int2simfloat(constrain(target_cv, 0, HEMISPHERE_MAX_CV));
        return true;
      }
      return false;
    }

    int apply_level_and_polarity(const Channel &c, int cv_in) {
      int out = simfloat2int(c.amplitude);
      int level = c.level;
      if (c.cv_map == CVMAP_LEVEL) {
        level = constrain(level + Proportion(cv_in, HEMISPHERE_MAX_INPUT_CV, STAGE_MAX_VALUE / 2), 0, 255);
      }
      out = Proportion(out, HEMISPHERE_MAX_CV, Proportion(level, 255, HEMISPHERE_MAX_CV));
      if (c.inverted) out = HEMISPHERE_MAX_CV - out;
      return constrain(out, 0, HEMISPHERE_MAX_CV);
    }

    uint64_t PackChannel(const Channel &c) {
      uint64_t data = 0;
      Pack(data, PackLocation {0,3}, c.type);
      Pack(data, PackLocation {3,8}, c.stage_value[STAGE_ATTACK]);
      Pack(data, PackLocation {11,8}, c.stage_value[STAGE_DECAY]);
      Pack(data, PackLocation {19,8}, c.stage_value[STAGE_SUSTAIN]);
      Pack(data, PackLocation {27,8}, c.stage_value[STAGE_RELEASE]);
      Pack(data, PackLocation {35,8}, c.delay_ms);
      Pack(data, PackLocation {43,2}, c.delay_mode);
      Pack(data, PackLocation {45,1}, c.trigger_source);
      Pack(data, PackLocation {46,3}, c.cv_map);
      Pack(data, PackLocation {49,8}, c.level);
      Pack(data, PackLocation {57,1}, c.inverted ? 1 : 0);
      return data;
    }

    void unpack_channel(Channel &c, uint64_t data) {
      c.type = constrain(Unpack(data, PackLocation {0,3}), 0, ENV_TYPE_LAST - 1);
      c.stage_value[STAGE_ATTACK] = constrain(Unpack(data, PackLocation {3,8}), 1, STAGE_MAX_VALUE);
      c.stage_value[STAGE_DECAY] = constrain(Unpack(data, PackLocation {11,8}), 1, STAGE_MAX_VALUE);
      c.stage_value[STAGE_SUSTAIN] = constrain(Unpack(data, PackLocation {19,8}), 0, STAGE_MAX_VALUE);
      c.stage_value[STAGE_RELEASE] = constrain(Unpack(data, PackLocation {27,8}), 1, STAGE_MAX_VALUE);
      c.delay_ms = constrain(Unpack(data, PackLocation {35,8}), 0, 255);
      c.delay_mode = constrain(Unpack(data, PackLocation {43,2}), 0, DELAY_MODE_LAST - 1);
      c.trigger_source = constrain(Unpack(data, PackLocation {45,1}), 0, 1);
      c.cv_map = constrain(Unpack(data, PackLocation {46,3}), 0, CVMAP_LAST - 1);
      c.level = constrain(Unpack(data, PackLocation {49,8}), 0, 255);
      c.inverted = Unpack(data, PackLocation {57,1});
    }

    static const char* type_name(uint8_t t) {
      static const char* names[ENV_TYPE_LAST] = { "AD", "AR", "ASR", "ADSR", "LPAD", "LPAR" };
      return names[constrain(t, 0, ENV_TYPE_LAST - 1)];
    }

    static const char* dmode_name(uint8_t m) {
      static const char* names[DELAY_MODE_LAST] = { "Off", "Repl", "Que" };
      return names[constrain(m, 0, DELAY_MODE_LAST - 1)];
    }

    static const char* cvmap_name(uint8_t m) {
      static const char* names[CVMAP_LAST] = { "None", "Att", "Dec", "Sus", "Rel", "ADR", "Lvl" };
      return names[constrain(m, 0, CVMAP_LAST - 1)];
    }

    static const char* shape_name(uint8_t s) {
      static const char* names[SHAPE_LAST] = { "Lin", "Exp", "Qua", "Sin", "Pla", "Cli", "Gat", "BD", "MD", "LD", "SF" };
      return names[constrain(s, 0, SHAPE_LAST - 1)];
    }

    void DrawIndicators() {
      ForEachChannel(ch) {
        int w = Proportion(ViewOut(ch), HEMISPHERE_MAX_CV, 62);
        gfxRect(0, 15 + (ch * 3), w, 2);
      }
      gfxPrint(0, 22, OutputLabel(selected_channel));
      gfxInvert(0, 21, 7, 9);
    }

    void DrawParamRow() {
      Channel &c = channels[selected_channel];
      gfxPrint(9, 22, cursor_label());
      switch (cursor) {
        case CURSOR_CHANNEL:
          gfxPrint(OutputLabel(selected_channel));
          break;
        case CURSOR_TYPE:
          gfxPrint(type_name(c.type));
          break;
        case CURSOR_ATTACK:
          gfxPrint(StageTicks(c.stage_value[STAGE_ATTACK]) / 17);
          gfxPrint("ms");
          break;
        case CURSOR_ATTACK_SHAPE:
          gfxPrint(shape_name(c.attack_shape));
          break;
        case CURSOR_DECAY:
          gfxPrint(StageTicks(c.stage_value[STAGE_DECAY]) / 17);
          gfxPrint("ms");
          break;
        case CURSOR_DECAY_SHAPE:
          gfxPrint(shape_name(c.decay_shape));
          break;
        case CURSOR_SUSTAIN:
          gfxPrint(Proportion(c.stage_value[STAGE_SUSTAIN], STAGE_MAX_VALUE, 100));
          gfxPrint("%");
          break;
        case CURSOR_RELEASE:
          gfxPrint((StageTicks(c.stage_value[STAGE_RELEASE]) * 2) / 17);
          gfxPrint("ms");
          break;
        case CURSOR_RELEASE_SHAPE:
          gfxPrint(shape_name(c.release_shape));
          break;
        case CURSOR_DELAY:
          gfxPrint(c.delay_ms);
          gfxPrint("ms");
          break;
        case CURSOR_DELAY_MODE:
          gfxPrint(dmode_name(c.delay_mode));
          break;
        case CURSOR_TRIG_SOURCE:
          gfxPrint(c.trigger_source ? "D2" : "D1");
          break;
        case CURSOR_CV_MAP:
          gfxPrint(cvmap_name(c.cv_map));
          break;
        case CURSOR_LEVEL:
          gfxPrint(Proportion(c.level, 255, 100));
          gfxPrint("%");
          break;
        case CURSOR_INVERT:
          gfxPrint(c.inverted ? "Yes" : "No");
          break;
        default:
          break;
      }

      if (EditMode()) gfxSpicyCursor(9, 31, 54);
    }

    const char* cursor_label() const {
      static const char* labels[CURSOR_LAST] = {
        "Ch:", "Ty:", "A:", "AShp:", "D:", "DShp:", "S:", "R:", "RShp:", "Dl:", "Dm:", "Tr:", "Cv:", "Lv:", "Inv:"
      };
      return labels[constrain(cursor, 0, CURSOR_LAST - 1)];
    }

    void DrawEnvelope() {
      Channel &c = channels[selected_channel];
      int length = c.stage_value[STAGE_ATTACK] + c.stage_value[STAGE_DECAY] + c.stage_value[STAGE_RELEASE] + 36;
      int x = 0;
      x = draw_attack(c, x, length);
      x = draw_decay(c, x, length);
      x = draw_sustain(c, x, length);
      draw_release(c, x, length);
    }

    int draw_attack(const Channel &c, int x, int length) {
      int x2 = x + Proportion(c.stage_value[STAGE_ATTACK], length, 62);
      gfxLine(x, BottomAlign(0), x2, BottomAlign(DISPLAY_HEIGHT), cursor != CURSOR_ATTACK);
      return x2;
    }

    int draw_decay(const Channel &c, int x, int length) {
      int x2 = x + Proportion(c.stage_value[STAGE_DECAY], length, 62);
      int ys = Proportion(c.stage_value[STAGE_SUSTAIN], STAGE_MAX_VALUE, DISPLAY_HEIGHT);
      gfxLine(x, BottomAlign(DISPLAY_HEIGHT), x2, BottomAlign(ys), cursor != CURSOR_DECAY);
      return x2;
    }

    int draw_sustain(const Channel &c, int x, int length) {
      int x2 = x + Proportion(36, length, 62);
      int ys = Proportion(c.stage_value[STAGE_SUSTAIN], STAGE_MAX_VALUE, DISPLAY_HEIGHT);
      gfxLine(x, BottomAlign(ys), x2, BottomAlign(ys), cursor != CURSOR_SUSTAIN);
      return x2;
    }

    int draw_release(const Channel &c, int x, int length) {
      int x2 = x + Proportion(c.stage_value[STAGE_RELEASE], length, 62);
      int ys = Proportion(c.stage_value[STAGE_SUSTAIN], STAGE_MAX_VALUE, DISPLAY_HEIGHT);
      gfxLine(x, BottomAlign(ys), x2, BottomAlign(0), cursor != CURSOR_RELEASE);
      return x2;
    }
};

#endif
