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

#include "UDPEventsPluginEditor.h"
#include "UDPEventsPlugin.h"

UDPEventsPluginEditor::UDPEventsPluginEditor(GenericProcessor *parentNode)
    : GenericEditor(parentNode)
{
    desiredWidth = 125;
    addTextBoxParameterEditor(Parameter::PROCESSOR_SCOPE, "host", 5, 22);
    addTextBoxParameterEditor(Parameter::PROCESSOR_SCOPE, "port", 5, 44);

    addComboBoxParameterEditor(Parameter::STREAM_SCOPE, "line", 5, 66);
    addComboBoxParameterEditor(Parameter::STREAM_SCOPE, "state", 5, 88);

    // Select stream with options updated dynamically.
    streamSelection = std::make_unique<ComboBox>("Stream Selector");
    streamSelection->setName("stream");
    streamSelection->setBounds(5, 110, 100, 20);
    streamSelection->addListener(this);
    addAndMakeVisible(streamSelection.get());
}

void UDPEventsPluginEditor::updateSettings()
{
    // Present each stream by string and associate each with its numeric stream id.
    streamSelection->clear();
    for (auto stream : getProcessor()->getDataStreams())
    {
        streamSelection->addItem(stream->getName(), stream->getStreamId());
    }

    // Reconcile the current selection with what streams are actually available to select.
    uint16 currentStreamId = (uint16)(int)getProcessor()->getParameter("stream")->getValue();
    if (streamSelection->getNumItems() == 0)
    {
        // There are no streams!
        currentStreamId = 0;
    }
    else if (streamSelection->indexOfItemId(currentStreamId) == -1)
    {
        // Default to selecting the first available stream.
        currentStreamId = streamSelection->getItemId(0);
    }

    if (currentStreamId > 0)
    {
        // Trigger callbacks for the selected stream.
        streamSelection->setSelectedId(currentStreamId, sendNotification);
    }
}

void UDPEventsPluginEditor::comboBoxChanged(ComboBox *cb)
{
    if (cb == streamSelection.get())
    {
        // Propagate the selected stream to the processor's int parameter.
        uint16 currentStreamId = cb->getSelectedId();
        if (currentStreamId > 0)
        {
            getProcessor()->getParameter("stream")->setNextValue(currentStreamId);
        }
    }
}

void UDPEventsPluginEditor::startAcquisition()
{
    // Disable changing stream during acquisition.
    streamSelection->setEnabled(false);
}

void UDPEventsPluginEditor::stopAcquisition()
{
    // Enable changing stream between acquisitions.
    streamSelection->setEnabled(true);
}
