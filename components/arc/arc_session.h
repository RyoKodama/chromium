// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_SESSION_H_
#define COMPONENTS_ARC_ARC_SESSION_H_

#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/arc/arc_stop_reason.h"

namespace arc {

class ArcBridgeService;

// Starts the ARC instance and bootstraps the bridge connection.
// Clients should implement the Delegate to be notified upon communications
// being available.
// The instance can be safely removed 1) before Start*() is called, or 2) after
// OnSessionStopped() is called.
// The number of instances must be at most one. Otherwise, ARC instances will
// conflict.
class ArcSession {
 public:
  // Observer to notify events corresponding to one ARC session run.
  class Observer {
   public:
    // Called when the connection with ARC instance has been established.
    virtual void OnSessionReady() = 0;

    // Called when ARC instance is stopped. This is called exactly once
    // per instance which is Start()ed.
    // |was_running| is true, if the stopped instance was fully set up
    // and running.
    virtual void OnSessionStopped(ArcStopReason reason, bool was_running) = 0;

   protected:
    virtual ~Observer() = default;
  };

  // Creates a default instance of ArcSession.
  static std::unique_ptr<ArcSession> Create(
      ArcBridgeService* arc_bridge_service);
  virtual ~ArcSession();

  // Starts an instance for login screen. The instance is not a fully functional
  // one, and Observer::OnSessionReady() will *never* be called.
  virtual void StartForLoginScreen() = 0;

  // Returns true if StartForLoginScreen() has been called but Start() hasn't.
  virtual bool IsForLoginScreen() = 0;

  // Starts and bootstraps a connection with the instance. The Observer's
  // OnSessionReady() will be called if the bootstrapping is successful, or
  // OnSessionStopped() if it is not. Start() should not be called twice or
  // more. When StartForLoginScreen() has already been called, Start() turns
  // the mini instance to a fully functional one.
  virtual void Start() = 0;

  // Requests to stop the currently-running instance whether or not it is for
  // login screen.
  // The completion is notified via OnSessionStopped() of the Observer.
  virtual void Stop() = 0;

  // Returns true if this instance is fully set up successfully, and running.
  // Currently, this means, this is a fully functional instance, and
  // Mojo connection is already connected successfully.
  virtual bool IsRunning() = 0;

  // Returns true if Stop() has been called already.
  virtual bool IsStopRequested() = 0;

  // Called when Chrome is in shutdown state. This is called when the message
  // loop is already stopped, and the instance will soon be deleted. Caller
  // may expect that OnSessionStopped() is synchronously called back except
  // when it has already been called before.
  virtual void OnShutdown() = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  ArcSession();

  base::ObserverList<Observer> observer_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcSession);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_SESSION_H_
