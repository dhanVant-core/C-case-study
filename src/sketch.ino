#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>
#include <string.h>

#define TIME_SCALE      1440UL
#define REPORT_HOURS    48UL
#define LEARN_REPORTS   1
#define TARIFF_RS       8.0f
#define LIVE_EVERY_H    3UL

#define NUM_APPL        9
#define V_PIN           A9
#define MAX_ROUNDS      24
#define CYCLE_US        20000UL
#define VOLTS_PER_COUNT (5.0f / 1023.0f)
#define ZMPT_SCALE      325.27f
#define SENSOR_NOISE_V  0.004f

LiquidCrystal_I2C lcd(0x27, 20, 4);

void accumulate(int c, float peak);

typedef struct {
  const char *id;
  const char *name;
  const char *shortName;
  uint8_t pin;
  const char *sensor;
  float sens;
  float runThr;
  float heavyThr;
  float ratedA;
} Appliance;

const Appliance APPL[NUM_APPL] = {
  {"FRIDGE_01", "Refrigerator",    "Fridge", A0, "ACS712-20A", 0.100f, 0.30f, 0,    0.9f},
  {"MICRO_01",  "Microwave",       "Micro",  A1, "ACS712-20A", 0.100f, 1.00f, 0,    5.3f},
  {"DISH_01",   "Dishwasher",      "Dishw",  A2, "ACS712-20A", 0.100f, 0.20f, 4.0f, 9.3f},
  {"TV_01",     "Television",      "TV",     A3, "ACS712-5A",  0.185f, 0.20f, 0,    0.5f},
  {"FAN_01",    "Ceiling fan",     "Fan",    A4, "ACS712-5A",  0.185f, 0.08f, 0,    0.3f},
  {"AC_01",     "Air conditioner", "AC",     A5, "ACS712-30A", 0.066f, 1.50f, 0,    6.2f},
  {"WASH_01",   "Washing machine", "Washer", A6, "ACS712-20A", 0.100f, 0.15f, 4.0f, 8.4f},
  {"VAC_01",    "Vacuum cleaner",  "Vacuum", A7, "ACS712-20A", 0.100f, 1.00f, 0,    6.0f},
  {"DRYER_01",  "Clothes dryer",   "Dryer",  A8, "ACS712-20A", 0.100f, 0.30f, 7.0f, 10.5f},
};

#define NUM_MET 12

enum {
  M_ENERGY,
  M_ONH,
  M_STARTS,
  M_AVGRUN,
  M_MAXRUN,
  M_POWER,
  M_IRMS,
  M_PF,
  M_FLUCT,
  M_IDLE,
  M_PEAK,
  M_HEAVY
};

const char *MET_NAME[NUM_MET] = {
  "Energy (kWh)",
  "Running time (h)",
  "Starts",
  "Average run length (min)",
  "Longest run (min)",
  "Average power while running (W)",
  "Average RMS current while running (A)",
  "Power factor while running",
  "Current fluctuation (%)",
  "Idle / standby power (W)",
  "Peak instantaneous current (A)",
  "Time above heavy-load current (min)"
};

const bool MET_PATTERN[NUM_MET] = {
  false, false, true, true, true, true,
  true, true, true, true, true, true
};

const float MET_FLOOR[NUM_MET] = {
  0.02f, 0.05f, 1, 0.5f, 1, 3,
  0.02f, 0.03f, 3.0f, 3, 0.3f, 2
};

typedef struct {
  float energyWh, onT, avgT, sumP, sumI, sumI2, sumPF;
  float pfT, idleE, idleT, heavyT, peak;
  float segStart, segSum, segMax;
  float pendP, pendI, pendPF, pendDt;
  uint16_t segCount, starts, runReadings;
  bool prevOn, hasPend;
} Acc;

void endSegment(Acc *a, float t);

Acc acc[NUM_APPL];
float metric[NUM_APPL][NUM_MET];
float baseSum[NUM_APPL][NUM_MET];
float baseSq[NUM_APPL][NUM_MET];

int baseCount = 0;

float nowI[NUM_APPL];
float nowP[NUM_APPL];
float nowPF[NUM_APPL];
float nowV = 0;

int16_t vRaw[MAX_ROUNDS + 1];
uint32_t tRound[MAX_ROUNDS + 1];
int16_t iRaw[NUM_APPL][MAX_ROUNDS];

int nRounds;

uint32_t lastSweepUs;
uint32_t periodIndex = 0;
uint32_t lastLiveHour = 0;
uint8_t lcdPage = 0;
uint32_t lastLcdMs = 0;

