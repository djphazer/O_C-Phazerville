#include "tideslite.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  TidesLiteSample sample;
  uint64_t samples = atol(argv[1]);
  int16_t pitch = atoi(argv[2]);
  uint32_t phase_inc = ComputePhaseIncrement(pitch);
  uint16_t slope = atoi(argv[3]);
  uint16_t shape = atoi(argv[4]);
  int16_t fold = atoi(argv[5]);

  uint32_t phase = 0;
  printf("unipolar,bipolar,high,low\n");
  for (uint64_t i = 0; i < samples; i++) {
    ProcessSample(slope, shape, fold, phase, sample);
    printf("%d,%d,%d,%d\n", sample.unipolar, sample.bipolar,
           (sample.flags & FLAG_EOA) ? 65535 : 0,
           (sample.flags & FLAG_EOR) ? 65535 : 0);
    phase += phase_inc;
  }
  return 0;
}