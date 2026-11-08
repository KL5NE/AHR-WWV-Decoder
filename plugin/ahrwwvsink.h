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

#ifndef INCLUDE_AHRWWVSINK_H
#define INCLUDE_AHRWWVSINK_H

#include <complex>
#include <memory>
#include <vector>

#include "dsp/channelsamplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "util/messagequeue.h"

#include "ahrwwv/decoder.h"
#include "ahrwwvsettings.h"
#include "ahrwwvreport.h"

class ChannelAPI;

//! Runs in the channel's baseband thread: NCO shift, resample to 12 kHz,
//! feed the portable decoder, post AHRWWVReport snapshots to the channel.
class AHRWWVSink : public ChannelSampleSink {
public:
    AHRWWVSink();
    ~AHRWWVSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const QStringList& settingsKeys, const AHRWWVSettings& settings, bool force = false);
    void setMessageQueueToChannel(MessageQueue *messageQueue) { m_messageQueueToChannel = messageQueue; }
    void setChannel(ChannelAPI *channel) { m_channel = channel; }
    void resetDecoder();

    double getMagSq() const { return m_magsq; }
    void getMagSqLevels(double& avg, double& peak, int& nbSamples);

private:
    struct MagSqLevelsStore
    {
        MagSqLevelsStore() : m_magsq(1e-12), m_magsqPeak(1e-12) {}
        double m_magsq;
        double m_magsqPeak;
    };

    AHRWWVSettings m_settings;
    ChannelAPI *m_channel;
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    NCO m_nco;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;

    double m_magsq;
    double m_magsqSum;
    double m_magsqPeak;
    int m_magsqCount;
    MagSqLevelsStore m_magSqLevelStore;

    MessageQueue *m_messageQueueToChannel;

    std::unique_ptr<ahrwwv::WwvDecoder> m_decoder;
    std::vector<std::complex<float>> m_block;   //!< 12 kHz samples waiting for the decoder
    double m_blockStampPc;                      //!< PC time (Unix s) of the first sample in m_block

    // report accumulation
    AHRWWVReport m_pending;
    double m_lastReportPc;
    quint32 m_framesAccepted;
    quint32 m_framesRejected;
    quint64 m_samplesProcessed;
    bool m_resetFlag;
    double m_envAcc;
    int m_envCount;

    // Sample-loss detector: compares the decoder's sample count with the PC
    // clock. Buffering only ever delays samples; lost samples leave a
    // permanent step in (PC time - sample time).
    double m_gapBucketMin;          //!< min(pc - sampleTime) in the current 5 s bucket
    double m_gapBucketEnd;          //!< sample time at which the current bucket closes
    double m_gapBaseline;           //!< min over the recent buckets
    std::vector<double> m_gapHistory; //!< recent bucket minima
    int m_gapSuspect;               //!< consecutive buckets above the baseline
    quint32 m_discontinuities;      //!< resets caused by detected sample loss

    static const int m_blockSize = AHRWWVSettings::AHRWWV_CHANNEL_SAMPLE_RATE / 20; //!< 50 ms
    static constexpr double m_reportInterval = 0.25; //!< s

    void processOneSample(const Complex& ci);
    void checkContinuity(double pcStamp, double sampleTime);
    void runDecoder();
    void createDecoder();
    static void envelopeTap(void *user, double sampleTime, float level);
    static double nowUnix();
};

#endif // INCLUDE_AHRWWVSINK_H
