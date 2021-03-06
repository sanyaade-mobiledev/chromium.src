// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PROCESS_PROXY_PROCESS_PROXY_REGISTRY_H_
#define CHROME_BROWSER_CHROMEOS_PROCESS_PROXY_PROCESS_PROXY_REGISTRY_H_

#include <map>

#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "chrome/browser/chromeos/process_proxy/process_proxy.h"

typedef base::Callback<void(pid_t, const std::string&, const std::string&)>
      ProcessOutputCallbackWithPid;

// Keeps track of all created ProcessProxies. It is created lazily and should
// live on the FILE thread (where all methods must be called).
class ProcessProxyRegistry {
 public:
  // Info we need about a ProcessProxy instance.
  struct ProcessProxyInfo {
    scoped_refptr<ProcessProxy> proxy;
    scoped_ptr<base::Thread> watcher_thread;
    ProcessOutputCallbackWithPid callback;
    pid_t process_id;

    ProcessProxyInfo();
    // This is to make map::insert happy, we don't init anything.
    ProcessProxyInfo(const ProcessProxyInfo& other);
    ~ProcessProxyInfo();
  };

  static ProcessProxyRegistry* Get();

  // Starts new ProcessProxy (which starts new process).
  bool OpenProcess(const std::string& command, pid_t* pid,
                   const ProcessOutputCallbackWithPid& callback);
  // Sends data to the process with id |pid|.
  bool SendInput(pid_t pid, const std::string& data);
  // Stops the process with id |pid|.
  bool CloseProcess(pid_t pid);
  // Reports terminal resize to process proxy.
  bool OnTerminalResize(pid_t pid, int width, int height);

  // Currently used for testing.
  void SetOutputCallback(const ProcessOutputCallback& callback);

 private:
  friend struct ::base::DefaultLazyInstanceTraits<ProcessProxyRegistry>;

  ProcessProxyRegistry();
  ~ProcessProxyRegistry();

  // Gets called when output gets detected.
  void OnProcessOutput(pid_t pid,
                       ProcessOutputType type,
                       const std::string& data);
  // Must be called on UI thread. This lives on FILE thread, thus static.
  // Notifies CroshProcessEventRouter about new process output. Assumes the
  // event router has already been initialized by someone else.
  static void DispatchProcessOutputOnUIThread(pid_t pid,
                                             const std::string& output_type,
                                             const std::string& output);

  // Map of all existing ProcessProxies.
  std::map<pid_t, ProcessProxyInfo> proxy_map_;

  DISALLOW_COPY_AND_ASSIGN(ProcessProxyRegistry);
};

#endif  // CHROME_BROWSER_CHROMEOS_PROCESS_PROXY_PROCESS_PROXY_REGISTRY_H_
