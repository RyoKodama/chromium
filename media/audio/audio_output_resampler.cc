// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_resampler.h"

#include <stdint.h>

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/audio/audio_output_proxy.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/limits.h"
#include "media/base/sample_rates.h"

namespace media {

class OnMoreDataConverter
    : public AudioOutputStream::AudioSourceCallback,
      public AudioConverter::InputCallback {
 public:
  OnMoreDataConverter(const AudioParameters& input_params,
                      const AudioParameters& output_params,
                      std::unique_ptr<AudioDebugRecorder> debug_recorder);
  ~OnMoreDataConverter() override;

  // AudioSourceCallback interface.
  int OnMoreData(base::TimeDelta delay,
                 base::TimeTicks delay_timestamp,
                 int prior_frames_skipped,
                 AudioBus* dest) override;
  void OnError() override;

  // Sets |source_callback_|.  If this is not a new object, then Stop() must be
  // called before Start().
  void Start(AudioOutputStream::AudioSourceCallback* callback);

  // Clears |source_callback_| and flushes the resampler.
  void Stop();

  bool started() const { return source_callback_ != nullptr; }

  bool error_occurred() const { return error_occurred_; }

 private:
  // AudioConverter::InputCallback implementation.
  double ProvideInput(AudioBus* audio_bus, uint32_t frames_delayed) override;

  // Ratio of input bytes to output bytes used to correct playback delay with
  // regard to buffering and resampling.
  const double io_ratio_;

  // Source callback.
  AudioOutputStream::AudioSourceCallback* source_callback_;

  // Last |delay| and |delay_timestamp| received via OnMoreData(). Used to
  // correct playback delay in ProvideInput() before calling |source_callback_|.
  base::TimeDelta current_delay_;
  base::TimeTicks current_delay_timestamp_;

  const int input_samples_per_second_;

  // Handles resampling, buffering, and channel mixing between input and output
  // parameters.
  AudioConverter audio_converter_;

  // True if OnError() was ever called.  Should only be read if the underlying
  // stream has been stopped.
  bool error_occurred_;

  // Information about input and output buffer sizes to be traced.
  const int input_buffer_size_;
  const int output_buffer_size_;

  // For audio debug recordings.
  std::unique_ptr<AudioDebugRecorder> debug_recorder_;

  DISALLOW_COPY_AND_ASSIGN(OnMoreDataConverter);
};

// Record UMA statistics for hardware output configuration.
static void RecordStats(const AudioParameters& output_params) {
  UMA_HISTOGRAM_EXACT_LINEAR("Media.HardwareAudioBitsPerChannel",
                             output_params.bits_per_sample(),
                             static_cast<int>(limits::kMaxBitsPerSample));
  UMA_HISTOGRAM_ENUMERATION(
      "Media.HardwareAudioChannelLayout", output_params.channel_layout(),
      CHANNEL_LAYOUT_MAX + 1);
  UMA_HISTOGRAM_EXACT_LINEAR("Media.HardwareAudioChannelCount",
                             output_params.channels(),
                             static_cast<int>(limits::kMaxChannels));

  AudioSampleRate asr;
  if (ToAudioSampleRate(output_params.sample_rate(), &asr)) {
    UMA_HISTOGRAM_ENUMERATION(
        "Media.HardwareAudioSamplesPerSecond", asr, kAudioSampleRateMax + 1);
  } else {
    UMA_HISTOGRAM_COUNTS(
        "Media.HardwareAudioSamplesPerSecondUnexpected",
        output_params.sample_rate());
  }
}

// Record UMA statistics for hardware output configuration after fallback.
static void RecordFallbackStats(const AudioParameters& output_params) {
  UMA_HISTOGRAM_BOOLEAN("Media.FallbackToHighLatencyAudioPath", true);
  UMA_HISTOGRAM_EXACT_LINEAR("Media.FallbackHardwareAudioBitsPerChannel",
                             output_params.bits_per_sample(),
                             static_cast<int>(limits::kMaxBitsPerSample));
  UMA_HISTOGRAM_ENUMERATION(
      "Media.FallbackHardwareAudioChannelLayout",
      output_params.channel_layout(), CHANNEL_LAYOUT_MAX + 1);
  UMA_HISTOGRAM_EXACT_LINEAR("Media.FallbackHardwareAudioChannelCount",
                             output_params.channels(),
                             static_cast<int>(limits::kMaxChannels));

  AudioSampleRate asr;
  if (ToAudioSampleRate(output_params.sample_rate(), &asr)) {
    UMA_HISTOGRAM_ENUMERATION(
        "Media.FallbackHardwareAudioSamplesPerSecond",
        asr, kAudioSampleRateMax + 1);
  } else {
    UMA_HISTOGRAM_COUNTS(
        "Media.FallbackHardwareAudioSamplesPerSecondUnexpected",
        output_params.sample_rate());
  }
}

// Record UMA statistics for input/output rebuffering.
static void RecordRebufferingStats(const AudioParameters& input_params,
                                   const AudioParameters& output_params) {
  const int input_buffer_size = input_params.frames_per_buffer();
  const int output_buffer_size = output_params.frames_per_buffer();
  DCHECK_NE(0, input_buffer_size);
  DCHECK_NE(0, output_buffer_size);

  // Buffer size mismatch; see Media.Audio.Render.BrowserCallbackRegularity
  // histogram for explanation.
  int value = 0;
  if (input_buffer_size >= output_buffer_size) {
    // 0 if input size is a multiple of output size; otherwise -1.
    value = (input_buffer_size % output_buffer_size) ? -1 : 0;
  } else {
    value = (output_buffer_size / input_buffer_size - 1) * 2;
    if (output_buffer_size % input_buffer_size) {
      // One more callback is issued periodically.
      value += 1;
    }
  }

  const int value_cap = (4096 / 128 - 1) * 2 + 1;
  if (value > value_cap)
    value = value_cap;

  switch (input_params.latency_tag()) {
    case AudioLatency::LATENCY_EXACT_MS:
      UMA_HISTOGRAM_SPARSE_SLOWLY(
          "Media.Audio.Render.BrowserCallbackRegularity.LatencyExactMs", value);
      return;
    case AudioLatency::LATENCY_INTERACTIVE:
      UMA_HISTOGRAM_SPARSE_SLOWLY(
          "Media.Audio.Render.BrowserCallbackRegularity.LatencyInteractive",
          value);
      return;
    case AudioLatency::LATENCY_RTC:
      UMA_HISTOGRAM_SPARSE_SLOWLY(
          "Media.Audio.Render.BrowserCallbackRegularity.LatencyRtc", value);
      return;
    case AudioLatency::LATENCY_PLAYBACK:
      UMA_HISTOGRAM_SPARSE_SLOWLY(
          "Media.Audio.Render.BrowserCallbackRegularity.LatencyPlayback",
          value);
      return;
    default:
      DVLOG(1) << "Latency tag is not set";
  }
}

// Converts low latency based |output_params| into high latency appropriate
// output parameters in error situations.
void AudioOutputResampler::SetupFallbackParams() {
// Only Windows has a high latency output driver that is not the same as the low
// latency path.
#if defined(OS_WIN)
  // Choose AudioParameters appropriate for opening the device in high latency
  // mode.  |kMinLowLatencyFrameSize| is arbitrarily based on Pepper Flash's
  // MAXIMUM frame size for low latency.
  static const int kMinLowLatencyFrameSize = 2048;
  const int frames_per_buffer =
      std::max(params_.frames_per_buffer(), kMinLowLatencyFrameSize);

  output_params_ = AudioParameters(
      AudioParameters::AUDIO_PCM_LINEAR, params_.channel_layout(),
      params_.sample_rate(), params_.bits_per_sample(),
      frames_per_buffer);
  device_id_ = "";
  Initialize();
#endif
}

AudioOutputResampler::AudioOutputResampler(
    AudioManager* audio_manager,
    const AudioParameters& input_params,
    const AudioParameters& output_params,
    const std::string& output_device_id,
    base::TimeDelta close_delay,
    const RegisterDebugRecordingSourceCallback&
        register_debug_recording_source_callback)
    : AudioOutputDispatcher(audio_manager, input_params, output_device_id),
      close_delay_(close_delay),
      output_params_(output_params),
      original_output_params_(output_params),
      streams_opened_(false),
      reinitialize_timer_(FROM_HERE,
                          close_delay_,
                          base::Bind(&AudioOutputResampler::Reinitialize,
                                     base::Unretained(this)),
                          false),
      register_debug_recording_source_callback_(
          register_debug_recording_source_callback),
      weak_factory_(this) {
  DCHECK(input_params.IsValid());
  DCHECK(output_params.IsValid());
  DCHECK_EQ(output_params_.format(), AudioParameters::AUDIO_PCM_LOW_LATENCY);
  DCHECK(register_debug_recording_source_callback_);

  // Record UMA statistics for the hardware configuration.
  RecordStats(output_params);

  Initialize();
}

AudioOutputResampler::~AudioOutputResampler() {
  for (const auto& item : callbacks_) {
    if (item.second->started())
      StopStreamInternal(item);
  }
}

void AudioOutputResampler::Reinitialize() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(streams_opened_);

