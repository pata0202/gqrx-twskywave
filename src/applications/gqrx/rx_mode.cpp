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
#include <QDebug>
#include <QStringList>
#include <iostream>
#include "rx_mode.h"

namespace RxMode {

// Filter preset table per mode, preset and lo/hi
static const int filter_preset_table[MODE_LAST][3][2] =
{   //     WIDE             NORMAL            NARROW
    {{      0,      0}, {     0,     0}, {     0,     0}},  // MODE_OFF
    {{ -15000,  15000}, { -5000,  5000}, { -1000,  1000}},  // MODE_RAW
    {{ -10000,  10000}, { -5000,  5000}, { -2500,  2500}},  // MODE_AM
    {{ -10000,  10000}, { -5000,  5000}, { -2500,  2500}},  // MODE_AMSYNC
    {{  -4000,   -100}, { -2800,  -100}, { -2400,  -300}},  // MODE_LSB
    {{    100,   4000}, {   100,  2800}, {   300,  2400}},  // MODE_USB
    {{  -1000,   1000}, {  -250,   250}, {  -100,   100}},  // MODE_CWL
    {{  -1000,   1000}, {  -250,   250}, {  -100,   100}},  // MODE_CWU
    {{ -10000,  10000}, { -5000,  5000}, { -2500,  2500}}   // MODE_NFM
};

static QStringList ModulationStrings()
{
    // Keep in sync with rxopt_mode_idx and filter_preset_table
    QStringList list;
    list.append("Demod Off");
    list.append("Raw I/Q");
    list.append("AM");
    list.append("AM-Sync");
    list.append("LSB");
    list.append("USB");
    list.append("CW-L");
    list.append("CW-U");
    list.append("Narrow FM");
    return list;
}

void getFilterPreset(int mode, int preset, int *lo, int *hi)
{
    if (mode < 0 || mode >= MODE_LAST)
    {
        qDebug() << __func__ << ": Invalid mode:" << mode;
        mode = MODE_AM;
    }
    else if (preset < 0 || preset > 2)
    {
        qDebug() << __func__ << ": Invalid preset:" << preset;
        preset = FILTER_PRESET_NORMAL;
    }
    *lo = filter_preset_table[mode][preset][0];
    *hi = filter_preset_table[mode][preset][1];
}

int GetEnumForModulationString(const QString &param)
{
    static const QStringList strings = ModulationStrings();
    int iModulation = -1;

    for (int i = 0; i < strings.size(); ++i)
    {
        if (param.compare(strings[i], Qt::CaseInsensitive) == 0)
        {
            iModulation = i;
            break;
        }
    }
    if (iModulation == -1)
    {
        std::cout << "Modulation '" << param.toStdString() << "' is unknown." << std::endl;
        iModulation = MODE_OFF;
    }
    return iModulation;
}

QString GetStringForModulationIndex(int iModulationIndex)
{
    static const QStringList strings = ModulationStrings();
    return strings.value(iModulationIndex);
}

} // namespace RxMode
