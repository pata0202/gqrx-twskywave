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
#include <algorithm>
#include <cmath>
#include <gnuradio/io_signature.h>
#include "dsp/rx_nr_ff.h"

#define FRAME_SECONDS   0.0053f /* ~5 ms frames: inaudible latency, ~190 Hz bins */
#define PSD_ALPHA       0.85f   /* power smoothing per frame (~18 ms) */
#define DD_ALPHA        0.98f   /* decision directed smoothing, higher = fewer artefacts */
#define MINSTAT_SECONDS 1.5f    /* noise = minimum power over this window */
#define NUM_SUBWIN      6
#define MINSTAT_BIAS    2.0f    /* minimum of smoothed power underestimates the mean */

rx_nr_ff_sptr make_rx_nr_ff(double sample_rate)
{
    return gnuradio::get_initial_sptr(new rx_nr_ff(sample_rate));
}

rx_nr_ff::rx_nr_ff(double sample_rate)
    : gr::sync_block ("rx_nr_ff",
          gr::io_signature::make(1, 1, sizeof(float)),
          gr::io_signature::make(1, 1, sizeof(float))),
      d_enabled(false),
      d_gain_floor(1.0f),
      d_hop_pos(0),
      d_frame_cnt(0),
      d_subwin_idx(0),
      d_first_frame(true)
{
    // power of two closest to the wanted frame length
    d_fft_size = 1 << std::max(6, (int)std::lround(std::log2(sample_rate * (double)FRAME_SECONDS)));
    d_hop = d_fft_size / 2;
    d_nbins = d_fft_size / 2 + 1;

    d_fwd = new gr::fft::fft_real_fwd(d_fft_size);
    d_rev = new gr::fft::fft_real_rev(d_fft_size);

    // sqrt periodic Hann on both sides: w^2 overlap-adds to exactly 1 at 50%
    d_window.resize(d_fft_size);
    for (int n = 0; n < d_fft_size; n++)
        d_window[n] = std::sqrt(0.5f - 0.5f * std::cos(2.0f * (float)M_PI * n / d_fft_size));

    d_inbuf.assign(d_fft_size, 0.0f);
    d_ola.assign(d_fft_size, 0.0f);
    d_outbuf.assign(d_hop, 0.0f);

    d_psd.assign(d_nbins, 0.0f);
    d_noise.assign(d_nbins, 0.0f);
    d_min_cur.assign(d_nbins, 0.0f);
    d_min_hist.assign(d_nbins * NUM_SUBWIN, 0.0f);
    d_prev_clean.assign(d_nbins, 0.0f);

    float frames_per_sec = sample_rate / d_hop;
    d_subwin_frames = std::max(1, (int)(MINSTAT_SECONDS * frames_per_sec / NUM_SUBWIN));

    set_level(10.0f);
}

rx_nr_ff::~rx_nr_ff()
{
    delete d_fwd;
    delete d_rev;
}

void rx_nr_ff::set_enabled(bool enabled)
{
    std::lock_guard<std::mutex> lock(d_mutex);
    d_enabled = enabled;
}

/*! \brief Set maximum attenuation in dB (0 = no effect, 20 = strongest). */
void rx_nr_ff::set_level(float level_db)
{
    std::lock_guard<std::mutex> lock(d_mutex);
    level_db = std::max(0.0f, std::min(20.0f, level_db));
    d_gain_floor = std::pow(10.0f, -level_db / 20.0f);
}

int rx_nr_ff::work(int noutput_items,
                   gr_vector_const_void_star &input_items,
                   gr_vector_void_star &output_items)
{
    const float *in = (const float *) input_items[0];
    float *out = (float *) output_items[0];

    std::lock_guard<std::mutex> lock(d_mutex);

    for (int i = 0; i < noutput_items; i++)
    {
        d_inbuf[d_fft_size - d_hop + d_hop_pos] = in[i];
        out[i] = d_outbuf[d_hop_pos];
        if (++d_hop_pos == d_hop)
        {
            process_frame();
            d_hop_pos = 0;
        }
    }

    return noutput_items;
}

void rx_nr_ff::process_frame()
{
    float *fin = d_fwd->get_inbuf();
    for (int n = 0; n < d_fft_size; n++)
        fin[n] = d_inbuf[n] * d_window[n];
    d_fwd->execute();

    const gr_complex *X = d_fwd->get_outbuf();
    gr_complex *Y = d_rev->get_inbuf();

    bool new_subwin = (++d_frame_cnt >= d_subwin_frames);

    for (int k = 0; k < d_nbins; k++)
    {
        float p = std::norm(X[k]);

        // minimum statistics noise tracking
        float *hist = &d_min_hist[k * NUM_SUBWIN];
        if (d_first_frame)
        {
            d_psd[k] = d_min_cur[k] = p;
            std::fill(hist, hist + NUM_SUBWIN, p);
        }
        else
        {
            d_psd[k] = PSD_ALPHA * d_psd[k] + (1.0f - PSD_ALPHA) * p;
            d_min_cur[k] = std::min(d_min_cur[k], d_psd[k]);
        }
        float min_all = std::min(d_min_cur[k], *std::min_element(hist, hist + NUM_SUBWIN));
        d_noise[k] = std::max(MINSTAT_BIAS * min_all, 1.0e-20f);

        if (new_subwin)
        {
            hist[d_subwin_idx] = d_min_cur[k];
            d_min_cur[k] = d_psd[k];
        }

        // Wiener gain with decision directed a-priori SNR
        float gamma = p / d_noise[k];
        float xi = DD_ALPHA * d_prev_clean[k] + (1.0f - DD_ALPHA) * std::max(gamma - 1.0f, 0.0f);
        float g = std::max(xi / (1.0f + xi), d_gain_floor);
        d_prev_clean[k] = g * g * gamma;

        Y[k] = d_enabled ? X[k] * g : X[k];
    }

    if (new_subwin)
    {
        d_subwin_idx = (d_subwin_idx + 1) % NUM_SUBWIN;
        d_frame_cnt = 0;
    }
    d_first_frame = false;

    d_rev->execute();
    const float *y = d_rev->get_outbuf();
    float scale = 1.0f / d_fft_size;  // fftw inverse is unnormalized
    for (int n = 0; n < d_fft_size; n++)
        d_ola[n] += y[n] * d_window[n] * scale;

    std::copy(d_ola.begin(), d_ola.begin() + d_hop, d_outbuf.begin());
    std::copy(d_ola.begin() + d_hop, d_ola.end(), d_ola.begin());
    std::fill(d_ola.begin() + d_hop, d_ola.end(), 0.0f);

    std::copy(d_inbuf.begin() + d_hop, d_inbuf.end(), d_inbuf.begin());
}
