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
    case FAN: c->nwin = 3; c->win_on[0] = 0;  c->win_off[0] = 6.5f + j; c->win_on[1] = 13 + j;
              c->win_off[1] = 16.5f; c->win_on[2] = 21.5f + j; c->win_off[2] = 24; break;
    case TV:  c->nwin = 2; c->win_on[0] = 7;  c->win_off[0] = 8 + j; c->win_on[1] = 19 + j;
              c->win_off[1] = 23 + j; break;
    case MICROWAVE: {
      const float at[3] = { 7.4f, 13.0f, 20.2f };
      float m = (c->fault == 1) ? 1.6f : 1.0f;   /* weak magnetron: food reheated for longer */
      c->nev = 3;
      for (int i = 0; i < 3; i++) {
        c->ev_start[i] = base + (at[i] + (frand(c) - 0.5f) * 0.2f) * 3600.0;
        c->ev_end[i] = c->ev_start[i] + (150.0f + frand(c) * 40.0f) * m;
      }
    } break;
    case VACUUM: {
      float m = (c->fault == 1) ? 1.4f : 1.0f;   /* clogged: cleaning takes longer */
      c->nev = 1; c->ev_start[0] = base + (10.5f + (frand(c) - 0.5f) * 0.3f) * 3600.0;
      c->ev_end[0] = c->ev_start[0] + (1000.0f + frand(c) * 120.0f) * m;
    } break;
    case WASHER:     c->prog_start = base + (9.0f  + frand(c) * 0.5f)  * 3600.0; break;
    case DISHWASHER: c->prog_start = base + (21.5f + frand(c) * 0.25f) * 3600.0; break;
    case DRYER:      c->prog_start = base + (12.5f + frand(c) * 0.3f)  * 3600.0; break;
    default: break;
  }
}

/* ---------- behaviour models: each returns the present load ---------- */
static load_t model_fridge(chip_t *c, double home, float dt) {
  float h = (float)(fmod(home, DAY_S) / 3600.0);
  float rc = 0.35f / 60, rw = 0.15f / 60 * (ambient(home) - 4) / 26;
  if (c->fault == 1) rc *= 0.55f;               /* refrigerant leak: weaker cooling */
  if (c->fault == 2) rw *= 1.5f;                /* worn gasket: warm air leaks in   */
  bool meal = (h > 7 && h < 9) || (h > 12.5f && h < 14) || (h > 19 && h < 21.5f);
  if (meal && frand(c) < dt / 1200.0f) c->temp += 0.6f;   /* door opened */
  if (c->trip > 0) { c->trip -= dt; c->on = false; }
  if (!c->on && c->temp > 5.5f && c->trip <= 0) {
    if (c->fault == 3 && frand(c) < 0.2f) {     /* weak start capacitor: failed start */
      c->trip = 90; start_inrush(c, 9.5f); c->temp += rw * dt;
      return L(5.2f, 0.45f, 0.05f, 0, 0.05f);
    }
    c->on = true; c->since = 0; start_inrush(c, c->fault == 3 ? 9.5f : 5.5f);
  }
  if (c->on && c->temp < 2.5f) c->on = false;
  c->temp += (c->on ? rw - rc : rw) * dt;
  return c->on ? L(c->fault == 1 ? 0.8f : 0.9f, 0.75f, 0.08f, 0.03f, 0.02f) : L(0.012f, 0.9f, 0, 0, 0);
}

