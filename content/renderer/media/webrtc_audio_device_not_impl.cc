// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_audio_device_not_impl.h"

namespace content {

WebRtcAudioDeviceNotImpl::WebRtcAudioDeviceNotImpl()
    : last_process_time_(base::TimeTicks::Now()) {
}

int64_t WebRtcAudioDeviceNotImpl::TimeUntilNextProcess() {
  const int64_t kMillisecondsBetweenProcessCalls = 5000;
  base::TimeDelta delta_time = (base::TimeTicks::Now() - last_process_time_);
  return kMillisecondsBetweenProcessCalls - delta_time.InMilliseconds();
}

void WebRtcAudioDeviceNotImpl::Process() {
  last_process_time_ = base::TimeTicks::Now();
}

int32_t WebRtcAudioDeviceNotImpl::RegisterEventObserver(
    webrtc::AudioDeviceObserver* event_callback) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::ActiveAudioLayer(
    AudioLayer* audio_layer) const {
  return 0;
}

webrtc::AudioDeviceModule::ErrorCode
WebRtcAudioDeviceNotImpl::LastError() const {
  return AudioDeviceModule::kAdmErrNone;
}

int16_t WebRtcAudioDeviceNotImpl::PlayoutDevices() {
  return 0;
}

int16_t WebRtcAudioDeviceNotImpl::RecordingDevices() {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::PlayoutDeviceName(
    uint16_t index, char name[webrtc::kAdmMaxDeviceNameSize],
    char guid[webrtc::kAdmMaxGuidSize]) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::RecordingDeviceName(
    uint16_t index, char name[webrtc::kAdmMaxDeviceNameSize],
    char guid[webrtc::kAdmMaxGuidSize]) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SetPlayoutDevice(uint16_t index) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SetPlayoutDevice(WindowsDeviceType device) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SetRecordingDevice(uint16_t index) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SetRecordingDevice(WindowsDeviceType device) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::InitPlayout() {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::InitRecording() {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::InitSpeaker() {
  return 0;
}

bool WebRtcAudioDeviceNotImpl::SpeakerIsInitialized() const {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::InitMicrophone() {
  return 0;
}

bool WebRtcAudioDeviceNotImpl::MicrophoneIsInitialized() const {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SpeakerVolumeIsAvailable(bool* available) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SetSpeakerVolume(uint32_t volume) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SpeakerVolume(uint32_t* volume) const {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::MaxSpeakerVolume(uint32_t* max_volume) const {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::MinSpeakerVolume(uint32_t* min_volume) const {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::MicrophoneVolumeIsAvailable(bool* available) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SpeakerMuteIsAvailable(bool* available) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SetSpeakerMute(bool enable) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SpeakerMute(bool* enabled) const {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::MicrophoneMuteIsAvailable(bool* available) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SetMicrophoneMute(bool enable) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::MicrophoneMute(bool* enabled) const {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SetStereoPlayout(bool enable) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::StereoPlayout(bool* enabled) const {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SetStereoRecording(bool enable) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::StereoRecording(bool* enabled) const {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SetRecordingChannel(
    const ChannelType channel) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::RecordingChannel(ChannelType* channel) const {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SetRecordingSampleRate(
    const uint32_t samples_per_sec) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SetPlayoutSampleRate(
    const uint32_t samples_per_sec) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SetLoudspeakerStatus(bool enable) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::GetLoudspeakerStatus(bool* enabled) const {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SetAGC(bool enable) {
  return 0;
}

bool WebRtcAudioDeviceNotImpl::AGC() const {
  return true;
}

bool WebRtcAudioDeviceNotImpl::BuiltInAECIsAvailable() const {
  return false;
}

int32_t WebRtcAudioDeviceNotImpl::EnableBuiltInAEC(bool enable) {
  return 0;
}

bool WebRtcAudioDeviceNotImpl::BuiltInAGCIsAvailable() const {
  return false;
}

int32_t WebRtcAudioDeviceNotImpl::EnableBuiltInAGC(bool enable) {
  return 0;
}

bool WebRtcAudioDeviceNotImpl::BuiltInNSIsAvailable() const {
  return false;
}

int32_t WebRtcAudioDeviceNotImpl::EnableBuiltInNS(bool enable) {
  return 0;
}

#if defined(OS_IOS)
int WebRtcAudioDeviceNotImpl::GetPlayoutAudioParameters(
    AudioParameters* params) const {
  return 0;
}

int WebRtcAudioDeviceNotImpl::GetRecordAudioParameters(
    AudioParameters* params) const {
  return 0;
}
#endif  // OS_IOS

}  // namespace content