  // We can only reinitialize the dispatcher if it has no active proxies. Check
  // if one has been created since the reinitialization timer was started.
  if (dispatcher_->HasOutputProxies())
    return;

  // Log a trace event so we can get feedback in the field when this happens.
  TRACE_EVENT0("audio", "AudioOutputResampler::Reinitialize");

  output_params_ = original_output_params_;
  streams_opened_ = false;
  Initialize();
}

void AudioOutputResampler::Initialize() {
  DCHECK(!streams_opened_);
  DCHECK(callbacks_.empty());
  dispatcher_ = base::MakeUnique<AudioOutputDispatcherImpl>(
      audio_manager_, output_params_, device_id_, close_delay_);
}

AudioOutputProxy* AudioOutputResampler::CreateStreamProxy() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return new AudioOutputProxy(weak_factory_.GetWeakPtr());
}

bool AudioOutputResampler::OpenStream() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (dispatcher_->OpenStream()) {
    // Only record the UMA statistic if we didn't fallback during construction
    // and only for the first stream we open.
    if (!streams_opened_ &&
        output_params_.format() == AudioParameters::AUDIO_PCM_LOW_LATENCY) {
      UMA_HISTOGRAM_BOOLEAN("Media.FallbackToHighLatencyAudioPath", false);
    }
    streams_opened_ = true;
    return true;
  }

  // If we've already tried to open the stream in high latency mode or we've
  // successfully opened a stream previously, there's nothing more to be done.
  if (output_params_.format() != AudioParameters::AUDIO_PCM_LOW_LATENCY ||
      streams_opened_ || !callbacks_.empty()) {
    return false;
  }

  DCHECK_EQ(output_params_.format(), AudioParameters::AUDIO_PCM_LOW_LATENCY);

  // Record UMA statistics about the hardware which triggered the failure so
  // we can debug and triage later.
  RecordFallbackStats(output_params_);

  // Only Windows has a high latency output driver that is not the same as the
  // low latency path.
