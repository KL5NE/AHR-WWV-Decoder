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

#ifndef INCLUDE_AHRWWVREPORT_H
#define INCLUDE_AHRWWVREPORT_H

#include <vector>

#include "ahrwwv/decoder.h"

//! Snapshot of the decoder handed from the DSP thread to the GUI thread.
//! Plain data, copied by value, so the GUI never touches the decoder object.
struct AHRWWVReport
{
    ahrwwv::DecoderStatus status;           //!< full decoder status (state, symbols, quality...)

    // Mapping between decoded UTC and the PC clock, for a smooth GUI clock.
    bool haveUtc = false;
    double utcAtRef = 0.0;                  //!< decoded (transmitter) UTC, Unix seconds, at the reference sample
    double pcAtRef = 0.0;                   //!< PC wall clock, Unix seconds, when that sample was processed

    // Events since the previous report
    std::vector<ahrwwv::SecondSymbol> newSeconds;
    struct Frame {
        bool accepted = false;
        ahrwwv::DecodeResult result;
    };
    std::vector<Frame> frames;

    // Diagnostics
    std::vector<float> envelope;            //!< 100 Hz level samples since previous report (pulse ~ 1.0)
    double envelopeRate = 100.0;            //!< samples per second in envelope
    double detectorDelay = 0.0;             //!< measured detector delay (s)

    // Counters since the decoder was last reset
    quint32 framesAccepted = 0;
    quint32 framesRejected = 0;
    quint64 samplesProcessed = 0;
    int channelSampleRate = 0;
    bool decoderEnabled = true;
    bool resetHappened = false;             //!< decoder was reset since previous report
    quint32 discontinuities = 0;            //!< decoder restarts caused by detected sample loss
};

#endif // INCLUDE_AHRWWVREPORT_H
