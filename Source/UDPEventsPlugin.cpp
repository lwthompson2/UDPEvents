/*
------------------------------------------------------------------

This file is part of the Open Ephys GUI
Copyright (C) 2022 Open Ephys

------------------------------------------------------------------

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "UDPEventsPlugin.h"

#include "UDPEventsPluginEditor.h"


UDPEventsPlugin::UDPEventsPlugin()
    : GenericProcessor("Plugin Name")
{

}


UDPEventsPlugin::~UDPEventsPlugin()
{

}


AudioProcessorEditor* UDPEventsPlugin::createEditor()
{
    editor = std::make_unique<UDPEventsPluginEditor>(this);
    return editor.get();
}


void UDPEventsPlugin::updateSettings()
{


}


void UDPEventsPlugin::process(AudioBuffer<float>& buffer)
{

    checkForEvents(true);

}


void UDPEventsPlugin::handleTTLEvent(TTLEventPtr event)
{

}


void UDPEventsPlugin::handleSpike(SpikePtr spike)
{

}


void UDPEventsPlugin::handleBroadcastMessage(String message)
{

}


void UDPEventsPlugin::saveCustomParametersToXml(XmlElement* parentElement)
{

}


void UDPEventsPlugin::loadCustomParametersFromXml(XmlElement* parentElement)
{

}
