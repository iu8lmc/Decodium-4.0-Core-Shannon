#!/usr/bin/env python3
"""A/B controls for the optional AVT boundary fit diagnostic."""
import numpy as np
from analyze_avt_boundary_trace import fit

x = np.column_stack((np.ones(7), np.arange(-3.0, 4.0)))
for slope in (2.0, -3.0):
    y = 75.0 + slope * x[:, 1]
    exact, _, _, _ = fit(x, y, 0.0)
    regularised, _, _, _ = fit(x, y, 100.0)
    assert np.allclose(x @ exact, y, atol=1e-9)
    assert abs(regularised[0] - 75.0) < 1e-9
    assert abs(regularised[1]) < abs(exact[1])
print('PASS AVT boundary fit A/B and regularisation controls')