static load_t model_ac(chip_t *c, double home, float dt) {
  float h = (float)(fmod(home, DAY_S) / 3600.0), amb = ambient(home) + 2;
  float rc = 0.2f / 60, k = 0.012f / 60, I = 6.2f, band = 0.5f, min_off = 180;
  if (c->fault == 1) { rc *= 0.7f; I *= 1.15f; }       /* clogged filter / dirty coil */
  if (c->fault == 2) { rc *= 0.45f; I *= 0.8f; }       /* low refrigerant             */
  if (c->fault == 3) { band = 0.08f; min_off = 0; }    /* faulty thermostat           */
  bool powered = in_windows(c, h);
  if (!powered) c->on = false;
  else {
    if (!c->on && c->temp > 24 + band && c->off_time >= min_off) { c->on = true; start_inrush(c, 18); }
    if (c->on && c->temp < 24 - band) { c->on = false; c->off_time = 0; }
  }
  c->temp += (k * (amb - c->temp) - (c->on ? rc : 0)) * dt;
  c->off_time += dt;
  if (c->on) return L(I, 0.92f, 0.05f, 0.02f, 0.015f);
  return powered ? L(0.35f, 0.85f, 0.05f, 0, 0) : L(0.01f, 0.5f, 0, 0, 0);
}

static load_t model_fan(chip_t *c, double home) {
  if (!in_windows(c, (float)(fmod(home, DAY_S) / 3600.0))) return L(0, 1, 0, 0, 0);
  if (c->fault == 1) return L(0.36f, 0.82f, 0.06f, 0, 0.1f);    /* worn bearings        */
  if (c->fault == 2) return L(0.33f, 0.66f, 0.12f, 0, 0.02f);   /* weak run capacitor   */
  return L(0.28f, 0.92f, 0.04f, 0, 0.01f);
}

static load_t model_tv(chip_t *c, double home) {
  if (in_windows(c, (float)(fmod(home, DAY_S) / 3600.0)))
    return c->fault == 2 ? L(0.4f, 0.97f, 0.65f, 0.35f, 0.22f) : L(0.48f, 0.97f, 0.65f, 0.35f, 0.06f);
  return c->fault == 1 ? L(0.075f, 0.6f, 0.8f, 0.3f, 0.05f) : L(0.004f, 0.6f, 0.8f, 0, 0);
}

static load_t model_microwave(chip_t *c, double home, float dt) {
  bool on = in_events(c, home);
  if (on && !c->on) start_inrush(c, 11);
  c->on = on;
  if (!on) return L(0.013f, 0.5f, 0, 0, 0);
  if (c->fault == 1) return L(3.8f, 0.95f, 0.18f, 0.05f, 0.08f);          /* ageing magnetron */
  if (c->fault == 2) {                                                   /* arcing HV diode  */
    if (frand(c) < 0.03f) start_inrush(c, 16);
    return L(5.0f, 0.9f, 0.25f, 0.08f, 0.3f);
  }
  return L(5.3f, 0.96f, 0.18f, 0.05f, 0.02f);
}

static load_t model_vacuum(chip_t *c, double home, float dt) {
  bool on = in_events(c, home);
  if (on && !c->on) start_inrush(c, 12);
  c->on = on;
  if (!on) return L(0, 1, 0, 0, 0);
  if (c->fault == 1) return L(4.9f, 0.96f, 0.12f, 0.04f, 0.06f);          /* clogged filter  */
  if (c->fault == 2) {                                                   /* worn brushes    */
    if (frand(c) < 0.015f) start_inrush(c, 14);
    return L(6.1f, 0.88f, 0.14f, 0.2f, 0.15f);
  }
  return L(6.0f, 0.96f, 0.12f, 0.04f, 0.03f);
}

