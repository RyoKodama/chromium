// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager_android.h"

#include "base/android/build_info.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/stringprintf.h"
#include "jni/MidiManagerAndroid_jni.h"
#include "media/midi/midi_device_android.h"
#include "media/midi/midi_manager_usb.h"
#include "media/midi/midi_output_port_android.h"
#include "media/midi/midi_service.h"
#include "media/midi/midi_switches.h"
#include "media/midi/task_service.h"
#include "media/midi/usb_midi_device_factory_android.h"

using base::android::JavaParamRef;
using midi::mojom::PortState;
using midi::mojom::Result;

namespace midi {

namespace {

// MidiManagerAndroid should be enabled only when the feature is enabled via
// chrome://flags on M+, or enabled by server configurations under specified
// Android versions, M+ or N+.
bool IsMidiManagerAndroidEnabled() {
  // The feature is not enabled by chrome://flags or field trials.
  if (!base::FeatureList::IsEnabled(features::kMidiManagerAndroid))
    return false;

  auto sdk_version = base::android::BuildInfo::GetInstance()->sdk_int();
  if (sdk_version < base::android::SDK_VERSION_MARSHMALLOW)
    return false;

  bool has_midi = Java_MidiManagerAndroid_hasSystemFeatureMidi(
      base::android::AttachCurrentThread());

  // If the feature is enabled, check the RequredAndroidVersion param. If the
  // param is provided and the value is "NOUGAT", use MidiManagerAndroid on N
  // and later versions. This string comparison should not match when users
  // enable the feature via chrome://flags.
  if (base::GetFieldTrialParamValueByFeature(features::kMidiManagerAndroid,
                                             "RequiredAndroidVersion") ==
      "NOUGAT") {
    return has_midi && sdk_version >= base::android::SDK_VERSION_NOUGAT;
  }

  // Otherwise, allow to use MidiManagerAndroid on M and later versions.
  return has_midi && sdk_version >= base::android::SDK_VERSION_MARSHMALLOW;
}

}  // namespace

MidiManager* MidiManager::Create(MidiService* service) {
  if (IsMidiManagerAndroidEnabled())
    return new MidiManagerAndroid(service);

  return new MidiManagerUsb(service,
                            std::make_unique<UsbMidiDeviceFactoryAndroid>());
}

MidiManagerAndroid::MidiManagerAndroid(MidiService* service)
    : MidiManager(service) {}

MidiManagerAndroid::~MidiManagerAndroid() {
  bool result = service()->task_service()->UnbindInstance();
  DCHECK(result);

  // TODO(toyoshim): Remove following code once the dynamic instantiation mode
  // is enabled by default.
  base::AutoLock lock(lock_);
  DCHECK(devices_.empty());
  DCHECK(all_input_ports_.empty());
  DCHECK(all_output_ports_.empty());
  DCHECK(raw_manager_.is_null());
}

void MidiManagerAndroid::StartInitialization() {
  bool result = service()->task_service()->BindInstance();
  DCHECK(result);

  JNIEnv* env = base::android::AttachCurrentThread();

  uintptr_t pointer = reinterpret_cast<uintptr_t>(this);
  raw_manager_.Reset(Java_MidiManagerAndroid_create(env, pointer));

  Java_MidiManagerAndroid_initialize(env, raw_manager_);
}

void MidiManagerAndroid::Finalize() {
  bool result = service()->task_service()->UnbindInstance();
  DCHECK(result);

  // TODO(toyoshim): Remove following code once the dynamic instantiation mode
  // is enabled by default.
  base::AutoLock lock(lock_);
  devices_.clear();
  input_port_to_index_.clear();
  output_port_to_index_.clear();
  raw_manager_.Reset();
}

void MidiManagerAndroid::DispatchSendMidiData(MidiManagerClient* client,
                                              uint32_t port_index,
                                              const std::vector<uint8_t>& data,
                                              double timestamp) {
  if (port_index >= all_output_ports_.size()) {
    // |port_index| is provided by a renderer so we can't believe that it is
    // in the valid range.
    return;
  }
  DCHECK_EQ(output_ports().size(), all_output_ports_.size());
  if (output_ports()[port_index].state == PortState::CONNECTED) {
    // We treat send call as implicit open.
    // TODO(yhirano): Implement explicit open operation from the renderer.
    if (all_output_ports_[port_index]->Open()) {
      SetOutputPortState(port_index, PortState::OPENED);
    } else {
      // We cannot open the port. It's useless to send data to such a port.
      return;
    }
  }

  // output_streams_[port_index] is alive unless MidiManagerAndroid is deleted.
  // The task posted to the TaskService will be disposed safely after unbinding
  // the service.
  base::TimeDelta delay = MidiService::TimestampToTimeDeltaDelay(timestamp);
  service()->task_service()->PostBoundDelayedTask(
      TaskService::kDefaultRunnerId,
      base::BindOnce(&MidiOutputPortAndroid::Send,
                     base::Unretained(all_output_ports_[port_index]), data),
      delay);
  service()->task_service()->PostBoundDelayedTask(
      TaskService::kDefaultRunnerId,
      base::BindOnce(&MidiManager::AccumulateMidiBytesSent,
                     base::Unretained(this), client, data.size()),
      delay);
}

void MidiManagerAndroid::OnReceivedData(MidiInputPortAndroid* port,
                                        const uint8_t* data,
                                        size_t size,
                                        base::TimeTicks timestamp) {
  const auto i = input_port_to_index_.find(port);
  DCHECK(input_port_to_index_.end() != i);
  ReceiveMidiData(i->second, data, size, timestamp);
}

void MidiManagerAndroid::OnInitialized(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller,
    const JavaParamRef<jobjectArray>& devices) {
  jsize length = env->GetArrayLength(devices);

  for (jsize i = 0; i < length; ++i) {
    base::android::ScopedJavaLocalRef<jobject> raw_device(
        env, env->GetObjectArrayElement(devices, i));
    AddDevice(std::make_unique<MidiDeviceAndroid>(env, raw_device, this));
  }
  CompleteInitialization(Result::OK);
}

void MidiManagerAndroid::OnInitializationFailed(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller) {
  CompleteInitialization(Result::INITIALIZATION_ERROR);
}

void MidiManagerAndroid::OnAttached(JNIEnv* env,
                                    const JavaParamRef<jobject>& caller,
                                    const JavaParamRef<jobject>& raw_device) {
  AddDevice(std::make_unique<MidiDeviceAndroid>(env, raw_device, this));
}

void MidiManagerAndroid::OnDetached(JNIEnv* env,
                                    const JavaParamRef<jobject>& caller,
                                    const JavaParamRef<jobject>& raw_device) {
  for (auto& device : devices_) {
    if (device->HasRawDevice(env, raw_device)) {
      for (auto& port : device->input_ports()) {
        DCHECK(input_port_to_index_.end() !=
               input_port_to_index_.find(port.get()));
        size_t index = input_port_to_index_[port.get()];
        SetInputPortState(index, PortState::DISCONNECTED);
      }
      for (auto& port : device->output_ports()) {
        DCHECK(output_port_to_index_.end() !=
               output_port_to_index_.find(port.get()));
        size_t index = output_port_to_index_[port.get()];
        SetOutputPortState(index, PortState::DISCONNECTED);
      }
    }
  }
}

void MidiManagerAndroid::AddDevice(std::unique_ptr<MidiDeviceAndroid> device) {
  for (auto& port : device->input_ports()) {
    // We implicitly open input ports here, because there are no signal
    // from the renderer when to open.
    // TODO(yhirano): Implement open operation in Blink.
    PortState state = port->Open() ? PortState::OPENED : PortState::CONNECTED;

    const size_t index = all_input_ports_.size();
    all_input_ports_.push_back(port.get());
    // Port ID must be unique in a MIDI manager. This ID setting is
    // sufficiently unique although there is no user-friendly meaning.
    // TODO(yhirano): Use a hashed string as ID.
    const std::string id(
        base::StringPrintf("native:port-in-%ld", static_cast<long>(index)));

    input_port_to_index_.insert(std::make_pair(port.get(), index));
    AddInputPort(MidiPortInfo(id, device->GetManufacturer(),
                              device->GetProductName(),
                              device->GetDeviceVersion(), state));
  }
  for (auto& port : device->output_ports()) {
    const size_t index = all_output_ports_.size();
    all_output_ports_.push_back(port.get());

    // Port ID must be unique in a MIDI manager. This ID setting is
    // sufficiently unique although there is no user-friendly meaning.
    // TODO(yhirano): Use a hashed string as ID.
    const std::string id(
        base::StringPrintf("native:port-out-%ld", static_cast<long>(index)));

    output_port_to_index_.insert(std::make_pair(port.get(), index));
    AddOutputPort(
        MidiPortInfo(id, device->GetManufacturer(), device->GetProductName(),
                     device->GetDeviceVersion(), PortState::CONNECTED));
  }
  devices_.push_back(std::move(device));
}

}  // namespace midi
