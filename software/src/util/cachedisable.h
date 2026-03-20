#pragma once
// LOTS of #defines and structures stolen from core_cm7.h
// far simpler to include that but then we get other missing include file errors.

#define SCB_CCR_DC_Pos                      16U                                           /*!< SCB CCR: Cache enable bit Position */
#define SCB_CCR_DC_Msk                     (1UL << SCB_CCR_DC_Pos)                        /*!< SCB CCR: Cache enable bit Mask */

#define SCB_CCSIDR_NUMSETS_Pos             13U                                            /*!< SCB CCSIDR: NumSets Position */
#define SCB_CCSIDR_NUMSETS_Msk             (0x7FFFUL << SCB_CCSIDR_NUMSETS_Pos)           /*!< SCB CCSIDR: NumSets Mask */

#define SCB_CCSIDR_ASSOCIATIVITY_Pos        3U                                            /*!< SCB CCSIDR: Associativity Position */
#define SCB_CCSIDR_ASSOCIATIVITY_Msk       (0x3FFUL << SCB_CCSIDR_ASSOCIATIVITY_Pos)      /*!< SCB CCSIDR: Associativity Mask */


/* SCB Configuration Control Register Definitions */
#define SCB_CCR_BP_Pos                      18U                                           /*!< SCB CCR: Branch prediction enable bit Position */
#define SCB_CCR_BP_Msk                     (1UL << SCB_CCR_BP_Pos)                        /*!< SCB CCR: Branch prediction enable bit Mask */

#define SCB_CCR_IC_Pos                      17U                                           /*!< SCB CCR: Instruction cache enable bit Position */
#define SCB_CCR_IC_Msk                     (1UL << SCB_CCR_IC_Pos)                        /*!< SCB CCR: Instruction cache enable bit Mask */


/* SCB D-Cache Clean and Invalidate by Set-way Register Definitions */
#define SCB_DCCISW_WAY_Pos                 30U                                            /*!< SCB DCCISW: Way Position */
#define SCB_DCCISW_WAY_Msk                 (3UL << SCB_DCCISW_WAY_Pos)                    /*!< SCB DCCISW: Way Mask */

#define SCB_DCCISW_SET_Pos                  5U                                            /*!< SCB DCCISW: Set Position */
#define SCB_DCCISW_SET_Msk                 (0x1FFUL << SCB_DCCISW_SET_Pos)                /*!< SCB DCCISW: Set Mask */

/* Cache Size ID Register Macros */
#define CCSIDR_WAYS(x)         (((x) & SCB_CCSIDR_ASSOCIATIVITY_Msk) >> SCB_CCSIDR_ASSOCIATIVITY_Pos)
#define CCSIDR_SETS(x)         (((x) & SCB_CCSIDR_NUMSETS_Msk      ) >> SCB_CCSIDR_NUMSETS_Pos      )

#ifdef __cplusplus
  #define   __I     volatile             /*!< Defines 'read only' permissions */
#else
  #define   __I     volatile const       /*!< Defines 'read only' permissions */
#endif
#define     __O     volatile             /*!< Defines 'write only' permissions */
#define     __IO    volatile             /*!< Defines 'read / write' permissions */

/* following defines should be used for structure members */
#define     __IM     volatile const      /*! Defines 'read only' structure member permissions */
#define     __OM     volatile            /*! Defines 'write only' structure member permissions */
#define     __IOM    volatile            /*! Defines 'read / write' structure member permissions */

/**
  \brief  Structure type to access the System Control Block (SCB).
 */
