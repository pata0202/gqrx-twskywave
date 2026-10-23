/* -*- c++ -*- */
/*
 * Gqrx SDR: Software defined radio receiver powered by GNU Radio and Qt
 *           https://gqrx.dk/
 *
 * Headless (GUI-less) controller. See headless_controller.h for details.
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
#include <cmath>
#include <limits>

#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QVariant>
#include <QtGlobal>

#include "gqrx.h"
#include "headless_controller.h"
#include "rx_mode.h"

// Lookup table for conversion from old (pre configversion 3) settings
static const int old2new[] = {
    RxMode::MODE_OFF,
    RxMode::MODE_RAW,
    RxMode::MODE_AM,
    RxMode::MODE_NFM,
    RxMode::MODE_WFM_MONO,
    RxMode::MODE_WFM_STEREO,
    RxMode::MODE_LSB,
    RxMode::MODE_USB,
    RxMode::MODE_CWL,
    RxMode::MODE_CWU,
    RxMode::MODE_WFM_STEREO_OIRT,
    RxMode::MODE_AM_SYNC
};

HeadlessController::HeadlessController(QObject *parent) :
    QObject(parent),
    rx(nullptr),
    remote(nullptr),
    m_settings(nullptr),
    meter_timer(nullptr),
    rds_timer(nullptr),
    d_lnb_lo(0),
    d_hw_freq(0),
    d_current_freq(0),
    d_filter_shape(receiver::FILTER_SHAPE_NORMAL),
    d_filter_preset(FILTER_PRESET_NORMAL),
    d_mode(RxMode::MODE_OFF),
    d_have_audio(false),
    d_audio_gain(0.0f),
    d_audio_muted(false),
    d_cw_offset(700),
    d_fm_maxdev(5000.0f),
    d_fm_deemph(75.0e-6),
    d_am_dcr(true),
    d_amsync_dcr(true),
    d_amsync_pllbw(1000.0f / 1.0e6f),
    d_sql_level(-150.0),
    d_nb_on{false, false},
    d_nb_threshold{3.3f, 2.5f},
    d_nr_on(false),
    d_nr_level(10.0f),
    d_agc_on(true),
    d_agc_hang(false),
    d_agc_threshold(-100),
    d_agc_slope(0),
    d_agc_decay(500),
    d_agc_gain(0)
{
    QByteArray xdg_dir = qgetenv("XDG_CONFIG_HOME");
    if (xdg_dir.isEmpty())
        m_cfg_dir = QString("%1/.config/gqrx").arg(QDir::homePath());
    else
        m_cfg_dir = QString("%1/gqrx").arg(xdg_dir.data());

    rx = new receiver("", "", 1);
    rx->set_rf_freq(144500000.0);

    remote = new RemoteControl();

    meter_timer = new QTimer(this);
    connect(meter_timer, &QTimer::timeout, this, &HeadlessController::meterTimeout);

    rds_timer = new QTimer(this);
    connect(rds_timer, &QTimer::timeout, this, &HeadlessController::rdsTimeout);

    connect(remote, &RemoteControl::newFrequency, this, &HeadlessController::onNewFrequency);
    connect(remote, &RemoteControl::newFilterOffset, this, &HeadlessController::onNewFilterOffset);
    connect(remote, &RemoteControl::newLnbLo, this, &HeadlessController::onNewLnbLo);
    connect(remote, &RemoteControl::newMode, this, &HeadlessController::onNewMode);
    connect(remote, &RemoteControl::newPassband, this, &HeadlessController::onNewPassband);
    connect(remote, &RemoteControl::newSquelchLevel, this, &HeadlessController::onNewSquelchLevel);
    connect(remote, &RemoteControl::newAudioGain, this, &HeadlessController::onNewAudioGain);
    connect(remote, &RemoteControl::newAudioMuted, this, &HeadlessController::onNewAudioMuted);
    connect(remote, &RemoteControl::gainChanged, this, &HeadlessController::onGainChanged);
    connect(remote, &RemoteControl::startAudioRecorderEvent, this, &HeadlessController::onStartAudioRecorder);
    connect(remote, &RemoteControl::stopAudioRecorderEvent, this, &HeadlessController::onStopAudioRecorder);
    connect(remote, &RemoteControl::startIqRecorderEvent, this, &HeadlessController::onStartIqRecorder);
    connect(remote, &RemoteControl::stopIqRecorderEvent, this, &HeadlessController::onStopIqRecorder);
    connect(remote, &RemoteControl::dspChanged, this, &HeadlessController::onDspChanged);
    connect(remote, &RemoteControl::newRDSmode, this, &HeadlessController::onNewRdsMode);
    connect(remote, &RemoteControl::newNbStatus, this, &HeadlessController::onNewNbStatus);
    connect(remote, &RemoteControl::newNbThreshold, this, &HeadlessController::onNewNbThreshold);
    connect(remote, &RemoteControl::newNrStatus, this, &HeadlessController::onNewNrStatus);
    connect(remote, &RemoteControl::newNrLevel, this, &HeadlessController::onNewNrLevel);
}

HeadlessController::~HeadlessController()
{
    stopDsp();
    delete remote;
    delete rx;
    delete m_settings;
}

bool HeadlessController::loadConfig(const QString &cfgfile)
{
    bool conv_ok;
    bool conf_ok = false;

    if (QDir::isAbsolutePath(cfgfile))
        m_settings = new QSettings(cfgfile, QSettings::IniFormat);
    else
        m_settings = new QSettings(QString("%1/%2").arg(m_cfg_dir).arg(cfgfile),
                                    QSettings::IniFormat);

    qInfo() << "Configuration file:" << m_settings->fileName();

    QString indev = m_settings->value("input/device", "").toString();
    if (!indev.isEmpty())
    {
        try
        {
            rx->set_input_device(indev.toStdString());
            conf_ok = true;
        }
        catch (std::runtime_error &x)
        {
            qCritical() << "Failed to set input device:" << x.what();
        }

        if (indev.contains("rtl", Qt::CaseInsensitive) && !m_settings->contains("input/gains"))
        {
            /* rtlsdr gain is 0 by default making users think their device is
             * deaf. Therefore, we don't read gain from the device, but
             * initialize it to the midpoint. */
            updateGainStages(false);
        }
        else
        {
            updateGainStages(true);
        }
    }

    QString outdev = m_settings->value("output/device", "").toString();
    try
    {
        rx->set_output_device(outdev.toStdString());
    }
    catch (std::exception &x)
    {
        qCritical() << "Failed to set output device:" << x.what();
    }

    double actual_rate;
    int int_val = m_settings->value("input/sample_rate", 0).toInt(&conv_ok);
    if (conv_ok && int_val > 0)
    {
        actual_rate = rx->set_input_rate(int_val);
        if (actual_rate == 0)
        {
            qWarning() << "Error configuring input sample rate; device not attached?";
            actual_rate = int_val;
        }
    }
    else
    {
        actual_rate = rx->get_input_rate();
    }

    if (actual_rate > 0.)
    {
        int_val = m_settings->value("input/decimation", 1).toInt(&conv_ok);
        if (conv_ok && int_val >= 2)
        {
            if (rx->set_input_decim(int_val) == (unsigned int)int_val)
                actual_rate /= (double)int_val;
            else
                qWarning() << "Failed to set decimation" << int_val;
        }
        else
        {
            rx->set_input_decim(1);
        }
    }

    qint64 bw_val = m_settings->value("input/bandwidth", 0).toLongLong(&conv_ok);
    if (conv_ok)
        rx->set_analog_bandwidth((double)bw_val);

    /* --- input control settings (was DockInputCtl::readSettings) --- */
    qint64 ppm_corr = m_settings->value("input/corr_freq", 0).toLongLong(&conv_ok);
    rx->set_freq_corr(((double)ppm_corr) / 1.0e6);

    rx->set_iq_swap(m_settings->value("input/swap_iq", false).toBool());
    rx->set_dc_cancel(m_settings->value("input/dc_cancel", false).toBool());

    try
    {
        rx->set_iq_balance(m_settings->value("input/iq_balance", false).toBool());
    }
    catch (std::exception &x)
    {
        qWarning() << "Failed to set IQ balance:" << x.what();
    }

    d_lnb_lo = m_settings->value("input/lnb_lo", 0).toLongLong(&conv_ok);
    remote->setLnbLo(((double)d_lnb_lo) / 1.0e6);

    QString ant = m_settings->value("input/antenna", "").toString();
    if (!ant.isEmpty())
        rx->set_antenna(ant.toStdString());

    // gains are stored as a QMap<QString, QVariant(int)>; values are dB*10
    if (m_settings->contains("input/gains"))
    {
        QMap<QString, QVariant> allgains = m_settings->value("input/gains").toMap();
        QMapIterator<QString, QVariant> git(allgains);
        while (git.hasNext())
        {
            git.next();
            double gain_value = 0.1 * (double)(git.value().toInt());
            rx->set_gain(git.key().toStdString(), gain_value);
        }
    }

    rx->set_auto_gain(m_settings->value("input/hwagc", false).toBool());

    /* --- demodulator / receiver settings (was DockRxOpt::readSettings) --- */
    int_val = m_settings->value("receiver/cwoffset", 700).toInt(&conv_ok);
    d_cw_offset = conv_ok ? int_val : 700;

    int_val = m_settings->value("receiver/fm_maxdev", 5000).toInt(&conv_ok);
    d_fm_maxdev = conv_ok ? (float)int_val : 5000.0f;

    double dbl_val = m_settings->value("receiver/fm_deemph", 75).toDouble(&conv_ok);
    if (conv_ok && dbl_val >= 0)
        d_fm_deemph = 1.0e-6 * dbl_val; // stored as usec

    dbl_val = m_settings->value("receiver/sql_level", 1.0).toDouble(&conv_ok);
    if (conv_ok && dbl_val < 1.0)
        d_sql_level = dbl_val;
    remote->setSquelchLevel(d_sql_level);

    // AGC: applied in selectDemod()
    int_val = m_settings->value("receiver/agc_threshold", -100).toInt(&conv_ok);
    d_agc_threshold = conv_ok ? int_val : -100;

    int_val = m_settings->value("receiver/agc_decay", 500).toInt(&conv_ok);
    d_agc_decay = conv_ok ? int_val : 500;

    int_val = m_settings->value("receiver/agc_slope", 0).toInt(&conv_ok);
    d_agc_slope = conv_ok ? int_val : 0;

    int_val = m_settings->value("receiver/agc_gain", 0).toInt(&conv_ok);
    d_agc_gain = conv_ok ? int_val : 0;

    d_agc_hang = m_settings->value("receiver/agc_usehang", false).toBool();
    d_agc_on = !m_settings->value("receiver/agc_off", false).toBool();

    d_am_dcr = m_settings->value("receiver/am_dcr", true).toBool();
    d_amsync_dcr = m_settings->value("receiver/amsync_dcr", true).toBool();

    int_val = m_settings->value("receiver/amsync_pllbw", 1000).toInt(&conv_ok);
    d_amsync_pllbw = (conv_ok ? int_val : 1000) / 1.0e6f;

    // Noise blanker (only effective in narrow band modes: AM, SSB, CW, NFM)
    for (int i = 0; i < 2; i++)
    {
        QString key = QString("receiver/nb%1").arg(i + 1);
        d_nb_on[i] = m_settings->value(key + "_on", false).toBool();
        float flt_val = m_settings->value(key + "_threshold", d_nb_threshold[i]).toFloat(&conv_ok);
        if (conv_ok) // same ranges as rx_nb_cc accepts: NB1 1-20, NB2 0-15
            d_nb_threshold[i] = std::max(i == 0 ? 1.0f : 0.0f,
                                         std::min(i == 0 ? 20.0f : 15.0f, flt_val));
        remote->setNbStatus(i + 1, d_nb_on[i]);
        remote->setNbThreshold(i + 1, d_nb_threshold[i]);
    }

    // Audio noise reduction (narrow band modes except raw I/Q)
    d_nr_on = m_settings->value("receiver/nr_on", false).toBool();
    float nr_level = m_settings->value("receiver/nr_level", d_nr_level).toFloat(&conv_ok);
    if (conv_ok)
        d_nr_level = std::max(0.0f, std::min(20.0f, nr_level));
    remote->setNrStatus(d_nr_on);
    remote->setNrLevel(d_nr_level);

    int mode_idx = RxMode::MODE_AM;
    if (m_settings->contains("receiver/demod"))
    {
        if (m_settings->value("configversion").toInt(&conv_ok) >= 3)
            mode_idx = RxMode::GetEnumForModulationString(m_settings->value("receiver/demod").toString());
        else
        {
            int old_idx = m_settings->value("receiver/demod").toInt(&conv_ok);
            if (conv_ok && old_idx >= 0 &&
                old_idx < (int)(sizeof(old2new) / sizeof(old2new[0])))
                mode_idx = old2new[old_idx];
        }
    }

    selectDemod(mode_idx);

    qint64 offs = m_settings->value("receiver/offset", 0).toLongLong(&conv_ok);
    if (offs)
    {
        rx->set_filter_offset((double)offs);
        d_current_freq += offs;
        remote->setFilterOffset(offs);
    }

    qint64 freq = m_settings->value("input/frequency", 14236000).toLongLong(&conv_ok);
    setNewFrequency(freq);

    {
        int flo = m_settings->value("receiver/filter_low_cut", 0).toInt(&conv_ok);
        int fhi = m_settings->value("receiver/filter_high_cut", 0).toInt(&conv_ok);
        if (conv_ok && mode_idx != RxMode::MODE_OFF && flo != fhi)
        {
            rx->set_filter((double)flo, (double)fhi, (receiver::filter_shape)d_filter_shape);
            remote->setPassband(flo, fhi);
        }
    }

    d_iq_rec_dir = m_settings->value("baseband/rec_dir", QDir::homePath()).toString();
    d_iq_rec_format = m_settings->value("baseband/rec_format", "Raw").toString();

    m_settings->beginGroup("audio");
    d_audio_rec_dir = m_settings->value("rec_dir", QDir::homePath()).toString();
    int_val = m_settings->value("gain", -60).toInt(&conv_ok);
    m_settings->endGroup();
    d_audio_gain = (conv_ok ? int_val : -60) / 10.0f;
    rx->set_af_gain(d_audio_gain);
    remote->setAudioGain(d_audio_gain);

    remote->setBandwidth((qint64)actual_rate);
    remote->readSettings(m_settings);

    // Headless gqrx exists specifically to be driven remotely, so the remote
    // control server defaults to "on" unless explicitly disabled in the
    // config file (upstream gqrx defaults this to "off").
    if (m_settings->value("remote_control/enabled", true).toBool())
        remote->start_server();

    startDsp();

    return conf_ok;
}

