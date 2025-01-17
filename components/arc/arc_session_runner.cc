// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_session_runner.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/arc/arc_util.h"

namespace arc {

namespace {

constexpr base::TimeDelta kDefaultRestartDelay =
    base::TimeDelta::FromSeconds(5);

chromeos::SessionManagerClient* GetSessionManagerClient() {
  // If the DBusThreadManager or the SessionManagerClient aren't available,
  // there isn't much we can do. This should only happen when running tests.
  if (!chromeos::DBusThreadManager::IsInitialized() ||
      !chromeos::DBusThreadManager::Get() ||
      !chromeos::DBusThreadManager::Get()->GetSessionManagerClient())
    return nullptr;
  return chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
}

// Returns true if restart is needed for given conditions.
bool IsRestartNeeded(bool run_requested,
                     ArcStopReason stop_reason,
                     bool was_running) {
  if (!run_requested) {
    // The request to run ARC is canceled by the caller. No need to restart.
    return false;
  }

  switch (stop_reason) {
    case ArcStopReason::SHUTDOWN:
      // This is a part of stop requested by ArcSessionRunner.
      // If ARC is re-requested to start, restart is necessary.
      // This case happens, e.g., RequestStart() -> RequestStop() ->
      // RequestStart(), case. If the second RequestStart() is called before
      // the instance previously running is stopped, then just |run_requested|
      // flag is set. On completion, restart is needed.
      return true;
    case ArcStopReason::GENERIC_BOOT_FAILURE:
    case ArcStopReason::LOW_DISK_SPACE:
      // These two are errors on starting. To prevent failure loop, do not
      // restart.
      return false;
    case ArcStopReason::CRASH:
      // ARC instance is crashed unexpectedly, so automatically restart.
      // However, to avoid crash loop, do not restart if it is not successfully
      // started yet. So, check |was_running|.
      return was_running;
  }

  NOTREACHED();
  return false;
}

}  // namespace

ArcSessionRunner::ArcSessionRunner(const ArcSessionFactory& factory)
    : restart_delay_(kDefaultRestartDelay),
      factory_(factory),
      weak_ptr_factory_(this) {
  chromeos::SessionManagerClient* client = GetSessionManagerClient();
  if (client)
    client->AddObserver(this);
}

ArcSessionRunner::~ArcSessionRunner() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (arc_session_)
    arc_session_->RemoveObserver(this);
  chromeos::SessionManagerClient* client = GetSessionManagerClient();
  if (client)
    client->RemoveObserver(this);
}

void ArcSessionRunner::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observer_list_.AddObserver(observer);
}

void ArcSessionRunner::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observer_list_.RemoveObserver(observer);
}

void ArcSessionRunner::RequestStart() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Consecutive RequestStart() call. Do nothing.
  if (run_requested_)
    return;

  VLOG(1) << "Session start requested";
  run_requested_ = true;
  // Here |run_requested_| transitions from false to true. So, |restart_timer_|
  // must be stopped (either not even started, or has been cancelled in
  // previous RequestStop() call).
  DCHECK(!restart_timer_.IsRunning());

  if (arc_session_ && arc_session_->IsStopRequested()) {
    // This is the case where RequestStop() was called, but before
    // |arc_session_| had finshed stopping, RequestStart() is called.
    // Do nothing in the that case, since when |arc_session_| does actually
    // stop, OnSessionStopped() will be called, where it should automatically
    // restart.
    return;
  }

  StartArcSession();
}

void ArcSessionRunner::RequestStop(bool always_stop_session) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!run_requested_) {
    // Call Stop() to stop an instance for login screen (if any.) If this is
    // just a consecutive RequestStop() call, Stop() does nothing.
    if (!always_stop_session || !arc_session_)
      return;
  }

  VLOG(1) << "Session stop requested";
  run_requested_ = false;

  if (arc_session_) {
    // If |arc_session_| is running, stop it.
    // Note that |arc_session_| may be already in the process of stopping or
    // be stopped.
    // E.g. RequestStart() -> RequestStop() -> RequestStart() -> RequestStop()
    // case. If the second RequestStop() is called before the first
    // RequestStop() is not yet completed for the instance, Stop() of the
    // instance is called again, but it is no-op as expected.
    arc_session_->Stop();
  }

  // In case restarting is in progress, cancel it.
  restart_timer_.Stop();
}

