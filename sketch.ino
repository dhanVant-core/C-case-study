// Real Power calculation
float calculateRealPower(float* v, float* i, int n) {
  float sum = 0;
  for(int k=0; k<n; k++) sum += v[k] * i[k];
  return sum / n;
}
