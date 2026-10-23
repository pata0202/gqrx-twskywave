/* -*- c++ -*- */
/*
 * Gqrx SDR: Software defined radio receiver powered by GNU Radio and Qt
 *           https://gqrx.dk/
 *
 * Gqrx is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * Gqrx is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Gqrx; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

/*
 * Self-check for rx_nr_ff. Run with ctest, or ./src/rx_nr_ff_test in the
 * build directory. Exits non-zero on failure.
 *
 *  1. Disabled: output is exactly the input delayed by latency().
 *  2. Noise only: attenuated by about the configured level, not more.
 *  3. Tone bursts + noise: tone level kept within 1 dB, noise clearly
 *     reduced. Bursts (0.4 s on/off) stand in for speech; a steady tone
 *     would rightly be tracked as noise.
 */
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>
#include "dsp/rx_nr_ff.h"

static const double RATE = 96000.0;
static int failures = 0;

static void check(bool ok, const char *what, double value)
{
    std::printf("%s  %-48s %.3f\n", ok ? "PASS" : "FAIL", what, value);
    if (!ok)
        failures++;
}

static std::vector<float> run(rx_nr_ff_sptr nr, const std::vector<float> &in)
{
    std::vector<float> out(in.size());
    const size_t chunk = 1000;  // odd size on purpose: not a multiple of the hop
    for (size_t i = 0; i < in.size(); i += chunk)
    {
        int n = (int)std::min(chunk, in.size() - i);
        gr_vector_const_void_star ii{&in[i]};
        gr_vector_void_star oo{&out[i]};
        nr->work(n, ii, oo);
    }
    return out;
}

static double rms(const std::vector<float> &x, size_t from)
{
    double s = 0;
    for (size_t i = from; i < x.size(); i++)
        s += (double)x[i] * (double)x[i];
    return std::sqrt(s / (x.size() - from));
}

/* amplitude of the f Hz component, by correlation */
static double tone_amp(const std::vector<float> &x, size_t from, double f)
{
    double c = 0, s = 0;
    for (size_t i = from; i < x.size(); i++)
    {
        c += (double)x[i] * std::cos(2 * M_PI * f * i / RATE);
        s += (double)x[i] * std::sin(2 * M_PI * f * i / RATE);
    }
    return 2 * std::hypot(c, s) / (x.size() - from);
}

int main()
{
    const size_t len = (size_t)(6 * RATE);
    const size_t settle = (size_t)(3 * RATE);  // noise tracker needs ~1.5 s
    std::mt19937 gen(1);
    std::normal_distribution<float> gauss(0.0f, 0.05f);

    std::vector<float> noise(len), tone(len);
    for (size_t i = 0; i < len; i++)
    {
        noise[i] = gauss(gen);
        bool on = (size_t)(i / (0.4 * RATE)) % 2 == 0;
        tone[i] = on ? 0.5f * (float)std::sin(2 * M_PI * 1000.0 * i / RATE) : 0.0f;
    }

    // 1. disabled = pure delay
    {
        auto nr = make_rx_nr_ff(RATE);
        auto out = run(nr, noise);
        int d = nr->latency();
        double err = 0;
        for (size_t i = d; i < len; i++)
            err = std::max(err, (double)std::fabs(out[i] - noise[i - d]));
        check(err < 1e-4, "disabled: max error vs delayed input", err);
        check(d / RATE < 0.010, "latency [s] < 10 ms", d / RATE);
    }

    // 2. noise only, level 10 dB
    {
        auto nr = make_rx_nr_ff(RATE);
        nr->set_enabled(true);
        nr->set_level(10.0f);
        auto out = run(nr, noise);
        double att = 20 * std::log10(rms(noise, settle) / rms(out, settle));
        check(att > 7.0 && att < 10.5, "noise only: attenuation [dB] ~10", att);
    }

    // 3. tone + noise
    {
        std::vector<float> mix(len);
        for (size_t i = 0; i < len; i++)
            mix[i] = tone[i] + noise[i];

        auto nr = make_rx_nr_ff(RATE);
        nr->set_enabled(true);
        nr->set_level(10.0f);
        auto out = run(nr, mix);
        int d = nr->latency();

        double tone_change = 20 * std::log10(tone_amp(out, settle, 1000.0) /
                                             tone_amp(tone, settle, 1000.0));
        check(std::fabs(tone_change) < 1.0, "tone + noise: tone level change [dB]", tone_change);

        // residual noise = output minus the (delayed) clean tone
        std::vector<float> resid(len, 0.0f);
        for (size_t i = d; i < len; i++)
            resid[i] = out[i] - tone[i - d];
        double nred = 20 * std::log10(rms(noise, settle) / rms(resid, settle));
        check(nred > 5.0, "tone + noise: noise reduction [dB]", nred);
    }

    return failures ? 1 : 0;
}