#if defined(OS_WIN)
  DLOG(ERROR) << "Unable to open audio device in low latency mode.  Falling "
              << "back to high latency audio output.";

  SetupFallbackParams();
  if (dispatcher_->OpenStream()) {
    streams_opened_ = true;
    return true;
  }
#endif

  DLOG(ERROR) << "Unable to open audio device in high latency mode.  Falling "
              << "back to fake audio output.";

  // Finally fall back to a fake audio output device.
  output_params_ = params_;
  output_params_.set_format(AudioParameters::AUDIO_FAKE);

  Initialize();
  if (dispatcher_->OpenStream()) {
    streams_opened_ = true;
    return true;
  }

  return false;
}

bool AudioOutputResampler::StartStream(
    AudioOutputStream::AudioSourceCallback* callback,
    AudioOutputProxy* stream_proxy) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  OnMoreDataConverter* resampler_callback = nullptr;
  CallbackMap::iterator it = callbacks_.find(stream_proxy);
  if (it == callbacks_.end()) {
    // If a register callback has been given, register and pass the returned
    // recoder to the converter. Data is fed to same recorder for the lifetime
    // of the converter, which is until the stream is closed.
    resampler_callback = new OnMoreDataConverter(
        params_, output_params_,
        register_debug_recording_source_callback_.Run(output_params_));
    callbacks_[stream_proxy] =
        base::WrapUnique<OnMoreDataConverter>(resampler_callback);
  } else {
    resampler_callback = it->second.get();
  }

  resampler_callback->Start(callback);
  bool result = dispatcher_->StartStream(resampler_callback, stream_proxy);
  if (!result)
    resampler_callback->Stop();
  return result;
}

void AudioOutputResampler::StreamVolumeSet(AudioOutputProxy* stream_proxy,
                                           double volume) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  dispatcher_->StreamVolumeSet(stream_proxy, volume);
}

void AudioOutputResampler::StopStream(AudioOutputProxy* stream_proxy) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  CallbackMap::iterator it = callbacks_.find(stream_proxy);
  DCHECK(it != callbacks_.end());
  StopStreamInternal(*it);
}