void HeadlessController::updateGainStages(bool read_from_device)
{
    gain_list_t gain_list;
    std::vector<std::string> gain_names = rx->get_gain_names();

    for (auto &name : gain_names)
    {
        gain_t gain;
        gain.name = name;
        rx->get_gain_range(gain.name, &gain.start, &gain.stop, &gain.step);
        if (read_from_device)
        {
            gain.value = rx->get_gain(gain.name);
        }
        else
        {
            gain.value = (gain.start + gain.stop) / 2;
            rx->set_gain(gain.name, gain.value);
        }
        gain_list.push_back(gain);
    }

    remote->setGainStages(gain_list);
}

void HeadlessController::selectDemod(int mode_idx)
{
    double cwofs = 0.0;
    int flo = 0, fhi = 0;

    if (mode_idx < RxMode::MODE_OFF || mode_idx >= RxMode::MODE_LAST)
    {
        qWarning() << "Invalid mode index:" << mode_idx;
        mode_idx = RxMode::MODE_OFF;
    }

    RxMode::getFilterPreset(mode_idx, d_filter_preset, &flo, &fhi);

    bool rds_enabled = rx->is_rds_decoder_active();
    if (rds_enabled)
        setRdsDecoder(false);

    switch (mode_idx)
    {
    case RxMode::MODE_OFF:
        onStopAudioRecorder();
        rx->set_demod(receiver::RX_DEMOD_OFF);
        break;

    case RxMode::MODE_RAW:
        rx->set_demod(receiver::RX_DEMOD_NONE);
        break;

    case RxMode::MODE_AM:
        rx->set_demod(receiver::RX_DEMOD_AM);
        rx->set_am_dcr(d_am_dcr);
        break;

    case RxMode::MODE_AM_SYNC:
        rx->set_demod(receiver::RX_DEMOD_AMSYNC);
        rx->set_amsync_dcr(d_amsync_dcr);
        rx->set_amsync_pll_bw(d_amsync_pllbw);
        break;

    case RxMode::MODE_NFM:
        rx->set_demod(receiver::RX_DEMOD_NFM);
        rx->set_fm_maxdev(d_fm_maxdev);
        rx->set_fm_deemph(d_fm_deemph);
        break;

    case RxMode::MODE_WFM_MONO:
    case RxMode::MODE_WFM_STEREO:
    case RxMode::MODE_WFM_STEREO_OIRT:
        if (mode_idx == RxMode::MODE_WFM_MONO)
            rx->set_demod(receiver::RX_DEMOD_WFM_M);
        else if (mode_idx == RxMode::MODE_WFM_STEREO_OIRT)
            rx->set_demod(receiver::RX_DEMOD_WFM_S_OIRT);
        else
            rx->set_demod(receiver::RX_DEMOD_WFM_S);

        if (rds_enabled)
            setRdsDecoder(true);
        break;

    case RxMode::MODE_LSB:
    case RxMode::MODE_USB:
        rx->set_demod(receiver::RX_DEMOD_SSB);
        break;

    case RxMode::MODE_CWL:
        rx->set_demod(receiver::RX_DEMOD_SSB);
        cwofs = -d_cw_offset;
        break;

    case RxMode::MODE_CWU:
        rx->set_demod(receiver::RX_DEMOD_SSB);
        cwofs = d_cw_offset;
        break;

    default:
        qWarning() << "Unsupported mode selection (can't happen!):" << mode_idx;
        flo = -5000;
        fhi = 5000;
        break;
    }

    rx->set_filter((double)flo, (double)fhi, (receiver::filter_shape)d_filter_shape);
    rx->set_cw_offset(cwofs);
    rx->set_sql_level(d_sql_level);
    for (int i = 0; i < 2; i++)
    {
        rx->set_nb_threshold(i + 1, d_nb_threshold[i]);
        rx->set_nb_on(i + 1, d_nb_on[i]);
    }
    rx->set_nr_level(d_nr_level);
    rx->set_nr_on(d_nr_on);
    rx->set_agc_threshold(d_agc_threshold);
    rx->set_agc_decay(d_agc_decay);
    rx->set_agc_slope(d_agc_slope);
    rx->set_agc_manual_gain(d_agc_gain);
    rx->set_agc_hang(d_agc_hang);
    rx->set_agc_on(d_agc_on);

    remote->setMode(mode_idx);
    remote->setPassband(flo, fhi);

    d_have_audio = (mode_idx != RxMode::MODE_OFF);
    d_mode = mode_idx;
}

