/*
    ------------------------------------------------------------------

    This file is part of the Open Ephys GUI
    Copyright (C) 2014 Open Ephys

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

#ifndef CYCLOPS_PROCESSOR_H_INCLUDED
#define CYCLOPS_PROCESSOR_H_INCLUDED

#ifdef _WIN32
#include <Windows.h>
#endif

#include <ProcessorHeaders.h>
#include "CyclopsEditor.h"
#include "plugin_manager/CLPluginManager.h"
#include <SerialLib.h>

namespace cyclops {

class CyclopsCanvas;
struct cl_serial;
/**
  The Cyclops Stimulator controls Cyclops boards to perform optpgenetic
  stimulation.

  @see GenericProcessor
*/
class CyclopsProcessor : public GenericProcessor

{
public:

    CyclopsProcessor();
    ~CyclopsProcessor();

    bool isSource()
    {
        return false;
    }

    bool isSink()
    {
        return false;
    }

    bool hasEditor() const
    {
        return true;
    }

    AudioProcessorEditor* createEditor();

    bool isReady();
    void process(AudioSampleBuffer& buffer, MidiBuffer& events);
    void handleEvent(int eventType, MidiMessage& event, int samplePosition = 0);
    void setParameter(int parameterIndex, float newValue);
    void updateSettings();

    bool enable();
    bool disable();

    static int getProcessorCount();

private:
    
    cl_serial* serialInfo;
    CyclopsPluginInfo* pluginInfo;
    CyclopsPlugin* plugin;
    static int node_count;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CyclopsProcessor);

};

} // NAMESPACE cyclops
#endif  // CYCLOPS_PROCESSOR_H_INCLUDED
