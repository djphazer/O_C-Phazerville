// Copyright 2014 Émilie Gillet.
// Copyright 2016 Tim Churches
//
// Original Author: Émilie Gillet (ol.gillet@gmail.com)
// Modifications for use of this code in firmare for the Ornament and Crime module:
// Tim Churches (tim.churches@gmail.com)
//
// Idea for using Rössler generator attributable to Hotlblack Desiato
// (see http://forbinthesynthesizer.blogspot.com.au/2015/11/rossler-barrow.html)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// Lorenz and Rössler systems.


#include "streams_lorenz_generator.h"
#include "streams_resources.h"

namespace streams {

// using namespace stmlib;

const int64_t sigma = 10.0 * (1 << 24);
//const int64_t rho = 28.0 * (1 << 24);
const int64_t beta = 8.0 / 3.0 * (1 << 24);

// Rossler constants
const int64_t a = 0.1 * (1 << 24);
const int64_t b = 0.1 * (1 << 24);
// const int64_t c = 13.0 * (1 << 24);

void LorenzGenerator::Init(uint8_t index) {
  if (index) {
    Lx2_ = 0.1 * (1 << 24);
    Ly2_ = 0;
    Lz2_ = 0;
    Rx2_ = 0.1 * (1 << 24);
    Ry2_ = 0;
    Rz2_ = 0;
  } else {
    Lx1_ = 0.1 * (1 << 24);
    Ly1_ = 0;
    Lz1_ = 0;
    Rx1_ = 0.1 * (1 << 24);
    Ry1_ = 0;
    Rz1_ = 0;
  }
}

void LorenzGenerator::Process(
    int32_t freq1,
    int32_t freq2,
    bool reset1,
    bool reset2,
    uint8_t freq_range1,
    uint8_t freq_range2) {
  int32_t rate1 =  (freq1 >> 8);
  if (rate1 < 0) rate1 = 0;
  if (rate1 > 255) rate1 = 255;
  int32_t rate2 = (freq2 >> 8);
  if (rate2 < 0) rate2 = 0;
  if (rate2 > 255) rate2 = 255;

  if (reset1) Init(0) ;
  if (reset2) Init(1) ; 

  // --- State updates ---
  // For performance, we only calculate values for a given
  // generator if one of its outputs (X, Y, or Z) is currently assigned to one
  // of the four DAC channels. This avoids redundant calculations while ensuring
  // the underlying state of the generator is always continuous.

  int32_t Lz1_scaled = 0;
  int32_t Lx1_scaled = 0;
  int32_t Ly1_scaled = 0;

  int32_t Rz1_scaled = 0;
  int32_t Rx1_scaled = 0;
  int32_t Ry1_scaled = 0;

  int32_t Lz2_scaled = 0;
  int32_t Lx2_scaled = 0;
  int32_t Ly2_scaled = 0;

  int32_t Rz2_scaled = 0;
  int32_t Rx2_scaled = 0;
  int32_t Ry2_scaled = 0;

  if (lorenz1_active) {
    int64_t Ldt1 = static_cast<int64_t>(lut_lorenz_rate[rate1] >> (5 - freq_range1));
    Lorenz(Lx1_, Ly1_, Lz1_, rho1_, sigma, beta, Ldt1);
    ScaleLorenz(Lx1_, Ly1_, Lz1_, Lx1_scaled, Ly1_scaled, Lz1_scaled);
  }

  if (lorenz2_active) {
    int64_t Ldt2 = static_cast<int64_t>(lut_lorenz_rate[rate2] >> (5 - freq_range2));
    Lorenz(Lx2_, Ly2_, Lz2_, rho2_, sigma, beta, Ldt2);
    ScaleLorenz(Lx2_, Ly2_, Lz2_, Lx2_scaled, Ly2_scaled, Lz2_scaled);
  }

  if (rossler1_active) {
    int64_t Rdt1 = static_cast<int64_t>(lut_lorenz_rate[rate1] >> 0);
    Rossler(Rx1_, Ry1_, Rz1_, c1_, a, b, Rdt1);
    ScaleRossler(Rx1_, Ry1_, Rz1_, Rx1_scaled, Ry1_scaled, Rz1_scaled);
  }

  if (rossler2_active) {
    int64_t Rdt2 = static_cast<int64_t>(lut_lorenz_rate[rate2] >> 0);
    Rossler(Rx2_, Ry2_, Rz2_, c2_, a, b, Rdt2);
    ScaleRossler(Rx2_, Ry2_, Rz2_, Rx2_scaled, Ry2_scaled, Rz2_scaled);
  }

  // --- Output mapping ---
  for (uint8_t i = 0; i < kNumChannels; ++i) {
    switch (out_[i]) {
      case LORENZ_OUTPUT_X1:
        dac_code_[i] = Lx1_scaled;
        break;
      case LORENZ_OUTPUT_Y1:
        dac_code_[i] = Ly1_scaled;
        break;
      case LORENZ_OUTPUT_Z1:
        dac_code_[i] = Lz1_scaled;
        break;
      case LORENZ_OUTPUT_X2:
        dac_code_[i] = Lx2_scaled;
        break;
      case LORENZ_OUTPUT_Y2:
        dac_code_[i] = Ly2_scaled;
        break;
      case LORENZ_OUTPUT_Z2:
        dac_code_[i] = Lz2_scaled;
        break;
      case ROSSLER_OUTPUT_X1:
        dac_code_[i] = Rx1_scaled;
        break;
      case ROSSLER_OUTPUT_Y1:
        dac_code_[i] = Ry1_scaled;
        break;
      case ROSSLER_OUTPUT_Z1:
        dac_code_[i] = Rz1_scaled;
        break;
      case ROSSLER_OUTPUT_X2:
        dac_code_[i] = Rx2_scaled;
        break;
      case ROSSLER_OUTPUT_Y2:
        dac_code_[i] = Ry2_scaled;
        break;
      case ROSSLER_OUTPUT_Z2:
        dac_code_[i] = Rz2_scaled;
        break;
      case LORENZ_OUTPUT_LX1_PLUS_RX1:
        dac_code_[i] = (Lx1_scaled + Rx1_scaled) >> 1;
        break;
      case LORENZ_OUTPUT_LX1_PLUS_RZ1:
        dac_code_[i] = (Lx1_scaled + Rz1_scaled) >> 1;
        break;
      case LORENZ_OUTPUT_LX1_PLUS_LY2:
        dac_code_[i] = (Lx1_scaled + Ly2_scaled) >> 1;
        break;
      case LORENZ_OUTPUT_LX1_PLUS_LZ2:
        dac_code_[i] = (Lx1_scaled + Lz2_scaled) >> 1;
        break;
      case LORENZ_OUTPUT_LX1_PLUS_RX2:
        dac_code_[i] = (Lx1_scaled + Rx2_scaled) >> 1;
        break;
      case LORENZ_OUTPUT_LX1_PLUS_RZ2:
        dac_code_[i] = (Lx1_scaled + Rz2_scaled) >> 1;
        break;
      case LORENZ_OUTPUT_LX1_XOR_LY1:
        dac_code_[i] = Lx1_scaled ^ Ly1_scaled ;
        break;
      case LORENZ_OUTPUT_LX1_XOR_LX2:
        dac_code_[i] = Lx1_scaled ^ Lx2_scaled ;
        break;
      case LORENZ_OUTPUT_LX1_XOR_RX1:
        dac_code_[i] = Lx1_scaled ^ Rx1_scaled ;
        break;
      case LORENZ_OUTPUT_LX1_XOR_RX2:
        dac_code_[i] = Lx1_scaled ^ Rx2_scaled ;
        break;
       default:
        break;
    }
  }

}

void LorenzGenerator::Lorenz(int32_t &x, int32_t &y, int32_t &z, int64_t rho, int64_t sigma, int64_t beta, int64_t dt) {
  int32_t next_x = x + (dt * ((sigma * (y - x)) >> 24) >> 24);
  int32_t next_y = y + (dt * ((x * (rho - z) >> 24) - y) >> 24);
  int32_t next_z = z + (dt * ((x * int64_t(next_y) >> 24) - (beta * z >> 24)) >> 24); 
  x = next_x;
  y = next_y;
  z = next_z;
}

void LorenzGenerator::Rossler(int32_t &x, int32_t &y, int32_t &z, int64_t c, int64_t a, int64_t b, int64_t dt) {
  int32_t new_x = x + ((dt * (-y - z)) >> 24);
  int32_t new_y = y + ((dt * (x + ((a * y) >> 24))) >> 24);
  int32_t new_z = z + ((dt * (b + ((z * (x - c)) >> 24))) >> 24);
  x = new_x;
  y = new_y;
  z = new_z;
}

void LorenzGenerator::ScaleLorenz(int32_t x, int32_t y, int32_t z, int32_t &x_scaled, int32_t &y_scaled, int32_t &z_scaled) {
  x_scaled = ((x * 3) >> 16) + 32769;
  y_scaled = ((y * 3) >> 16) + 32769;
  z_scaled = ((z * 3) >> 16);
}

void LorenzGenerator::ScaleRossler(int32_t x, int32_t y, int32_t z, int32_t &x_scaled, int32_t &y_scaled, int32_t &z_scaled) {
  x_scaled = (x >> 14) + 32769;
  y_scaled = (y >> 14) + 32769;
  z_scaled = (z >> 14);
}

void LorenzGenerator::DetermineActiveGenerators() {
  lorenz1_active = false;
  rossler1_active = false;
  lorenz2_active = false;
  rossler2_active = false;

  for (size_t i = 0; i < kNumChannels; ++i) {
    switch (out_[i]) {
      case LORENZ_OUTPUT_X1:
      case LORENZ_OUTPUT_Y1:
      case LORENZ_OUTPUT_Z1:
      case LORENZ_OUTPUT_LX1_XOR_LY1:
        lorenz1_active = true;
        break;

      case LORENZ_OUTPUT_X2:
      case LORENZ_OUTPUT_Y2:
      case LORENZ_OUTPUT_Z2:
        lorenz2_active = true;
        break;

      case ROSSLER_OUTPUT_X1:
      case ROSSLER_OUTPUT_Y1:
      case ROSSLER_OUTPUT_Z1:
        rossler1_active = true;
        break;

      case ROSSLER_OUTPUT_X2:
      case ROSSLER_OUTPUT_Y2:
      case ROSSLER_OUTPUT_Z2:
        rossler2_active = true;
        break;

      case LORENZ_OUTPUT_LX1_PLUS_RX1:
      case LORENZ_OUTPUT_LX1_XOR_RX1:
      case LORENZ_OUTPUT_LX1_PLUS_RZ1:
        lorenz1_active = true;
        rossler1_active = true;
        break;

      case LORENZ_OUTPUT_LX1_PLUS_LY2:
      case LORENZ_OUTPUT_LX1_PLUS_LZ2:
      case LORENZ_OUTPUT_LX1_XOR_LX2:
        lorenz1_active = true;
        lorenz2_active = true;
        break;

      case LORENZ_OUTPUT_LX1_PLUS_RX2:
      case LORENZ_OUTPUT_LX1_XOR_RX2:
      case LORENZ_OUTPUT_LX1_PLUS_RZ2:
        lorenz1_active = true;
        rossler2_active = true;
        break;

      case LORENZ_OUTPUT_LAST:
        break;
    }
  }
}

}  // namespace streams
