/*
 * Copyright (C) 2014 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <blpwtk2_profilehost.h>

#include <blpwtk2_browsercontextimpl.h>
#include <blpwtk2_browsercontextimplmanager.h>
#include <blpwtk2_processhost.h>
#include <blpwtk2_profile_messages.h>
#include <blpwtk2_statics.h>

#include <ipc/ipc_message_macros.h>

namespace blpwtk2 {

ProfileHost::ProfileHost(ProcessHost* processHost,
                         int routingId,
                         const std::string& dataDir,
                         bool diskCacheEnabled)
: d_processHost(processHost)
, d_routingId(routingId)
{
    d_processHost->addRoute(d_routingId, this);
    d_browserContext =
        Statics::browserContextImplManager->obtainBrowserContextImpl(
            dataDir,
            diskCacheEnabled);
}

ProfileHost::~ProfileHost()
{
    d_browserContext->destroy();
    d_processHost->removeRoute(d_routingId);
}

BrowserContextImpl* ProfileHost::browserContext() const
{
    return d_browserContext;
}

// IPC::Listener overrides

bool ProfileHost::OnMessageReceived(const IPC::Message& message)
{
    bool handled = true;
    bool msgIsOk = true;
    IPC_BEGIN_MESSAGE_MAP_EX(ProfileHost, message, msgIsOk)
        IPC_MESSAGE_HANDLER(BlpProfileHostMsg_SetProxyConfig, onSetProxyConfig)
        IPC_MESSAGE_HANDLER(BlpProfileHostMsg_UseSystemProxyConfig, onUseSystemProxyConfig)
        IPC_MESSAGE_HANDLER(BlpProfileHostMsg_SetSpellCheckConfig, onSetSpellCheckConfig)
        IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP_EX()

    if (!msgIsOk) {
        LOG(ERROR) << "bad message " << message.type();
    }

    return handled;
}

// Message handlers

void ProfileHost::onSetProxyConfig(const ProxyConfig& config)
{
    d_browserContext->setProxyConfig(config);
}

void ProfileHost::onUseSystemProxyConfig()
{
    d_browserContext->useSystemProxyConfig();
}

void ProfileHost::onSetSpellCheckConfig(const SpellCheckConfig& config)
{
    d_browserContext->setSpellCheckConfig(config);
}

}  // close namespace blpwtk2