typedef struct
{
  __IM  uint32_t CPUID;                  /*!< Offset: 0x000 (R/ )  CPUID Base Register */
  __IOM uint32_t ICSR;                   /*!< Offset: 0x004 (R/W)  Interrupt Control and State Register */
  __IOM uint32_t VTOR;                   /*!< Offset: 0x008 (R/W)  Vector Table Offset Register */
  __IOM uint32_t AIRCR;                  /*!< Offset: 0x00C (R/W)  Application Interrupt and Reset Control Register */
  __IOM uint32_t SCR;                    /*!< Offset: 0x010 (R/W)  System Control Register */
  __IOM uint32_t CCR;                    /*!< Offset: 0x014 (R/W)  Configuration Control Register */
  __IOM uint8_t  SHPR[12U];              /*!< Offset: 0x018 (R/W)  System Handlers Priority Registers (4-7, 8-11, 12-15) */
  __IOM uint32_t SHCSR;                  /*!< Offset: 0x024 (R/W)  System Handler Control and State Register */
  __IOM uint32_t CFSR;                   /*!< Offset: 0x028 (R/W)  Configurable Fault Status Register */
  __IOM uint32_t HFSR;                   /*!< Offset: 0x02C (R/W)  HardFault Status Register */
  __IOM uint32_t DFSR;                   /*!< Offset: 0x030 (R/W)  Debug Fault Status Register */
  __IOM uint32_t MMFAR;                  /*!< Offset: 0x034 (R/W)  MemManage Fault Address Register */
  __IOM uint32_t BFAR;                   /*!< Offset: 0x038 (R/W)  BusFault Address Register */
  __IOM uint32_t AFSR;                   /*!< Offset: 0x03C (R/W)  Auxiliary Fault Status Register */
  __IM  uint32_t ID_PFR[2U];             /*!< Offset: 0x040 (R/ )  Processor Feature Register */
  __IM  uint32_t ID_DFR;                 /*!< Offset: 0x048 (R/ )  Debug Feature Register */
  __IM  uint32_t ID_AFR;                 /*!< Offset: 0x04C (R/ )  Auxiliary Feature Register */
  __IM  uint32_t ID_MFR[4U];             /*!< Offset: 0x050 (R/ )  Memory Model Feature Register */
  __IM  uint32_t ID_ISAR[5U];            /*!< Offset: 0x060 (R/ )  Instruction Set Attributes Register */
        uint32_t RESERVED0[1U];
  __IM  uint32_t CLIDR;                  /*!< Offset: 0x078 (R/ )  Cache Level ID register */
  __IM  uint32_t CTR;                    /*!< Offset: 0x07C (R/ )  Cache Type register */
  __IM  uint32_t CCSIDR;                 /*!< Offset: 0x080 (R/ )  Cache Size ID Register */
  __IOM uint32_t CSSELR;                 /*!< Offset: 0x084 (R/W)  Cache Size Selection Register */
  __IOM uint32_t CPACR;                  /*!< Offset: 0x088 (R/W)  Coprocessor Access Control Register */
        uint32_t RESERVED3[93U];
  __OM  uint32_t STIR;                   /*!< Offset: 0x200 ( /W)  Software Triggered Interrupt Register */
        uint32_t RESERVED4[15U];
  __IM  uint32_t MVFR0;                  /*!< Offset: 0x240 (R/ )  Media and VFP Feature Register 0 */
  __IM  uint32_t MVFR1;                  /*!< Offset: 0x244 (R/ )  Media and VFP Feature Register 1 */
  __IM  uint32_t MVFR2;                  /*!< Offset: 0x248 (R/ )  Media and VFP Feature Register 2 */
        uint32_t RESERVED5[1U];
  __OM  uint32_t ICIALLU;                /*!< Offset: 0x250 ( /W)  I-Cache Invalidate All to PoU */
        uint32_t RESERVED6[1U];
  __OM  uint32_t ICIMVAU;                /*!< Offset: 0x258 ( /W)  I-Cache Invalidate by MVA to PoU */
  __OM  uint32_t DCIMVAC;                /*!< Offset: 0x25C ( /W)  D-Cache Invalidate by MVA to PoC */
  __OM  uint32_t DCISW;                  /*!< Offset: 0x260 ( /W)  D-Cache Invalidate by Set-way */
  __OM  uint32_t DCCMVAU;                /*!< Offset: 0x264 ( /W)  D-Cache Clean by MVA to PoU */
  __OM  uint32_t DCCMVAC;                /*!< Offset: 0x268 ( /W)  D-Cache Clean by MVA to PoC */
  __OM  uint32_t DCCSW;                  /*!< Offset: 0x26C ( /W)  D-Cache Clean by Set-way */
  __OM  uint32_t DCCIMVAC;               /*!< Offset: 0x270 ( /W)  D-Cache Clean and Invalidate by MVA to PoC */
  __OM  uint32_t DCCISW;                 /*!< Offset: 0x274 ( /W)  D-Cache Clean and Invalidate by Set-way */
        uint32_t RESERVED7[6U];
  __IOM uint32_t ITCMCR;                 /*!< Offset: 0x290 (R/W)  Instruction Tightly-Coupled Memory Control Register */
  __IOM uint32_t DTCMCR;                 /*!< Offset: 0x294 (R/W)  Data Tightly-Coupled Memory Control Registers */
  __IOM uint32_t AHBPCR;                 /*!< Offset: 0x298 (R/W)  AHBP Control Register */
  __IOM uint32_t CACR;                   /*!< Offset: 0x29C (R/W)  L1 Cache Control Register */
  __IOM uint32_t AHBSCR;                 /*!< Offset: 0x2A0 (R/W)  AHB Slave Control Register */
        uint32_t RESERVED8[1U];
  __IOM uint32_t ABFSR;                  /*!< Offset: 0x2A8 (R/W)  Auxiliary Bus Fault Status Register */
} SCB_Type;

#define SCS_BASE            (0xE000E000UL)                            /*!< System Control Space Base Address */
#define SCB_BASE            (SCS_BASE +  0x0D00UL)                    /*!< System Control Block Base Address */
#define SCB                 ((SCB_Type       *)     SCB_BASE      )   /*!< SCB configuration struct */


FLASHMEM static void disableCache() {
  SCB_MPU_CTRL = 0; // turn off MPU
  SYST_CSR = 0; // turn off system tick
  for (int i=0;i<NVIC_NUM_INTERRUPTS;i++) {
    NVIC_DISABLE_IRQ(i);
  }
  uint32_t ccsidr;
  uint32_t sets;
  uint32_t ways;

  SCB->CSSELR = 0U; /*(0U << 1U) | 0U;*/  /* Level 1 data cache */
    asm("dsb");

  SCB->CCR &= ~(uint32_t)SCB_CCR_DC_Msk;  /* disable D-Cache */
    asm("dsb");

  ccsidr = SCB->CCSIDR;
                                          /* clean & invalidate D-Cache */
  sets = (uint32_t)(CCSIDR_SETS(ccsidr));
  do {
    ways = (uint32_t)(CCSIDR_WAYS(ccsidr));
    do {
      SCB->DCCISW = (((sets << SCB_DCCISW_SET_Pos) & SCB_DCCISW_SET_Msk) |
                     ((ways << SCB_DCCISW_WAY_Pos) & SCB_DCCISW_WAY_Msk)  );
    } while (ways-- != 0U);
  } while(sets-- != 0U);

  asm("dsb");
  asm("isb");
  SCB->CCR &= ~(uint32_t)SCB_CCR_IC_Msk;  /* disable I-Cache */
  SCB->ICIALLU = 0UL;                     /* invalidate I-Cache */
  asm("dsb");
  asm("isb");
}
