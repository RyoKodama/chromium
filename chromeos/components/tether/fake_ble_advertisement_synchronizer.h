// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_FAKE_BLE_ADVERTISEMENT_SYNCHRONIZER_H_
#define CHROMEOS_COMPONENTS_TETHER_FAKE_BLE_ADVERTISEMENT_SYNCHRONIZER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chromeos/components/tether/ble_advertisement_synchronizer.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_advertisement.h"

namespace chromeos {

namespace tether {

// Test double for BleAdvertisementSynchronizer.
class FakeBleAdvertisementSynchronizer : public BleAdvertisementSynchronizer {
 public:
  FakeBleAdvertisementSynchronizer();
  ~FakeBleAdvertisementSynchronizer() override;

  size_t GetNumCommands();

  device::BluetoothAdvertisement::Data& GetAdvertisementData(size_t index);
  const device::BluetoothAdapter::CreateAdvertisementCallback&
  GetRegisterCallback(size_t index);
  const device::BluetoothAdapter::AdvertisementErrorCallback&
  GetRegisterErrorCallback(size_t index);

  const device::BluetoothAdvertisement::SuccessCallback& GetUnregisterCallback(
      size_t index);
  const device::BluetoothAdvertisement::ErrorCallback&
  GetUnregisterErrorCallback(size_t index);

 protected:
  void ProcessQueue() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeBleAdvertisementSynchronizer);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_FAKE_BLE_ADVERTISEMENT_SYNCHRONIZER_H_
