// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_U2F_COMMAND_TYPE_H_
#define DEVICE_U2F_U2F_COMMAND_TYPE_H_

#include <stdint.h>

namespace device {

// The type of a command that can be sent either to or from a U2F authenticator,
// i.e. a request or a response.
//
// Each request sent to a device results in a response of *the same* type sent
// back, unless there was an error in which case a CMD_ERROR is returned.
//
// TODO(pkalinnikov): Merge BLE-specific commands to this type.
enum class U2fCommandType : uint8_t {
  // Sends arbitrary data to the device which echoes the same data back.
  CMD_PING = 0x81,
  // Encapsulates a U2F protocol raw message.
  CMD_MSG = 0x83,
  // Requests a unique channel from a USB/HID device.
  CMD_INIT = 0x86,
  // Instructs a USB/HID authenticator to show the user that it is active.
  CMD_WINK = 0x88,
  // Used as a response in case an error occurs during a request.
  CMD_ERROR = 0xBF,
};

}  // namespace device

#endif  // DEVICE_U2F_U2F_COMMAND_TYPE_H_
