// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/audio_decoder_wrapper.h"

#include <algorithm>

#include "base/logging.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_manager.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "chromecast/public/media/cast_decoder_buffer.h"

namespace chromecast {
namespace media {

AudioDecoderWrapper::AudioDecoderWrapper(
    MediaPipelineBackendManager* backend_manager,
    AudioDecoder* decoder,
    AudioContentType type,
    MediaPipelineBackendManager::BufferDelegate* buffer_delegate)
    : backend_manager_(backend_manager),
      decoder_(decoder),
      content_type_(type),
      buffer_delegate_(buffer_delegate),
      delegate_active_(false),
      global_volume_multiplier_(1.0f),
      stream_volume_multiplier_(1.0f) {
  DCHECK(backend_manager_);
  DCHECK(decoder_);

  backend_manager_->AddAudioDecoder(this);
}

AudioDecoderWrapper::~AudioDecoderWrapper() {
  backend_manager_->RemoveAudioDecoder(this);
}

void AudioDecoderWrapper::SetGlobalVolumeMultiplier(float multiplier) {
  global_volume_multiplier_ = multiplier;
  if (!delegate_active_) {
    decoder_->SetVolume(stream_volume_multiplier_ * global_volume_multiplier_);
  }
}

void AudioDecoderWrapper::SetDelegate(Delegate* delegate) {
  decoder_->SetDelegate(delegate);
}

MediaPipelineBackend::BufferStatus AudioDecoderWrapper::PushBuffer(
    CastDecoderBuffer* buffer) {
  if (buffer_delegate_ && buffer_delegate_->IsActive()) {
    // Mute the decoder, we are sending audio to delegate.
    if (!delegate_active_) {
      delegate_active_ = true;
      decoder_->SetVolume(0.0);
    }
    buffer_delegate_->OnPushBuffer(buffer);
  } else {
    // Restore original volume.
    if (delegate_active_) {
      delegate_active_ = false;
      if (!decoder_->SetVolume(stream_volume_multiplier_ *
                               global_volume_multiplier_)) {
        LOG(ERROR) << "SetVolume failed";
      }
    }
  }
  return decoder_->PushBuffer(buffer);
}

bool AudioDecoderWrapper::SetConfig(const AudioConfig& config) {
  if (buffer_delegate_) {
    buffer_delegate_->OnSetConfig(config);
  }
  return decoder_->SetConfig(config);
}

bool AudioDecoderWrapper::SetVolume(float multiplier) {
  stream_volume_multiplier_ = std::max(0.0f, std::min(multiplier, 1.0f));
  if (delegate_active_) {
    return true;
  }
  return decoder_->SetVolume(stream_volume_multiplier_ *
                             global_volume_multiplier_);
}

AudioDecoderWrapper::RenderingDelay AudioDecoderWrapper::GetRenderingDelay() {
  return decoder_->GetRenderingDelay();
}

void AudioDecoderWrapper::GetStatistics(Statistics* statistics) {
  decoder_->GetStatistics(statistics);
}

}  // namespace media
}  // namespace chromecast
