/* -*- c++ -*- */
/*
 * Gqrx SDR: Software defined radio receiver powered by GNU Radio and Qt
 *           https://gqrx.dk/
 *
 * Copyright 2011-2013 Alexandru Csete OZ9AEC.
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
#ifndef RX_MODE_H
#define RX_MODE_H

#include <string>
#include <vector>
#include <QString>

/*! \brief Structure describing a gain parameter with its range. */
typedef struct
{
    std::string name;   /*!< The name of this gain stage. */
    double      value;  /*!< Initial value. */
    double      start;  /*!< The lower limit. */
    double      stop;   /*!< The uppewr limit. */
    double      step;   /*!< The resolution/step. */
} gain_t;

/*! \brief A vector with gain parameters.
 *
 * This data structure is used for transferring
 * information about available gain stages.
 */
typedef std::vector<gain_t> gain_list_t;

#define FILTER_PRESET_WIDE      0
#define FILTER_PRESET_NORMAL    1
#define FILTER_PRESET_NARROW    2
#define FILTER_PRESET_USER      3

/*! \brief Demodulator / mode selection and channel filter presets.
 *
 * This mirrors the mode table that used to live in the (now removed) GUI
 * class DockRxOpt. It is kept as a plain, Qt-Widgets-free table so that it
 * can be shared between the headless receiver controller and the remote
 * control (hamlib rigctld compatible) TCP server.
 */
namespace RxMode {

/**
 * Mode selector entries.
 *
 * @note If you change this enum, remember to update the TCP interface.
 * @note Keep in same order as the Strings in ModulationStrings, see rx_mode.cpp.
 */
enum rxopt_mode_idx {
    MODE_OFF        = 0, /*!< Demodulator completely off. */
    MODE_RAW        = 1, /*!< Raw I/Q passthrough. */
    MODE_AM         = 2, /*!< Amplitude modulation. */
    MODE_AM_SYNC    = 3, /*!< Amplitude modulation (synchronous demod). */
    MODE_LSB        = 4, /*!< Lower side band. */
    MODE_USB        = 5, /*!< Upper side band. */
    MODE_CWL        = 6, /*!< CW using LSB filter. */
    MODE_CWU        = 7, /*!< CW using USB filter. */
    MODE_NFM        = 8, /*!< Narrow band FM. */
    MODE_WFM_MONO   = 9, /*!< Broadcast FM (mono). */
    MODE_WFM_STEREO = 10, /*!< Broadcast FM (stereo). */
    MODE_WFM_STEREO_OIRT = 11, /*!< Broadcast FM (stereo oirt). */
    MODE_LAST       = 12
};

/*! \brief Get filter lo/hi for a given mode and preset (0=wide, 1=normal, 2=narrow). */
void getFilterPreset(int mode, int preset, int *lo, int *hi);

/*! \brief Convert a modulation name (as persisted in the config file) to its enum value. */
int GetEnumForModulationString(const QString &param);

/*! \brief Convert a modulation enum value to its canonical display/config name. */
QString GetStringForModulationIndex(int iModulationIndex);

} // namespace RxMode

#endif // RX_MODE_H
