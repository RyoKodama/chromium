// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_device_client.h"

#include "content/public/browser/browser_thread.h"
#include "device/usb/usb_service.h"

#if !defined(OS_ANDROID)
#include "device/hid/hid_service.h"
#endif  // !defined(OS_ANDROID)

using content::BrowserThread;

ChromeDeviceClient::ChromeDeviceClient() = default;

ChromeDeviceClient::~ChromeDeviceClient() = default;

device::UsbService* ChromeDeviceClient::GetUsbService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!usb_service_)
    usb_service_ = device::UsbService::Create();
  return usb_service_.get();
}

device::HidService* ChromeDeviceClient::GetHidService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if defined(OS_ANDROID)
  return nullptr;
#else
  if (!hid_service_)
    hid_service_ = device::HidService::Create();
  return hid_service_.get();
#endif  // defined(OS_ANDROID)
}
