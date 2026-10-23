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
#ifndef RX_NR_FF_H
#define RX_NR_FF_H

#include <mutex>
#include <vector>
#include <gnuradio/sync_block.h>
#include <gnuradio/fft/fft.h>

class rx_nr_ff;

#if GNURADIO_VERSION < 0x030900
typedef boost::shared_ptr<rx_nr_ff> rx_nr_ff_sptr;
#else
typedef std::shared_ptr<rx_nr_ff> rx_nr_ff_sptr;
#endif

/*! \brief Return a shared_ptr to a new instance of rx_nr_ff.
 *  \param sample_rate Audio sample rate.
 */
rx_nr_ff_sptr make_rx_nr_ff(double sample_rate);

/*! \brief Gentle spectral noise reduction for voice.
 *  \ingroup DSP
 *
 * Short time FFT (~5 ms frames, 50% overlap) with a Wiener gain per bin.
 * The noise floor is tracked continuously with a minimum statistics
 * estimator, so it adapts during speech without needing pauses. The gain
 * is smoothed with the decision directed a-priori SNR estimate and never
 * drops below a floor set by the level (max attenuation in dB). Both are
 * what keep voice natural and avoid "musical noise" artefacts.
 *
 * The block always runs (constant latency, noise estimate stays warm);
 * when disabled the gain is simply 1.
 */
class rx_nr_ff : public gr::sync_block
{
    friend rx_nr_ff_sptr make_rx_nr_ff(double sample_rate);

protected:
    rx_nr_ff(double sample_rate);

public:
    ~rx_nr_ff();

    int work(int noutput_items,
             gr_vector_const_void_star &input_items,
             gr_vector_void_star &output_items);

    void set_enabled(bool enabled);
    void set_level(float level_db);

    /*! \brief Processing delay in samples. */
    int latency() const { return d_fft_size; }

private:
    void process_frame();

    std::mutex d_mutex;
    bool    d_enabled;
    float   d_gain_floor;   /*!< Minimum gain, 10^(-level/20) */

    int     d_fft_size;
    int     d_hop;
    int     d_nbins;
    int     d_hop_pos;

    gr::fft::fft_real_fwd *d_fwd;
    gr::fft::fft_real_rev *d_rev;

    std::vector<float> d_window;  /*!< sqrt periodic Hann (analysis and synthesis) */
    std::vector<float> d_inbuf;   /*!< Last fft_size input samples */
    std::vector<float> d_ola;     /*!< Overlap-add accumulator */
    std::vector<float> d_outbuf;  /*!< Next hop of output samples */

    /* per bin state */
    std::vector<float> d_psd;        /*!< Smoothed power */
    std::vector<float> d_noise;      /*!< Noise power estimate */
    std::vector<float> d_min_cur;    /*!< Minimum in current sub-window */
    std::vector<float> d_min_hist;   /*!< Minima of previous sub-windows, NUM_SUBWIN per bin */
    std::vector<float> d_prev_clean; /*!< Previous frame clean power / noise (DD estimate) */
    int     d_subwin_frames;  /*!< Frames per sub-window */
    int     d_frame_cnt;
    int     d_subwin_idx;
    bool    d_first_frame;
};

#endif /* RX_NR_FF_H */
