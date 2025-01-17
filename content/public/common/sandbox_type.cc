// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/sandbox_type.h"

#include <string>

#include "content/public/common/content_switches.h"

namespace content {

namespace {

// Must be in sync with "sandbox_type" value in mojo service manifest.json
// files.
const char kNoSandbox[] = "none";
const char kNetworkSandbox[] = "network";
const char kPpapiSandbox[] = "ppapi";
const char kUtilitySandbox[] = "utility";
const char kCdmSandbox[] = "cdm";

}  // namespace

void SetCommandLineFlagsForSandboxType(base::CommandLine* command_line,
                                       SandboxType sandbox_type) {
  switch (sandbox_type) {
    case SANDBOX_TYPE_NO_SANDBOX:
      command_line->AppendSwitch(switches::kNoSandbox);
      break;
    case SANDBOX_TYPE_RENDERER:
      DCHECK(command_line->GetSwitchValueASCII(switches::kProcessType) ==
             switches::kRendererProcess);
      break;
    case SANDBOX_TYPE_UTILITY:
      DCHECK(command_line->GetSwitchValueASCII(switches::kProcessType) ==
             switches::kUtilityProcess);
      DCHECK(!command_line->HasSwitch(switches::kUtilityProcessSandboxType));
      command_line->AppendSwitchASCII(switches::kUtilityProcessSandboxType,
                                      kUtilitySandbox);
      break;
    case SANDBOX_TYPE_GPU:
      DCHECK(command_line->GetSwitchValueASCII(switches::kProcessType) ==
             switches::kGpuProcess);
      break;
    case SANDBOX_TYPE_PPAPI:
      if (command_line->GetSwitchValueASCII(switches::kProcessType) ==
          switches::kUtilityProcess) {
        command_line->AppendSwitchASCII(switches::kUtilityProcessSandboxType,
                                        kPpapiSandbox);
      } else {
        DCHECK(command_line->GetSwitchValueASCII(switches::kProcessType) ==
               switches::kPpapiPluginProcess);
      }
      break;
    case SANDBOX_TYPE_NETWORK:
      DCHECK(command_line->GetSwitchValueASCII(switches::kProcessType) ==
             switches::kUtilityProcess);
      DCHECK(!command_line->HasSwitch(switches::kUtilityProcessSandboxType));
      command_line->AppendSwitchASCII(switches::kUtilityProcessSandboxType,
                                      kNetworkSandbox);
      break;
    case SANDBOX_TYPE_CDM:
      DCHECK(command_line->GetSwitchValueASCII(switches::kProcessType) ==
             switches::kUtilityProcess);
      DCHECK(!command_line->HasSwitch(switches::kUtilityProcessSandboxType));
      command_line->AppendSwitchASCII(switches::kUtilityProcessSandboxType,
                                      kCdmSandbox);
      break;
    default:
      break;
  }
}

SandboxType SandboxTypeFromCommandLine(const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kNoSandbox))
    return SANDBOX_TYPE_NO_SANDBOX;

  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  if (process_type.empty())
    return SANDBOX_TYPE_NO_SANDBOX;

  if (process_type == switches::kRendererProcess)
    return SANDBOX_TYPE_RENDERER;

  if (process_type == switches::kUtilityProcess) {
    return UtilitySandboxTypeFromString(
        command_line.GetSwitchValueASCII(switches::kUtilityProcessSandboxType));
  }
  if (process_type == switches::kGpuProcess) {
    if (command_line.HasSwitch(switches::kDisableGpuSandbox))
      return SANDBOX_TYPE_NO_SANDBOX;
    return SANDBOX_TYPE_GPU;
  }
  if (process_type == switches::kPpapiBrokerProcess)
    return SANDBOX_TYPE_NO_SANDBOX;

  if (process_type == switches::kPpapiPluginProcess)
    return SANDBOX_TYPE_PPAPI;

  // This is a process which we don't know about, i.e. an embedder-defined
  // process. If the embedder wants it sandboxed, they have a chance to return
  // the sandbox profile in ContentClient::GetSandboxProfileForSandboxType.
  return SANDBOX_TYPE_INVALID;
}

SandboxType UtilitySandboxTypeFromString(const std::string& sandbox_string) {
  if (sandbox_string == kNoSandbox)
    return SANDBOX_TYPE_NO_SANDBOX;
  if (sandbox_string == kNetworkSandbox)
    return SANDBOX_TYPE_NETWORK;
  if (sandbox_string == kPpapiSandbox)
    return SANDBOX_TYPE_PPAPI;
  if (sandbox_string == kCdmSandbox)
    return SANDBOX_TYPE_CDM;
  return SANDBOX_TYPE_UTILITY;
}

}  // namespace content
