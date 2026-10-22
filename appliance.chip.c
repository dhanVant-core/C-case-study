/*
 * appliance.chip.c  -  Wokwi custom chip: household appliance + ACS712 current sensor
 * ----------------------------------------------------------------------------------
 * One chip file, used ten times in diagram.json. The "kind" attribute chooses what
 * each copy simulates:
 *
 *   kind 0 Refrigerator     (ACS712-20A)     kind 5 Air conditioner (ACS712-30A)
 *   kind 1 Microwave        (ACS712-20A)     kind 6 Washing machine (ACS712-20A)
 *   kind 2 Dishwasher       (ACS712-20A)     kind 7 Vacuum cleaner  (ACS712-20A)
 *   kind 3 Television       (ACS712-5A)      kind 8 Clothes dryer   (ACS712-20A)
 *   kind 4 Ceiling fan      (ACS712-5A)      kind 9 ZMPT101B mains voltage sensor
 *
 * What the chip does
 *   1. Behaviour model (every 10 ms of simulated time): decides what the appliance is
 *      doing in "home time" (compressor on/off, wash phase, heater cycling ...).
 *      Home time runs TIME_SCALE times faster than simulator time, so two home days
 *      pass in a few minutes. timeScale must match TIME_SCALE in sketch.ino.
 *   2. Waveform (every 500 us of simulated time): produces the real 50 Hz current,
 *      with power factor and harmonics, and converts it to the ACS712 output voltage
 *            Vout = 2.5 V + current x sensitivity + sensor noise
 *      which the Arduino reads with analogRead().
 *
 * Fault control: click the chip while the simulation runs and move the "Fault" slider.
 *   0 = healthy. 1..3 = the faults listed in README.md for that appliance.
 */
#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#define TWO_PI_F        6.2831853f
#define MAINS_HZ        50.0f
#define WAVE_STEP_US    500     /* waveform update period (simulator time)   */
#define MODEL_STEP_US   10000   /* behaviour update period (simulator time)  */
#define DAY_S           86400.0

enum { FRIDGE, MICROWAVE, DISHWASHER, TV, FAN, AC, WASHER, VACUUM, DRYER, VOLTAGE };

/* A load: RMS fundamental current (A), displacement power factor, 3rd and 5th
   harmonic ratios, and random amplitude variation from cycle to cycle. */
typedef struct { float I, pf, h3, h5, noise; } load_t;

/* Program phases for washing machine and dishwasher */
enum { PH_FILL, PH_HEAT, PH_MOTOR, PH_PUMP, PH_SPIN, PH_DRAIN, PH_DRY };
typedef struct { uint8_t type; float dur; float target; float max; } phase_t;

static const phase_t WASH_PROG[] = {
  {PH_FILL, 180, 0, 0}, {PH_HEAT, 0, 40, 3600}, {PH_MOTOR, 2100, 0, 0}, {PH_DRAIN, 120, 0, 0},
  {PH_FILL, 120, 0, 0}, {PH_MOTOR, 360, 0, 0}, {PH_DRAIN, 120, 0, 0},
  {PH_FILL, 120, 0, 0}, {PH_MOTOR, 360, 0, 0}, {PH_DRAIN, 120, 0, 0}, {PH_SPIN, 600, 0, 0}
};
static const phase_t DISH_PROG[] = {
  {PH_FILL, 150, 0, 0}, {PH_PUMP, 600, 0, 0}, {PH_HEAT, 0, 55, 2400}, {PH_PUMP, 900, 0, 0},
  {PH_DRAIN, 90, 0, 0}, {PH_FILL, 120, 0, 0}, {PH_HEAT, 0, 65, 2400}, {PH_DRY, 1500, 0, 0},
  {PH_DRAIN, 90, 0, 0}
};
#define WASH_N (sizeof(WASH_PROG) / sizeof(WASH_PROG[0]))
#d