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

#include <algorithm>
#include <chrono>

#include <QDebug>

#include "ahrwwv.h"
#include "ahrwwvsink.h"

AHRWWVSink::AHRWWVSink() :
    m_channel(nullptr),
    m_channelSampleRate(AHRWWVSettings::AHRWWV_CHANNEL_SAMPLE_RATE),
    m_channelFrequencyOffset(0),
    m_interpolatorDistance(1.0f),
    m_interpolatorDistanceRemain(1.0f),
    m_magsq(0.0),
    m_magsqSum(0.0),
    m_magsqPeak(0.0),
    m_magsqCount(0),
    m_messageQueueToChannel(nullptr),
    m_blockStampPc(0.0),
    m_lastReportPc(0.0),
    m_framesAccepted(0),
    m_framesRejected(0),
    m_samplesProcessed(0),
    m_resetFlag(false),
    m_envAcc(0.0),
    m_envCount(0),
    m_gapBucketMin(1e300),
    m_gapBucketEnd(0.0),
    m_gapBaseline(1e300),
    m_gapSuspect(0),
    m_discontinuities(0)
{
    m_block.reserve(m_blockSize);
    createDecoder();
    applySettings(QStringList(), m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

AHRWWVSink::~AHRWWVSink()
{
}

double AHRWWVSink::nowUnix()
{
    using namespace std::chrono;
    return duration_cast<microseconds>(system_clock::now().time_since_epoch()).count() / 1e6;
}

void AHRWWVSink::createDecoder()
{
    ahrwwv::DecoderConfig cfg;
    cfg.sampleRate = AHRWWVSettings::AHRWWV_CHANNEL_SAMPLE_RATE;
    // Propagation delay and SDRangel buffering are removed in the GUI, where the
    // station / location / latency settings live, so the decoder itself is not
    // rebuilt (and does not lose lock) when they change.
    cfg.propagationDelay = 0.0;
    m_decoder.reset(new ahrwwv::WwvDecoder(cfg));
    m_decoder->setEnvelopeTap(&AHRWWVSink::envelopeTap, this);
    m_block.clear();
    m_pending = AHRWWVReport();
    m_framesAccepted = 0;
    m_framesRejected = 0;
    m_samplesProcessed = 0;
    m_envAcc = 0.0;
    m_envCount = 0;
    m_resetFlag = true;
    m_gapBucketMin = 1e300;
    m_gapBucketEnd = 0.0;
    m_gapBaseline = 1e300;
    m_gapHistory.clear();
    m_gapSuspect = 0;
}

void AHRWWVSink::resetDecoder()
{
    qDebug("AHRWWVSink::resetDecoder");
    createDecoder();
}

void AHRWWVSink::envelopeTap(void *user, double sampleTime, float level)
{
    (void) sampleTime;
    AHRWWVSink *sink = static_cast<AHRWWVSink*>(user);
    // ~1 kHz in, 100 Hz out (10-sample means) for the GUI plot
    sink->m_envAcc += level;

    if (++sink->m_envCount >= 10)
    {
        if (sink->m_pending.envelope.size() < 2000) { // cap if the GUI stalls
            sink->m_pending.envelope.push_back((float) (sink->m_envAcc / sink->m_envCount));
        }

        sink->m_envAcc = 0.0;
        sink->m_envCount = 0;
    }
}

void AHRWWVSink::getMagSqLevels(double& avg, double& peak, int& nbSamples)
{
    if (m_magsqCount > 0)
    {
        m_magsq = m_magsqSum / m_magsqCount;
        m_magSqLevelStore.m_magsq = m_magsq;
        m_magSqLevelStore.m_magsqPeak = m_magsqPeak;
    }

    avg = m_magSqLevelStore.m_magsq;
    peak = m_magSqLevelStore.m_magsqPeak;
    nbSamples = m_magsqCount == 0 ? 1 : m_magsqCount;

    m_magsqSum = 0.0f;
    m_magsqPeak = 0.0f;
    m_magsqCount = 0;
}

void AHRWWVSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    Complex ci;

    for (SampleVector::const_iterator it = begin; it != end; ++it)
    {
        Complex c(it->real(), it->imag());
        c *= m_nco.nextIQ();

        if (m_interpolatorDistance < 1.0f) // interpolate
        {
            while (!m_interpolator.interpolate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(ci);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
        else // decimate
        {
            if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(ci);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
    }
}

void AHRWWVSink::processOneSample(const Complex& ci)
{
    Complex c = ci / SDR_RX_SCALEF;
    double magsq = c.real() * c.real() + c.imag() * c.imag();
    m_magsqSum += magsq;
    m_magsqPeak = std::max<double>(magsq, m_magsqPeak);
    m_magsqCount++;

    if (!m_settings.m_decoderEnabled) {
        return;
    }

    if (m_block.empty()) {
        // Wall-clock stamp of the first sample of this block. It is taken when
        // the sample reaches the plugin, so it includes the SDR and SDRangel
        // buffering latency; the GUI subtracts the user's latency correction.
        m_blockStampPc = nowUnix();
    }

    m_block.push_back(std::complex<float>(c.real(), c.imag()));

    if ((int) m_block.size() >= m_blockSize) {
        runDecoder();
    }
}

void AHRWWVSink::checkContinuity(double pcStamp, double sampleTime)
{
    const double bucket = 5.0;      // s of samples per bucket
    const double threshold = 0.25;  // s: smaller steps are within SDRangel's normal jitter

    m_gapBucketMin = std::min(m_gapBucketMin, pcStamp - sampleTime);

    if (sampleTime < m_gapBucketEnd) {
        return;
    }

    if (m_gapBucketEnd > 0.0) // a full bucket has been seen
    {
        if (m_gapBaseline < 1e299 && m_gapBucketMin - m_gapBaseline > threshold)
        {
            // Two buckets in a row above the baseline: not a temporary
            // processing delay but missing samples (FIFO overflow, USB drop).
            if (++m_gapSuspect >= 2)
            {
                qWarning("AHRWWVSink: about %.2f s of samples missing; restarting the decoder",
                         m_gapBucketMin - m_gapBaseline);
                m_discontinuities++;
                quint32 d = m_discontinuities;
                createDecoder();
                m_discontinuities = d;
                return;
            }
        }
        else
        {
            m_gapSuspect = 0;
            m_gapHistory.push_back(m_gapBucketMin);

            if (m_gapHistory.size() > 3) {
                m_gapHistory.erase(m_gapHistory.begin());
            }

            m_gapBaseline = *std::min_element(m_gapHistory.begin(), m_gapHistory.end());
        }
    }

    m_gapBucketEnd = sampleTime + bucket;
    m_gapBucketMin = 1e300;
}

void AHRWWVSink::runDecoder()
{
    checkContinuity(m_blockStampPc, m_samplesProcessed / (double) AHRWWVSettings::AHRWWV_CHANNEL_SAMPLE_RATE);

    if (m_block.empty()) { // decoder was restarted by checkContinuity
        return;
    }

    m_decoder->setNextSampleTimestamp(m_blockStampPc);
    m_decoder->processIQ(m_block.data(), m_block.size());
    m_samplesProcessed += m_block.size();
    double pcAtEnd = m_blockStampPc + (m_block.size() - 1) / (double) AHRWWVSettings::AHRWWV_CHANNEL_SAMPLE_RATE;
    m_block.clear();

    for (const auto& ev : m_decoder->takeEvents())
    {
        switch (ev.type)
        {
        case ahrwwv::DecoderEvent::Type::Second:
            m_pending.newSeconds.push_back(ev.second);
            break;
        case ahrwwv::DecoderEvent::Type::FrameDecoded:
        case ahrwwv::DecoderEvent::Type::FrameRejected:
        {
            AHRWWVReport::Frame fr;
            fr.accepted = ev.type == ahrwwv::DecoderEvent::Type::FrameDecoded;
            fr.result = ev.decode;
            m_pending.frames.push_back(fr);

            if (fr.accepted) {
                m_framesAccepted++;
            } else {
                m_framesRejected++;
            }
            break;
        }
        default:
            break;
        }
    }

    if ((pcAtEnd - m_lastReportPc < m_reportInterval) && m_pending.frames.empty()) {
        return;
    }

    m_lastReportPc = pcAtEnd;

    if (m_messageQueueToChannel)
    {
        AHRWWVReport& r = m_pending;
        r.status = m_decoder->status();
        double utc;
        // No provisional UTC reaches the GUI clock or PC-clock comparison.
        // Only VERIFIED and subsequent verified HOLDOVER are trusted.
        bool trusted = r.status.state == ahrwwv::DecoderState::Verified
            || (r.status.state == ahrwwv::DecoderState::Holdover
                && r.status.verifiedFrames >= 2);
        r.haveUtc = trusted && m_decoder->currentUtc(utc);
        r.utcAtRef = r.haveUtc ? utc : 0.0;
        r.pcAtRef = pcAtEnd;
        r.detectorDelay = m_decoder->detectorDelay();
        r.framesAccepted = m_framesAccepted;
        r.framesRejected = m_framesRejected;
        r.samplesProcessed = m_samplesProcessed;
        r.channelSampleRate = AHRWWVSettings::AHRWWV_CHANNEL_SAMPLE_RATE;
        r.decoderEnabled = m_settings.m_decoderEnabled;
        r.resetHappened = m_resetFlag;
        r.discontinuities = m_discontinuities;
        m_messageQueueToChannel->push(AHRWWV::MsgReport::create(r));
    }

    m_pending = AHRWWVReport();
    m_resetFlag = false;
}

void AHRWWVSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "AHRWWVSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((m_channelFrequencyOffset != channelFrequencyOffset) ||
        (m_channelSampleRate != channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((m_channelSampleRate != channelSampleRate) || force)
    {
        m_interpolator.create(16, channelSampleRate, AHRWWVSettings::AHRWWV_CHANNEL_HALF_BANDWIDTH);
        m_interpolatorDistance = (Real) channelSampleRate / (Real) AHRWWVSettings::AHRWWV_CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    // A retune moves the carrier and breaks the sample-clock continuity the
    // decoder relies on, so start over. The first call (force) has no history.
    if (!force && ((m_channelFrequencyOffset != channelFrequencyOffset) || (m_channelSampleRate != channelSampleRate))) {
        resetDecoder();
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void AHRWWVSink::applySettings(const QStringList& settingsKeys, const AHRWWVSettings& settings, bool force)
{
    qDebug() << "AHRWWVSink::applySettings:" << settings.getDebugString(settingsKeys, force);

    if ((settingsKeys.contains("decoderEnabled") || force) && (settings.m_decoderEnabled != m_settings.m_decoderEnabled))
    {
        // Disabled: samples stop flowing into the decoder, so its sample clock
        // would no longer match real time. Start fresh when it comes back.
        m_block.clear();

        if (settings.m_decoderEnabled) {
            resetDecoder();
        }
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}
