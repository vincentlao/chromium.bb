/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2012 Intel Inc. All rights reserved.
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

#ifndef Performance_h
#define Performance_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/events/EventTarget.h"
#include "core/frame/DOMWindowProperty.h"
#include "core/timing/MemoryInfo.h"
#include "core/timing/PerformanceEntry.h"
#include "core/timing/PerformanceNavigation.h"
#include "core/timing/PerformanceTiming.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class Document;
class ExceptionState;
class ResourceRequest;
class ResourceResponse;
class ResourceTimingInfo;
class UserTiming;

class Performance FINAL : public ScriptWrappable, public RefCounted<Performance>, public DOMWindowProperty, public EventTargetWithInlineData {
    REFCOUNTED_EVENT_TARGET(Performance);
public:
    static PassRefPtr<Performance> create(Frame* frame) { return adoptRef(new Performance(frame)); }
    virtual ~Performance();

    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE;

    PassRefPtr<MemoryInfo> memory() const;
    PerformanceNavigation* navigation() const;
    PerformanceTiming* timing() const;
    double now() const;

    Vector<RefPtr<PerformanceEntry> > getEntries() const;
    Vector<RefPtr<PerformanceEntry> > getEntriesByType(const String& entryType);
    Vector<RefPtr<PerformanceEntry> > getEntriesByName(const String& name, const String& entryType);

    void webkitClearResourceTimings();
    void webkitSetResourceTimingBufferSize(unsigned int);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitresourcetimingbufferfull);

    void addResourceTiming(const ResourceTimingInfo&, Document*);

    void mark(const String& markName, ExceptionState&);
    void clearMarks(const String& markName);

    void measure(const String& measureName, const String& startMark, const String& endMark, ExceptionState&);
    void clearMeasures(const String& measureName);

private:
    explicit Performance(Frame*);

    bool isResourceTimingBufferFull();
    void addResourceTimingBuffer(PassRefPtr<PerformanceEntry>);

    mutable RefPtr<PerformanceNavigation> m_navigation;
    mutable RefPtr<PerformanceTiming> m_timing;

    Vector<RefPtr<PerformanceEntry> > m_resourceTimingBuffer;
    unsigned m_resourceTimingBufferSize;
    double m_referenceTime;

    RefPtr<UserTiming> m_userTiming;
};

}

#endif // Performance_h