void HeadlessController::setNewFrequency(qint64 abs_freq)
{
    double hw_freq = (double)(abs_freq - d_lnb_lo) - rx->get_filter_offset();

    d_hw_freq = (qint64)hw_freq;
    d_current_freq = abs_freq;

    rx->set_rf_freq(hw_freq);

    // Keep the remote control's cached frequency in sync. When the change
    // originated from the remote control itself (TCP client), it has
    // already updated its own cache before calling us, so this is a no-op;
    // this call matters for changes that originate locally (config load,
    // LNB LO changes).
    remote->setNewFrequency(abs_freq);
}

void HeadlessController::onNewFrequency(qint64 freq)
{
    setNewFrequency(freq);
}

void HeadlessController::onNewFilterOffset(qint64 offset)
{
    rx->set_filter_offset((double)offset);
    d_current_freq = d_hw_freq + d_lnb_lo + offset;

    if (rx->is_rds_decoder_active())
        rx->reset_rds_parser();
}

void HeadlessController::onNewLnbLo(double freq_mhz)
{
    qint64 rf_freq = d_current_freq - d_lnb_lo;

    d_lnb_lo = qint64(freq_mhz * 1e6);
    qDebug() << "New LNB LO:" << d_lnb_lo << "Hz";

    setNewFrequency(d_lnb_lo + rf_freq);
}