uint32_t homeSeconds(void) {
  return (uint32_t)(((uint64_t)millis() * TIME_SCALE) / 1000ULL);
}

const uint32_t PERIOD_S = REPORT_HOURS * 3600UL;

void printClock(uint32_t s) {

  uint32_t d = s / 86400UL + 1;
  uint32_t h = (s % 86400UL) / 3600UL;
  uint32_t m = (s % 3600UL) / 60UL;

  Serial.print(F("Day "));
  Serial.print(d);
  Serial.print(' ');

  if (h < 10) Serial.print('0');
  Serial.print(h);
  Serial.print(':');

  if (m < 10) Serial.print('0');
  Serial.print(m);
}

void captureCycle(void) {

  uint32_t t0 = micros();
  int r = 0;

  while (r < MAX_ROUNDS &&
         (micros() - t0) < CYCLE_US + 1500UL) {

    tRound[r] = micros();

    vRaw[r] = analogRead(V_PIN);

    for (int c = 0; c < NUM_APPL; c++) {
      iRaw[c][r] = analogRead(APPL[c].pin);
    }

    r++;
  }

  tRound[r] = micros();
  vRaw[r] = analogRead(V_PIN);

  nRounds = r;
}

float sampleTime(int c, int r) {

  float frac =
    (c < 0)
    ? 0.0f
    : (float)(c + 1) / (NUM_APPL + 1);

  return (float)(tRound[r] - tRound[0])
       + frac * (float)(tRound[r + 1] - tRound[r]);
}

float sampleWeight(int c, int r) {

  float tStart = sampleTime(c, 0);
  float tEnd = tStart + CYCLE_US;

  float a = sampleTime(c, r);

  float b =
    (r + 1 < nRounds)
    ? sampleTime(c, r + 1)
    : a + (float)(tRound[r + 1] - tRound[r]);

  if (b > tEnd)
    b = tEnd;

  return b > a ? b - a : 0.0f;
}

void processCycle(void) {

  float wSum = 0;
  float meanV = 0;
  float sumV2 = 0;

  for (int r = 0; r < nRounds; r++) {

    float w = sampleWeight(-1, r);

    wSum += w;
    meanV += w * vRaw[r];
  }

  meanV /= wSum;

  for (int r = 0; r < nRounds; r++) {

    float v =
      (vRaw[r] - meanV)
      * VOLTS_PER_COUNT
      * ZMPT_SCALE;

    sumV2 +=
      sampleWeight(-1, r)
      * v
      * v;
  }

  nowV = sqrtf(sumV2 / wSum);

  for (int c = 0; c < NUM_APPL; c++) {

    float frac =
      (float)(c + 1) / (NUM_APPL + 1);

    float w[MAX_ROUNDS];
    float ws = 0;
    float meanI = 0;

    for (int r = 0; r < nRounds; r++) {

      w[r] = sampleWeight(c, r);

      ws += w[r];
      meanI += w[r] * iRaw[c][r];
    }

    meanI /= ws;

    float sumI2 = 0;
    float sumVI = 0;
    float peak = 0;

    for (int r = 0; r < nRounds; r++) {

      float i =
        (iRaw[c][r] - meanI)
        * VOLTS_PER_COUNT
        / APPL[c].sens;

      float vr =
        vRaw[r]
        + frac * (vRaw[r + 1] - vRaw[r]);

      float v =
        (vr - meanV)
        * VOLTS_PER_COUNT
        * ZMPT_SCALE;

      sumI2 += w[r] * i * i;
      sumVI += w[r] * v * i;

      if (w[r] > 0 && fabsf(i) > peak)
        peak = fabsf(i);
    }

    float noiseA =
      sqrtf(
        SENSOR_NOISE_V * SENSOR_NOISE_V
        + VOLTS_PER_COUNT * VOLTS_PER_COUNT / 12.0f
      )
      / APPL[c].sens;

    float ms =
      sumI2 / ws
      - noiseA * noiseA;

    nowI[c] =
      ms > 0 ? sqrtf(ms) : 0;

    nowP[c] = sumVI / ws;

    if (nowP[c] < 0)
      nowP[c] = 0;

    nowPF[c] =
      (nowI[c] > 3 * noiseA && nowV > 1)
      ? nowP[c] / (nowV * nowI[c])
      : -1;

    if (nowPF[c] > 1)
      nowPF[c] = 1;

    accumulate(c, peak);
  }
}

float sweepDtHome;
float periodElapsed;

void accumulate(int c, float peak) {

  Acc *a = &acc[c];

  float dt = sweepDtHome;

  bool on =
    nowI[c] > APPL[c].runThr;

  a->energyWh +=
    nowP[c] * dt / 3600.0f;

  if (peak > a->peak)
    a->peak = peak;

  if (on) {

    a->onT += dt;

    if (!a->prevOn) {

      a->starts++;

      a->segStart =
        periodElapsed;

      a->runReadings = 0;
      a->hasPend = false;
    }

    a->runReadings++;

    if (a->hasPend) {

      a->avgT += a->pendDt;

      a->sumP +=
        a->pendP * a->pendDt;

      a->sumI +=
        a->pendI * a->pendDt;

      a->sumI2 +=
        a->pendI * a->pendI
        * a->pendDt;

      if (a->pendPF >= 0) {

        a->sumPF +=
          a->pendPF * a->pendDt;

        a->pfT +=
          a->pendDt;
      }

      a->hasPend = false;
    }

    bool surge =
      nowI[c] >
      1.3f * APPL[c].ratedA;

    if (!surge && a->runReadings > 2) {

      a->pendP = nowP[c];
      a->pendI = nowI[c];
      a->pendPF = nowPF[c];
      a->pendDt = dt;

      a->hasPend = true;
    }

  } else {

    a->hasPend = false;

    a->idleE +=
      nowP[c] * dt;

    a->idleT += dt;

    if (a->prevOn)
      endSegment(a, periodElapsed);
  }

  if (APPL[c].heavyThr > 0 &&
      nowI[c] > APPL[c].heavyThr) {

    a->heavyT += dt;
  }

  a->prevOn = on;
}

void endSegment(Acc *a, float t) {

  float len =
    t - a->segStart;

  a->segSum += len;
  a->segCount++;

  if (len > a->segMax)
    a->segMax = len;
}

void finishPeriod(void) {

  for (int c = 0; c < NUM_APPL; c++) {

    Acc *a = &acc[c];

    if (a->prevOn)
      endSegment(a, periodElapsed);

    float *m = metric[c];

    float avgI =
      a->avgT > 0
      ? a->sumI / a->avgT
      : 0;

    m[M_ENERGY] =
      a->energyWh / 1000.0f;

    m[M_ONH] =
      a->onT / 3600.0f;

    m[M_STARTS] =
      a->starts;

    m[M_AVGRUN] =
      a->segCount
      ? a->segSum / a->segCount / 60.0f
      : 0;

    m[M_MAXRUN] =
      a->segMax / 60.0f;

    m[M_POWER] =
      a->avgT > 0
      ? a->sumP / a->avgT
      : 0;

    m[M_IRMS] =
      avgI;

    m[M_PF] =
      a->pfT > 0
      ? a->sumPF / a->pfT
      : 0;

    float var =
      a->avgT > 0
      ? a->sumI2 / a->avgT - avgI * avgI
      : 0;

    m[M_FLUCT] =
      (avgI > 0 && var > 0)
      ? sqrtf(var) / avgI * 100.0f
      : 0;

    m[M_IDLE] =
      a->idleT > 0
      ? a->idleE / a->idleT
      : 0;

    m[M_PEAK] =
      a->peak;

    m[M_HEAVY] =
      a->heavyT / 60.0f;
  }
}

void resetPeriod(void) {

  memset(acc, 0, sizeof(acc));

  periodElapsed = 0;
}

void addToBaseline(void) {

  for (int c = 0; c < NUM_APPL; c++) {

    for (int k = 0; k < NUM_MET; k++) {

      baseSum[c][k] +=
        metric[c][k];

      baseSq[c][k] +=
        metric[c][k] *
        metric[c][k];
    }
  }

  baseCount++;
}

int compareMetric(
  int c,
  int k,
  float *pct
) {

  float mean =
    baseSum[c][k] / baseCount;

  float diff =
    metric[c][k] - mean;

  *pct =
    fabsf(mean) > 1e-6f
    ? diff / fabsf(mean) * 100.0f
    : (
        fabsf(diff) > MET_FLOOR[k]
        ? 999
        : 0
      );

  if (fabsf(diff) < MET_FLOOR[k])
    return 0;

  if (baseCount > 1) {

    float var =
      baseSq[c][k] / baseCount
      - mean * mean;

    float sd =
      var > 0
      ? sqrtf(
          var * baseCount
          / (baseCount - 1)
        )
      : 0;

    if (fabsf(diff) < 3 * sd)
      return 0;
  }

  bool big =
    (k == M_IDLE)
    ? fabsf(diff) >= 5
    : fabsf(*pct) >= 40;

  bool sig =
    (k == M_IDLE)
    ? true
    : fabsf(*pct) >= 25;

  if (!sig)
    return 0;

  return big ? 2 : 1;
}

const char *screen(int c) {

  bool patBig = false;
  bool pat = false;

  int energyDir = 0;

  for (int k = 0; k < NUM_MET; k++) {

    if (k == M_HEAVY &&
        APPL[c].heavyThr == 0)
      continue;

    float pct;

    int s =
      compareMetric(c, k, &pct);

    if (!s)
      continue;

    if (MET_PATTERN[k]) {

      pat = true;

      if (s == 2)
        patBig = true;
    }

    if (k == M_ENERGY)
      energyDir =
        pct > 0 ? 1 : -1;
  }

  if (patBig)
    return "ATTENTION";

  if (pat)
    return "UNUSUAL PATTERN";

  if (energyDir > 0)
    return "HIGH USAGE";

  if (energyDir < 0)
    return "LOW USAGE";

  return "NORMAL";
}

void printNum(float x) {

  float a = fabsf(x);

  Serial.print(
    x,
    a >= 100
      ? 0
      : (a >= 10 ? 1 : 2)
  );
}

void printDocument(uint32_t index) {

  bool learning =
    (int)index < LEARN_REPORTS;

  float totalKWh = 0;

  Serial.println();
  Serial.println(
    F("##### STATISTICS DOCUMENT START #####")
  );

  Serial.print(
    F("# Virtual home statistics document, report ")
  );

  Serial.print(index + 1);

  Serial.print(F(" (home hours "));
  Serial.print(index * REPORT_HOURS);

  Serial.print(F(" to "));
  Serial.print((index + 1) * REPORT_HOURS);

  Serial.println(F(")"));

  Serial.println(
    F("Source: one ACS712 current sensor per appliance and a ZMPT101B voltage sensor, read by an Arduino Mega.")
  );

  Serial.println(
    F("Each reading captures one full 50 Hz mains cycle on every channel.")
  );

  if (learning) {

    Serial.println(
      F("Status: LEARNING. This period is being recorded as the healthy baseline, so there is no comparison yet.")
    );

  } else {

    Serial.print(
      F("Baseline: average of the first ")
    );

    Serial.print(baseCount);

    Serial.println(
      F(" healthy report(s). A change is flagged when it is at least 25% and above the minimum meaningful size.")
    );
  }

  for (int c = 0; c < NUM_APPL; c++) {

    const Appliance *ap =
      &APPL[c];

    totalKWh +=
      metric[c][M_ENERGY];

    Serial.println();

    Serial.print(F("## "));
    Serial.print(ap->id);
    Serial.print(F(" - "));
    Serial.println(ap->name);

    Serial.print(F("Sensor: "));
    Serial.print(ap->sensor);
    Serial.print(F(" on pin A"));
    Serial.println(ap->pin - A0);

    if (!learning) {

      Serial.print(F("Screening: "));
      Serial.println(screen(c));
    }

    Serial.print(F("Energy cost: Rs "));
    Serial.print(
      metric[c][M_ENERGY] * TARIFF_RS,
      2
    );

    Serial.print(F(" at Rs "));
    Serial.print(TARIFF_RS, 0);

    Serial.println(F(" per kWh"));

    Serial.println(
      learning
      ? F("| Statistic | This period |")
      : F("| Statistic | This period | Baseline | Change | Flag |")
    );

    for (int k = 0; k < NUM_MET; k++) {

      if (k == M_HEAVY &&
          ap->heavyThr == 0)
        continue;

      Serial.print(F("| "));
      Serial.print(MET_NAME[k]);
      Serial.print(F(" | "));

      printNum(metric[c][k]);

      if (!learning) {

        float pct;

        int s =
          compareMetric(c, k, &pct);

        Serial.print(F(" | "));

        printNum(
          baseSum[c][k]
          / baseCount
        );

        Serial.print(F(" | "));

        if (fabsf(pct) >= 999) {

          Serial.print(F("new"));

        } else {

          if (pct > 0)
            Serial.print('+');

          Serial.print(pct, 0);
          Serial.print('%');
        }

        Serial.print(F(" | "));

        Serial.print(
          s == 2
          ? F("LARGE")
          : (s == 1 ? F("yes") : F("no"))
        );
      }

      Serial.println(F(" |"));
    }
  }

  Serial.println();

  Serial.print(F("Whole home: "));
  Serial.print(totalKWh, 2);
  Serial.print(F(" kWh, Rs "));

  Serial.println(
    totalKWh * TARIFF_RS,
    2
  );

  Serial.println(
    F("##### STATISTICS DOCUMENT END #####")
  );

  Serial.println();
}

void printLive(uint32_t s) {

  float total = 0;

  for (int c = 0; c < NUM_APPL; c++)
    total += nowP[c];

  Serial.print('[');

  printClock(s);

  Serial.print(F("] home "));
  Serial.print(total / 1000.0f, 2);

  Serial.print(F(" kW, "));
  Serial.print(nowV, 0);

  Serial.print(F(" V | running:"));

  bool any = false;

  for (int c = 0; c < NUM_APPL; c++) {

    if (nowI[c] >
        APPL[c].runThr) {

      Serial.print(' ');
      Serial.print(
        APPL[c].shortName
      );

      Serial.print(' ');

      Serial.print(
        nowP[c],
        0
      );

      Serial.print('W');

      any = true;
    }
  }

  if (!any)
    Serial.print(F(" none"));

  Serial.println();
}

void updateLcd(uint32_t s) {

  char line[21];

  uint32_t d =
    s / 86400UL + 1;

  uint32_t h =
    (s % 86400UL) / 3600UL;

  uint32_t m =
    (s % 3600UL) / 60UL;

  lcd.setCursor(0, 0);

  snprintf(
    line,
    sizeof(line),
    "D%lu %02lu:%02lu R%lu %-6s",
    (unsigned long)d,
    (unsigned long)h,
    (unsigned long)m,
    (unsigned long)(periodIndex + 1),
    (int)periodIndex < LEARN_REPORTS
      ? "LEARN"
      : "CHECK"
  );

  lcd.print(line);

  for (int row = 0; row < 3; row++) {

    int c =
      (lcdPage * 3 + row)
      % NUM_APPL;

    char amps[8];
    char watts[8];

    dtostrf(
      nowI[c],
      5,
      2,
      amps
    );

    dtostrf(
      nowP[c],
      5,
      0,
      watts
    );

    snprintf(
      line,
      sizeof(line),
      "%-6s%sA%sW",
      APPL[c].shortName,
      amps,
      watts
    );

    lcd.setCursor(
      0,
      row + 1
    );

    lcd.print(line);
  }

  lcdPage =
    (lcdPage + 1) % 3;
}

void smartUpdate(float dtHomeSeconds);
void smartReport();
void smartCommand();
void setup() {

  Serial.begin(115200);

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print(F("Virtual home"));

  lcd.setCursor(0, 1);
  lcd.print(F("load profiling"));

  Serial.println(
    F("Virtual Home Load Profiling - Arduino Mega")
  );

  Serial.print(
    F("1 simulator second = ")
  );

  Serial.print(
    TIME_SCALE / 60
  );

  Serial.println(
    F(" home minutes")
  );

  Serial.print(
    F("Report every ")
  );

  Serial.print(
    REPORT_HOURS
  );

  Serial.print(
    F(" home hours = ")
  );

  Serial.print(
    PERIOD_S / TIME_SCALE
  );

  Serial.println(
    F(" simulator seconds")
  );

  Serial.print(
    F("Report(s) 1 to ")
  );

  Serial.print(
    LEARN_REPORTS
  );

  Serial.println(
    F(" record the healthy baseline. Keep every Fault slider at 0 until then.")
  );

  for (int c = 0; c < NUM_APPL; c++) {

    Serial.print(F("  A"));
    Serial.print(APPL[c].pin - A0);

    Serial.print(F("  "));
    Serial.print(APPL[c].sensor);

    Serial.print(F("  "));
    Serial.println(APPL[c].name);
  }

  Serial.println(
    F("  A9  ZMPT101B mains voltage")
  );

  resetPeriod();

  lastSweepUs = micros();
}

void loop() {

  captureCycle();

  uint32_t nowUs =
    micros();

  sweepDtHome =
    (float)(nowUs - lastSweepUs)
    * 1e-6f
    * TIME_SCALE;

  lastSweepUs =
    nowUs;

  periodElapsed +=
    sweepDtHome;

  processCycle();
  smartUpdate(sweepDtHome);

  uint32_t s =
    homeSeconds();

  if (s / PERIOD_S >
      periodIndex) {

    finishPeriod();

    printDocument(periodIndex);
    smartReport();

    if ((int)periodIndex <
        LEARN_REPORTS)

      addToBaseline();

    periodIndex =
      s / PERIOD_S;

    resetPeriod();
  }

  uint32_t hour =
    s / 3600UL;

  if (hour / LIVE_EVERY_H !=
      lastLiveHour / LIVE_EVERY_H) {

    lastLiveHour =
      hour;

    printLive(s);
  }

  if (millis() - lastLcdMs >
      1500) {

    lastLcdMs =
      millis();

    updateLcd(s);
    smartCommand();
  }
}
#include "smart_features.h"