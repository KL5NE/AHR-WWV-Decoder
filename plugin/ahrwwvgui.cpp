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

#include <cmath>
#include <chrono>

#include <QDebug>

#include "device/deviceuiset.h"
#include "dsp/dspcommands.h"
#include "plugin/pluginapi.h"
#include "util/db.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "maincore.h"

#include <QLabel>

#include "ui_ahrwwvgui.h"
#include "ahrwwvgui.h"
#include "ahrwwv.h"

using ahrwwv::DecoderState;
using ahrwwv::StationId;

namespace {

double nowUnix()
{
    using namespace std::chrono;
    return duration_cast<microseconds>(system_clock::now().time_since_epoch()).count() / 1e6;
}

const double kNoDataSeconds = 2.5; //!< no report for this long: samples are not arriving

// setStyleSheet re-polishes the widget even when nothing changed; the clock
// updates every 50 ms, so only touch it on a change.
void setLabelStyle(QLabel *label, const QString& style)
{
    if (label->styleSheet() != style) {
        label->setStyleSheet(style);
    }
}

QString badgeStyle(const char *bg, const char *fg = "#ffffff")
{
    return QString("QLabel { background-color: %1; color: %2; border-radius: 4px; padding: 2px 6px; }").arg(bg).arg(fg);
}

} // namespace

AHRWWVGUI* AHRWWVGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    AHRWWVGUI* gui = new AHRWWVGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void AHRWWVGUI::destroy()
{
    delete this;
}

void AHRWWVGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(QStringList(), true);
}

QByteArray AHRWWVGUI::serialize() const
{
    return m_settings.serialize();
}

