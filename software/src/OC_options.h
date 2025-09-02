/*
 *
 * compile options.
 *
 */
// Edit this file to customize when compiling with Arduino IDE
// Sets of these flags are also defined in platformio.ini

#ifndef OC_OPTIONS_H_
#define OC_OPTIONS_H_

/* ------------ uncomment for use with all Northernlight 4U modules - cOC, 2OC, hOC ---------- */
//#define NORTHERNLIGHT
/* ------------ uncomment for use with Northernlight 2OC on the left side: --------------------------  */
//#define NORTHERNLIGHT_2OC_LEFTSIDE
/* ------------ uncomment for the Northernlight hOC (to correct button mappings) ----- */
//#define NLM_hOC
/* ------------ uncomment for the Northernlight cardOC (to correct button mappings) ----- */
//#define NLM_cardOC
/* ------------ uncomment for older hardware revisions of the Northernlight 2OC ----- */
//#define NLM_DIY

/* ------------ print debug messages to USB serial --------------------------------------------------  */
//#define PRINT_DEBUG
/* ------------ flip screen / IO mapping ------------------------------------------------------------  */
//#define FLIP_180
/* ------------ invert screen pixels ----------------------------------------------------------------  */
//#define INVERT_DISPLAY
/* ------------ use DAC8564 -------------------------------------------------------------------------  */
//#define DAC8564


/* ------------ uncomment for use with Plum Audio VOR enabled versions (OCP, 1uO_c v2, 4Robots) -----  */
//#define VOR

// idk what this means so I'm keeping it -NJM
#if defined(VOR)
  #define IO_10V
#endif

// backward compatibility
#if defined(NORTHERNLIGHT)
  #define BUCHLA_4U
  #define BUCHLA_cOC
#endif

// Here are some custom flags:
/* --- special Phazerville mode w/ easter eggs --- */
// #define PEWPEWPEW
/* --- alternate Grids patterns for DrumMap applet --- */
// #define DRUMMAP_GRIDS2
// 16 presets in Hemisphere
// #define MOAR_PRESETS


/* Flags for the full-width apps, these enable/disable them in OC_apps.ino but also zero out the app   */
/* files to prevent them from taking up space.                                                         */

// #define ENABLE_APP_CALIBR8OR
// #define ENABLE_APP_SCENES
// #define ENABLE_APP_ENIGMA
// #define ENABLE_APP_MIDI
// #define ENABLE_APP_PONG
// #define ENABLE_APP_PIQUED
// #define ENABLE_APP_POLYLFO
// #define ENABLE_APP_H1200
// #define ENABLE_APP_BYTEBEATGEN
// #define ENABLE_APP_NEURAL_NETWORK
// #define ENABLE_APP_DARKEST_TIMELINE
// #define ENABLE_APP_LORENZ
// #define ENABLE_APP_ASR
// #define ENABLE_APP_QUANTERMAIN
// #define ENABLE_APP_METAQ
// #define ENABLE_APP_CHORDS
// #define ENABLE_APP_PASSENCORE
// #define ENABLE_APP_SEQUINS
// #define ENABLE_APP_AUTOMATONNETZ
// #define ENABLE_APP_BBGEN
// #define ENABLE_APP_REFERENCES

// Disable Hemisphere and all the applets, freeing up space.
// If you really want to squeeze everything else in, use this.
// If you want to exclude individual applets, edit hemisphere_config.h
// #define NO_HEMISPHERE

#endif