void ArcSessionRunner::OnShutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  VLOG(1) << "OnShutdown";
  run_requested_ = false;
  restart_timer_.Stop();
  if (arc_session_)
    arc_session_->OnShutdown();
  // ArcSession::OnShutdown() invokes OnSessionStopped() synchronously.
  // In the observer method, |arc_session_| should be destroyed.
  DCHECK(!arc_session_);
}

// TODO(hidehiko,lhchavez,yusukes): Revisit following state accessors.
bool ArcSessionRunner::IsRunning() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // For historical reason, exclude "stopping" instance phase.
  return arc_session_ && arc_session_->IsRunning() &&
         !arc_session_->IsStopRequested();
}

bool ArcSessionRunner::IsStopped() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return !arc_session_;
}

bool ArcSessionRunner::IsStopping() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return arc_session_ && arc_session_->IsStopRequested();
}

bool ArcSessionRunner::IsLoginScreenInstanceStarting() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return arc_session_ && arc_session_->IsForLoginScreen() &&
         !arc_session_->IsStopRequested();
}

void ArcSessionRunner::SetRestartDelayForTesting(
    const base::TimeDelta& restart_delay) {
  DCHECK(!arc_session_);
  DCHECK(!restart_timer_.IsRunning());
  restart_delay_ = restart_delay;
}

void ArcSessionRunner::StartArcSession() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!restart_timer_.IsRunning());

  VLOG(1) << "Starting ARC instance";
  if (!arc_session_) {
    arc_session_ = factory_.Run();
    arc_session_->AddObserver(this);
  } else {
    DCHECK(arc_session_->IsForLoginScreen());
  }
  arc_session_->Start();
}

void ArcSessionRunner::RestartArcSession() {
  VLOG(0) << "Restarting ARC instance";
  // The order is important here. Call StartArcSession(), then notify observers.
  StartArcSession();
  for (auto& observer : observer_list_)
    observer.OnSessionRestarting();
}

void ArcSessionRunner::OnSessionReady() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(arc_session_);
  DCHECK(!restart_timer_.IsRunning());
  VLOG(0) << "ARC ready";
}

void ArcSessionRunner::OnSessionStopped(ArcStopReason stop_reason,
                                        bool was_running) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(arc_session_);
  DCHECK(!restart_timer_.IsRunning());

  VLOG(0) << "ARC stopped: " << stop_reason;

  // The observers should be agnostic to the existence of the limited-purpose
  // instance.
  const bool notify_observers = !arc_session_->IsForLoginScreen();

  arc_session_->RemoveObserver(this);
  arc_session_.reset();

  const bool restarting =
      IsRestartNeeded(run_requested_, stop_reason, was_running);
  if (restarting) {
    // There was a previous invocation and it crashed for some reason. Try
    // starting ARC instance later again.
    // Note that even |restart_delay_| is 0 (for testing), it needs to
    // PostTask, because observer callback may call RequestStart()/Stop().
    VLOG(0) << "ARC restarting";
    restart_timer_.Start(FROM_HERE, restart_delay_,
                         base::Bind(&ArcSessionRunner::RestartArcSession,
                                    weak_ptr_factory_.GetWeakPtr()));
  }

  if (notify_observers) {
    for (auto& observer : observer_list_)
      observer.OnSessionStopped(stop_reason, restarting);
  }
}

void ArcSessionRunner::EmitLoginPromptVisibleCalled() {
  if (ShouldArcOnlyStartAfterLogin()) {
    // Skip starting ARC for now. We'll have another chance to start the full
    // instance after the user logs in.
    return;
  }
  // Since 'login-prompt-visible' Upstart signal starts all Upstart jobs the
  // container may depend on such as cras, EmitLoginPromptVisibleCalled() is the
  // safe place to start the container for login screen.
  DCHECK(!arc_session_);
  arc_session_ = factory_.Run();
  arc_session_->AddObserver(this);
  arc_session_->StartForLoginScreen();
}

}  // namespace arc
