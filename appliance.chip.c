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
#define DISH_N (sizeof(DISH_PROG) / sizeof(DISH_PROG[0]))

typedef struct {
  pin_t out;
  uint32_t attr_kind, attr_fault, attr_scale;
  int kind, fault;
  float scale, sens;            /* time scale, ACS712 sensitivity in V/A */
  uint32_t rng;

  /* waveform coefficients (amps, peak) updated by the behaviour model */
  float a1s, a1c, a3s, a3c, a5;
  float inrush_amp;             /* peak amps of the current start-up surge */
  uint64_t inrush_until_ns;

  double last_home; int day;

  /* thermostat appliances (fridge, AC) */
  float temp, since, trip, off_time; bool on;
  /* scheduled on-windows (hours) and one-off events (home seconds) */
  float win_on[3], win_off[3]; int nwin;
  double ev_start[3], ev_end[3]; int nev;
  /* program appliances */
  double prog_start; bool running; int pi; float pt, water;
  /* dryer */
  float drum, damp; bool heater;
} chip_t;

/* ---------- small helpers ---------- */
static float frand(chip_t *c) {                 /* uniform 0..1 */
  c->rng = c->rng * 1664525u + 1013904223u;
  return (float)(c->rng >> 8) / 16777216.0f;
}
static float fnoise(chip_t *c) {                /* approx. normal, mean 0, sd 1 */
  return (frand(c) + frand(c) + frand(c) - 1.5f) * 2.0f;
}
static load_t L(float I, float pf, float h3, float h5, float noise) {
  load_t l = { I, pf, h3, h5, noise }; return l;
}
static float ambient(double home) {             /* outdoor temperature, deg C */
  float h = (float)(fmod(home, DAY_S) / 3600.0);
  return 30.0f + 4.0f * sinf(TWO_PI_F * (h - 9.0f) / 24.0f);
}
static bool in_windows(chip_t *c, float hour) {
  for (int i = 0; i < c->nwin; i++) if (hour >= c->win_on[i] && hour < c->win_off[i]) return true;
  return false;
}
static bool in_events(chip_t *c, double home) {
  for (int i = 0; i < c->nev; i++) if (home >= c->ev_start[i] && home < c->ev_end[i]) return true;
  return false;
}
static void start_inrush(chip_t *c, float amps_rms) {  /* surge lasting 2 mains cycles */
  c->inrush_amp = amps_rms * 1.41421f;
  c->inrush_until_ns = get_sim_nanos() + 40000000ull;
}

/* Turn a load into waveform coefficients, with this cycle's random variation */
static void set_load(chip_t *c, load_t l) {
  float j = 1.0f + (l.noise > 0 ? fnoise(c) * l.noise : 0.0f);
  if (j < 0) j = 0;
  float A = 1.41421f * l.I * j;
  float pf = l.pf < 0.05f ? 0.05f : (l.pf > 1.0f ? 1.0f : l.pf);
  float phi = acosf(pf);
  c->a1s = A * cosf(phi);  c->a1c = -A * sinf(phi);
  c->a3s = A * l.h3 * cosf(0.3f); c->a3c = -A * l.h3 * sinf(0.3f);
  c->a5 = A * l.h5;
}

/* ---------- daily plans (called when a new home day starts) ---------- */
static void plan_day(chip_t *c, int d) {
  double base = d * DAY_S;
  float j = (frand(c) - 0.5f) * 0.3f;            /* +/- 9 minutes of daily variation */
  switch (c->kind) {
    case AC:  c->nwin = 3; c->win_on[0] = 0;  c->win_off[0] = 6 + j; c->win_on[1] = 14 + j;
              c->win_off[1] = 17;    c->win_on[2] = 22.5f + j; c->win_off[2] = 24; break;
    case FAN: c->nwin = 3; c->win_on[0] = 0; 