static load_t program_load(chip_t *c, const phase_t *ph, float dt) {
  if (c->kind == WASHER) {
    bool brg = c->fault == 2;
    switch (ph->type) {
      case PH_HEAT:  c->water += (c->fault == 1 ? 0.55f : 1.0f) / 60 * dt;       /* scaled element */
                     return c->fault == 1 ? L(8.1f, 1, 0.01f, 0, 0) : L(8.4f, 1, 0.01f, 0, 0);
      case PH_MOTOR: return brg ? L(1.65f, 0.5f, 0.2f, 0.05f, 0.45f) : L(0.95f, 0.62f, 0.2f, 0.05f, 0.18f);
      case PH_SPIN:  return brg ? L(3.6f, 0.65f, 0.25f, 0.06f, 0.2f) : L(2.3f, 0.78f, 0.25f, 0.05f, 0.08f);
      case PH_DRAIN: return c->fault == 3 ? L(0.6f, 0.45f, 0.05f, 0, 0.15f) : L(0.28f, 0.5f, 0.05f, 0, 0);
      default:       return L(0.08f, 0.6f, 0, 0, 0);
    }
  } else { /* DISHWASHER */
    load_t pump = c->fault == 2 ? L(1.0f, 0.5f, 0.1f, 0, 0.12f) : L(0.55f, 0.6f, 0.08f, 0, 0.02f);
    switch (ph->type) {
      case PH_HEAT:
        if (ph->target > 60 && c->water < 40) c->water = 40;
        if (c->fault == 1) { c->water += 0.02f / 60 * dt; return pump; }           /* open element */
        c->water += (c->fault == 2 ? 0.8f : 1.2f) / 60 * dt;
        return L(8.7f + pump.I, 0.99f, 0.02f, 0, 0.01f);
      case PH_PUMP:  return pump;
      case PH_DRAIN: return L(0.3f, 0.5f, 0.05f, 0, 0);
      case PH_DRY:   return L(0.04f, 0.6f, 0, 0, 0);
      default:       return L(0.08f, 0.6f, 0, 0, 0);
    }
  }
}

static load_t model_program(chip_t *c, double home, float dt) {
  const phase_t *prog = c->kind == WASHER ? WASH_PROG : DISH_PROG;
  int n = c->kind == WASHER ? (int)WASH_N : (int)DISH_N;
  if (!c->running && c->prog_start > 0 && home >= c->prog_start) {
    c->running = true; c->pi = 0; c->pt = 0; c->water = 26 + frand(c) * 3; c->prog_start = -1;
  }
  if (!c->running) return L(0.004f, 0.5f, 0, 0, 0);
  const phase_t *ph = &prog[c->pi];
  c->pt += dt;
  load_t l = program_load(c, ph, dt);
  float dur = ph->dur;
  if (c->kind == WASHER && ph->type == PH_DRAIN && c->fault == 3) dur = 900;  /* clogged drain */
  bool done = ph->type == PH_HEAT ? (c->water >= ph->target || c->pt >= ph->max) : c->pt >= dur;
  if (done) { c->pi++; c->pt = 0; if (c->pi >= n) c->running = false; }
  return l;
}

static load_t model_dryer(chip_t *c, double home, float dt) {
  if (!c->running && c->prog_start > 0 && home >= c->prog_start) {
    c->running = true; c->pi = 0; c->pt = 0; c->drum = 25; c->damp = 0.9f + frand(c) * 0.2f;
    c->heater = true; c->prog_start = -1;
  }
  if (!c->running) return L(0.004f, 0.5f, 0, 0, 0);
  bool half = c->fault == 2, vent = c->fault == 1, belt = c->fault == 3;
  float Ih = half ? 4.8f : 9.6f, Im = belt ? 2.9f : 0.9f;
  load_t motor = L(Im, belt ? 0.42f : 0.65f, 0.12f, 0, belt ? 0.3f : 0.04f);
  c->pt += dt;
  if (c->pi == 0) {                                   /* drying */
    float up = (half ? 1.2f : vent ? 3.5f : 2.0f) / 60, down = (vent ? 1.8f : 3.0f) / 60;
    if (c->heater && c->drum >= 70) c->heater = false;
    if (!c->heater && c->drum <= 60) c->heater = true;
    c->drum += (c->heater ? up : -down) * dt;
    if (c->heater) c->damp -= dt / 2700.0f * (Ih / 9.6f) * (vent ? 0.7f : 1.0f);
    if (c->damp <= 0.03f || c->pt >= 10800) { c->pi = 1; c->pt = 0; }
    return c->heater ? L(Ih + Im, belt ? 0.86f : 0.97f, 0.03f, 0, belt ? 0.1f : 0.02f) : motor;
  }
  if (c->pt >= 600) c->running = false;               /* cooling down */
  return motor;
}

