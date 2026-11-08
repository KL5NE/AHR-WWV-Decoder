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

#include <QColor>

#include "util/simpleserializer.h"
#include "settings/serializable.h"

#include "ahrwwvsettings.h"

AHRWWVSettings::AHRWWVSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void AHRWWVSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_decoderEnabled = true;
    m_station = StationAuto;
    m_showDiagnostics = false;
    m_latencyCorrectionMs = 0.0f;
    m_rxLatitude = 0.0f;
    m_rxLongitude = 0.0f;
    m_useStationLocation = true;
    m_rgbColor = QColor(0, 160, 120).rgb();
    m_title = "WWV Time Decoder";
    m_streamIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray AHRWWVSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeBool(2, m_decoderEnabled);
    s.writeS32(3, (int) m_station);
    s.writeBool(4, m_showDiagnostics);
    s.writeFloat(5, m_latencyCorrectionMs);
    s.writeFloat(6, m_rxLatitude);
    s.writeFloat(7, m_rxLongitude);
    s.writeBool(8, m_useStationLocation);

    s.writeU32(12, m_rgbColor);
    s.writeString(13, m_title);

    if (m_channelMarker) {
        s.writeBlob(14, m_channelMarker->serialize());
    }

    s.writeS32(15, m_streamIndex);

    if (m_rollupState) {
        s.writeBlob(22, m_rollupState->serialize());
    }

    s.writeS32(23, m_workspaceIndex);
    s.writeBlob(24, m_geometryBytes);
    s.writeBool(25, m_hidden);

    return s.final();
}

bool AHRWWVSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        QByteArray bytetmp;
        int itmp;

        d.readS32(1, &m_inputFrequencyOffset, 0);
        d.readBool(2, &m_decoderEnabled, true);
        d.readS32(3, &itmp, (int) StationAuto);
        m_station = (itmp >= (int) StationAuto && itmp <= (int) StationWWVH) ? (Station) itmp : StationAuto;
        d.readBool(4, &m_showDiagnostics, false);
        d.readFloat(5, &m_latencyCorrectionMs, 0.0f);
        d.readFloat(6, &m_rxLatitude, 0.0f);
        d.readFloat(7, &m_rxLongitude, 0.0f);
        d.readBool(8, &m_useStationLocation, true);

        d.readU32(12, &m_rgbColor, QColor(0, 160, 120).rgb());
        d.readString(13, &m_title, "WWV Time Decoder");

        if (m_channelMarker)
        {
            d.readBlob(14, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readS32(15, &m_streamIndex, 0);

        if (m_rollupState)
        {
            d.readBlob(22, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(23, &m_workspaceIndex, 0);
        d.readBlob(24, &m_geometryBytes);
        d.readBool(25, &m_hidden, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void AHRWWVSettings::applySettings(const QStringList& settingsKeys, const AHRWWVSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("decoderEnabled")) {
        m_decoderEnabled = settings.m_decoderEnabled;
    }
    if (settingsKeys.contains("station")) {
        m_station = settings.m_station;
    }
    if (settingsKeys.contains("showDiagnostics")) {
        m_showDiagnostics = settings.m_showDiagnostics;
    }
    if (settingsKeys.contains("latencyCorrectionMs")) {
        m_latencyCorrectionMs = settings.m_latencyCorrectionMs;
    }
    if (settingsKeys.contains("rxLatitude")) {
        m_rxLatitude = settings.m_rxLatitude;
    }
    if (settingsKeys.contains("rxLongitude")) {
        m_rxLongitude = settings.m_rxLongitude;
    }
    if (settingsKeys.contains("useStationLocation")) {
        m_useStationLocation = settings.m_useStationLocation;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("streamIndex")) {
        m_streamIndex = settings.m_streamIndex;
    }
    if (settingsKeys.contains("workspaceIndex")) {
        m_workspaceIndex = settings.m_workspaceIndex;
    }
    if (settingsKeys.contains("geometryBytes")) {
        m_geometryBytes = settings.m_geometryBytes;
    }
    if (settingsKeys.contains("hidden")) {
        m_hidden = settings.m_hidden;
    }
}

QString AHRWWVSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    QStringList parts;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        parts << QString("inputFrequencyOffset: %1").arg(m_inputFrequencyOffset);
    }
    if (settingsKeys.contains("decoderEnabled") || force) {
        parts << QString("decoderEnabled: %1").arg(m_decoderEnabled);
    }
    if (settingsKeys.contains("station") || force) {
        parts << QString("station: %1").arg((int) m_station);
    }
    if (settingsKeys.contains("showDiagnostics") || force) {
        parts << QString("showDiagnostics: %1").arg(m_showDiagnostics);
    }
    if (settingsKeys.contains("latencyCorrectionMs") || force) {
        parts << QString("latencyCorrectionMs: %1").arg(m_latencyCorrectionMs);
    }
    if (settingsKeys.contains("title") || force) {
        parts << QString("title: %1").arg(m_title);
    }

    return parts.join(" ");
}
