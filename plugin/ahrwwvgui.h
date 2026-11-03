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

#ifndef INCLUDE_AHRWWVGUI_H
#define INCLUDE_AHRWWVGUI_H

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "ahrwwvsettings.h"
#include "ahrwwvreport.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class AHRWWV;

namespace Ui {
    class AHRWWVGUI;
}

class AHRWWVGUI : public ChannelGUI {
    Q_OBJECT

public:
    static AHRWWVGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
    virtual void destroy();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual void setWorkspaceIndex(int index) { m_settings.m_workspaceIndex = index; };
    virtual int getWorkspaceIndex() const { return m_settings.m_workspaceIndex; };
    virtual void setGeometryBytes(const QByteArray& blob) { m_settings.m_geometryBytes = blob; };
    virtual QByteArray getGeometryBytes() const { return m_settings.m_geometryBytes; };
    virtual QString getTitle() const { return m_settings.m_title; };
    virtual QColor getTitleColor() const  { return m_settings.m_rgbColor; };
    virtual void zetHidden(bool hidden) { m_settings.m_hidden = hidden; }
    virtual bool getHidden() const { return m_settings.m_hidden; }
    virtual ChannelMarker& getChannelMarker() { return m_channelMarker; }
    virtual int getStreamIndex() const { return m_settings.m_streamIndex; }
    virtual void setStreamIndex(int streamIndex) { m_settings.m_streamIndex = streamIndex; }

    //! Plain text of what the panel shows, for tests and logging.
    static QString stateText(const AHRWWVReport& report, double reportAgeSeconds);
    static double pathDelaySeconds(int station, int detectedStation, double lat, double lon, bool *valid);

public slots:
    void channelMarkerChangedByCursor();
    void channelMarkerHighlightedByCursor();

private:
    Ui::AHRWWVGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    AHRWWVSettings m_settings;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
    bool m_doApplySettings;
    AHRWWV* m_ahrWWV;
    uint32_t m_tickCount;
    MessageQueue m_inputMessageQueue;

    AHRWWVReport m_report;          //!< last report from the DSP thread
    bool m_haveReport;
    double m_reportReceivedPc;      //!< PC time the last report arrived
    QString m_lastFrameText;

    explicit AHRWWVGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = nullptr);
    virtual ~AHRWWVGUI();

    void blockApplySettings(bool block);
    void applySettings(const QStringList& settingsKeys, bool force = false);
    void displaySettings();
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();
    void handleReport(const AHRWWVReport& report);
    void updateClock();
    void updateStatusPanels();
    void updatePathDelay();
    void getRxPosition(double& lat, double& lon) const;
    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

private slots:
    void on_deltaFrequency_changed(qint64 value);
    void on_decoderEnabled_toggled(bool checked);
    void on_station_currentIndexChanged(int index);
    void on_resetDecoder_clicked();
    void on_showDiagnostics_toggled(bool checked);
    void on_latencyCorrection_valueChanged(int value);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
    void tick();
};

#endif // INCLUDE_AHRWWVGUI_H
