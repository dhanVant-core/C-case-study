#ifndef SMART_FEATURES_H

#define SMART_FEATURES_H



// Intelligent demand management add-on for the existing Wokwi Arduino Mega project.

// It reads nowP[] after processCycle() and does not alter the sensor model.

// Recommendations are simulated advice; this code does NOT switch mains loads.



float smLimitW = 2500.0f;

float smBudgetKWh = 30.0f;

float smTariffRs = TARIFF_RS;

float smPeriodWh = 0.0f;

float smPeakW = 0.0f;

float smAboveLimitSeconds = 0.0f;

uint16_t smAlertEvents = 0;

bool smWasAboveLimit = false;

unsigned long smLastAlertMs = 0;

unsigned long smLastForecastMs = 0;

char smCommand[32];

uint8_t smCommandLength = 0;



// Load priorities: fridge is deliberately NEVER proposed for postponement.

// Indices match APPL[] in the original project.

// Dishwasher, washer and dryer are usually more deferrable than AC or microwave.

const uint8_t smCandidates[] = { 8, 2, 6, 7, 1, 5 };



float smWholeHomePower() {

  float total = 0.0f;

  for (int i = 0; i < NUM_APPL; i++) total += nowP[i];

  return total;

}



int smSelectDeferrable(float total) {

  int chosen = -1;

  float best = -1.0f;

  for (unsigned int j = 0; j < sizeof(smCandidates); j++) {

    int i = smCandidates[j];

    if (nowI[i] <= APPL[i].runThr || nowP[i] < 100.0f) continue;

    // Prefer an appliance whose postponement would bring demand under the limit.

    float score = nowP[i];

    if (total - nowP[i] <= smLimitW) score += 10000.0f;

    if (score > best) { best = score; chosen = i; }

  }

  return chosen;

}



void smPrintRecommendation(float total) {

  int chosen = smSelectDeferrable(total);

  if (chosen < 0) {

    Serial.println(F("No suitable discretionary load currently detected; reduce concurrent use manually."));

    return;

  }

  Serial.print(F("SUGGEST POSTPONING: "));

  Serial.print(APPL[chosen].name);

  Serial.print(F(" | current draw "));

  Serial.print(nowP[chosen], 0);

  Serial.print(F(" W | demand if postponed ~"));

  Serial.print(total - nowP[chosen], 0);

  Serial.println(F(" W"));

  Serial.print(F("Illustrative energy/cost for 1 hour postponed: "));

  Serial.print(nowP[chosen] / 1000.0f, 2);

  Serial.print(F(" kWh / Rs "));

  Serial.println(nowP[chosen] / 1000.0f * smTariffRs, 2);

  Serial.println(F("NOTE: postponement shifts usage; it does not automatically save this energy."));

}



void smartUpdate(float dtHomeSeconds) {

  float total = smWholeHomePower();

  if (dtHomeSeconds < 0) dtHomeSeconds = 0;

  smPeriodWh += total * dtHomeSeconds / 3600.0f;

  if (total > smPeakW) smPeakW = total;



  bool above = total > smLimitW;

  if (above) {

    smAboveLimitSeconds += dtHomeSeconds;

    if (!smWasAboveLimit) smAlertEvents++;

  }

  smWasAboveLimit = above;



  if (above && millis() - smLastAlertMs >= 5000UL) {

    smLastAlertMs = millis();

    Serial.println();

    Serial.println(F("========== SMART PEAK LOAD ALERT =========="));

    Serial.print(F("Aggregate real power: "));

    Serial.print(total, 0);

    Serial.print(F(" W | configured ceiling: "));

    Serial.print(smLimitW, 0);

    Serial.println(F(" W"));

    smPrintRecommendation(total);

    Serial.println(F("==========================================="));

  }



  // Forecast is meaningful only after at least one simulated hour.

  if (periodElapsed >= 3600.0f && millis() - smLastForecastMs >= 12000UL) {

    smLastForecastMs = millis();

    float projected = (smPeriodWh / 1000.0f) * (float)PERIOD_S / periodElapsed;

    Serial.print(F("[48h forecast] Current "));

    Serial.print(smPeriodWh / 1000.0f, 2);

    Serial.print(F(" kWh; projected "));

    Serial.print(projected, 2);

    Serial.print(F(" kWh; budget "));

    Serial.print(smBudgetKWh, 2);

    Serial.println(projected > smBudgetKWh ? F(" kWh -> OVER BUDGET") : F(" kWh -> WITHIN BUDGET"));

  }

}



void smartReport() {

  float kWh = smPeriodWh / 1000.0f;

  Serial.println();

  Serial.println(F("######## INTELLIGENT ENERGY MANAGEMENT ########"));

