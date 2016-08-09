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

#include "CyclopsCanvas.h"

namespace cyclops {

OwnedArray<CyclopsCanvas> CyclopsCanvas::canvasList;
OwnedArray<HookView>      CyclopsCanvas::hookViews;
int                       CyclopsCanvas::numCanvases = 0;

ScopedPointer<CyclopsPluginManager> CyclopsCanvas::pluginManager = new CyclopsPluginManager();

const int CyclopsCanvas::BAUDRATES[12] = {300, 1200, 2400, 4800, 9600, 14400, 19200, 28800, 38400, 57600, 115200, 230400};

CyclopsCanvas::CyclopsCanvas() : tabIndex(-1)
                               , realIndex(-1)
                               , dataWindow(nullptr)
                               , progress(0)
                               , in_a_test(false)
{
    realIndex = CyclopsCanvas::numCanvases++; // 0 onwards
    // Add "port" list
    portCombo = new ComboBox();
    portCombo->addListener(this);
    portCombo->setTooltip("Select the serial port on which Cyclops is connected.");
    portCombo->addItemList(getDevices(), 1);
    addAndMakeVisible(portCombo);

    // Add refresh button
    refreshButton = new UtilityButton("R", Font("Default", 9, Font::plain));
    refreshButton->setRadius(3.0f);
    refreshButton->addListener(this);
    addAndMakeVisible(refreshButton);

    // Add baudrate list
    baudrateCombo = new ComboBox();
    baudrateCombo->addListener(this);
    baudrateCombo->setTooltip("Set the baud rate (115200 recommended).");

    devStatus = new IndicatorLED(CyclopsColours::disconnected, Colours::black);
    addChildComponent(devStatus);

    Array<int> baudrates(getBaudrates());
    for (int i = 0; i < baudrates.size(); i++)
    {
        baudrateCombo->addItem(String(baudrates[i]), baudrates[i]);
    }
    baudrateCombo->setSelectedId(115200);
    addAndMakeVisible(baudrateCombo);

    // Add TEST buttons
    for (int i=0; i < 4; i++){
        testButtons.add(new UtilityButton(String("Test ") + String(i), Font("Default", 11, Font::bold)));
        testButtons[i]->addListener(this);
        addAndMakeVisible(testButtons[i]);
    }
    progressBar = new ProgressBar(progress);
    progressBar->setPercentageDisplay(false);
    addChildComponent(progressBar);
    pstep = 0.01;
    // communicate with teensy?
    
    // Add close Button
    closeButton = new UtilityButton("close", Font("Default", 9, Font::plain));
    closeButton->addListener(this);
    addChildComponent(closeButton);

    hookViewDisplay = new HookViewDisplay(this);
    hookViewport = new HookViewport(hookViewDisplay);
    addAndMakeVisible(hookViewport);

    signalDisplay = new SignalDisplay(this);
    signalViewport = new SignalViewport(signalDisplay);
    addAndMakeVisible(signalViewport);

    refreshPlugins();
}

void CyclopsCanvas::refreshPlugins()
{
    if (pluginManager->getNumPlugins() == 0){
        std::cout << "CPM> Making Cyclops-Plugin List" << std::endl;
        pluginManager->loadAllPlugins();
        std::cout << "CPM> Loaded " << pluginManager->getNumPlugins() << " cyclops plugin(s)." << std::endl;
    }
}

CyclopsCanvas::~CyclopsCanvas()
{
    
}

void CyclopsCanvas::beginAnimation()
{
    std::cout << "CyclopsCanvas beginning animation." << std::endl;
    disableAllInputWidgets();
    if (serialInfo.portName != ""){
        // serialInfo->Serial.writeByte(<launch>);
        devStatus->update(CyclopsColours::connected, "Device is in sync!");
        devStatus->setVisible(true);
        devStatus->repaint();
    }
    startTimer(40);
}

void CyclopsCanvas::endAnimation()
{
    std::cout << "CyclopsCanvas ending animation." << std::endl;
    enableAllInputWidgets();
    devStatus->setVisible(false);
    stopTimer();
}

void CyclopsCanvas::setParameter(int x, float f)
{

}

void CyclopsCanvas::setParameter(int a, int b, int c, float d)
{
}

void CyclopsCanvas::update()
{
    std::cout << "Updating CyclopsCanvas" << std::endl;
}


void CyclopsCanvas::refreshState()
{
    // called when the component's tab becomes visible again
    repaint();
}

void CyclopsCanvas::resized()
{
    int width = getWidth(), height = getHeight();
    baudrateCombo->setBounds(width-75, 5, 70, 20);
    portCombo->setBounds(width-75-5-70, 5, 70, 20);
    refreshButton->setBounds(width-75-5-70-5-20, 5, 20, 20);
    for (int i=0; i < 4; i++){
        testButtons[i]->setBounds(jmax(100, width-53), jmax(45, (height/5))*(i+1)-(25/2), 50, 25);
    }
    progressBar->setBounds(2, height-16, width-4, 16);
    closeButton->setBounds(width/2-20, 5, 40, 20);

    hookViewport->setBounds(2, 50, width-100, height/2-30);
    hookViewDisplay->setBounds( 0
                              , 0
                              , width-100-hookViewport->getScrollBarThickness()
                              , 40+50*canvasEventListeners.size());

    signalViewport->setBounds(2, 50+10+height/2-30, width-100, height/2-30);
    signalDisplay->setBounds( 0
                            , 0
                            , width-100-signalViewport->getScrollBarThickness()
                            , 20+18*CyclopsSignal::signals.size());
}

int CyclopsCanvas::getNumCanvas()
{
    return CyclopsCanvas::canvasList.size();
}

void CyclopsCanvas::getEditorIds(CyclopsCanvas* cc, Array<int>& editorIdList)
{
    for (int i=0; i<cc->canvasEventListeners.size(); i++){
        editorIdList.add(cc->canvasEventListeners.getListeners()[i]->getEditorId());
    }
}

int CyclopsCanvas::migrateEditor(CyclopsCanvas* dest, CyclopsCanvas* src, CyclopsCanvas::Listener* listener, bool refreshNow /*=true*/)
{
    int node_id = listener->getEditorId();
    std::cout << "migrating " << node_id << " to Cyclops B" << dest->realIndex << std::endl;
    // update combo selection
    for (int i=0; i<CyclopsCanvas::canvasList.size(); i++){
        if (CyclopsCanvas::canvasList[i]->realIndex == dest->realIndex)
            break;
    }
    listener->changeCanvas(dest);
    listener->updateButtons(CanvasEvent::COMBO_BUTTON, true);

    src->removeListener(listener);
    HookView* hv = CyclopsCanvas::getHookView(node_id);
    jassert(hv != nullptr);
    src->hookViewDisplay->removeChildComponent(hv);
    dest->addListener(listener);
    dest->hookViewDisplay->addAndMakeVisible(hv);
    
    if (refreshNow) {
        dest->refresh();
        src->refresh();
    }
    return 0;
}

int CyclopsCanvas::migrateEditor(CyclopsCanvas* dest, CyclopsCanvas* src, int nodeId)
{
    CyclopsCanvas::Listener* listener = CyclopsCanvas::findListenerById(src, nodeId);
    jassert (listener != nullptr);
    return CyclopsCanvas::migrateEditor(dest, src, listener, false);
}

void CyclopsCanvas::dropEditor(CyclopsCanvas* closingCanvas, int node_id)
{
    CyclopsCanvas::Listener* listener = CyclopsCanvas::findListenerById(closingCanvas, node_id);
    jassert (listener != nullptr);
    listener->updateIndicators(CanvasEvent::TRANSFER_DROP);
    listener->changeCanvas(nullptr);
}

void CyclopsCanvas::refresh()
{
    hookViewDisplay->refresh();
    resized();
}

void CyclopsCanvas::disableAllInputWidgets()
{
    // Disable the whole gui
    for (int i=0; i<4; i++)
        testButtons[i]->setEnabled(false);
    portCombo->setEnabled(false);
    baudrateCombo->setEnabled(false);
    refreshButton->setEnabled(false);
    hookViewDisplay->disableAllInputWidgets();
}

void CyclopsCanvas::enableAllInputWidgets()
{
    // Reenable the whole gui
    for (int i=0; i<4; i++)
        testButtons[i]->setEnabled(true);
    portCombo->setEnabled(true);
    baudrateCombo->setEnabled(true);
    refreshButton->setEnabled(true);
    hookViewDisplay->enableAllInputWidgets();
}

bool CyclopsCanvas::isReady()
{
    return hookViewDisplay->isReady() && serialInfo.portName != "";
}

void CyclopsCanvas::paint(Graphics& g)
{
    g.fillAll(Colour(0xff6e6e6e));
    if (!in_a_test)
        progressBar->setVisible(false);
    if (canvasList.size() > 1)
        closeButton->setVisible(true);
}

void CyclopsCanvas::buttonClicked(Button* button)
{  
    int test_index = -1;
    for (int i=0; i < 4; i++){
        if (button == testButtons[i]){
            test_index = i;
            break;
        }
    }
    if (test_index >= 0){
        disableAllInputWidgets();
        broadcastEditorInteractivity(CanvasEvent::FREEZE);
        std::cout << "Testing LED channel " << test_index << "\n";
        in_a_test = true;
        //serialInfo.Serial->testChannel(test_index);
        progressBar->setTextToDisplay("Testing LED channel" + String(test_index));
        progressBar->setVisible(true);
        startTimer(20);
        test_index = -1;
        //for (auto& )
    }
    if (button == refreshButton)
    {
        // Refresh list of devices
        portCombo->clear();
        portCombo->addItemList(getDevices(), 1);
    }
    else if (button == closeButton)
    {
        // close this canvas!
        int choice = AlertWindow::showYesNoCancelBox (AlertWindow::QuestionIcon, "Migrate or Drop?", "Choose 'Migrate' if you want to preserve the configuration of hooks and sub-plugins,\nand move (some / all of) the hooks to another Cyclops\nOR\nChoose 'Drop' to orphan all the hooks.", "Migrate some", "Drop all", "Cancel");
        switch (choice){
        case 0: // cancel, do nothing
        break;

        case 1: // migrate
            DialogWindow::LaunchOptions dw;
            dw.dialogTitle                  = "Select hooks and target board";
            dw.dialogBackgroundColour       = Colours::grey;
            dw.componentToCentreAround      = this;
            dw.escapeKeyTriggersCloseButton = true;
            dw.useNativeTitleBar            = false;
            dw.resizable                    = true;
            dw.useBottomRightCornerResizer  = false;
            MigrateComponent* mw            = new MigrateComponent(this);
            dw.content.set(mw, true);

            // freeze all cyclops elements
            for (auto& canvas : CyclopsCanvas::canvasList){
                canvas->disableAllInputWidgets();
                canvas->broadcastEditorInteractivity(CanvasEvent::FREEZE);
            }
            dw.launchAsync();
        break;
        }
    }
}

void CyclopsCanvas::comboBoxChanged(ComboBox* comboBox)
{
    if (comboBox == portCombo){
        setDevice(comboBox->getText().toStdString());
    }
    else if (comboBox == baudrateCombo){
        setBaudrate(comboBox->getSelectedId());
    }
}

bool CyclopsCanvas::keyPressed(const KeyPress& key)
{
    return true;
}

void CyclopsCanvas::timerCallback()
{
    if (in_a_test){
        progress += pstep;
        if (progress >= 1.0){
            progressBar->setVisible(false);
            progress = 0;
            in_a_test = false;
            stopTimer();
            enableAllInputWidgets();
            broadcastEditorInteractivity(CanvasEvent::THAW);
        }
    }
    else{
        int response;
        while (serialInfo.Serial->available() > 0){
            response = serialInfo.Serial->readByte();
            switch (response){
            case 0:   //CL_RC_LAUNCH
            break;

            case 1:   //CL_RC_END
            break;

            case 8:   //CL_RC_SBDONE
            break;

            case 9:   //CL_RC_IDENTITY
            break;

            case 16:  //CL_RC_MBDONE
            break;

            case 240: //CL_RC_EA_FAIL
            case 241: //CL_RC_NEA_FAIL
                devStatus->update(CyclopsColours::notResponding, "Device sent an ErrorCode.");
                devStatus->repaint();
            break;
            }
        }
    }
}

bool CyclopsCanvas::screenLikelyNames(const String& port_name)
{
    #ifdef TARGET_OSX
        return port_name.contains("cu.") || port_name.contains("tty.");
    #endif
    #ifdef TARGET_LINUX
        return port_name.contains("ttyUSB") || port_name.contains("ttyA");
    #endif
    return true; // for TARGET_WIN32
}

StringArray CyclopsCanvas::getDevices()
{
    vector<ofSerialDeviceInfo> allDeviceInfos = serialInfo.Serial->getDeviceList();
    StringArray allDevices;
    String port_name;
    for (unsigned int i = 0; i < allDeviceInfos.size(); i++)
    {
        port_name = allDeviceInfos[i].getDeviceName();
        if (screenLikelyNames(port_name))
        {
            allDevices.add(port_name);
        }
    }
    return allDevices;
}

Array<int> CyclopsCanvas::getBaudrates()
{
    Array<int> allBaudrates(BAUDRATES, 12);
    return allBaudrates;
}

CyclopsPluginInfo* CyclopsCanvas::getPluginInfoById(int node_id)
{
    HookView* hv = getHookView(node_id);
    if (hv != nullptr){
        return hv->hookInfo->pluginInfo;
    }
    return nullptr;
}

void CyclopsCanvas::setDevice(string port)
{
    serialInfo.portName = port;
    if (serialInfo.portName != ""){
        serialInfo.Serial->setup(serialInfo.portName, serialInfo.baudRate);
        CyclopsRPC rpc;
        identify(&rpc);
        serialInfo.Serial->writeBytes(rpc.message, rpc.length);
        
        unsigned char identity[RPC_IDENTITY_SZ+1] = {0};
        while (serialInfo.Serial->available() < RPC_IDENTITY_SZ);
        serialInfo.Serial->readBytes(identity, RPC_IDENTITY_SZ);

        for (int i=0; i<RPC_IDENTITY_SZ-1-14; i++)
            std::cout << identity[i];
        std::cout << identity[52] << identity[55] << identity[58] << identity[61] << std::endl;
        /*
        if (identity[52] == '0')
        if (identity[55] == '0')
        if (identity[58] == '0')
        if (identity[61] == '0')
        */
    }
    canvasEventListeners.call(&CyclopsCanvas::Listener::updateIndicators, CanvasEvent::SERIAL_LED);
}

void CyclopsCanvas::setBaudrate(int baudrate)
{
    if (serialInfo.baudRate != baudrate){
        serialInfo.baudRate = baudrate;
        setDevice(serialInfo.portName);
    }
}

const cl_serial* CyclopsCanvas::getSerialInfo()
{
    return &serialInfo;
}

void CyclopsCanvas::addListener(CyclopsCanvas::Listener* const newListener)
{
    canvasEventListeners.add(newListener);
    // and other info
}

void CyclopsCanvas::removeListener(CyclopsCanvas::Listener* const oldListener)
{
    canvasEventListeners.remove(oldListener);
    // and other cleanup!!
}

void CyclopsCanvas::addHook(int node_id)
{
    HookView* hv = CyclopsCanvas::hookViews.add(new HookView(node_id));
    hookViewDisplay->addAndMakeVisible(hv);
}

bool CyclopsCanvas::removeHook(int node_id)
{
    HookView* hv = getHookView(node_id);
    if (hv == nullptr)
        return false;
    CyclopsCanvas::hookViews.removeObject(hv, true); // deletes
    return true;
}

HookView* CyclopsCanvas::getHookView(int node_id)
{
    HookView* res = nullptr;
    for (auto& hv : CyclopsCanvas::hookViews){
        if (hv->nodeId == node_id){
            res = hv;
            break;
        }
    }
    return res;
}

bool CyclopsCanvas::isReady(int node_id)
{
    HookView* hv = CyclopsCanvas::getHookView(node_id);
    return hv->isReady();
}

void CyclopsCanvas::broadcastButtonState(CanvasEvent whichButton, bool state)
{
    canvasEventListeners.call(&CyclopsCanvas::Listener::updateButtons, whichButton, state);
}

void CyclopsCanvas::broadcastEditorInteractivity(CanvasEvent interactivity)
{
    canvasEventListeners.call(&CyclopsCanvas::Listener::setInteractivity, interactivity);
}

void CyclopsCanvas::unicastPluginIndicator(CanvasEvent pluginState, int node_id)
{
    CyclopsCanvas::Listener* listener = CyclopsCanvas::findListenerById(this, node_id);
    listener->updateIndicators(CanvasEvent::PLUGIN_SELECTED);
}

void CyclopsCanvas::unicastUpdatePluginInfo(int node_id)
{
    CyclopsCanvas::Listener* listener = CyclopsCanvas::findListenerById(this, node_id);
    listener->refreshPluginInfo();
}

void CyclopsCanvas::broadcastNewCanvas()
{
    for (auto& c : CyclopsCanvas::canvasList){
        c->broadcastButtonState(CanvasEvent::COMBO_BUTTON, true);
    }
}

int  CyclopsCanvas::getNumListeners()
{
    return canvasEventListeners.size();
}

void CyclopsCanvas::saveVisualizerParameters(XmlElement* xml)
{
    XmlElement* parameters = xml->createNewChildElement("PARAMETERS");
    parameters->setAttribute("device", portCombo->getText().toStdString());
    parameters->setAttribute("baudrate", baudrateCombo->getSelectedId());
}

void CyclopsCanvas::loadVisualizerParameters(XmlElement* xml)
{
    forEachXmlChildElement(*xml, subNode) {
        if (subNode->hasTagName("PARAMETERS")) {
            portCombo->setText(subNode->getStringAttribute("device", ""));
            baudrateCombo->setSelectedId(subNode->getIntAttribute("baudrate"));
        }
    }
}

CyclopsCanvas::Listener* CyclopsCanvas::findListenerById(CyclopsCanvas* cc, int nodeId)
{
    auto& listener_list = cc->canvasEventListeners.getListeners();
    for (auto& listener : listener_list){
        if (nodeId == listener->getEditorId())
            return listener;
    }
    return nullptr;
}

/*
  +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
  |                                 INDICATOR LEDS                                |
  +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
*/

IndicatorLED::IndicatorLED(const Colour& fill, const Colour& line)
{
    fillColour = fill;
    lineColour = line;
}

void IndicatorLED::paint(Graphics& g)
{
    g.setColour(fillColour);
    g.fillEllipse(1, 1, getWidth()-2, getHeight()-2);
    g.setColour(lineColour);
    g.drawEllipse(1, 1, getWidth()-2, getHeight()-2, 1.2);
}

void IndicatorLED::update(const Colour& fill, String tooltip)
{
    fillColour = fill;
    setTooltip(tooltip);
}

void IndicatorLED::update(const Colour& fill, const Colour& line, String tooltip)
{
    fillColour = fill;
    lineColour = line;
    setTooltip(tooltip);
}
















HookViewport::HookViewport(HookViewDisplay* display) : hvDisplay(display)
{
    setViewedComponent(hvDisplay, false);
    setScrollBarsShown(true, false);
}

void HookViewport::visibleAreaChanged(const Rectangle<int>& newVisibleArea)
{
}

void HookViewport::paint(Graphics& g)
{
}

















HookViewDisplay::HookViewDisplay(CyclopsCanvas* _canvas) : canvas(_canvas)
{
}

void HookViewDisplay::paint(Graphics& g)
{
    g.fillAll(Colours::orange);
}

void HookViewDisplay::refresh()
{
    shownIds.clear();
    CyclopsCanvas::getEditorIds(canvas, shownIds);
    for (int i=0; i<shownIds.size(); i++){
        HookView* hv = CyclopsCanvas::getHookView(shownIds[i]);
        jassert(hv != nullptr);
        jassert(isParentOf(hv));
        hv->setBounds(5, 50+i*(hv->getHeight()+5), getWidth()-80, hv->getHeight());
        hv->repaint();
    }
    repaint();
}

void HookViewDisplay::resized()
{
    for (int i=0; i<shownIds.size(); i++){
        HookView* hv = CyclopsCanvas::getHookView(shownIds[i]);
        jassert(hv != nullptr);
        jassert(isParentOf(hv));
        hv->setSize(getWidth()-80, 45);
    }
}

bool HookViewDisplay::isReady()
{
    for (int i=0; i<shownIds.size(); i++){
        HookView* hv = CyclopsCanvas::getHookView(shownIds[i]);
        if (!hv->isReady())
            return false;
    }
    return true;
}

void HookViewDisplay::disableAllInputWidgets()
{
    for (int i=0; i<shownIds.size(); i++){
        HookView* hv = CyclopsCanvas::getHookView(shownIds[i]);
        hv->disableAllInputWidgets();
    }
}

void HookViewDisplay::enableAllInputWidgets()
{
    for (int i=0; i<shownIds.size(); i++){
        HookView* hv = CyclopsCanvas::getHookView(shownIds[i]);
        hv->enableAllInputWidgets();
    }
}












HookView::HookView(int node_id) : nodeId(node_id)
{
    hookIdLabel = new Label("hook_id", String(nodeId));
    hookIdLabel->setFont(Font("Default", 16, Font::plain));
    hookIdLabel->setColour(Label::textColourId, Colours::black);
    addAndMakeVisible(hookIdLabel);

    pluginSelect = new ComboBox();
    pluginSelect->setTooltip("Select the sub-plugin for this \"hook\".");
    StringArray nameList;
    CyclopsCanvas::pluginManager->getPluginNames(nameList);
    pluginSelect->addItemList(nameList, 1);
    pluginSelect->setTextWhenNothingSelected("Choose");
    pluginSelect->addListener(this);
    addAndMakeVisible(pluginSelect);

    hookInfo = new HookInfo(nodeId);
    setSize(80, 45);
}

void HookView::comboBoxChanged(ComboBox* cb)
{
    HookViewDisplay* parent = findParentComponentOfClass<HookViewDisplay>();
    parent->canvas->unicastPluginIndicator(CanvasEvent::PLUGIN_SELECTED, nodeId);
    parent->canvas->unicastUpdatePluginInfo(nodeId);
    std::cout << cb->getSelectedItemIndex() <<std::endl;
    String name = cb->getItemText(cb->getSelectedItemIndex());
    hookInfo->pluginInfo = CyclopsCanvas::pluginManager->getInfo(name.toStdString());
}

void HookView::paint(Graphics& g)
{
    g.fillAll(Colours::lightgrey);
}

void HookView::resized()
{
    hookIdLabel->setBounds(5, 0, 35, 30);
    pluginSelect->setBounds(40, 2, 180, 30);
}

void HookView::refresh()
{
    // parse all things in HookInfo
    repaint();
}

bool HookView::isReady()
{
    if (hookInfo->pluginInfo == nullptr)
        return false;
    return true;
}

void HookView::disableAllInputWidgets()
{
    pluginSelect->setEnabled(false);
}

void HookView::enableAllInputWidgets()
{
    pluginSelect->setEnabled(true);
}








HookInfo::HookInfo(int node_id) : nodeId(node_id), pluginInfo(nullptr) {}








SignalDisplay::SignalDisplay(CyclopsCanvas *cc) : canvas(cc)
{
    std::ifstream inFile("build/cyclops_plugins/signals");
    if (inFile)
        CyclopsSignal::readSignals(inFile);
    else
        jassert(false);
    inFile.close();
    setSize(300, CyclopsSignal::signals.size()*18 + 40);
}

void SignalDisplay::paint(Graphics& g)
{
    g.fillAll(Colours::cyan);
    g.setColour(Colours::black);
    //std::cout << getHeight() << " " << CyclopsSignal::signals.size() << std::endl;
    for (int i=0; i<CyclopsSignal::signals.size(); i++){
        g.drawSingleLineText(CyclopsSignal::signals[i]->name, 5, 28+18*i);
        //std::cout << i << " " << 20+18*i << std::endl;
    }
}

void SignalDisplay::resized()
{

}










SignalViewport::SignalViewport(SignalDisplay* sd) : signalDisplay(sd)
{
    setViewedComponent(signalDisplay, false);
    setScrollBarsShown(true, false);
}

void SignalViewport::paint(Graphics& g)
{
}








MigrateComponent::MigrateComponent(CyclopsCanvas* closing_canvas) : closingCanvas(closing_canvas)
{
    group = new GroupComponent ("group", "Select Hooks");
    addAndMakeVisible(group);

    canvasCombo = new ComboBox("target_canvas");

    allEditorsButton = new ToggleButton("All");
    addAndMakeVisible(allEditorsButton);
    allEditorsButton->addListener(this);

    int num_canvases = CyclopsCanvas::getNumCanvas();
    CyclopsCanvas::getEditorIds(closingCanvas, editorIdList);
    for (auto& editorId : editorIdList){
        ToggleButton* tb = editorButtonList.add(new ToggleButton(String(editorId)));
        //tb->setRadioGroupId(editorId);
        tb->setToggleState(false, dontSendNotification);
        addAndMakeVisible(tb);
        tb->addListener(this);
    }
    
    comboText = new Label("combo label", "Migrate to");
    comboText->setFont(Font("Default", 16, Font::plain));
    comboText->setColour(Label::textColourId, Colours::black);
    addAndMakeVisible(comboText);

    for (int i=0; i<num_canvases; i++){
        if (CyclopsCanvas::canvasList[i]->realIndex != closingCanvas->realIndex){
            canvasCombo->addItem("Cyclops" + String(CyclopsCanvas::canvasList[i]->realIndex), i+1);
        }
    }
    addAndMakeVisible(canvasCombo);
    canvasCombo->addListener(this);

    doneButton = new UtilityButton("DONE", Font("Default", 12, Font::plain));
    doneButton->addListener(this);
    doneButton->setEnabled(false);
    cancelButton = new UtilityButton("CANCEL", Font("Default", 12, Font::plain));
    cancelButton->addListener(this);
    addAndMakeVisible(doneButton);
    addAndMakeVisible(cancelButton);

    setSize(300, 60+30+40+30+25*editorButtonList.size());
}

void MigrateComponent::resized()
{
    int width = getWidth(), height = getHeight(), i=0;
    allEditorsButton->setBounds(width/2-200/2+10, 25, 60, 20);
    for (auto& tb : editorButtonList){
        tb->setBounds(width/2-200/2+10, 25+30+i*22, 60, 20);
        i++;
    }
    group->setBounds(width/2-200/2, 10, 200, 25+30*(i)+20);
    comboText->setBounds(width/2-100/2, height-100, 100, 25);
    canvasCombo->setBounds(width/2-90/2, height-70, 90, 25);
    doneButton->setBounds((width-80*2)/3, height-30, 80, 25);
    cancelButton->setBounds((width-80*2)*2/3+80, height-30, 80, 25);
}

void MigrateComponent::buttonClicked(Button* button)
{
    if (button == doneButton) {
        CyclopsCanvas *newCanvas = CyclopsCanvas::canvasList.getUnchecked(canvasCombo->getSelectedId()-1);
        for (int i=0; i<editorIdList.size(); i++){
            // this will changeCanvas and update SelectorButtons of src->listeners only
            if (editorButtonList[i]->getToggleState()){
                jassert(CyclopsCanvas::migrateEditor(newCanvas, closingCanvas, editorIdList[i]) == 0);
            }
            else{
                CyclopsCanvas::dropEditor(closingCanvas, editorIdList[i]);
            }
        }
        newCanvas->refresh();
        // remove tab if open
        // remove window if open
        // tell all that they need to update combo-list
        CyclopsCanvas::canvasList.removeObject(closingCanvas, true);
        for (auto& canvas : CyclopsCanvas::canvasList){
            canvas->broadcastButtonState(CanvasEvent::COMBO_BUTTON, true);
        }
        closeWindow();
    }
    else if (button == cancelButton) {
        closeWindow();
    }
    else if ( button == allEditorsButton) {
        for (auto& editorButton : editorButtonList){
            editorButton->setToggleState(true, dontSendNotification);
        }
    }
    // must be one in editorButtonList, do nothing for that (except maybe
    // 'highlight" in  EditorViewport?)
    else {
        allEditorsButton->setToggleState(false, dontSendNotification);
    }
    
}

void MigrateComponent::comboBoxChanged(ComboBox* cb)
{
    if (cb == canvasCombo){
        doneButton->setEnabled(true);
    }
}

void MigrateComponent::closeWindow()
{
    for (auto& canvas : CyclopsCanvas::canvasList){
        canvas->broadcastButtonState(CanvasEvent::COMBO_BUTTON, true);
        canvas->enableAllInputWidgets();
        canvas->broadcastEditorInteractivity(CanvasEvent::THAW);
    }
    
    if (DialogWindow* dw = findParentComponentOfClass<DialogWindow>())
        dw->exitModalState(0);
}

} // NAMESPACE cyclops
