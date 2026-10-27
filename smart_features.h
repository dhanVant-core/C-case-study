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

  Serial.print(F("48-hour energy (computed from live whole-home power): "));

  Serial.print(kWh, 2);

  Serial.println(F(" kWh"));

  Serial.print(F("Estimated period electricity cost: Rs "));

  Serial.println(kWh * smTariffRs, 2);

  Serial.print(F("Configured energy budget: "));

  Serial.print(smBudgetKWh, 2);

  Serial.println(F(" kWh"));

  Serial.print(F("Budget result: "));

  Serial.println(kWh > smBudgetKWh ? F("EXCEEDED") : F("WITHIN LIMIT"));

  Serial.print(F("Peak sampled household real power: "));

  Serial.print(smPeakW, 0);

  Serial.println(F(" W"));

  Serial.print(F("Simulated time above load ceiling: "));

  Serial.print(smAboveLimitSeconds / 60.0f, 1);

  Serial.println(F(" minutes"));

  Serial.print(F("Separate threshold-crossing episodes: "));

  Serial.println(smAlertEvents);

  Serial.println(F("Recommendation: stagger discretionary high-power loads to reduce concurrent demand."));

  Serial.println(F("No automatic switching or appliance-level fault diagnosis is performed."));

  Serial.println(F("################################################"));

  Serial.println();

  smPeriodWh = 0.0f;

  smPeakW = 0.0f;

  smAboveLimitSeconds = 0.0f;

  smAlertEvents = 0;

  smWasAboveLimit = false;

}



void smExecuteCommand(const char *cmd) {

  char key = toupper((unsigned char)cmd[0]);

  if (key == 'H' || key == '?') {

    Serial.println(F("Commands: L 2500 (max W), B 30 (48h kWh budget), T 8 (Rs/kWh), S (status), H (help)"));

    return;

  }

  if (key == 'S') {

    Serial.print(F("SMART STATUS | now "));

    Serial.print(smWholeHomePower(), 0);

    Serial.print(F(" W | limit "));

    Serial.print(smLimitW, 0);

    Serial.print(F(" W | budget "));

    Serial.print(smBudgetKWh, 1);

    Serial.print(F(" kWh | tariff Rs "));

    Serial.println(smTariffRs, 2);

    return;

  }

  float value = atof(cmd + 1);

  if (value <= 0) {

    Serial.println(F("Invalid value. Type H for command help."));

    return;

  }

  if (key == 'L') smLimitW = value;

  else if (key == 'B') smBudgetKWh = value;

  else if (key == 'T') smTariffRs = value;

  else {

    Serial.println(F("Unknown command. Type H for help."));

    return;

  }

  Serial.println(F("Smart setting updated."));

  smExecuteCommand("S");

}



void smartCommand() {

  while (Serial.available()) {

    char c = (char)Serial.read();

    if (c == '\r' || c == '\n') {

      if (smCommandLength) {

        smCommand[smCommandLength] = '\0';

        smExecuteCommand(smCommand);

        smCommandLength = 0;

      }

    } else if (smCommandLength < sizeof(smCommand) - 1) {

      smCommand[smCommandLength++] = c;

    }

  }

}



#endif