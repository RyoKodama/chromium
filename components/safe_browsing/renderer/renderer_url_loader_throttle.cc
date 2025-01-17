// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/renderer/renderer_url_loader_throttle.h"

#include "base/logging.h"
#include "components/safe_browsing/common/utils.h"
#include "content/public/common/resource_request.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "net/url_request/redirect_info.h"

namespace safe_browsing {

RendererURLLoaderThrottle::RendererURLLoaderThrottle(
    mojom::SafeBrowsing* safe_browsing,
    int render_frame_id)
    : safe_browsing_(safe_browsing),
      render_frame_id_(render_frame_id),
      weak_factory_(this) {}

RendererURLLoaderThrottle::~RendererURLLoaderThrottle() = default;

void RendererURLLoaderThrottle::DetachFromCurrentSequence() {
  // Create a new pipe to the SafeBrowsing interface that can be bound to a
  // different sequence.
  safe_browsing_->Clone(mojo::MakeRequest(&safe_browsing_ptr_info_));
  safe_browsing_ = nullptr;
}

void RendererURLLoaderThrottle::WillStartRequest(
    const content::ResourceRequest& request,
    bool* defer) {
  DCHECK_EQ(0u, pending_checks_);
  DCHECK(!blocked_);
  DCHECK(!url_checker_);

  if (safe_browsing_ptr_info_.is_valid()) {
    // Bind the pipe created in DetachFromCurrentSequence to the current
    // sequence.
    safe_browsing_ptr_.Bind(std::move(safe_browsing_ptr_info_));
    safe_browsing_ = safe_browsing_ptr_.get();
  }

  pending_checks_++;
  // Use a weak pointer to self because |safe_browsing_| may not be owned by
  // this object.
  net::HttpRequestHeaders headers;
  headers.CopyFrom(request.headers);
  safe_browsing_->CreateCheckerAndCheck(
      render_frame_id_, mojo::MakeRequest(&url_checker_), request.url,
      request.method, headers, request.load_flags, request.resource_type,
      request.has_user_gesture,
      base::BindOnce(&RendererURLLoaderThrottle::OnCheckUrlResult,
                     weak_factory_.GetWeakPtr()));
  safe_browsing_ = nullptr;

  url_checker_.set_connection_error_handler(base::BindOnce(
      &RendererURLLoaderThrottle::OnConnectionError, base::Unretained(this)));
}

void RendererURLLoaderThrottle::WillRedirectRequest(
    const net::RedirectInfo& redirect_info,
    bool* defer) {
  // If |blocked_| is true, the resource load has been canceled and there
  // shouldn't be such a notification.
  DCHECK(!blocked_);

  if (!url_checker_) {
    DCHECK_EQ(0u, pending_checks_);
    return;
  }

  pending_checks_++;
  url_checker_->CheckUrl(
      redirect_info.new_url, redirect_info.new_method,
      base::BindOnce(&RendererURLLoaderThrottle::OnCheckUrlResult,
                     base::Unretained(this)));
}

void RendererURLLoaderThrottle::WillProcessResponse(bool* defer) {
  // If |blocked_| is true, the resource load has been canceled and there
  // shouldn't be such a notification.
  DCHECK(!blocked_);

  if (pending_checks_ == 0) {
    LogDelay(base::TimeDelta());
    return;
  }

  DCHECK(!deferred_);
  deferred_ = true;
  defer_start_time_ = base::TimeTicks::Now();
  *defer = true;
}

void RendererURLLoaderThrottle::OnCompleteCheck(bool proceed,
                                                bool showed_interstitial) {
  if (blocked_ || !url_checker_)
    return;

  DCHECK_LT(0u, pending_checks_);
  pending_checks_--;

  if (proceed) {
    if (pending_checks_ == 0 && deferred_) {
      LogDelay(base::TimeTicks::Now() - defer_start_time_);
      deferred_ = false;
      delegate_->Resume();
    }
  } else {
    url_checker_.reset();
    blocked_ = true;
    pending_checks_ = 0;
    delegate_->CancelWithError(net::ERR_ABORTED);
  }
}

void RendererURLLoaderThrottle::OnCheckUrlResult(
    mojom::UrlCheckNotifierRequest slow_check_notifier,
    bool proceed,
    bool showed_interstitial) {
  if (!slow_check_notifier.is_pending()) {
    OnCompleteCheck(proceed, showed_interstitial);
    return;
  }

  // TODO(yzshen): Notify the network service to pause processing response body.
  if (!notifier_bindings_) {
    notifier_bindings_ =
        std::make_unique<mojo::BindingSet<mojom::UrlCheckNotifier>>();
  }
  notifier_bindings_->AddBinding(this, std::move(slow_check_notifier));
}

void RendererURLLoaderThrottle::OnConnectionError() {
  DCHECK(!blocked_);

  // If a service-side disconnect happens, treat all URLs as if they are safe.
  url_checker_.reset();
  pending_checks_ = 0;
  notifier_bindings_.reset();

  if (deferred_) {
    deferred_ = false;
    LogDelay(base::TimeTicks::Now() - defer_start_time_);
    delegate_->Resume();
  }
}

}  // namespace safe_browsing
