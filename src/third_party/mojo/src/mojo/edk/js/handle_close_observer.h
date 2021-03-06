// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_MOJO_SRC_MOJO_EDK_JS_HANDLE_CLOSE_OBSERVER_H_
#define THIRD_PARTY_MOJO_SRC_MOJO_EDK_JS_HANDLE_CLOSE_OBSERVER_H_

namespace mojo {
namespace js {

class HandleCloseObserver {
 public:
  virtual void OnWillCloseHandle() = 0;

 protected:
  virtual ~HandleCloseObserver() {}
};

}  // namespace js
}  // namespace mojo

#endif  // THIRD_PARTY_MOJO_SRC_MOJO_EDK_JS_HANDLE_CLOSE_OBSERVER_H_