/* ---------- timers ---------- */
static void on_model_step(void *user_data) {
  chip_t *c = (chip_t *)user_data;
  if (c->kind == VOLTAGE) return;
  float f = attr_read_float(c->attr_fault);
  c->fault = (int)(f + 0.5f);
  double home = (double)get_sim_nanos() * 1e-9 * c->scale;
  float dt = (float)(home - c->last_home);
  c->last_home = home;
  int d = (int)(home / DAY_S);
  if (d != c->day) { c->day = d; plan_day(c, d); }
  load_t l;
  switch (c->kind) {
    case FRIDGE:     l = model_fridge(c, home, dt); break;
    case AC:         l = model_ac(c, home, dt); break;
    case FAN:        l = model_fan(c, home); break;
    case TV:         l = model_tv(c, home); break;
    case MICROWAVE:  l = model_microwave(c, home, dt); break;
    case VACUUM:     l = model_vacuum(c, home, dt); break;
    case WASHER:
    case DISHWASHER: l = model_program(c, home, dt); break;
    case DRYER:      l = model_dryer(c, home, dt); break;
    default:         l = L(0, 1, 0, 0, 0);
  }
  set_load(c, l);
}

static void on_wave_step(void *user_data) {
  chip_t *c = (chip_t *)user_data;
  uint64_t ns = get_sim_nanos();
  float cyc = (float)((ns % 20000000ull) * 1e-9) * MAINS_HZ;   /* position within one 20 ms cycle */
  float th = TWO_PI_F * cyc;
  float v;
  if (c->kind == VOLTAGE) {
    /* ZMPT101B module: 2.5 V offset, 1.0 V peak = 325 V peak (230 V RMS) */
    v = 2.5f + 1.0f * sinf(th) + fnoise(c) * 0.002f;
  } else {
    float i;
    if (ns < c->inrush_until_ns) i = c->inrush_amp * (0.45f * sinf(th) - 0.893f * cosf(th));
    else i = c->a1s * sinf(th) + c->a1c * cosf(th) + c->a3s * sinf(3 * th) + c->a3c * cosf(3 * th) + c->a5 * sinf(5 * th);
    v = 2.5f + i * c->sens + fnoise(c) * 0.004f;      /* ACS712 output with ~4 mV noise */
  }
  if (v < 0) v = 0;
  if (v > 5) v = 5;
  pin_dac_write(c->out, v);
}

void chip_init(void) {
  chip_t *c = (chip_t *)calloc(1, sizeof(chip_t));
  c->attr_kind  = attr_init("kind", 0);
  c->attr_fault = attr_init_float("fault", 0);
  c->attr_scale = attr_init_float("timeScale", 1440);
  c->kind  = (int)attr_read(c->attr_kind);
  c->scale = attr_read_float(c->attr_scale);
  c->out = pin_init("OUT", ANALOG);
  c->sens = (c->kind == TV || c->kind == FAN) ? 0.185f : (c->kind == AC ? 0.066f : 0.100f);
  c->rng = 12345u + 7919u * (uint32_t)c->kind;
  c->day = -1; c->temp = (c->kind == AC) ? 30.0f : 2.5f + 3.0f * (float)c->kind / 9.0f;
  c->off_time = 1e9f;
  set_load(c, L(0, 1, 0, 0, 0));
  pin_dac_write(c->out, 2.5f);

  const timer_config_t wave = { .callback = on_wave_step, .user_data = c };
  timer_start(timer_init(&wave), WAVE_STEP_US, true);
  if (c->kind != VOLTAGE) {
    const timer_config_t model = { .callback = on_model_step, .user_data = c };
    timer_start(timer_init(&model), MODEL_STEP_US, true);
  }
  printf("Appliance chip kind %d ready (time scale %.0f)\n", c->kind, c->scale);
}