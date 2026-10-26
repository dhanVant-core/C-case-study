import math
import unittest

def compute_real_power_discrete(v_samples, i_samples):
    """Compute active real power: P = (1/N) * sum(v[k] * i[k])"""
    n = len(v_samples)
    if n == 0 or n != len(i_samples):
        raise ValueError("Invalid sample buffer lengths")
    return sum(v * i for v, i in zip(v_samples, i_samples)) / n

def compute_rms_discrete(samples):
    """Compute RMS: sqrt((1/N) * sum(x[k]^2))"""
    n = len(samples)
    return math.sqrt(sum(x * x for x in samples) / n)

class TestPowerSampling(unittest.TestCase):
    def test_unity_power_factor(self):
        # 230V RMS, 10A RMS, pure resistive (PF = 1.0)
        v_rms_expected = 230.0
        i_rms_expected = 10.0
        v_peak = v_rms_expected * math.sqrt(2)
        i_peak = i_rms_expected * math.sqrt(2)
        
        n_samples = 40
        v_buf = [v_peak * math.sin(2 * math.pi * k / n_samples) for k in range(n_samples)]
        i_buf = [i_peak * math.sin(2 * math.pi * k / n_samples) for k in range(n_samples)]
        
        v_rms = compute_rms_discrete(v_buf)
        i_rms = compute_rms_discrete(i_buf)
        real_power = compute_real_power_discrete(v_buf, i_buf)
        apparent_power = v_rms * i_rms
        pf = real_power / apparent_power
        
        self.assertAlmostEqual(v_rms, v_rms_expected, places=2)
        self.assertAlmostEqual(i_rms, i_rms_expected, places=2)
        self.assertAlmostEqual(real_power, 2300.0, places=1)
        self.assertAlmostEqual(pf, 1.00, places=3)

    def test_inductive_lag_power_factor(self):
        # Phase lag of 36.87 degrees => cos(phi) = 0.80
        phi = math.radians(36.87)
        v_peak = 230.0 * math.sqrt(2)
        i_peak = 10.0 * math.sqrt(2)
        n_samples = 40
        
        v_buf = [v_peak * math.sin(2 * math.pi * k / n_samples) for k in range(n_samples)]
        i_buf = [i_peak * math.sin(2 * math.pi * k / n_samples - phi) for k in range(n_samples)]
        
        real_power = compute_real_power_discrete(v_buf, i_buf)
        apparent_power = compute_rms_discrete(v_buf) * compute_rms_discrete(i_buf)
        pf = real_power / apparent_power
        
        expected_power = 2300.0 * 0.80
        self.assertAlmostEqual(real_power, expected_power, places=0)
        self.assertAlmostEqual(pf, 0.80, places=2)

if __name__ == '__main__':
    unittest.main()
