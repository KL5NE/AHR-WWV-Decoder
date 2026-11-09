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
#include "dsp/downchannelizer.h"

#include "ahrwwvbaseband.h"

MESSAGE_CLASS_DEFINITION(AHRWWVBaseband::MsgConfigureAHRWWVBaseband, Message)
MESSAGE_CLASS_DEFINITION(AHRWWVBaseband::MsgResetDecoder, Message)

AHRWWVBaseband::AHRWWVBaseband() :
    m_running(false),
    m_centerFrequency(0),
    m_haveCenterFrequency(false),
    m_basebandSampleRate(0)
{
    qDebug("AHRWWVBaseband::AHRWWVBaseband");
    m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(48000));
    m_channelizer = new DownChannelizer(&m_sink);
}

AHRWWVBaseband::~AHRWWVBaseband()
{
    m_inputMessageQueue.clear();
    delete m_channelizer;
}

void AHRWWVBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_inputMessageQueue.clear();
    m_sampleFifo.reset();
    m_sink.resetDecoder(); // device (re)started: sample stream is not continuous with what came before
}

void AHRWWVBaseband::startWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    QObject::connect(
        &m_sampleFifo,
        &SampleSinkFifo::dataReady,
        this,
        &AHRWWVBaseband::handleData,
        Qt::QueuedConnection
    );
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_running = true;
}

void AHRWWVBaseband::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    QObject::disconnect(
        &m_sampleFifo,
        &SampleSinkFifo::dataReady,
        this,
        &AHRWWVBaseband::handleData
    );
    m_running = false;
}

void AHRWWVBaseband::setChannel(ChannelAPI *channel)
{
    m_sink.setChannel(channel);
}

void AHRWWVBaseband::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    m_sampleFifo.write(begin, end);
}

void AHRWWVBaseband::handleData()
{
    QMutexLocker mutexLocker(&m_mutex);

    while ((m_sampleFifo.fill() > 0) && (m_inputMessageQueue.size() == 0))
    {
        SampleVector::iterator part1begin;
        SampleVector::iterator part1end;
        SampleVector::iterator part2begin;
        SampleVector::iterator part2end;

        std::size_t count = m_sampleFifo.readBegin(m_sampleFifo.fill(), &part1begin, &part1end, &part2begin, &part2end);

        if (part1begin != part1end) {
            m_channelizer->feed(part1begin, part1end);
        }

        if (part2begin != part2end) {
            m_channelizer->feed(part2begin, part2end);
        }

        m_sampleFifo.readCommit((unsigned int) count);
    }
}

void AHRWWVBaseband::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (!handleMessage(*message)) {
            qDebug("%s: unhandled message: %s", Q_FUNC_INFO, message->getIdentifier());
        }

        delete message;
    }
}

bool AHRWWVBaseband::handleMessage(const Message& cmd)
{
    if (MsgConfigureAHRWWVBaseband::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureAHRWWVBaseband& cfg = (MsgConfigureAHRWWVBaseband&) cmd;
        qDebug() << "AHRWWVBaseband::handleMessage: MsgConfigureAHRWWVBaseband";
        applySettings(cfg.getSettingsKeys(), cfg.getSettings(), cfg.getForce());
        return true;
    }
    else if (MsgResetDecoder::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        m_sink.resetDecoder();
        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        qDebug() << "AHRWWVBaseband::handleMessage: DSPSignalNotification: basebandSampleRate: " << notif.getSampleRate()
                 << " centerFrequency: " << notif.getCenterFrequency();
        setBasebandSampleRate(notif.getSampleRate());
        m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(notif.getSampleRate()));

        if (m_haveCenterFrequency &&
            ((notif.getCenterFrequency() != m_centerFrequency) || (notif.getSampleRate() != m_basebandSampleRate)))
        {
            // Receiver retuned (the carrier moved) or its sample rate changed
            // (the device stream restarts, so samples are not continuous).
            m_sink.resetDecoder();
        }

        m_basebandSampleRate = notif.getSampleRate();

        m_centerFrequency = notif.getCenterFrequency();
        m_haveCenterFrequency = true;
        return true;
    }
    else
    {
        return false;
    }
}

void AHRWWVBaseband::applySettings(const QStringList& settingsKeys, const AHRWWVSettings& settings, bool force)
{
    if ((settingsKeys.contains("inputFrequencyOffset") && settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force)
    {
        m_channelizer->setChannelization(AHRWWVSettings::AHRWWV_CHANNEL_SAMPLE_RATE, settings.m_inputFrequencyOffset);
        m_sink.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());
    }

    m_sink.applySettings(settingsKeys, settings, force);

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

void AHRWWVBaseband::setBasebandSampleRate(int sampleRate)
{
    m_channelizer->setBasebandSampleRate(sampleRate);
    m_sink.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());
}

int AHRWWVBaseband::getChannelSampleRate() const
{
    return m_channelizer->getChannelSampleRate();
}
