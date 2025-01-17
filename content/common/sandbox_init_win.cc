// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/sandbox_init.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "content/common/sandbox_win.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandbox_type.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_types.h"

namespace content {

bool InitializeSandbox(sandbox::SandboxInterfaceInfo* sandbox_info) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  SandboxType sandbox_type = SandboxTypeFromCommandLine(command_line);
  sandbox::BrokerServices* broker_services = sandbox_info->broker_services;
  if (broker_services) {
    if (!InitBrokerServices(broker_services))
      return false;

    // IMPORTANT: This piece of code needs to run as early as possible in the
    // process because it will initialize the sandbox broker, which requires the
    // process to swap its window station. During this time all the UI will be
    // broken. This has to run before threads and windows are created.
    if (!IsUnsandboxedSandboxType(sandbox_type)) {
      // Precreate the desktop and window station used by the renderers.
      scoped_refptr<sandbox::TargetPolicy> policy =
          broker_services->CreatePolicy();
      sandbox::ResultCode result = policy->CreateAlternateDesktop(true);
      CHECK(sandbox::SBOX_ERROR_FAILED_TO_SWITCH_BACK_WINSTATION != result);
    }
    return true;
  }

  return IsUnsandboxedSandboxType(sandbox_type) ||
         InitTargetServices(sandbox_info->target_services);
}

}  // namespace content