void HeadlessController::onNewMode(int mode_idx)
{
    selectDemod(mode_idx);
}

void HeadlessController::onNewPassband(int passband)
{
    int lo, hi;
    RxMode::getFilterPreset(d_mode, d_filter_preset, &lo, &hi);

    if (lo + hi == 0)
    {
        lo = -passband / 2;
        hi = passband / 2;
    }
    else if (lo >= 0 && hi >= 0)
    {
        hi = lo + passband;
    }
    else if (lo <= 0 && hi <= 0)
    {
        lo = hi - passband;
    }

    remote->setPassband(lo, hi);
    rx->set_filter((double)lo, (double)hi, (receiver::filter_shape)d_filter_shape);
}

void HeadlessController::onNewSquelchLevel(double level)
{
    d_sql_level = level;
    rx->set_sql_level(level);
}

void HeadlessController::onNewNbStatus(int nbid, bool enabled)
{
    d_nb_on[nbid - 1] = enabled;
    rx->set_nb_on(nbid, enabled);
}

void HeadlessController::onNewNbThreshold(int nbid, float threshold)
{
    d_nb_threshold[nbid - 1] = threshold;
    rx->set_nb_threshold(nbid, threshold);
}

void HeadlessController::onNewNrStatus(bool enabled)
{
    d_nr_on = enabled;
    rx->set_nr_on(enabled);
}

