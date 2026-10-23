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
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#ifdef WITH_PORTAUDIO
#include <portaudio.h>
#endif
#ifdef WITH_PULSEAUDIO
#include <pulse/error.h>
#include <pulse/simple.h>
#endif

#include "gqrx.h"
#include "headless_controller.h"

#include <iostream>

static void reset_conf(const QString &file_name);
static void list_conf();

int main(int argc, char *argv[])
{
    QString cfg_file;

    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(GQRX_ORG_NAME);
    QCoreApplication::setOrganizationDomain(GQRX_ORG_DOMAIN);
    QCoreApplication::setApplicationName(GQRX_APP_NAME);
    QCoreApplication::setApplicationVersion(VERSION);
    QLoggingCategory::setFilterRules("*.debug=false");

    // setup controlport via environment variables
    // see http://lists.gnu.org/archive/html/discuss-gnuradio/2013-05/msg00270.html
    if (qputenv("GR_CONF_CONTROLPORT_ON", "False"))
        qDebug() << "Controlport disabled";
    else
        qDebug() << "Failed to disable controlport";

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Gqrx software defined radio receiver " VERSION " (headless)");
    parser.addHelpOption();
    parser.addOptions({
        {{"l", "list"}, "List existing configurations"},
        {{"c", "conf"}, "Start with this config file", "file"},
        {{"r", "reset"}, "Reset configuration file"},
    });
    parser.process(app);

    if (parser.isSet("list"))
    {
        list_conf();
        return 0;
    }

    // check whether audio backend is functional
#ifdef WITH_PORTAUDIO
    PaError     pa_err = Pa_Initialize();
    if (pa_err != paNoError)
    {
        std::cerr << "Portaudio error: " << Pa_GetErrorText(pa_err) << std::endl;
        return 1;
    }
#endif

#ifdef WITH_PULSEAUDIO
    int         pulse_error = 0;
    pa_simple  *test_sink;
    pa_sample_spec ss;

    ss.format = PA_SAMPLE_FLOAT32LE;
    ss.rate = 48000;
    ss.channels = 2;
    test_sink = pa_simple_new(NULL, "Gqrx Test", PA_STREAM_PLAYBACK, NULL,
                              "Test stream", &ss, NULL, NULL, &pulse_error);
    if (!test_sink)
    {
        std::cerr << "Pulseaudio error: " << pa_strerror(pulse_error) << std::endl;
        return 1;
    }
    pa_simple_free(test_sink);
#endif

    if (parser.isSet("conf"))
    {
        cfg_file = parser.value("conf");
        qDebug() << "User specified config file:" << cfg_file;
    }
    else
    {
        cfg_file = "default.conf";
        qDebug() << "No user supplied config file. Using" << cfg_file;
    }

    if (parser.isSet("reset"))
        reset_conf(cfg_file);

    int return_code = 0;
    try
    {
        HeadlessController controller;

        if (controller.loadConfig(cfg_file))
        {
            std::cout << "gqrx headless running. Press Ctrl+C to stop." << std::endl;
            return_code = QCoreApplication::exec();
        }
        else
        {
            std::cerr << "No usable input device configured in " << cfg_file.toStdString()
                       << ". Set input/device in the config file." << std::endl;
            return_code = 1;
        }
    }
    catch (std::exception &x)
    {
        std::cerr << "gqrx exited with an exception: " << x.what() << std::endl;
        return_code = 1;
    }

#ifdef WITH_PORTAUDIO
    Pa_Terminate();
#endif

    return return_code;
}

/** Reset configuration file specified by file_name. */
static void reset_conf(const QString &file_name)
{
    QString     cfg_file;
    QByteArray  xdg_dir = qgetenv("XDG_CONFIG_HOME");

    if (xdg_dir.isEmpty())
        cfg_file = QString("%1/.config/gqrx/%2").arg(QDir::homePath()).arg(file_name);
    else
        cfg_file = QString("%1/gqrx/%2").arg(xdg_dir.data()).arg(file_name);

    if (QFile::exists(cfg_file))
    {
        if (QFile::remove(cfg_file))
            qDebug() << cfg_file << "deleted";
        else
            qDebug() << "Failed to remove" << cfg_file;
    }
    else
    {
        qDebug() << "Can not delete" << cfg_file << "- file does not exist!";
    }
}

/** List available configurations. */
static void list_conf()
{
    QString     conf_path;
    QByteArray  xdg_dir = qgetenv("XDG_CONFIG_HOME");

    if (xdg_dir.isEmpty())
        conf_path = QString("%1/.config/gqrx/").arg(QDir::homePath());
    else
        conf_path = QString("%1/gqrx/").arg(xdg_dir.data());

    QDir conf_dir = QDir(conf_path, "*.conf", QDir::Name, QDir::Files);
    QStringList conf_files = conf_dir.entryList(QStringList("*.conf"));

    std::cout << std::endl
              << " Existing configuration files in "
              << conf_path.toStdString()
              << std::endl;

    if (conf_files.isEmpty())
    {
        std::cout << "     *** NONE ***" << std::endl;
    }
    else
    {
        for (int i = 0; i < conf_files.count(); i++)
        {
            std::cout << "     ";
            std::cout << conf_files.at(i).toLocal8Bit().constData() << std::endl;
        }
    }
}
