// Xenomorphicle: whole-file guard, F32 core is used on T4.1 audio builds only (see README.md)
#ifdef ARDUINO_TEENSY41
#include "basic_DSPutils.h"


/**
 * @brief scale a float vector (range -1.0 - 1.0) to a new float vector
 *  in range of int32_t + saturation.
 *  based on arm_float_to_31 
 * 
 * @param pSrc pointer to the source vector
 * @param pDst 	pointer to the destination verctor
 * @param blockSize 
 */
void scale_float_to_int32range(const float32_t *pSrc, float32_t *pDst, uint32_t blockSize)
{
	uint32_t blkCnt;			 /* Loop counter */
	const float32_t *pIn = pSrc; /* Source pointer */
	/* Loop unrolling: Compute 4 outputs at a time */
	blkCnt = blockSize >> 2U;

	while (blkCnt > 0U)
	{
		/* C = A * 2147483648 */
		/* Convert from float to Q31 and then store the results in the destination buffer */
		*pDst++ = (float32_t)clip_q63_to_q31((q63_t)(*pIn++ * 2147483648.0f));
		*pDst++ = (float32_t)clip_q63_to_q31((q63_t)(*pIn++ * 2147483648.0f));
		*pDst++ = (float32_t)clip_q63_to_q31((q63_t)(*pIn++ * 2147483648.0f));
		*pDst++ = (float32_t)clip_q63_to_q31((q63_t)(*pIn++ * 2147483648.0f));
		blkCnt--;
	}

	/* Loop unrolling: Compute remaining outputs */
	blkCnt = blockSize % 0x4U;

	while (blkCnt > 0U)
	{
		/* C = A * 2147483648 */
		*pDst++ = (float32_t)clip_q63_to_q31((q63_t)(*pIn++ * 2147483648.0f));
		/* Decrement loop counter */
		blkCnt--;
	}
}


#endif // ARDUINO_TEENSY41