void HeadlessController::onNewNrLevel(float level_db)
{
    d_nr_level = level_db;
    rx->set_nr_level(level_db);
}

void HeadlessController::onNewAudioGain(float gain)
{
    d_audio_gain = gain;
    if (!d_audio_muted)
        rx->set_af_gain(gain);

    // Unlike newSquelchLevel/newMode, RemoteControl::cmd_set_level() does not
    // update its own "audio_gain" cache before emitting newAudioGain(); echo
    // the applied value back so "l AF" reflects it (mirrors the GUI audio
    // dock's slider round trip).
    remote->setAudioGain(gain);
}

void HeadlessController::onNewAudioMuted(bool muted)
{
    d_audio_muted = muted;
    if (muted)
        rx->set_af_gain(-std::numeric_limits<float>::infinity());
    else
        rx->set_af_gain(d_audio_gain);

    // RemoteControl::newAudioMuted() only broadcasts the request; it relies
    // on the receiving side to call back setAudioMuted() once applied so
    // that its own "u MUTE" cache reflects the real state (mirrors how the
    // GUI's audio dock used to close this loop).
    remote->setAudioMuted(muted);
}

void HeadlessController::onGainChanged(QString name, double value)
{
    rx->set_gain(name.toStdString(), value);
}

void HeadlessController::onStartAudioRecorder()
{
    if (!d_have_audio)
    {
        qWarning() << "Cannot start audio recording: demodulator is off";
        return;
    }
    if (rx->is_recording_audio())
        return;

    QString file_name = QDateTime::currentDateTime().toUTC().toString("gqrx_yyyyMMdd_hhmmss");
    QString filename = QString("%1/%2_%3.wav").arg(d_audio_rec_dir).arg(file_name).arg(d_current_freq);

    if (rx->start_audio_recording(filename.toStdString()))
        qWarning() << "Error starting audio recorder";
    // on success, receiver::start_audio_recording() already logs the path
}

