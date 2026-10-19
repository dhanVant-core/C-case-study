// Power Factor calculation
float calculatePF(float p, float v_rms, float i_rms) {
  float s = v_rms * i_rms;
  return (s > 0) ? (p / s) : 1.0f;
}
