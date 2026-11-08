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

#ifndef INCLUDE_AHRWWVBASEBAND_H
#define INCLUDE_AHRWWVBASEBAND_H

#include <QObject>
#include <QRecursiveMutex>

#include "dsp/samplesinkfifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "ahrwwvsink.h"

class DownChannelizer;
class ChannelAPI;

//! Lives in the channel's own QThread. Buffers device samples in a FIFO,
//! channelizes them around the channel offset and drives AHRWWVSink.
class AHRWWVBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureAHRWWVBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const AHRWWVSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureAHRWWVBaseband* create(const QStringList& settingsKeys, const AHRWWVSettings& settings, bool force) {
            return new MsgConfigureAHRWWVBaseband(settingsKeys, settings, force);
        }

    private:
        AHRWWVSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureAHRWWVBaseband(const QStringList& settingsKeys, const AHRWWVSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    class MsgResetDecoder : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        static MsgResetDecoder* create() { return new MsgResetDecoder(); }
    private:
        MsgResetDecoder() : Message() {}
    };

    AHRWWVBaseband();
    ~AHRWWVBaseband();
    void reset();
    void startWork();
    void stopWork();
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void getMagSqLevels(double& avg, double& peak, int& nbSamples) {
        m_sink.getMagSqLevels(avg, peak, nbSamples);
    }
    void setMessageQueueToChannel(MessageQueue *messageQueue) { m_sink.setMessageQueueToChannel(messageQueue); }
    void setBasebandSampleRate(int sampleRate);
    void setChannel(ChannelAPI *channel);
    double getMagSq() const { return m_sink.getMagSq(); }
    bool isRunning() const { return m_running; }
    int getChannelSampleRate() const;
    unsigned int getFifoFill() { return m_sampleFifo.fill(); } //!< samples waiting (used by the integration tests)

private:
    SampleSinkFifo m_sampleFifo;
    DownChannelizer *m_channelizer;
    AHRWWVSink m_sink;
    MessageQueue m_inputMessageQueue;
    AHRWWVSettings m_settings;
    bool m_running;
    qint64 m_centerFrequency;
    bool m_haveCenterFrequency;
    int m_basebandSampleRate;
    QRecursiveMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void applySettings(const QStringList& settingsKeys, const AHRWWVSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData();
};

#endif // INCLUDE_AHRWWVBASEBAND_H