void HeadlessController::onStopAudioRecorder()
{
    if (!rx->is_recording_audio())
        return;

    if (rx->stop_audio_recording())
        qWarning() << "Error stopping audio recorder";
    // on success, receiver::stop_audio_recording() already logs this
}

void HeadlessController::onStartIqRecorder()
{
    // generate file name using date, time, rf freq in kHz and BW in Hz
    // gqrx_iq_yyyymmdd_hhmmss_freq_bw_fc.raw
    auto freq = qRound64(rx->get_rf_freq());
    auto sr = qRound64(rx->get_input_rate());
    auto dec = (quint32)(rx->get_input_decim());
    auto currentDate = QDateTime::currentDateTimeUtc();
    auto filenameTemplate = currentDate.toString("%1/gqrx_yyyyMMdd_hhmmss_%2_%3_fc.%4")
                                 .arg(d_iq_rec_dir).arg(freq).arg(sr / dec);
    bool sigmf = (d_iq_rec_format == "SigMF");
    auto lastRec = filenameTemplate.arg(sigmf ? "sigmf-data" : "raw");

    bool ok = true;
    QFile metaFile(filenameTemplate.arg("sigmf-meta"));
    if (sigmf)
    {
        auto meta = QJsonDocument{QJsonObject{
            {"global", QJsonObject{
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
                {"core:datatype", "cf32_be"},
#else
                {"core:datatype", "cf32_le"},
#endif
                {"core:sample_rate", (qint64)(sr / dec)},
                {"core:version", "1.0.0"},
                {"core:recorder", "Gqrx " VERSION},
                {"core:hw", QString("OsmoSDR: ") + m_settings->value("input/device", "").toString()},
            }}, {"captures", QJsonArray{
                QJsonObject{
                    {"core:sample_start", 0},
                    {"core:frequency", freq},
                    {"core:datetime", currentDate.toString(Qt::ISODateWithMs)},
                },
            }}, {"annotations", QJsonArray{}},
        }}.toJson();

        if (!metaFile.open(QIODevice::WriteOnly) || metaFile.write(meta) != meta.size())
            ok = false;
    }

    if (!ok || rx->start_iq_recording(lastRec.toStdString()))
    {
        if (sigmf && metaFile.isOpen())
            metaFile.remove();
        qWarning() << "Error starting I/Q recorder";
    }
    else
    {
        qInfo() << "Recording I/Q data to:" << lastRec;
    }
}

