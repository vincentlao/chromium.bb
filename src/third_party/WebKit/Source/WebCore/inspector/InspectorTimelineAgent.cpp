/*
* Copyright (C) 2009 Google Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
*     * Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
* copyright notice, this list of conditions and the following disclaimer
* in the documentation and/or other materials provided with the
* distribution.
*     * Neither the name of Google Inc. nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "config.h"

#if ENABLE(INSPECTOR)

#include "InspectorTimelineAgent.h"

#include "Event.h"
#include "Frame.h"
#include "IdentifiersFactory.h"
#include "InspectorClient.h"
#include "InspectorCounters.h"
#include "InspectorFrontend.h"
#include "InspectorPageAgent.h"
#include "InspectorState.h"
#include "InstrumentingAgents.h"
#include "IntRect.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "TimelineRecordFactory.h"

#include <wtf/CurrentTime.h>

namespace WebCore {

namespace TimelineAgentState {
static const char timelineAgentEnabled[] = "timelineAgentEnabled";
static const char timelineMaxCallStackDepth[] = "timelineMaxCallStackDepth";
static const char includeMemoryDetails[] = "includeMemoryDetails";
}

// Must be kept in sync with WebInspector.TimelineModel.RecordType in TimelineModel.js
namespace TimelineRecordType {
static const char EventDispatch[] = "EventDispatch";
static const char BeginFrame[] = "BeginFrame";
static const char Layout[] = "Layout";
static const char RecalculateStyles[] = "RecalculateStyles";
static const char Paint[] = "Paint";
static const char CompositeLayers[] = "CompositeLayers";

static const char ParseHTML[] = "ParseHTML";

static const char TimerInstall[] = "TimerInstall";
static const char TimerRemove[] = "TimerRemove";
static const char TimerFire[] = "TimerFire";

static const char EvaluateScript[] = "EvaluateScript";

static const char MarkLoad[] = "MarkLoad";
static const char MarkDOMContent[] = "MarkDOMContent";

static const char TimeStamp[] = "TimeStamp";

static const char ScheduleResourceRequest[] = "ScheduleResourceRequest";
static const char ResourceSendRequest[] = "ResourceSendRequest";
static const char ResourceReceiveResponse[] = "ResourceReceiveResponse";
static const char ResourceReceivedData[] = "ResourceReceivedData";
static const char ResourceFinish[] = "ResourceFinish";

static const char XHRReadyStateChange[] = "XHRReadyStateChange";
static const char XHRLoad[] = "XHRLoad";

static const char FunctionCall[] = "FunctionCall";
static const char GCEvent[] = "GCEvent";

static const char RequestAnimationFrame[] = "RequestAnimationFrame";
static const char CancelAnimationFrame[] = "CancelAnimationFrame";
static const char FireAnimationFrame[] = "FireAnimationFrame";
}

void InspectorTimelineAgent::pushGCEventRecords()
{
    if (!m_gcEvents.size())
        return;

    GCEvents events = m_gcEvents;
    m_gcEvents.clear();
    for (GCEvents::iterator i = events.begin(); i != events.end(); ++i) {
        RefPtr<InspectorObject> record = TimelineRecordFactory::createGenericRecord(timestampFromMicroseconds(i->startTime), m_maxCallStackDepth);
        record->setObject("data", TimelineRecordFactory::createGCEventData(i->collectedBytes));
        record->setNumber("endTime", timestampFromMicroseconds(i->endTime));
        addRecordToTimeline(record.release(), TimelineRecordType::GCEvent, String());
    }
}

void InspectorTimelineAgent::didGC(double startTime, double endTime, size_t collectedBytesCount)
{
    m_gcEvents.append(GCEvent(startTime, endTime, collectedBytesCount));
}

InspectorTimelineAgent::~InspectorTimelineAgent()
{
    clearFrontend();
}

void InspectorTimelineAgent::setFrontend(InspectorFrontend* frontend)
{
    m_frontend = frontend->timeline();
}

void InspectorTimelineAgent::clearFrontend()
{
    ErrorString error;
    stop(&error);
    m_frontend = 0;
}

void InspectorTimelineAgent::restore()
{
    if (m_state->getBoolean(TimelineAgentState::timelineAgentEnabled)) {
        m_maxCallStackDepth = m_state->getLong(TimelineAgentState::timelineMaxCallStackDepth);
        ErrorString error;
        start(&error, &m_maxCallStackDepth);
    }
}

void InspectorTimelineAgent::start(ErrorString*, const int* maxCallStackDepth)
{
    if (!m_frontend)
        return;

    if (maxCallStackDepth && *maxCallStackDepth > 0)
        m_maxCallStackDepth = *maxCallStackDepth;
    else
        m_maxCallStackDepth = 5;
    m_state->setLong(TimelineAgentState::timelineMaxCallStackDepth, m_maxCallStackDepth);
    m_timestampOffset = currentTime() - monotonicallyIncreasingTime();

    if (m_client)
        m_client->startMainThreadMonitoring();

    m_instrumentingAgents->setInspectorTimelineAgent(this);
    ScriptGCEvent::addEventListener(this);
    m_state->setBoolean(TimelineAgentState::timelineAgentEnabled, true);
}

void InspectorTimelineAgent::stop(ErrorString*)
{
    if (!m_state->getBoolean(TimelineAgentState::timelineAgentEnabled))
        return;

    if (m_client)
        m_client->stopMainThreadMonitoring();

    m_instrumentingAgents->setInspectorTimelineAgent(0);
    ScriptGCEvent::removeEventListener(this);

    clearRecordStack();
    m_gcEvents.clear();

    m_state->setBoolean(TimelineAgentState::timelineAgentEnabled, false);
}

void InspectorTimelineAgent::setIncludeMemoryDetails(ErrorString*, bool value)
{
    m_state->setBoolean(TimelineAgentState::includeMemoryDetails, value);
}

void InspectorTimelineAgent::supportsFrameInstrumentation(ErrorString*, bool* result)
{
    *result = m_client && m_client->supportsFrameInstrumentation();
}

void InspectorTimelineAgent::didBeginFrame()
{
    pushCancelableRecord(InspectorObject::create(), TimelineRecordType::BeginFrame, 0);
}

void InspectorTimelineAgent::didCancelFrame()
{
    cancelRecord(TimelineRecordType::BeginFrame);
}

void InspectorTimelineAgent::willCallFunction(const String& scriptName, int scriptLine, Frame* frame)
{
    pushCurrentRecord(TimelineRecordFactory::createFunctionCallData(scriptName, scriptLine), TimelineRecordType::FunctionCall, true, frame);
}

void InspectorTimelineAgent::didCallFunction()
{
    didCompleteCurrentRecord(TimelineRecordType::FunctionCall);
}

void InspectorTimelineAgent::willDispatchEvent(const Event& event, Frame* frame)
{
    pushCurrentRecord(TimelineRecordFactory::createEventDispatchData(event), TimelineRecordType::EventDispatch, false, frame);
}

void InspectorTimelineAgent::didDispatchEvent()
{
    didCompleteCurrentRecord(TimelineRecordType::EventDispatch);
}

void InspectorTimelineAgent::willLayout(Frame* frame)
{
    pushCurrentRecord(InspectorObject::create(), TimelineRecordType::Layout, true, frame);
}

void InspectorTimelineAgent::didLayout()
{
    didCompleteCurrentRecord(TimelineRecordType::Layout);
}

void InspectorTimelineAgent::willRecalculateStyle(Frame* frame)
{
    pushCurrentRecord(InspectorObject::create(), TimelineRecordType::RecalculateStyles, true, frame);
}

void InspectorTimelineAgent::didRecalculateStyle()
{
    didCompleteCurrentRecord(TimelineRecordType::RecalculateStyles);
}

void InspectorTimelineAgent::willPaint(const LayoutRect& rect, Frame* frame)
{
    pushCurrentRecord(TimelineRecordFactory::createPaintData(rect), TimelineRecordType::Paint, true, frame);
}

void InspectorTimelineAgent::didPaint()
{
    didCompleteCurrentRecord(TimelineRecordType::Paint);
}

void InspectorTimelineAgent::willComposite()
{
    pushCurrentRecord(InspectorObject::create(), TimelineRecordType::CompositeLayers, false, 0);
}

void InspectorTimelineAgent::didComposite()
{
    didCompleteCurrentRecord(TimelineRecordType::CompositeLayers);
}

void InspectorTimelineAgent::willWriteHTML(unsigned int length, unsigned int startLine, Frame* frame)
{
    pushCurrentRecord(TimelineRecordFactory::createParseHTMLData(length, startLine), TimelineRecordType::ParseHTML, true, frame);
}

void InspectorTimelineAgent::didWriteHTML(unsigned int endLine)
{
    if (!m_recordStack.isEmpty()) {
        TimelineRecordEntry entry = m_recordStack.last();
        entry.data->setNumber("endLine", endLine);
        didCompleteCurrentRecord(TimelineRecordType::ParseHTML);
    }
}

void InspectorTimelineAgent::didInstallTimer(int timerId, int timeout, bool singleShot, Frame* frame)
{
    appendRecord(TimelineRecordFactory::createTimerInstallData(timerId, timeout, singleShot), TimelineRecordType::TimerInstall, true, frame);
}

void InspectorTimelineAgent::didRemoveTimer(int timerId, Frame* frame)
{
    appendRecord(TimelineRecordFactory::createGenericTimerData(timerId), TimelineRecordType::TimerRemove, true, frame);
}

void InspectorTimelineAgent::willFireTimer(int timerId, Frame* frame)
{
    pushCurrentRecord(TimelineRecordFactory::createGenericTimerData(timerId), TimelineRecordType::TimerFire, false, frame);
}

void InspectorTimelineAgent::didFireTimer()
{
    didCompleteCurrentRecord(TimelineRecordType::TimerFire);
}

void InspectorTimelineAgent::willChangeXHRReadyState(const String& url, int readyState, Frame* frame)
{
    pushCurrentRecord(TimelineRecordFactory::createXHRReadyStateChangeData(url, readyState), TimelineRecordType::XHRReadyStateChange, false, frame);
}

void InspectorTimelineAgent::didChangeXHRReadyState()
{
    didCompleteCurrentRecord(TimelineRecordType::XHRReadyStateChange);
}

void InspectorTimelineAgent::willLoadXHR(const String& url, Frame* frame)
{
    pushCurrentRecord(TimelineRecordFactory::createXHRLoadData(url), TimelineRecordType::XHRLoad, true, frame);
}

void InspectorTimelineAgent::didLoadXHR()
{
    didCompleteCurrentRecord(TimelineRecordType::XHRLoad);
}

void InspectorTimelineAgent::willEvaluateScript(const String& url, int lineNumber, Frame* frame)
{
    pushCurrentRecord(TimelineRecordFactory::createEvaluateScriptData(url, lineNumber), TimelineRecordType::EvaluateScript, true, frame);
}
    
void InspectorTimelineAgent::didEvaluateScript()
{
    didCompleteCurrentRecord(TimelineRecordType::EvaluateScript);
}

void InspectorTimelineAgent::didScheduleResourceRequest(const String& url, Frame* frame)
{
    appendRecord(TimelineRecordFactory::createScheduleResourceRequestData(url), TimelineRecordType::ScheduleResourceRequest, true, frame);
}

void InspectorTimelineAgent::willSendResourceRequest(unsigned long identifier, const ResourceRequest& request, Frame* frame)
{
    pushGCEventRecords();
    RefPtr<InspectorObject> recordRaw = TimelineRecordFactory::createGenericRecord(timestamp(), m_maxCallStackDepth);
    String requestId = IdentifiersFactory::requestId(identifier);
    recordRaw->setObject("data", TimelineRecordFactory::createResourceSendRequestData(requestId, request));
    recordRaw->setString("type", TimelineRecordType::ResourceSendRequest);
    if (frame && m_pageAgent) {
        String frameId(m_pageAgent->frameId(frame));
        recordRaw->setString("frameId", frameId);
    }
    setHeapSizeStatistic(recordRaw.get());
    // FIXME: runtimeCast is a hack. We do it because we can't build TimelineEvent directly now.
    RefPtr<TypeBuilder::Timeline::TimelineEvent> record = TypeBuilder::Timeline::TimelineEvent::runtimeCast(recordRaw.release());
    m_frontend->eventRecorded(record.release());
}

void InspectorTimelineAgent::willReceiveResourceData(unsigned long identifier, Frame* frame)
{
    String requestId = IdentifiersFactory::requestId(identifier);
    pushCurrentRecord(TimelineRecordFactory::createReceiveResourceData(requestId), TimelineRecordType::ResourceReceivedData, false, frame);
}

void InspectorTimelineAgent::didReceiveResourceData()
{
    didCompleteCurrentRecord(TimelineRecordType::ResourceReceivedData);
}
    
void InspectorTimelineAgent::willReceiveResourceResponse(unsigned long identifier, const ResourceResponse& response, Frame* frame)
{
    String requestId = IdentifiersFactory::requestId(identifier);
    pushCurrentRecord(TimelineRecordFactory::createResourceReceiveResponseData(requestId, response), TimelineRecordType::ResourceReceiveResponse, false, frame);
}

void InspectorTimelineAgent::didReceiveResourceResponse()
{
    didCompleteCurrentRecord(TimelineRecordType::ResourceReceiveResponse);
}

void InspectorTimelineAgent::didFinishLoadingResource(unsigned long identifier, bool didFail, double finishTime, Frame* frame)
{
    appendRecord(TimelineRecordFactory::createResourceFinishData(IdentifiersFactory::requestId(identifier), didFail, finishTime * 1000), TimelineRecordType::ResourceFinish, false, frame);
}

void InspectorTimelineAgent::didTimeStamp(const String& message)
{
    appendRecord(TimelineRecordFactory::createTimeStampData(message), TimelineRecordType::TimeStamp, true, 0);
}

void InspectorTimelineAgent::didMarkDOMContentEvent(Frame* frame)
{
    appendRecord(InspectorObject::create(), TimelineRecordType::MarkDOMContent, false, frame);
}

void InspectorTimelineAgent::didMarkLoadEvent(Frame* frame)
{
    appendRecord(InspectorObject::create(), TimelineRecordType::MarkLoad, false, frame);
}

void InspectorTimelineAgent::didCommitLoad()
{
    clearRecordStack();
}

void InspectorTimelineAgent::didRequestAnimationFrame(int callbackId, Frame* frame)
{
    appendRecord(TimelineRecordFactory::createAnimationFrameData(callbackId), TimelineRecordType::RequestAnimationFrame, true, frame);
}

void InspectorTimelineAgent::didCancelAnimationFrame(int callbackId, Frame* frame)
{
    appendRecord(TimelineRecordFactory::createAnimationFrameData(callbackId), TimelineRecordType::CancelAnimationFrame, true, frame);
}

void InspectorTimelineAgent::willFireAnimationFrame(int callbackId, Frame* frame)
{
    pushCurrentRecord(TimelineRecordFactory::createAnimationFrameData(callbackId), TimelineRecordType::FireAnimationFrame, false, frame);
}

void InspectorTimelineAgent::didFireAnimationFrame()
{
    didCompleteCurrentRecord(TimelineRecordType::FireAnimationFrame);
}

void InspectorTimelineAgent::willProcessTask()
{
    // TODO: Record task processing start time.
}

void InspectorTimelineAgent::didProcessTask()
{
    // TODO: Record task processing end time.
}

void InspectorTimelineAgent::addRecordToTimeline(PassRefPtr<InspectorObject> record, const String& type, const String& frameId)
{
    commitCancelableRecords();
    innerAddRecordToTimeline(record, type, frameId);
}

void InspectorTimelineAgent::innerAddRecordToTimeline(PassRefPtr<InspectorObject> prpRecord, const String& type, const String& frameId)
{
    RefPtr<InspectorObject> record(prpRecord);
    record->setString("type", type);
    if (!frameId.isEmpty())
        record->setString("frameId", frameId);
    setHeapSizeStatistic(record.get());
    if (m_recordStack.isEmpty()) {
        // FIXME: runtimeCast is a hack. We do it because we can't build TimelineEvent directly now.
        RefPtr<TypeBuilder::Timeline::TimelineEvent> recordChecked = TypeBuilder::Timeline::TimelineEvent::runtimeCast(record.release());
        m_frontend->eventRecorded(recordChecked.release());
    } else {
        TimelineRecordEntry parent = m_recordStack.last();
        parent.children->pushObject(record.release());
    }
}

void InspectorTimelineAgent::setHeapSizeStatistic(InspectorObject* record)
{
    size_t usedHeapSize = 0;
    size_t totalHeapSize = 0;
    size_t heapSizeLimit = 0;
    ScriptGCEvent::getHeapSize(usedHeapSize, totalHeapSize, heapSizeLimit);
    record->setNumber("usedHeapSize", usedHeapSize);
    record->setNumber("totalHeapSize", totalHeapSize);

    if (m_state->getBoolean(TimelineAgentState::includeMemoryDetails)) {
        RefPtr<InspectorObject> counters = InspectorObject::create();
        counters->setNumber("nodes", (m_inspectorType == PageInspector) ? InspectorCounters::counterValue(InspectorCounters::NodeCounter) : 0);
        counters->setNumber("documents", (m_inspectorType == PageInspector) ? InspectorCounters::counterValue(InspectorCounters::DocumentCounter) : 0);
        counters->setNumber("jsEventListeners", ThreadLocalInspectorCounters::current().counterValue(ThreadLocalInspectorCounters::JSEventListenerCounter));
        record->setObject("counters", counters.release());
    }
}

void InspectorTimelineAgent::didCompleteCurrentRecord(const String& type)
{
    // An empty stack could merely mean that the timeline agent was turned on in the middle of
    // an event.  Don't treat as an error.
    if (!m_recordStack.isEmpty()) {
        pushGCEventRecords();
        TimelineRecordEntry entry = m_recordStack.last();
        m_recordStack.removeLast();
        ASSERT(entry.type == type);
        entry.record->setObject("data", entry.data);
        entry.record->setArray("children", entry.children);
        entry.record->setNumber("endTime", timestamp());
        addRecordToTimeline(entry.record, type, entry.frameId);
    }
}

InspectorTimelineAgent::InspectorTimelineAgent(InstrumentingAgents* instrumentingAgents, InspectorPageAgent* pageAgent, InspectorState* state, InspectorType type, InspectorClient* client)
    : InspectorBaseAgent<InspectorTimelineAgent>("Timeline", instrumentingAgents, state)
    , m_pageAgent(pageAgent)
    , m_frontend(0)
    , m_timestampOffset(0)
    , m_id(1)
    , m_maxCallStackDepth(5)
    , m_inspectorType(type)
    , m_client(client)
{
}

void InspectorTimelineAgent::appendRecord(PassRefPtr<InspectorObject> data, const String& type, bool captureCallStack, Frame* frame)
{
    pushGCEventRecords();
    RefPtr<InspectorObject> record = TimelineRecordFactory::createGenericRecord(timestamp(), captureCallStack ? m_maxCallStackDepth : 0);
    record->setObject("data", data);
    record->setString("type", type);
    String frameId;
    if (frame && m_pageAgent)
        frameId = m_pageAgent->frameId(frame);
    addRecordToTimeline(record.release(), type, frameId);
}

void InspectorTimelineAgent::pushCurrentRecord(PassRefPtr<InspectorObject> data, const String& type, bool captureCallStack, Frame* frame)
{
    pushGCEventRecords();
    commitCancelableRecords();
    RefPtr<InspectorObject> record = TimelineRecordFactory::createGenericRecord(timestamp(), captureCallStack ? m_maxCallStackDepth : 0);
    String frameId;
    if (frame && m_pageAgent)
        frameId = m_pageAgent->frameId(frame);
    m_recordStack.append(TimelineRecordEntry(record.release(), data, InspectorArray::create(), type, frameId));
}

void InspectorTimelineAgent::pushCancelableRecord(PassRefPtr<InspectorObject> data, const String& type, Frame* frame)
{
    RefPtr<InspectorObject> record = TimelineRecordFactory::createGenericRecord(timestamp(), 0);
    String frameId;
    if (frame && m_pageAgent)
        frameId = m_pageAgent->frameId(frame);
    m_recordStack.append(TimelineRecordEntry(record.release(), data, 0, type, frameId, true));
}

void InspectorTimelineAgent::commitCancelableRecords()
{
    Vector<TimelineRecordEntry> cancelableRecords;
    while (!m_recordStack.isEmpty()) {
        TimelineRecordEntry entry = m_recordStack.last();
        if (!m_recordStack.last().cancelable)
            break;
        m_recordStack.removeLast();
        cancelableRecords.append(entry);
    }
    while (!cancelableRecords.isEmpty()) {
        TimelineRecordEntry entry = cancelableRecords.last();
        cancelableRecords.removeLast();
        entry.record->setObject("data", entry.data);
        innerAddRecordToTimeline(entry.record.release(), entry.type, entry.frameId);
    }
}

void InspectorTimelineAgent::cancelRecord(const String& type)
{
    if (m_recordStack.isEmpty())
        return;
    TimelineRecordEntry entry = m_recordStack.last();
    if (entry.cancelable && entry.type == type)
        m_recordStack.removeLast();
}

void InspectorTimelineAgent::clearRecordStack()
{
    m_recordStack.clear();
    m_id++;
}

double InspectorTimelineAgent::timestamp()
{
    return timestampFromMicroseconds(WTF::monotonicallyIncreasingTime());
}

double InspectorTimelineAgent::timestampFromMicroseconds(double microseconds)
{
    return (microseconds + m_timestampOffset) * 1000.0;
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
