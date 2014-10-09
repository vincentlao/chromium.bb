/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

/**
 * @constructor
 * @extends {WebInspector.SidebarPane}
 */
WebInspector.WorkersSidebarPane = function()
{
    WebInspector.SidebarPane.call(this, WebInspector.UIString("Workers"));

    this._enableWorkersCheckbox = new WebInspector.Checkbox(
        WebInspector.UIString("Pause on start"),
        "sidebar-label",
        WebInspector.UIString("Automatically attach to new workers and pause them. Enabling this option will force opening inspector for all new workers."));
    this._enableWorkersCheckbox.element.id = "pause-workers-checkbox";
    this.bodyElement.appendChild(this._enableWorkersCheckbox.element);
    this._enableWorkersCheckbox.addEventListener(this._autoattachToWorkersClicked.bind(this));
    this._enableWorkersCheckbox.checked = false;

    var note = this.bodyElement.createChild("div");
    note.id = "shared-workers-list";
    note.classList.add("sidebar-label")
    note.textContent = WebInspector.UIString("Shared workers can be inspected in the Task Manager");

    var separator = this.bodyElement.createChild("div", "sidebar-separator");
    separator.textContent = WebInspector.UIString("Dedicated worker inspectors");

    this._workerListElement = document.createElement("ol");
    this._workerListElement.tabIndex = 0;
    this._workerListElement.classList.add("properties-tree");
    this._workerListElement.classList.add("sidebar-label");
    this.bodyElement.appendChild(this._workerListElement);

    this._idToWorkerItem = {};

    var threadList = WebInspector.workerManager.threadsList();
    for (var i = 0; i < threadList.length; ++i) {
        var threadId = threadList[i];
        if (threadId === WebInspector.WorkerManager.MainThreadId)
            continue;

        this._addWorker(threadId, WebInspector.workerManager.threadUrl(threadId));
    }

    WebInspector.workerManager.addEventListener(WebInspector.WorkerManager.Events.WorkerAdded, this._workerAdded, this);
    WebInspector.workerManager.addEventListener(WebInspector.WorkerManager.Events.WorkerRemoved, this._workerRemoved, this);
    WebInspector.workerManager.addEventListener(WebInspector.WorkerManager.Events.WorkersCleared, this._workersCleared, this);
}

WebInspector.WorkersSidebarPane.prototype = {
    _workerAdded: function(event)
    {
        this._addWorker(event.data.workerId, event.data.url);
    },

    _workerRemoved: function(event)
    {
        this._idToWorkerItem[event.data].remove();
        delete this._idToWorkerItem[event.data];
    },

    _workersCleared: function(event)
    {
        this._idToWorkerItem = {};
        this._workerListElement.removeChildren();
    },

    _addWorker: function(workerId, url)
    {
        var item = this._workerListElement.createChild("div", "dedicated-worker-item");
        var link = item.createChild("a");
        link.textContent = url;
        link.href = "#";
        link.target = "_blank";
        link.addEventListener("click", this._workerItemClicked.bind(this, workerId), true);
        this._idToWorkerItem[workerId] = item;
    },

    _workerItemClicked: function(workerId, event)
    {
        event.preventDefault();
        WebInspector.workerFrontendManager.openWorkerInspector(workerId);
    },

    _autoattachToWorkersClicked: function(event)
    {
        WorkerAgent.setAutoconnectToWorkers(this._enableWorkersCheckbox.checked);
    },

    __proto__: WebInspector.SidebarPane.prototype
}
