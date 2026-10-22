// Drift detection calculation
float computeDriftPct(float current, float baseline) {
  return ((current - baseline) / baseline) * 100.0f;
}