bool AHRWWVGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        displaySettings();
        applySettings(QStringList(), true);
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool AHRWWVGUI::handleMessage(const Message& message)
{
    if (AHRWWV::MsgConfigureAHRWWV::match(message))
    {
        qDebug("AHRWWVGUI::handleMessage: AHRWWV::MsgConfigureAHRWWV");
        const AHRWWV::MsgConfigureAHRWWV& cfg = (AHRWWV::MsgConfigureAHRWWV&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (AHRWWV::MsgReport::match(message))
    {
        const AHRWWV::MsgReport& report = (AHRWWV::MsgReport&) message;
        handleReport(report.getReport());
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        const DSPSignalNotification& notif = (const DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        ui->deltaFrequency->setValueRange(false, 7, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        updateAbsoluteCenterFrequency();
        return true;
    }

    return false;
}

void AHRWWVGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != nullptr)
    {
        if (!handleMessage(*message)) {
            qDebug("%s: unhandled message: %s", Q_FUNC_INFO, message->getIdentifier());
        }

        delete message;
    }
}

void AHRWWVGUI::handleReport(const AHRWWVReport& report)
{
    if (report.resetHappened)
    {
        ui->timeCodeStrip->clear();
        ui->envelopePlot->clear();
        m_lastFrameText = "none";
    }

    for (const auto& fr : report.frames)
    {
        if (fr.accepted)
        {
            const ahrwwv::TimeCode& tc = fr.result.tc;
            int mon, day;
            ahrwwv::dayOfYearToDate(tc.year, tc.dayOfYear, mon, day);
            m_lastFrameText = QString("%8%1-%2-%3 %4:%5 UTC (day %6), received %7 local")
                .arg(tc.year, 4, 10, QChar('0')).arg(mon, 2, 10, QChar('0')).arg(day, 2, 10, QChar('0'))
                .arg(tc.hour, 2, 10, QChar('0')).arg(tc.minute, 2, 10, QChar('0'))
                .arg(tc.dayOfYear, 3, 10, QChar('0'))
                .arg(QTime::currentTime().toString("HH:mm:ss"))
                .arg(report.status.state == DecoderState::Verified ? "" : "UNCONFIRMED candidate: ");
        }
    }

    ui->envelopePlot->addSamples(report.envelope, report.envelopeRate);

    for (const auto& s : report.newSeconds) {
        ui->envelopePlot->addSecond(s);
    }

    if (report.status.framePosition >= 0 && (report.status.state != DecoderState::Searching)) {
        ui->timeCodeStrip->setFrame(report.status.frame, report.status.framePosition);
    } else if (report.status.state == DecoderState::Searching || report.status.state == DecoderState::SecondLock) {
        ui->timeCodeStrip->clear();
    }

    m_report = report;
    m_haveReport = true;
    m_reportReceivedPc = nowUnix();
    updateStatusPanels();
    updateClock();
}

QString AHRWWVGUI::stateText(const AHRWWVReport& report, double age)
{
    if (age > kNoDataSeconds) {
        return report.decoderEnabled ? "NO DATA" : "DISABLED";
    }

    const ahrwwv::DecoderStatus& st = report.status;

    switch (st.state)
    {
    case DecoderState::Verified: return "VERIFIED";
    case DecoderState::Decoded: return "DECODED (unverified)";
    case DecoderState::Holdover:
        return QString("HOLDOVER %1:%2").arg((int) st.holdoverAge / 60).arg((int) st.holdoverAge % 60, 2, 10, QChar('0'));
    case DecoderState::FrameLock: return "FRAME SYNC";
    case DecoderState::SecondLock: return "SECOND SYNC";
    default: return "SEARCHING";
    }
}

void AHRWWVGUI::getRxPosition(double& lat, double& lon) const
{
    if (m_settings.m_useStationLocation)
    {
        lat = MainCore::instance()->getSettings().getLatitude();
        lon = MainCore::instance()->getSettings().getLongitude();
    }
    else
    {
        lat = m_settings.m_rxLatitude;
        lon = m_settings.m_rxLongitude;
    }
}

double AHRWWVGUI::pathDelaySeconds(int station, int detectedStation, double lat, double lon, bool *valid)
{
    int s = station;

    if (s == (int) AHRWWVSettings::StationAuto)
    {
        if (detectedStation == (int) StationId::WWV) {
            s = (int) AHRWWVSettings::StationWWV;
        } else if (detectedStation == (int) StationId::WWVH) {
            s = (int) AHRWWVSettings::StationWWVH;
        }
    }

    if ((s == (int) AHRWWVSettings::StationAuto) || ((lat == 0.0) && (lon == 0.0)))
    {
        *valid = false;
        return 0.0;
    }

    *valid = true;

    if (s == (int) AHRWWVSettings::StationWWV) {
        return ahrwwv::estimateSkywaveDelay(lat, lon, ahrwwv::kWwvLat, ahrwwv::kWwvLon);
    } else {
        return ahrwwv::estimateSkywaveDelay(lat, lon, ahrwwv::kWwvhLat, ahrwwv::kWwvhLon);
    }
}

void AHRWWVGUI::updatePathDelay()
{
    double lat, lon;
    getRxPosition(lat, lon);
    bool valid;
    double d = pathDelaySeconds((int) m_settings.m_station, (int) m_report.status.station, lat, lon, &valid);

    if (valid) {
        ui->pathDelay->setText(QString("%1 ms").arg(d * 1e3, 0, 'f', 1));
    } else if ((lat == 0.0) && (lon == 0.0)) {
        ui->pathDelay->setText("set My Position");
    } else {
        ui->pathDelay->setText("station?");
    }
}

void AHRWWVGUI::updateClock()
{
    double now = nowUnix();
    double age = m_haveReport ? now - m_reportReceivedPc : 1e9;
    const ahrwwv::DecoderStatus& st = m_report.status;
    QString badge = m_haveReport ? stateText(m_report, age) : QString("NO DATA");

    // badge colour
    if (badge == "VERIFIED") {
        setLabelStyle(ui->syncBadge, badgeStyle("#1e8a3c"));
    } else if (badge.startsWith("DECODED")) {
        setLabelStyle(ui->syncBadge, badgeStyle("#b08a10", "#000000"));
    } else if (badge.startsWith("HOLDOVER")) {
        setLabelStyle(ui->syncBadge, badgeStyle("#c05a10"));
    } else if (badge.endsWith("SYNC")) {
        setLabelStyle(ui->syncBadge, badgeStyle("#2a5d8a"));
    } else {
        setLabelStyle(ui->syncBadge, badgeStyle("#555555"));
    }

    ui->syncBadge->setText(badge);

    // A single strong-looking joint decode can still be minutes wrong under
    // fading. Present only a grey, minute-resolution candidate until an
    // independent raw-minute window verifies the UTC clock.
    if (m_haveReport && age <= kNoDataSeconds
        && st.state == DecoderState::Decoded && st.haveTime)
    {
        const ahrwwv::TimeCode& guess = st.lastTimeCode;
        ui->utcTime->setText(QString("%1:%2:--")
            .arg(guess.hour, 2, 10, QChar('0'))
            .arg(guess.minute, 2, 10, QChar('0')));
        setLabelStyle(ui->utcTime, "QLabel { color: #808080; }");
        ui->utcDate->setText("UNCONFIRMED candidate minute — not a verified UTC clock");
        ui->pcOffset->setText("PC clock: comparison withheld until VERIFIED");
        return;
    }

    bool live = m_haveReport && (age <= kNoDataSeconds) && m_report.haveUtc && st.haveTime;

    if (!live)
    {
        ui->utcTime->setText("--:--:--");
        setLabelStyle(ui->utcTime, "QLabel { color: #808080; }");

        if (!m_haveReport || age > kNoDataSeconds) {
            ui->utcDate->setText(m_settings.m_decoderEnabled ? "No samples: is the device running?" : "Decoder disabled");
        } else {
            ui->utcDate->setText("Waiting for independently VERIFIED minute frames");
        }

        ui->pcOffset->setText("PC clock: no comparison (no VERIFIED time)");
        return;
    }

    double utc = m_report.utcAtRef + (now - m_report.pcAtRef);
    qint64 secs = (qint64) std::floor(utc);
    ahrwwv::TimeCode tc = ahrwwv::unixToTimeCode(secs);
    int sec = (int) (((secs % 60) + 60) % 60);
    int mon, day;
    ahrwwv::dayOfYearToDate(tc.year, tc.dayOfYear, mon, day);
    static const char *wd[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    qint64 days = (secs >= 0) ? secs / 86400 : (secs - 86399) / 86400;
    int weekday = (int) (((days + 4) % 7 + 7) % 7); // 1970-01-01 was a Thursday

    ui->utcTime->setText(QString("%1:%2:%3").arg(tc.hour, 2, 10, QChar('0')).arg(tc.minute, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0')));

    if (st.state == DecoderState::Verified) {
        setLabelStyle(ui->utcTime, "QLabel { color: #40d070; }");
    } else if (st.state == DecoderState::Decoded) {
        setLabelStyle(ui->utcTime, "QLabel { color: #e0c040; }");
    } else {
        setLabelStyle(ui->utcTime, "QLabel { color: #e08040; }"); // holdover: predicted
    }

    QString kind = st.state == DecoderState::Holdover ? "predicted from last good frame" :
                   st.state == DecoderState::Decoded ? "one frame, not yet confirmed" : "live, confirmed";
    ui->utcDate->setText(QString("%1-%2-%3  %4  day %5   (%6)")
        .arg(tc.year, 4, 10, QChar('0')).arg(mon, 2, 10, QChar('0')).arg(day, 2, 10, QChar('0'))
        .arg(wd[weekday]).arg(tc.dayOfYear, 3, 10, QChar('0')).arg(kind));

    // PC clock comparison (read only)
    if (st.haveClockOffset && st.state == DecoderState::Verified)
    {
        double lat, lon;
        getRxPosition(lat, lon);
        bool valid;
        double path = pathDelaySeconds((int) m_settings.m_station, (int) st.station, lat, lon, &valid);
        double off = st.clockOffset - path - m_settings.m_latencyCorrectionMs / 1000.0;
        double unc = st.clockOffsetUncertainty;

        if (!valid) {
            unc = std::sqrt(unc * unc + 0.015 * 0.015); // unknown path: ~10-20 ms for North America
        }

        if (std::fabs(off) > 5.0)
        {
            // Far beyond any radio-path error: a replayed recording, or a PC clock that is badly set.
            ui->pcOffset->setText(QString("PC clock differs by %1 s from decoded time (replaying a recording, or the PC clock is badly off)")
                .arg(off, 0, 'f', 1));
            return;
        }

        QString dir = std::fabs(off) < unc ? "agrees within" : (off > 0 ? "is fast by" : "is slow by");
        ui->pcOffset->setText(QString("PC clock %1 %2 s  (±%3 s, approximate%4)")
            .arg(dir)
            .arg(std::fabs(off) < unc ? QString("±%1").arg(unc, 0, 'f', 3) : QString::number(std::fabs(off), 'f', 3))
            .arg(unc, 0, 'f', 3)
            .arg(valid ? "" : ", path delay not removed"));
    }
    else if (st.state == DecoderState::Holdover)
    {
        ui->pcOffset->setText("PC clock: comparison paused during holdover");
    }
    else
    {
        ui->pcOffset->setText("PC clock: waiting for VERIFIED time");
    }
}

void AHRWWVGUI::updateStatusPanels()
{
    const ahrwwv::DecoderStatus& st = m_report.status;
    double age = nowUnix() - m_reportReceivedPc;

    ui->lastFrame->setText(QString("Last frame: %1").arg(m_lastFrameText));

    // Time code fields (only from accepted frames)
    if (st.haveTime)
    {
        const ahrwwv::TimeCode& tc = st.lastTimeCode;
        ui->dut1Value->setText(QString("%1%2 s").arg(tc.dut1Tenths >= 0 ? "+" : "-").arg(std::abs(tc.dut1Tenths) / 10.0, 0, 'f', 1));
        ui->dstValue->setText(ahrwwv::dstStateName(ahrwwv::dstState(tc)));
        ui->lswValue->setText(tc.leapSecondWarning ? "Yes (leap second at end of month)" : "No");
    }
    else
    {
        ui->dut1Value->setText("-");
        ui->dstValue->setText("-");
        ui->lswValue->setText("-");
    }

    ui->confidenceValue->setText(st.verifiedFrames > 0 || st.frameConfidence > 0 ? QString::number(st.frameConfidence, 'f', 2) : "-");
    {
        // Success rate since the decoder last started (Reset, Decode on, or a Δf change).
        quint32 total = m_report.framesAccepted + m_report.framesRejected;
        if (total == 0) {
            ui->framesValue->setText("none yet");
        } else {
            double pct = 100.0 * m_report.framesAccepted / total;
            ui->framesValue->setText(QString("%1 / %2 decoded (%3 %)")
                .arg(m_report.framesAccepted).arg(total).arg(pct, 0, 'f', 0));
        }
    }
    ui->problemValue->setText(st.lastProblem.empty() ? QString("-") : QString::fromStdString(st.lastProblem));

    // Signal
    double snr = st.subcarrierSnrDb;
    bool locked = st.state != DecoderState::Searching;
    ui->snrBar->setValue(locked ? (int) std::max(0.0, std::min(100.0, snr / 30.0 * 100.0)) : 0);
    ui->snrValue->setText(locked ? QString("%1 dB").arg(snr, 0, 'f', 1) : "-");
    ui->lockBar->setValue((int) (st.secondLockQuality * 100.0));
    ui->lockValue->setText(QString::number(st.secondLockQuality, 'f', 2));

    QString state = ahrwwv::decoderStateName(st.state);

    if (st.framePosition >= 0) {
        state += QString(", second %1 of the minute").arg(st.framePosition);
    }

    ui->stateValue->setText(state);

    QString heard = ahrwwv::stationName(st.station);
    QString stationText;

    if (m_settings.m_station == AHRWWVSettings::StationAuto) {
        stationText = st.station == StationId::Unknown ? QString("not identified yet") : QString("%1 (from tick tone)").arg(heard);
    } else {
        QString sel = m_settings.m_station == AHRWWVSettings::StationWWV ? "WWV" : "WWVH";
        stationText = QString("%1 (selected)").arg(sel);

        if (st.station != StationId::Unknown && heard != sel) {
            stationText += QString(", ticks suggest %1").arg(heard);
        }
    }

    ui->stationValue->setText(stationText);

    if (!m_settings.m_decoderEnabled) {
        ui->signalValue->setText("Decoder disabled");
    } else if (age > kNoDataSeconds) {
        ui->signalValue->setText("No samples arriving");
    } else if (st.state == DecoderState::Holdover) {
        ui->signalValue->setText(QString("Time code lost %1 s ago, holding over").arg((int) st.holdoverAge));
    } else if (st.state == DecoderState::Searching) {
        ui->signalValue->setText("Listening, no 100 Hz pulses found");
    } else {
        ui->signalValue->setText("Receiving time code");
    }

    if (m_report.discontinuities > 0) {
        ui->signalValue->setText(ui->signalValue->text() + QString(" (restarted %1x after lost samples)").arg(m_report.discontinuities));
    }

    QString timing = QString("jitter %1 ms").arg(st.timingJitter * 1e3, 0, 'f', 1);

    if (st.sampleClockPpmValid) {
        timing += QString(", SDR clock %1%2 ppm").arg(st.sampleClockPpm >= 0 ? "+" : "").arg(st.sampleClockPpm, 0, 'f', 1);
    }

    if (st.haveTime) {
        timing += QString(", ±%1 ms").arg(st.timeUncertainty * 1e3, 0, 'f', 1);
    }

    ui->timingValue->setText(timing);
    updatePathDelay();
}

void AHRWWVGUI::channelMarkerChangedByCursor()
{
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    ui->deltaFrequency->blockSignals(true);
    ui->deltaFrequency->setValue(m_settings.m_inputFrequencyOffset);
    ui->deltaFrequency->blockSignals(false);
    updateAbsoluteCenterFrequency();
    applySettings(QStringList({"inputFrequencyOffset"}));
}

void AHRWWVGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void AHRWWVGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings(QStringList({"inputFrequencyOffset"}));
}

void AHRWWVGUI::on_decoderEnabled_toggled(bool checked)
{
    m_settings.m_decoderEnabled = checked;
    applySettings(QStringList({"decoderEnabled"}));
}

void AHRWWVGUI::on_station_currentIndexChanged(int index)
{
    m_settings.m_station = (AHRWWVSettings::Station) index;
    updateStatusPanels();
    applySettings(QStringList({"station"}));
}

void AHRWWVGUI::on_resetDecoder_clicked()
{
    m_ahrWWV->getInputMessageQueue()->push(AHRWWV::MsgResetDecoder::create());
}

void AHRWWVGUI::on_showDiagnostics_toggled(bool checked)
{
    m_settings.m_showDiagnostics = checked;
    ui->diagContainer->setVisible(checked);
    getRollupContents()->arrangeRollups();
    applySettings(QStringList({"showDiagnostics"}));
}

void AHRWWVGUI::on_latencyCorrection_valueChanged(int value)
{
    m_settings.m_latencyCorrectionMs = value;
    applySettings(QStringList({"latencyCorrectionMs"}));
}

void AHRWWVGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;
    getRollupContents()->saveState(m_rollupState);
    applySettings(QStringList());
}

void AHRWWVGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuType::ContextMenuChannelSettings)
    {
        BasicChannelSettingsDialog dialog(&m_channelMarker, this);
        dialog.setDefaultTitle(m_displayedName);

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            dialog.setNumberOfStreams(m_ahrWWV->getNumberOfDeviceStreams());
            dialog.setStreamIndex(m_settings.m_streamIndex);
        }

        dialog.move(p);
        new DialogPositioner(&dialog, false);
        dialog.exec();

        m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
        m_settings.m_title = m_channelMarker.getTitle();

        setWindowTitle(m_settings.m_title);
        setTitle(m_channelMarker.getTitle());
        setTitleColor(m_settings.m_rgbColor);

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
            m_channelMarker.clearStreamIndexes();
            m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
            updateIndexLabel();
        }

        applySettings(QStringList({"rgbColor", "title", "streamIndex"}));
    }

    resetContextMenuType();
}

