// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushError_h
#define PushError_h

#include "core/dom/DOMException.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/push_messaging/WebPushError.h"

namespace blink {

class ScriptPromiseResolver;

class PushError {
    WTF_MAKE_NONCOPYABLE(PushError);
public:
    // For CallbackPromiseAdapter.
    typedef WebPushError WebType;
    static PassRefPtrWillBeRawPtr<DOMException> take(ScriptPromiseResolver*, WebType* webErrorRaw);
    static void dispose(WebType* webErrorRaw);

private:
    PushError() = delete;
};

} // namespace blink

#endif // PushError_h