void HeadlessController::onStopIqRecorder()
{
    if (rx->stop_iq_recording())
        qWarning() << "Error stopping I/Q recorder";
    else
        qInfo() << "I/Q recorder stopped";
}

void HeadlessController::onDspChanged(bool enabled)
{
    if (enabled)
        startDsp();
    else
        stopDsp();
}

void HeadlessController::setRdsDecoder(bool enabled)
{
    if (enabled)
    {
        qInfo() << "Starting RDS decoder";
        rx->start_rds_decoder();
        rx->reset_rds_parser();
        rds_timer->start(250);
    }
    else
    {
        qInfo() << "Stopping RDS decoder";
        rx->stop_rds_decoder();
        rds_timer->stop();
    }
    remote->setRDSstatus(enabled);
}

void HeadlessController::onNewRdsMode(bool enabled)
{
    setRdsDecoder(enabled);
}

void HeadlessController::startDsp()
{
    remote->setReceiverStatus(true);
    rx->start();
    meter_timer->start(100);
}

void HeadlessController::stopDsp()
{
    if (meter_timer)
        meter_timer->stop();
    if (rds_timer)
        rds_timer->stop();
    if (rx)
        rx->stop();
    if (remote)
        remote->setReceiverStatus(false);
}

void HeadlessController::meterTimeout()
{
    remote->setSignalLevel(rx->get_signal_pwr());
}

void HeadlessController::rdsTimeout()
{
    std::string buffer;
    int type;

    rx->get_rds_data(buffer, type);
    while (type != -1)
    {
        QString text = QString::fromStdString(buffer);
        switch (type)
        {
        case 0: // PI
            remote->rdsPI(text);
            break;
        case 1: // PS (station name)
            remote->setRdsStation(text);
            break;
        case 4: // RadioText
            remote->setRdsRadiotext(text);
            break;
        default:
            // PTY, flags, clocktime and alternative frequencies are not
            // exposed over the remote control protocol.
            break;
        }
        rx->get_rds_data(buffer, type);
    }
}