void AudioOutputResampler::CloseStream(AudioOutputProxy* stream_proxy) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  dispatcher_->CloseStream(stream_proxy);

  // We assume that StopStream() is always called prior to CloseStream(), so
  // that it is safe to delete the OnMoreDataConverter here.
  callbacks_.erase(stream_proxy);

  // Start the reinitialization timer if there are no active proxies and we're
  // not using the originally requested output parameters.  This allows us to
  // recover from transient output creation errors.
  if (!dispatcher_->HasOutputProxies() && callbacks_.empty() &&
      !output_params_.Equals(original_output_params_)) {
    reinitialize_timer_.Reset();
  }
}

void AudioOutputResampler::StopStreamInternal(
    const CallbackMap::value_type& item) {
  AudioOutputProxy* stream_proxy = item.first;
  OnMoreDataConverter* callback = item.second.get();
  DCHECK(callback->started());

  // Stop the underlying physical stream.
  dispatcher_->StopStream(stream_proxy);

  // Now that StopStream() has completed the underlying physical stream should
  // be stopped and no longer calling OnMoreData(), making it safe to Stop() the
  // OnMoreDataConverter.
  callback->Stop();

  // Destroy idle streams if any errors occurred during output; this ensures
  // bad streams will not be reused.  Note: Errors may occur during the Stop()
  // call above.
  if (callback->error_occurred())
    dispatcher_->CloseAllIdleStreams();
}

OnMoreDataConverter::OnMoreDataConverter(
    const AudioParameters& input_params,
    const AudioParameters& output_params,
    std::unique_ptr<AudioDebugRecorder> debug_recorder)
    : io_ratio_(static_cast<double>(input_params.GetBytesPerSecond()) /
                output_params.GetBytesPerSecond()),
      source_callback_(nullptr),
      input_samples_per_second_(input_params.sample_rate()),
      audio_converter_(input_params, output_params, false),
      error_occurred_(false),
      input_buffer_size_(input_params.frames_per_buffer()),
      output_buffer_size_(output_params.frames_per_buffer()),
      debug_recorder_(std::move(debug_recorder)) {
  RecordRebufferingStats(input_params, output_params);
}

OnMoreDataConverter::~OnMoreDataConverter() {
  // Ensure Stop() has been called so we don't end up with an AudioOutputStream
  // calling back into OnMoreData() after destruction.
  CHECK(!source_callback_);
}

void OnMoreDataConverter::Start(
    AudioOutputStream::AudioSourceCallback* callback) {
  CHECK(!source_callback_);
  source_callback_ = callback;

  // While AudioConverter can handle multiple inputs, we're using it only with
  // a single input currently.  Eventually this may be the basis for a browser
  // side mixer.
  audio_converter_.AddInput(this);
}

void OnMoreDataConverter::Stop() {
  CHECK(source_callback_);
  source_callback_ = nullptr;
  audio_converter_.RemoveInput(this);
}

int OnMoreDataConverter::OnMoreData(base::TimeDelta delay,
                                    base::TimeTicks delay_timestamp,
                                    int /* prior_frames_skipped */,
                                    AudioBus* dest) {
  TRACE_EVENT2("audio", "OnMoreDataConverter::OnMoreData", "input buffer size",
               input_buffer_size_, "output buffer size", output_buffer_size_);
  current_delay_ = delay;
  current_delay_timestamp_ = delay_timestamp;
  audio_converter_.Convert(dest);

  if (debug_recorder_)
    debug_recorder_->OnData(dest);

  // Always return the full number of frames requested, ProvideInput()
  // will pad with silence if it wasn't able to acquire enough data.
  return dest->frames();
}

double OnMoreDataConverter::ProvideInput(AudioBus* dest,
                                         uint32_t frames_delayed) {
  base::TimeDelta new_delay =
      current_delay_ + AudioTimestampHelper::FramesToTime(
                           frames_delayed, input_samples_per_second_);
  // Retrieve data from the original callback.
  const int frames = source_callback_->OnMoreData(
      new_delay, current_delay_timestamp_, 0, dest);

  // Zero any unfilled frames if anything was filled, otherwise we'll just
  // return a volume of zero and let AudioConverter drop the output.
  if (frames > 0 && frames < dest->frames())
    dest->ZeroFramesPartial(frames, dest->frames() - frames);
  return frames > 0 ? 1 : 0;
}

void OnMoreDataConverter::OnError() {
  error_occurred_ = true;
  source_callback_->OnError();
}

}  // namespace media