AHRWWVGUI::AHRWWVGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::AHRWWVGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
    m_doApplySettings(true),
    m_tickCount(0),
    m_haveReport(false),
    m_reportReceivedPc(0.0),
    m_lastFrameText("none")
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "https://github.com/KL5NE/AHR-WWV-Decoder/blob/main/plugin/readme.md";
    RollupContents *rollupContents = getRollupContents();
    ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
    connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    m_ahrWWV = reinterpret_cast<AHRWWV*>(rxChannel);
    m_ahrWWV->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(QColor(m_settings.m_rgbColor));
    m_channelMarker.setBandwidth(2 * AHRWWVSettings::AHRWWV_CHANNEL_HALF_BANDWIDTH);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle("WWV Time Decoder");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true);

    setTitleColor(m_channelMarker.getColor());
    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    displaySettings();
    makeUIConnections();
    applySettings(QStringList(), true);
    DialPopup::addPopupsToChildDials(this);
    m_resizer.enableChildMouseTracking();
    updateClock();
}

AHRWWVGUI::~AHRWWVGUI()
{
    delete ui;
}

void AHRWWVGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void AHRWWVGUI::applySettings(const QStringList& settingsKeys, bool force)
{
    if (m_doApplySettings)
    {
        AHRWWV::MsgConfigureAHRWWV* message = AHRWWV::MsgConfigureAHRWWV::create(settingsKeys, m_settings, force);
        m_ahrWWV->getInputMessageQueue()->push(message);
    }
}

void AHRWWVGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setBandwidth(2 * AHRWWVSettings::AHRWWV_CHANNEL_HALF_BANDWIDTH);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    ui->decoderEnabled->setChecked(m_settings.m_decoderEnabled);
    ui->station->setCurrentIndex((int) m_settings.m_station);
    ui->showDiagnostics->setChecked(m_settings.m_showDiagnostics);
    ui->latencyCorrection->setValue((int) std::lround(m_settings.m_latencyCorrectionMs));
    ui->diagContainer->setVisible(m_settings.m_showDiagnostics);

    updateIndexLabel();
    getRollupContents()->restoreState(m_rollupState);
    ui->diagContainer->setVisible(m_settings.m_showDiagnostics);
    getRollupContents()->arrangeRollups();
    updateAbsoluteCenterFrequency();
    updatePathDelay();
    blockApplySettings(false);
}

void AHRWWVGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void AHRWWVGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void AHRWWVGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_ahrWWV->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);
    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(QString::number(powDbAvg, 'f', 1));
    }

    updateClock(); // every 50 ms so seconds roll over on time

    if (m_tickCount % 10 == 0 && m_haveReport && (nowUnix() - m_reportReceivedPc > kNoDataSeconds)) {
        updateStatusPanels(); // reports stopped: show it
    }

    m_tickCount++;
}

void AHRWWVGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &AHRWWVGUI::on_deltaFrequency_changed);
    QObject::connect(ui->decoderEnabled, &QCheckBox::toggled, this, &AHRWWVGUI::on_decoderEnabled_toggled);
    QObject::connect(ui->station, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AHRWWVGUI::on_station_currentIndexChanged);
    QObject::connect(ui->resetDecoder, &QPushButton::clicked, this, &AHRWWVGUI::on_resetDecoder_clicked);
    QObject::connect(ui->showDiagnostics, &QCheckBox::toggled, this, &AHRWWVGUI::on_showDiagnostics_toggled);
    QObject::connect(ui->latencyCorrection, QOverload<int>::of(&QSpinBox::valueChanged), this, &AHRWWVGUI::on_latencyCorrection_valueChanged);
}

void AHRWWVGUI::updateAbsoluteCenterFrequency()
{
    qint64 f = m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset;
    setStatusFrequency(f);
    ui->tunedFrequency->setText(QString("%1 kHz").arg(f / 1000.0, 0, 'f', 3));

    if ((m_basebandSampleRate > 1)
        && ((m_settings.m_inputFrequencyOffset >= m_basebandSampleRate / 2)
            || (m_settings.m_inputFrequencyOffset < -m_basebandSampleRate / 2))) {
        setStatusText("Frequency out of band");
    } else {
        setStatusText("");
    }
}
