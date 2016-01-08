/*
* ornament + crime // 4xCV DAC8565  // "ASR" 
*
* --------------------------------
* TR 1 = clock
* TR 2 = hold
* TR 3 = oct +
* TR 4 = oct -
*
* CV 1 = sample in
* CV 2 = index CV
* CV 3 = # notes (constrain)
* CV 4 = octaves/offset
*
* left  encoder = scale select
* right encoder = param. select
* 
* button 1 (top) =  oct +
* button 2       =  oct -
* --------------------------------
*
*/

#include <spi4teensy3.h>
#include <u8g_teensy.h>
#include <rotaryplus.h>
#include <EEPROM.h>
#include "util_app.h"
#include "util_button.h"
#include "util_pagestorage.h"
#include "EEPROMStorage.h"
#include "DAC.h"

//#define ENABLE_DEBUG_PINS
#define DEBUG_PIN_1 24
#define DEBUG_PIN_2 25
#include "util_debugpins.h"

#define CS 10  // DAC CS 
#define RST 9  // DAC RST

#define CV1 19
#define CV2 18
#define CV3 20
#define CV4 17

#define TR1 0
#define TR2 1
#define TR3 2
#define TR4 3

#define encR1 15
#define encR2 16
#define butR  14

#define encL1 22
#define encL2 21
#define butL  23

#define but_top 5
#define but_bot 4

U8GLIB u8g(&u8g_dev_sh1106_128x64_2x_hw_spi, u8g_com_hw_spi_fn);

Rotary encoder[2] =
{
  {encL1, encL2}, 
  {encR1, encR2}
}; 

//  UI mode select
extern uint8_t UI_MODE;

/*  ------------------------ ASR ------------------------------------  */

#define MAX_VALUE 65535 // DAC fullscale 
#define MAX_ITEMS 256   // ASR ring buffer size
#define OCTAVES 10      // # octaves
uint16_t octaves[OCTAVES+1] = {0, 6553, 13107, 19661, 26214, 32768, 39321, 45875, 52428, 58981, 65535}; // in practice  
const uint16_t THEORY[OCTAVES+1] = {0, 6553, 13107, 19661, 26214, 32768, 39321, 45875, 52428, 58981, 65535}; // in theory  
extern const uint16_t _ZERO;

/*  ---------------------  CV   stuff  --------------------------------- */

#define _ADC_RATE 1000 // 100us = 1kHz
#define _ADC_RES  12
#define numADC 4
int16_t cvval[numADC];                        // store cv values
volatile uint_fast8_t _ADC = false;

/*  --------------------- clk / buttons / ISR -------------------------   */

uint32_t _CLK_TIMESTAMP = 0;
uint32_t _BUTTONS_TIMESTAMP = 0;
const uint16_t TRIG_LENGTH = 150;
const uint16_t DEBOUNCE = 250;

volatile int CLK_STATE[4] = {0,0,0,0};
#define CLK_STATE1 (CLK_STATE[TR1])

void FASTRUN tr1_ISR() {  
    CLK_STATE[TR1] = true; 
}  // main clock

void FASTRUN tr2_ISR() {
  CLK_STATE[TR2] = true;
}

void FASTRUN tr3_ISR() {
  CLK_STATE[TR3] = true;
}

void FASTRUN tr4_ISR() {
  CLK_STATE[TR4] = true;
}

uint32_t _UI_TIMESTAMP;

enum the_buttons 
{  
  BUTTON_TOP,
  BUTTON_BOTTOM,
  BUTTON_LEFT,
  BUTTON_RIGHT
};

enum encoders
{
  LEFT,
  RIGHT
};

volatile uint_fast8_t _ENC = false;
static const uint32_t _ENC_RATE = 15000;

/*  ------------------------ core timer ISR ---------------------------   */

static const uint32_t CORE_TIMER_RATE = 100; // 100uS = 10 Khz
IntervalTimer CORE_timer;

uint32_t ADC_timer_counter = 0;
uint32_t ENC_timer_counter = 0;

void FASTRUN CORE_timer_ISR() {
  DEBUG_PIN_SCOPE(DEBUG_PIN_2);

  DAC::WriteAll();

  // Update ADC/ENC "ISR" here at 1kHz instead of having an extra timer interrupts
  if (ADC_timer_counter < _ADC_RATE / CORE_TIMER_RATE - 1) {
    ++ADC_timer_counter;
  } else {
    ADC_timer_counter = 0;
    _ADC = true;
  }

  if (ENC_timer_counter < _ENC_RATE / CORE_TIMER_RATE - 1) {
    ++ENC_timer_counter;
  } else {
    ENC_timer_counter = 0;
    _ENC = true;
  }
}

/*       ---------------------------------------------------------         */

void setup(){
  
  NVIC_SET_PRIORITY(IRQ_PORTB, 0); // TR1 = 0 = PTB16
  analogReadResolution(_ADC_RES);
  analogReadAveraging(0x10);
  spi4teensy3::init();
  delay(10);
  // pins 
  pinMode(butL, INPUT);
  pinMode(butR, INPUT);
  pinMode(but_top, INPUT);
  pinMode(but_bot, INPUT);
  buttons_init();
 
  pinMode(TR1, INPUT); // INPUT_PULLUP);
  pinMode(TR2, INPUT);
  pinMode(TR3, INPUT);
  pinMode(TR4, INPUT);

  DebugPins::Init();

  // clock ISR 
  attachInterrupt(TR1, tr1_ISR, FALLING);
  attachInterrupt(TR2, tr2_ISR, FALLING);
  attachInterrupt(TR3, tr3_ISR, FALLING);
  attachInterrupt(TR4, tr4_ISR, FALLING);
  // encoder ISR 
  attachInterrupt(encL1, left_encoder_ISR, CHANGE);
  attachInterrupt(encL2, left_encoder_ISR, CHANGE);
  attachInterrupt(encR1, right_encoder_ISR, CHANGE);
  attachInterrupt(encR2, right_encoder_ISR, CHANGE);
  // set up DAC pins 
  pinMode(CS, OUTPUT);
  pinMode(RST,OUTPUT);
  // pull RST high 
  digitalWrite(RST, HIGH); 

  // set all outputs to zero 
  DAC::Init();
  DAC::WriteAll();

  Serial.begin(9600); 

  CORE_timer.begin(CORE_timer_ISR, CORE_TIMER_RATE);

  // splash screen, sort of ... 
  hello();
  delay(2000);
  // calibrate? else use EEPROM; else use things in theory :
  if (!digitalRead(butL))  calibrate_main();
  else if (EEPROM.read(0x2) > 0) read_settings(); 
  else in_theory(); // uncalibrated DAC code 
  delay(750);   
  // initialize 
  init_DACtable();
  init_apps();
}


/*  ---------    main loop  --------  */

//uint32_t testclock;

void loop() {
  while (1) {
    // don't change current_app while it's running
    if (SELECT_APP) select_app();
    current_app->loop();
    if (UI_MODE) timeout();
  }
}
