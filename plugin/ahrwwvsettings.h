// SPDX-License-Identifier: GPL-3.0-or-later
///////////////////////////////////////////////////////////////////////////////////
// AHR WWV/WWVH Time Decoder - SDRangel receive channel plugin                   //
// Copyright (C) 2026 KL5NE, Alaska HamRadio                                     //
//                                                                               //
// Structure follows SDRangel's Radio Clock channel plugin,                      //
// (C) Edouard Griffiths F4EXB, Jon Beniston M7RCE and contributors.             //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_AHRWWVSETTINGS_H
#define INCLUDE_AHRWWVSETTINGS_H

#include <QByteArray>
#include <QString>
#include <QStringList>

#include "dsp/dsptypes.h"

class Serializable;

struct AHRWWVSettings
{
    enum Station {
        StationAuto,   //!< label from the tick-tone detector (WWV, WWVH or both)
        StationWWV,    //!< operator says the signal is WWV (Fort Collins)
        StationWWVH    //!< operator says the signal is WWVH (Kauai)
    };

    qint32 m_inputFrequencyOffset;  //!< channel offset from the device centre (Hz)
    bool m_decoderEnabled;          //!< false: channel passes samples but the decoder is idle
    Station m_station;              //!< station label / propagation path used for the PC offset
    bool m_showDiagnostics;         //!< show the envelope / pulse diagnostic rollup
    float m_latencyCorrectionMs;    //!< known SDR + SDRangel buffering delay subtracted from the PC offset
    float m_rxLatitude;             //!< receiver position for the skywave delay estimate (degrees)
    float m_rxLongitude;
    bool m_useStationLocation;      //!< take the receiver position from SDRangel's My Position

    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
    Serializable *m_rollupState;
    int m_streamIndex;              //!< MIMO channel. Not relevant when connected to SI (single Rx).
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    //! Rate the decoder runs at. Exact; SDRangel's interpolator resamples to it.
    static const int AHRWWV_CHANNEL_SAMPLE_RATE = 12000;
    //! Anti-alias / channel filter half bandwidth handed to the interpolator (Hz).
    static const int AHRWWV_CHANNEL_HALF_BANDWIDTH = 2000;

    AHRWWVSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const AHRWWVSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force = false) const;
};

#endif // INCLUDE_AHRWWVSETTINGS_H
