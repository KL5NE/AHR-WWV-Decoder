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

#include <QDebug>

#include "dsp/dspcommands.h"
#include "device/deviceapi.h"

#include "ahrwwv.h"

MESSAGE_CLASS_DEFINITION(AHRWWV::MsgConfigureAHRWWV, Message)
MESSAGE_CLASS_DEFINITION(AHRWWV::MsgReport, Message)
MESSAGE_CLASS_DEFINITION(AHRWWV::MsgResetDecoder, Message)

const char * const AHRWWV::m_channelIdURI = "sdrangel.channel.ahrwwvtimedecoder";
const char * const AHRWWV::m_channelId = "AHRWWVTimeDecoder";

AHRWWV::AHRWWV(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
    m_deviceAPI(deviceAPI),
    m_basebandSampleRate(0),
    m_centerFrequency(0)
{
    setObjectName(m_channelId);
    m_thread.setObjectName("AHRWWVBB");

    m_basebandSink = new AHRWWVBaseband();
    m_basebandSink->setMessageQueueToChannel(getInputMessageQueue());
    m_basebandSink->setChannel(this);
    m_basebandSink->moveToThread(&m_thread);

    applySettings(QStringList(), m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);
}

AHRWWV::~AHRWWV()
{
    qDebug("AHRWWV::~AHRWWV");
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this, true);

    if (m_basebandSink->isRunning()) {
        stop();
    }

    delete m_basebandSink;
}

void AHRWWV::setDeviceAPI(DeviceAPI *deviceAPI)
{
    if (deviceAPI != m_deviceAPI)
    {
        m_deviceAPI->removeChannelSinkAPI(this);
        m_deviceAPI->removeChannelSink(this, false);
        m_deviceAPI = deviceAPI;
        m_deviceAPI->addChannelSink(this);
        m_deviceAPI->addChannelSinkAPI(this);
    }
}

uint32_t AHRWWV::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void AHRWWV::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void AHRWWV::start()
{
    qDebug("AHRWWV::start");

    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread.start();

    DSPSignalNotification *dspMsg = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(dspMsg);

    AHRWWVBaseband::MsgConfigureAHRWWVBaseband *msg = AHRWWVBaseband::MsgConfigureAHRWWVBaseband::create(QStringList(), m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

void AHRWWV::stop()
{
    qDebug("AHRWWV::stop");
    m_basebandSink->stopWork();
    m_thread.quit();
    m_thread.wait();
}

void AHRWWV::setCenterFrequency(qint64 frequency)
{
    AHRWWVSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(QStringList("inputFrequencyOffset"), settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureAHRWWV *msgToGUI = MsgConfigureAHRWWV::create(QStringList("inputFrequencyOffset"), settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

bool AHRWWV::handleMessage(const Message& cmd)
{
    if (MsgConfigureAHRWWV::match(cmd))
    {
        MsgConfigureAHRWWV& cfg = (MsgConfigureAHRWWV&) cmd;
        qDebug() << "AHRWWV::handleMessage: MsgConfigureAHRWWV";
        applySettings(cfg.getSettingsKeys(), cfg.getSettings(), cfg.getForce());
        return true;
    }
    else if (MsgResetDecoder::match(cmd))
    {
        m_basebandSink->getInputMessageQueue()->push(AHRWWVBaseband::MsgResetDecoder::create());
        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        m_centerFrequency = notif.getCenterFrequency();
        qDebug() << "AHRWWV::handleMessage: DSPSignalNotification";
        m_basebandSink->getInputMessageQueue()->push(new DSPSignalNotification(notif));

        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (MsgReport::match(cmd))
    {
        MsgReport& report = (MsgReport&) cmd;
        m_lastReport = report.getReport();

        // One log line per minute frame, so a session can be reviewed afterwards.
        for (const auto& fr : m_lastReport.frames)
        {
            const ahrwwv::TimeCode& tc = fr.result.tc;

            if (fr.accepted) {
                qInfo("AHRWWV: frame accepted %04d day %03d %02d:%02d UTC, DUT1 %+.1f s, state %s, station %s",
                      tc.year, tc.dayOfYear, tc.hour, tc.minute, tc.dut1Tenths / 10.0,
                      ahrwwv::decoderStateName(m_lastReport.status.state), ahrwwv::stationName(m_lastReport.status.station));
            } else {
                qInfo("AHRWWV: frame rejected: %s",
                      fr.result.problems.empty() ? "invalid" : fr.result.problems.back().c_str());
            }
        }

        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(MsgReport::create(report.getReport()));
        }

        return true;
    }
    else
    {
        return false;
    }
}

void AHRWWV::applySettings(const QStringList& settingsKeys, const AHRWWVSettings& settings, bool force)
{
    qDebug() << "AHRWWV::applySettings:" << settings.getDebugString(settingsKeys, force);

    if (settingsKeys.contains("streamIndex") && m_settings.m_streamIndex != settings.m_streamIndex)
    {
        if (m_deviceAPI->getSampleMIMO()) // change of stream is possible for MIMO devices only
        {
            m_deviceAPI->removeChannelSinkAPI(this);
            m_deviceAPI->removeChannelSink(this, false, m_settings.m_streamIndex);
            m_deviceAPI->addChannelSink(this, settings.m_streamIndex);
            m_deviceAPI->addChannelSinkAPI(this);
            m_settings.m_streamIndex = settings.m_streamIndex;
            emit streamIndexChanged(settings.m_streamIndex);
        }
    }

    AHRWWVBaseband::MsgConfigureAHRWWVBaseband *msg = AHRWWVBaseband::MsgConfigureAHRWWVBaseband::create(settingsKeys, settings, force);
    m_basebandSink->getInputMessageQueue()->push(msg);

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

QByteArray AHRWWV::serialize() const
{
    return m_settings.serialize();
}

bool AHRWWV::deserialize(const QByteArray& data)
{
    bool ok = m_settings.deserialize(data);

    if (!ok) {
        m_settings.resetToDefaults();
    }

    MsgConfigureAHRWWV *msg = MsgConfigureAHRWWV::create(QStringList(), m_settings, true);
    m_inputMessageQueue.push(msg);
    return ok;
}
