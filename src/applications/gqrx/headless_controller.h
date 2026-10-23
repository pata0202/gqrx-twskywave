/* -*- c++ -*- */
/*
 * Gqrx SDR: Software defined radio receiver powered by GNU Radio and Qt
 *           https://gqrx.dk/
 *
 * Headless (GUI-less) controller.
 *
 * This class replaces MainWindow for headless operation: it owns the
 * receiver and the remote control TCP server, loads configuration directly
 * from the ini file (bypassing the GUI dock widgets that used to hold and
 * forward the settings), and wires the remote control signals straight to
 * the receiver so that frequency/mode/gain/etc. can be controlled over the
 * network (hamlib rigctld compatible protocol).
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
#ifndef HEADLESS_CONTROLLER_H
#define HEADLESS_CONTROLLER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QSettings>

#include "receiver.h"
#include "remote_control.h"

class HeadlessController : public QObject
{
    Q_OBJECT

public:
    explicit HeadlessController(QObject *parent = nullptr);
    ~HeadlessController() override;

    /*! \brief Load configuration from file and start the receiver.
     *  \returns true if a usable input device was configured.
     */
    bool loadConfig(const QString &cfgfile);

private slots:
    /* from RemoteControl */
    void onNewFrequency(qint64 freq);
    void onNewFilterOffset(qint64 offset);
    void onNewLnbLo(double freq_mhz);
    void onNewMode(int mode_idx);
    void onNewPassband(int passband);
    void onNewSquelchLevel(double level);
    void onNewAudioGain(float gain);
    void onNewAudioMuted(bool muted);
    void onGainChanged(QString name, double value);
    void onStartAudioRecorder();
    void onStopAudioRecorder();
    void onStartIqRecorder();
    void onStopIqRecorder();
    void onDspChanged(bool enabled);
    void onNewRdsMode(bool enabled);

    /* timers */
    void meterTimeout();
    void rdsTimeout();

private:
    void selectDemod(int mode_idx);
    void setNewFrequency(qint64 abs_freq);
    void updateGainStages(bool read_from_device);
    void startDsp();
    void stopDsp();
    void setRdsDecoder(bool enabled);

private:
    receiver        *rx;
    RemoteControl    *remote;
    QSettings        *m_settings;

    QTimer  *meter_timer;
    QTimer  *rds_timer;

    QString  m_cfg_dir;

    qint64   d_lnb_lo;        /*!< LNB LO in Hz */
    qint64   d_hw_freq;       /*!< Current hardware (RF) frequency in Hz */
    qint64   d_current_freq;  /*!< Last commanded absolute frequency (LNB + RF + filter offset) */

    int      d_filter_shape;  /*!< receiver::filter_shape */
    int      d_filter_preset; /*!< FILTER_PRESET_* ; always NORMAL, no GUI to change it */
    int      d_mode;          /*!< RxMode::rxopt_mode_idx */
    bool     d_have_audio;

    float    d_audio_gain;
    bool     d_audio_muted;

    /* Per-mode demodulator options. These are only ever set from the config
     * file at startup; unlike frequency/mode/gain/etc. there is no remote
     * control command that changes them at runtime (same as upstream gqrx).
     */
    int      d_cw_offset;
    float    d_fm_maxdev;
    double   d_fm_deemph;
    bool     d_am_dcr;
    bool     d_amsync_dcr;
    float    d_amsync_pllbw;

    /* Current squelch level. Unlike the options above, this one *can* be
     * changed at runtime via remote control, and like upstream gqrx that
     * live value (not the config file default) is what gets re-applied
     * whenever the demodulator is switched.
     */
    double   d_sql_level;

    QString  d_iq_rec_dir;
    QString  d_iq_rec_format;
    QString  d_audio_rec_dir;
};

#endif // HEADLESS_CONTROLLER_H
