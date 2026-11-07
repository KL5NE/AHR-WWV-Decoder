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

#ifndef INCLUDE_AHRWWV_H
#define INCLUDE_AHRWWV_H

#include <QThread>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "ahrwwvbaseband.h"
#include "ahrwwvsettings.h"
#include "ahrwwvreport.h"

class DeviceAPI;

//! The channel object. Owns the baseband worker and its thread, relays
//! settings to it and decoder reports to the GUI. The web API is not
//! implemented: SDRangel answers 501 for this channel's settings/report
//! endpoints, because adding them would require changes to SDRangel itself.
class AHRWWV : public BasebandSampleSink, public ChannelAPI {
public:
    class MsgConfigureAHRWWV : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const AHRWWVSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureAHRWWV* create(const QStringList& settingsKeys, const AHRWWVSettings& settings, bool force) {
            return new MsgConfigureAHRWWV(settingsKeys, settings, force);
        }

    private:
        AHRWWVSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureAHRWWV(const QStringList& settingsKeys, const AHRWWVSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    //! Decoder snapshot: DSP thread -> channel -> GUI
    class MsgReport : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const AHRWWVReport& getReport() const { return m_report; }
        static MsgReport* create(const AHRWWVReport& report) { return new MsgReport(report); }

    private:
        AHRWWVReport m_report;
        MsgReport(const AHRWWVReport& report) : Message(), m_report(report) {}
    };

    //! GUI "Reset" button
    class MsgResetDecoder : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        static MsgResetDecoder* create() { return new MsgResetDecoder(); }
    private:
        MsgResetDecoder() : Message() {}
    };

    AHRWWV(DeviceAPI *deviceAPI);
    virtual ~AHRWWV();
    virtual void destroy() { delete this; }
    virtual void setDeviceAPI(DeviceAPI *deviceAPI);
    virtual DeviceAPI *getDeviceAPI() { return m_deviceAPI; }

    using BasebandSampleSink::feed;
    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
    virtual void start();
    virtual void stop();
    virtual void pushMessage(Message *msg) { m_inputMessageQueue.push(msg); }
    virtual QString getSinkName() { return objectName(); }

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual const QString& getURI() const { return getName(); }
    virtual void getTitle(QString& title) { title = m_settings.m_title; }
    virtual qint64 getCenterFrequency() const { return m_settings.m_inputFrequencyOffset; }
    virtual void setCenterFrequency(qint64 frequency);
    int getChannelSampleRate() const override { return AHRWWVSettings::AHRWWV_CHANNEL_SAMPLE_RATE; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }
    virtual int getStreamIndex() const { return m_settings.m_streamIndex; }

    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
    {
        (void) streamIndex;
        (void) sinkElseSource;
        return 0;
    }

    double getMagSq() const { return m_basebandSink->getMagSq(); }
    void getMagSqLevels(double& avg, double& peak, int& nbSamples) {
        m_basebandSink->getMagSqLevels(avg, peak, nbSamples);
    }
    uint32_t getNumberOfDeviceStreams() const;
    const AHRWWVReport& getLastReport() const { return m_lastReport; }

    static const char * const m_channelIdURI;
    static const char * const m_channelId;

private:
    DeviceAPI *m_deviceAPI;
    QThread m_thread;
    AHRWWVBaseband *m_basebandSink;
    AHRWWVSettings m_settings;
    int m_basebandSampleRate;
    qint64 m_centerFrequency;
    AHRWWVReport m_lastReport;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const QStringList& settingsKeys, const AHRWWVSettings& settings, bool force = false);
};

#endif // INCLUDE_AHRWWV_H
