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

#include <QtPlugin>
#include "plugin/pluginapi.h"

#ifndef SERVER_MODE
#include "ahrwwvgui.h"
#endif
#include "ahrwwv.h"
#include "ahrwwvplugin.h"

const PluginDescriptor AHRWWVPlugin::m_pluginDescriptor = {
    AHRWWV::m_channelId,
    QStringLiteral("AHR WWV/WWVH Time Decoder"),
    QStringLiteral("8.0.0"), // SDRangel version this plugin is built against
    QStringLiteral("(c) KL5NE, Alaska HamRadio"),
    QStringLiteral("https://github.com/KL5NE/AHR-WWV-Decoder"),
    true,
    QStringLiteral("https://github.com/KL5NE/AHR-WWV-Decoder")
};

AHRWWVPlugin::AHRWWVPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(nullptr)
{
}

const PluginDescriptor& AHRWWVPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void AHRWWVPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;
    m_pluginAPI->registerRxChannel(AHRWWV::m_channelIdURI, AHRWWV::m_channelId, this);
}

void AHRWWVPlugin::createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const
{
    if (bs || cs)
    {
        AHRWWV *instance = new AHRWWV(deviceAPI);

        if (bs) {
            *bs = instance;
        }

        if (cs) {
            *cs = instance;
        }
    }
}

#ifdef SERVER_MODE
ChannelGUI* AHRWWVPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
    (void) deviceUISet;
    (void) rxChannel;
    return nullptr;
}
#else
ChannelGUI* AHRWWVPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
    return AHRWWVGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif
