// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/frame_processor.h"

#include <stdint.h>

#include <cstdlib>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/timestamp_constants.h"

namespace media {

const int kMaxDroppedPrerollWarnings = 10;
const int kMaxDtsBeyondPtsWarnings = 10;
const int kMaxAudioNonKeyframeWarnings = 10;
const int kMaxNumKeyframeTimeGreaterThanDependantWarnings = 1;
const int kMaxMuxedSequenceModeWarnings = 1;

// Helper class to capture per-track details needed by a frame processor. Some
// of this information may be duplicated in the short-term in the associated
// ChunkDemuxerStream and SourceBufferStream for a track.
// This parallels the MSE spec each of a SourceBuffer's Track Buffers at
// http://www.w3.org/TR/media-source/#track-buffers.
class MseTrackBuffer {
 public:
  MseTrackBuffer(ChunkDemuxerStream* stream,
                 MediaLog* media_log,
                 const SourceBufferParseWarningCB& parse_warning_cb);
  ~MseTrackBuffer();

  // Get/set |last_decode_timestamp_|.
  DecodeTimestamp last_decode_timestamp() const {
    return last_decode_timestamp_;
  }
  void set_last_decode_timestamp(DecodeTimestamp timestamp) {
    last_decode_timestamp_ = timestamp;
  }

  // Get/set |last_frame_duration_|.
  base::TimeDelta last_frame_duration() const {
    return last_frame_duration_;
  }
  void set_last_frame_duration(base::TimeDelta duration) {
    last_frame_duration_ = duration;
  }

  // Gets |highest_presentation_timestamp_|.
  base::TimeDelta highest_presentation_timestamp() const {
    return highest_presentation_timestamp_;
  }

  // Get/set |needs_random_access_point_|.
  bool needs_random_access_point() const {
    return needs_random_access_point_;
  }
  void set_needs_random_access_point(bool needs_random_access_point) {
    needs_random_access_point_ = needs_random_access_point;
  }

  DecodeTimestamp last_processed_decode_timestamp() const {
    return last_processed_decode_timestamp_;
  }

  // Gets a pointer to this track's ChunkDemuxerStream.
  ChunkDemuxerStream* stream() const { return stream_; }

  // Unsets |last_decode_timestamp_|, unsets |last_frame_duration_|,
  // unsets |highest_presentation_timestamp_|, and sets
  // |needs_random_access_point_| to true.
  void Reset();

  // If |highest_presentation_timestamp_| is unset or |timestamp| is greater
  // than |highest_presentation_timestamp_|, sets
  // |highest_presentation_timestamp_| to |timestamp|. Note that bidirectional
  // prediction between coded frames can cause |timestamp| to not be
  // monotonically increasing even though the decode timestamps are
  // monotonically increasing.
  void SetHighestPresentationTimestampIfIncreased(base::TimeDelta timestamp);

  // Adds |frame| to the end of |processed_frames_|.
  void EnqueueProcessedFrame(const scoped_refptr<StreamParserBuffer>& frame);

  // Appends |processed_frames_|, if not empty, to |stream_| and clears
  // |processed_frames_|. Returns false if append failed, true otherwise.
  // |processed_frames_| is cleared in both cases.
  bool FlushProcessedFrames();

  // Signals this track buffer's stream that a coded frame group is starting
  // with decode timestamp |start_timestamp|.
  void NotifyStartOfCodedFrameGroup(DecodeTimestamp start_time);

 private:
  // The decode timestamp of the last coded frame appended in the current coded
  // frame group. Initially kNoTimestamp, meaning "unset".
  DecodeTimestamp last_decode_timestamp_;

  // On signalling the stream of a new coded frame group start time, this is
  // reset to that start time. Any buffers subsequently enqueued for emission to
  // the stream update this. This is managed separately from
  // |last_decode_timestamp_| because |last_processed_decode_timestamp_| is not
  // reset during Reset(), to especially be able to track the need to signal
  // coded frame group start time for muxed post-discontiuity edge cases. See
  // also FrameProcessor::ProcessFrame().
  DecodeTimestamp last_processed_decode_timestamp_;

  // This is used to understand if the stream parser is producing random access
  // points that are not SAP Type 1, whose support is likely going to be
  // deprecated from MSE API pending real-world usage data. This is kNoTimestamp
  // if no frames have been enqueued ever or since the last
  // NotifyStartOfCodedFrameGroup() or Reset(). Otherwise, this is the most
  // recently enqueued keyframe's presentation timestamp.
  base::TimeDelta last_keyframe_presentation_timestamp_;

  // The coded frame duration of the last coded frame appended in the current
  // coded frame group. Initially kNoTimestamp, meaning "unset".
  base::TimeDelta last_frame_duration_;

  // The highest presentation timestamp encountered in a coded frame appended
  // in the current coded frame group. Initially kNoTimestamp, meaning
  // "unset".
  base::TimeDelta highest_presentation_timestamp_;

  // Keeps track of whether the track buffer is waiting for a random access
  // point coded frame. Initially set to true to indicate that a random access
  // point coded frame is needed before anything can be added to the track
  // buffer.
  bool needs_random_access_point_;

  // Pointer to the stream associated with this track. The stream is not owned
  // by |this|.
  ChunkDemuxerStream* const stream_;

  // Queue of processed frames that have not yet been appended to |stream_|.
  // EnqueueProcessedFrame() adds to this queue, and FlushProcessedFrames()
  // clears it.
  StreamParser::BufferQueue processed_frames_;

  // MediaLog for reporting messages and properties to debug content and engine.
  MediaLog* media_log_;

  // Callback for reporting problematic conditions that are not necessarily
  // errors.
  SourceBufferParseWarningCB parse_warning_cb_;

  // Counter that limits spam to |media_log_| for MseTrackBuffer warnings.
  int num_keyframe_time_greater_than_dependant_warnings_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MseTrackBuffer);
};

MseTrackBuffer::MseTrackBuffer(
    ChunkDemuxerStream* stream,
    MediaLog* media_log,
    const SourceBufferParseWarningCB& parse_warning_cb)
    : last_decode_timestamp_(kNoDecodeTimestamp()),
      last_processed_decode_timestamp_(DecodeTimestamp()),
      last_keyframe_presentation_timestamp_(kNoTimestamp),
      last_frame_duration_(kNoTimestamp),
      highest_presentation_timestamp_(kNoTimestamp),
      needs_random_access_point_(true),
      stream_(stream),
      media_log_(media_log),
      parse_warning_cb_(parse_warning_cb) {
  DCHECK(stream_);
  DCHECK(!parse_warning_cb_.is_null());
}

MseTrackBuffer::~MseTrackBuffer() {
  DVLOG(2) << __func__ << "()";
}

void MseTrackBuffer::Reset() {
  DVLOG(2) << __func__ << "()";

  last_decode_timestamp_ = kNoDecodeTimestamp();
  last_frame_duration_ = kNoTimestamp;
  highest_presentation_timestamp_ = kNoTimestamp;
  needs_random_access_point_ = true;
  last_keyframe_presentation_timestamp_ = kNoTimestamp;
}

void MseTrackBuffer::SetHighestPresentationTimestampIfIncreased(
    base::TimeDelta timestamp) {
  if (highest_presentation_timestamp_ == kNoTimestamp ||
      timestamp > highest_presentation_timestamp_) {
    highest_presentation_timestamp_ = timestamp;
  }
}

void MseTrackBuffer::EnqueueProcessedFrame(
    const scoped_refptr<StreamParserBuffer>& frame) {
  if (frame->is_key_frame()) {
    last_keyframe_presentation_timestamp_ = frame->timestamp();
  } else {
    DCHECK(last_keyframe_presentation_timestamp_ != kNoTimestamp);
    // This is just one case of potentially problematic GOP structures, though
    // others are more clearly disallowed in at least some of the MSE bytestream
    // specs, especially ISOBMFF. See https://crbug.com/739931 for more
    // information.
    if (frame->timestamp() < last_keyframe_presentation_timestamp_) {
      if (!num_keyframe_time_greater_than_dependant_warnings_) {
        // At most once per each track (but potentially multiple times per
        // playback, if there are more than one tracks that exhibit this
        // sequence in a playback) report a RAPPOR URL instance and also run the
        // warning's callback.
        media_log_->RecordRapporWithSecurityOrigin(
            "Media.OriginUrl.MSE.KeyframeTimeGreaterThanDependant");
        DCHECK(!parse_warning_cb_.is_null());
        parse_warning_cb_.Run(
            SourceBufferParseWarning::kKeyframeTimeGreaterThanDependant);
      }

      LIMITED_MEDIA_LOG(DEBUG, media_log_,
                        num_keyframe_time_greater_than_dependant_warnings_,
                        kMaxNumKeyframeTimeGreaterThanDependantWarnings)
          << "Warning: presentation time of most recently processed random "
             "access point ("
          << last_keyframe_presentation_timestamp_
          << ") is later than the presentation time of a non-keyframe ("
          << frame->timestamp()
          << ") that depends on it. This type of random access point is not "
             "well supported by MSE; buffered range reporting may be less "
             "precise.";
    }
  }

  last_processed_decode_timestamp_ = frame->GetDecodeTimestamp();
  processed_frames_.push_back(frame);
}

bool MseTrackBuffer::FlushProcessedFrames() {
  if (processed_frames_.empty())
    return true;

  bool result = stream_->Append(processed_frames_);
  processed_frames_.clear();

  DVLOG_IF(3, !result) << __func__
                       << "(): Failure appending processed frames to stream";

  return result;
}

void MseTrackBuffer::NotifyStartOfCodedFrameGroup(DecodeTimestamp start_time) {
  last_keyframe_presentation_timestamp_ = kNoTimestamp;
  last_processed_decode_timestamp_ = start_time;
  stream_->OnStartOfCodedFrameGroup(start_time);
}

FrameProcessor::FrameProcessor(const UpdateDurationCB& update_duration_cb,
                               MediaLog* media_log)
    : group_start_timestamp_(kNoTimestamp),
      update_duration_cb_(update_duration_cb),
      media_log_(media_log) {
  DVLOG(2) << __func__ << "()";
  DCHECK(!update_duration_cb.is_null());
}

FrameProcessor::~FrameProcessor() {
  DVLOG(2) << __func__ << "()";
}

void FrameProcessor::SetParseWarningCallback(
    const SourceBufferParseWarningCB& parse_warning_cb) {
  DCHECK(parse_warning_cb_.is_null());
  DCHECK(!parse_warning_cb.is_null());
  parse_warning_cb_ = parse_warning_cb;
}

void FrameProcessor::SetSequenceMode(bool sequence_mode) {
  DVLOG(2) << __func__ << "(" << sequence_mode << ")";
  // Per June 9, 2016 MSE spec editor's draft:
  // https://rawgit.com/w3c/media-source/d8f901f22/
  //     index.html#widl-SourceBuffer-mode
  // Step 7: If the new mode equals "sequence", then set the group start
  // timestamp to the group end timestamp.
  if (sequence_mode) {
    DCHECK(kNoTimestamp != group_end_timestamp_);
    group_start_timestamp_ = group_end_timestamp_;
  } else if (sequence_mode_) {
    // We're switching from 'sequence' to 'segments' mode. Be safe and signal a
    // new coded frame group on the next frame emitted.
    pending_notify_all_group_start_ = true;
  }

  // Step 8: Update the attribute to new mode.
  sequence_mode_ = sequence_mode;
}

bool FrameProcessor::ProcessFrames(
    const StreamParser::BufferQueueMap& buffer_queue_map,
    base::TimeDelta append_window_start,
    base::TimeDelta append_window_end,
    base::TimeDelta* timestamp_offset) {
  StreamParser::BufferQueue frames;
  if (!MergeBufferQueues(buffer_queue_map, &frames)) {
    MEDIA_LOG(ERROR, media_log_) << "Parsed buffers not in DTS sequence";
    return false;
  }

  DCHECK(!frames.empty());

  if (sequence_mode_ && track_buffers_.size() > 1) {
    if (!num_muxed_sequence_mode_warnings_) {
      // At most once per SourceBuffer (but potentially multiple times per
      // playback, if there are more than one SourceBuffers used this way in a
      // playback) report a RAPPOR URL instance and also run the warning's
      // callback.
      media_log_->RecordRapporWithSecurityOrigin(
          "Media.OriginUrl.MSE.MuxedSequenceModeSourceBuffer");
      DCHECK(!parse_warning_cb_.is_null());
      parse_warning_cb_.Run(SourceBufferParseWarning::kMuxedSequenceMode);
    }

    LIMITED_MEDIA_LOG(DEBUG, media_log_, num_muxed_sequence_mode_warnings_,
                      kMaxMuxedSequenceModeWarnings)
        << "Warning: using MSE 'sequence' AppendMode for a SourceBuffer with "
           "multiple tracks may cause loss of track synchronization. In some "
           "cases, buffered range gaps and playback stalls can occur. It is "
           "recommended to instead use 'segments' mode for a multitrack "
           "SourceBuffer.";
  }

  // Implements the coded frame processing algorithm's outer loop for step 1.
  // Note that ProcessFrame() implements an inner loop for a single frame that
  // handles "jump to the Loop Top step to restart processing of the current
  // coded frame" per June 9, 2016 MSE spec editor's draft:
  // https://rawgit.com/w3c/media-source/d8f901f22/
  //     index.html#sourcebuffer-coded-frame-processing
  // 1. For each coded frame in the media segment run the following steps:
  for (StreamParser::BufferQueue::const_iterator frames_itr = frames.begin();
       frames_itr != frames.end(); ++frames_itr) {
    if (!ProcessFrame(*frames_itr, append_window_start, append_window_end,
                      timestamp_offset)) {
      FlushProcessedFrames();
      return false;
    }
  }

  if (!FlushProcessedFrames())
    return false;

  // 2. - 4. Are handled by the WebMediaPlayer / Pipeline / Media Element.

  // 5. If the media segment contains data beyond the current duration, then run
  //    the duration change algorithm with new duration set to the maximum of
  //    the current duration and the group end timestamp.
  update_duration_cb_.Run(group_end_timestamp_);

  return true;
}

void FrameProcessor::SetGroupStartTimestampIfInSequenceMode(
    base::TimeDelta timestamp_offset) {
  DVLOG(2) << __func__ << "(" << timestamp_offset.InSecondsF() << ")";
  DCHECK(kNoTimestamp != timestamp_offset);
  if (sequence_mode_)
    group_start_timestamp_ = timestamp_offset;

  // Changes to timestampOffset should invalidate the preroll buffer.
  audio_preroll_buffer_ = NULL;
}

bool FrameProcessor::AddTrack(StreamParser::TrackId id,
                              ChunkDemuxerStream* stream) {
  DVLOG(2) << __func__ << "(): id=" << id;

  MseTrackBuffer* existing_track = FindTrack(id);
  DCHECK(!existing_track);
  if (existing_track) {
    MEDIA_LOG(ERROR, media_log_) << "Failure adding track with duplicate ID "
                                 << id;
    return false;
  }

  track_buffers_[id] =
      base::MakeUnique<MseTrackBuffer>(stream, media_log_, parse_warning_cb_);
  return true;
}

bool FrameProcessor::UpdateTrackIds(const TrackIdChanges& track_id_changes) {
  TrackBuffersMap& old_track_buffers = track_buffers_;
  TrackBuffersMap new_track_buffers;

  for (const auto& ids : track_id_changes) {
    if (old_track_buffers.find(ids.first) == old_track_buffers.end() ||
        new_track_buffers.find(ids.second) != new_track_buffers.end()) {
      MEDIA_LOG(ERROR, media_log_) << "Failure updating track id from "
                                   << ids.first << " to " << ids.second;
      return false;
    }
    new_track_buffers[ids.second] = std::move(old_track_buffers[ids.first]);
    CHECK_EQ(1u, old_track_buffers.erase(ids.first));
  }

  // Process remaining track buffers with unchanged ids.
  for (const auto& t : old_track_buffers) {
    if (new_track_buffers.find(t.first) != new_track_buffers.end()) {
      MEDIA_LOG(ERROR, media_log_) << "Track id " << t.first << " conflict";
      return false;
    }
    new_track_buffers[t.first] = std::move(old_track_buffers[t.first]);
  }

  std::swap(track_buffers_, new_track_buffers);
  return true;
}

void FrameProcessor::SetAllTrackBuffersNeedRandomAccessPoint() {
  for (auto itr = track_buffers_.begin(); itr != track_buffers_.end(); ++itr) {
    itr->second->set_needs_random_access_point(true);
  }
}

void FrameProcessor::Reset() {
  DVLOG(2) << __func__ << "()";
  for (auto itr = track_buffers_.begin(); itr != track_buffers_.end(); ++itr) {
    itr->second->Reset();
  }

  // Maintain current |pending_notify_all_group_start_| state for Reset() during
  // sequence mode. Reset it here only if in segments mode. In sequence mode,
  // the current coded frame group may be continued across Reset() operations to
  // allow the stream to coalesce what might otherwise be gaps in the buffered
  // ranges. See also the declaration for |pending_notify_all_group_start_|.
  if (!sequence_mode_) {
    pending_notify_all_group_start_ = true;
    return;
  }

  // Sequence mode
  DCHECK(kNoTimestamp != group_end_timestamp_);
  group_start_timestamp_ = group_end_timestamp_;
}

void FrameProcessor::OnPossibleAudioConfigUpdate(
    const AudioDecoderConfig& config) {
  DCHECK(config.IsValidConfig());

  // Always clear the preroll buffer when a config update is received.
  audio_preroll_buffer_ = NULL;

  if (config.Matches(current_audio_config_))
    return;

  current_audio_config_ = config;
  sample_duration_ = base::TimeDelta::FromSecondsD(
      1.0 / current_audio_config_.samples_per_second());
}

MseTrackBuffer* FrameProcessor::FindTrack(StreamParser::TrackId id) {
  auto itr = track_buffers_.find(id);
  if (itr == track_buffers_.end())
    return NULL;

  return itr->second.get();
}

void FrameProcessor::NotifyStartOfCodedFrameGroup(
    DecodeTimestamp start_timestamp) {
  DVLOG(2) << __func__ << "(" << start_timestamp.InSecondsF() << ")";

  for (auto itr = track_buffers_.begin(); itr != track_buffers_.end(); ++itr) {
    itr->second->NotifyStartOfCodedFrameGroup(start_timestamp);
  }
}

bool FrameProcessor::FlushProcessedFrames() {
  DVLOG(2) << __func__ << "()";

  bool result = true;
  for (auto itr = track_buffers_.begin(); itr != track_buffers_.end(); ++itr) {
    if (!itr->second->FlushProcessedFrames())
      result = false;
  }

  return result;
}

bool FrameProcessor::HandlePartialAppendWindowTrimming(
    base::TimeDelta append_window_start,
    base::TimeDelta append_window_end,
    const scoped_refptr<StreamParserBuffer>& buffer) {
  DCHECK(buffer->duration() >= base::TimeDelta());
  DCHECK_EQ(DemuxerStream::AUDIO, buffer->type());
  DCHECK(buffer->is_key_frame());

  const base::TimeDelta frame_end_timestamp =
      buffer->timestamp() + buffer->duration();

  // If the buffer is entirely before |append_window_start|, save it as preroll
  // for the first buffer which overlaps |append_window_start|.
  if (buffer->timestamp() < append_window_start &&
      frame_end_timestamp <= append_window_start) {
    audio_preroll_buffer_ = buffer;
    return false;
  }

  // If the buffer is entirely after |append_window_end| there's nothing to do.
  if (buffer->timestamp() >= append_window_end)
    return false;

  DCHECK(buffer->timestamp() >= append_window_start ||
         frame_end_timestamp > append_window_start);

  bool processed_buffer = false;

  // If we have a preroll buffer see if we can attach it to the first buffer
  // overlapping or after |append_window_start|.
  if (audio_preroll_buffer_.get()) {
    // We only want to use the preroll buffer if it directly precedes (less
    // than one sample apart) the current buffer.
    const int64_t delta =
        (audio_preroll_buffer_->timestamp() +
         audio_preroll_buffer_->duration() - buffer->timestamp())
            .InMicroseconds();
    if (std::abs(delta) < sample_duration_.InMicroseconds()) {
      DVLOG(1) << "Attaching audio preroll buffer ["
               << audio_preroll_buffer_->timestamp().InSecondsF() << ", "
               << (audio_preroll_buffer_->timestamp() +
                   audio_preroll_buffer_->duration()).InSecondsF() << ") to "
               << buffer->timestamp().InSecondsF();
      buffer->SetPrerollBuffer(audio_preroll_buffer_);
      processed_buffer = true;
    } else {
      LIMITED_MEDIA_LOG(DEBUG, media_log_, num_dropped_preroll_warnings_,
                        kMaxDroppedPrerollWarnings)
          << "Partial append window trimming dropping unused audio preroll "
             "buffer with PTS "
          << audio_preroll_buffer_->timestamp().InMicroseconds()
          << "us that ends too far (" << delta
          << "us) from next buffer with PTS "
          << buffer->timestamp().InMicroseconds() << "us";
    }
    audio_preroll_buffer_ = NULL;
  }

  // See if a partial discard can be done around |append_window_start|.
  if (buffer->timestamp() < append_window_start) {
    DVLOG(1) << "Truncating buffer which overlaps append window start."
             << " presentation_timestamp " << buffer->timestamp().InSecondsF()
             << " frame_end_timestamp " << frame_end_timestamp.InSecondsF()
             << " append_window_start " << append_window_start.InSecondsF();

    // Mark the overlapping portion of the buffer for discard.
    buffer->set_discard_padding(std::make_pair(
        append_window_start - buffer->timestamp(), base::TimeDelta()));

    // Adjust the timestamp of this buffer forward to |append_window_start| and
    // decrease the duration to compensate. Adjust DTS by the same delta as PTS
    // to help prevent spurious discontinuities when DTS > PTS.
    base::TimeDelta pts_delta = append_window_start - buffer->timestamp();
    buffer->set_timestamp(append_window_start);
    buffer->SetDecodeTimestamp(buffer->GetDecodeTimestamp() + pts_delta);
    buffer->set_duration(frame_end_timestamp - append_window_start);
    processed_buffer = true;
  }

  // See if a partial discard can be done around |append_window_end|.
  if (frame_end_timestamp > append_window_end) {
    DVLOG(1) << "Truncating buffer which overlaps append window end."
             << " presentation_timestamp " << buffer->timestamp().InSecondsF()
             << " frame_end_timestamp " << frame_end_timestamp.InSecondsF()
             << " append_window_end " << append_window_end.InSecondsF();

    // Mark the overlapping portion of the buffer for discard.
    buffer->set_discard_padding(
        std::make_pair(buffer->discard_padding().first,
                       frame_end_timestamp - append_window_end));

    // Decrease the duration of the buffer to remove the discarded portion.
    buffer->set_duration(append_window_end - buffer->timestamp());
    processed_buffer = true;
  }

  return processed_buffer;
}

bool FrameProcessor::ProcessFrame(
    const scoped_refptr<StreamParserBuffer>& frame,
    base::TimeDelta append_window_start,
    base::TimeDelta append_window_end,
    base::TimeDelta* timestamp_offset) {
  // Implements the loop within step 1 of the coded frame processing algorithm
  // for a single input frame per June 9, 2016 MSE spec editor's draft:
  // https://rawgit.com/w3c/media-source/d8f901f22/
  //     index.html#sourcebuffer-coded-frame-processing
  while (true) {
    // 1. Loop Top:
    // Otherwise case: (See SourceBufferState's |auto_update_timestamp_offset_|,
    // too).
    // 1.1. Let presentation timestamp be a double precision floating point
    //      representation of the coded frame's presentation timestamp in
    //      seconds.
    // 1.2. Let decode timestamp be a double precision floating point
    //      representation of the coded frame's decode timestamp in seconds.
    // 2. Let frame duration be a double precision floating point representation
    //    of the coded frame's duration in seconds.
    // We use base::TimeDelta and DecodeTimestamp instead of double.
    base::TimeDelta presentation_timestamp = frame->timestamp();
    DecodeTimestamp decode_timestamp = frame->GetDecodeTimestamp();
    base::TimeDelta frame_duration = frame->duration();

    DVLOG(3) << __func__ << ": Processing frame Type=" << frame->type()
             << ", TrackID=" << frame->track_id()
             << ", PTS=" << presentation_timestamp.InSecondsF()
             << ", DTS=" << decode_timestamp.InSecondsF()
             << ", DUR=" << frame_duration.InSecondsF()
             << ", RAP=" << frame->is_key_frame();

    // Buffering, splicing, append window trimming, etc., all depend on the
    // assumption that all audio coded frames are key frames. Metadata in the
    // bytestream may not indicate that, so we need to enforce that assumption
    // here with a warning log.
    if (frame->type() == DemuxerStream::AUDIO && !frame->is_key_frame()) {
      LIMITED_MEDIA_LOG(DEBUG, media_log_, num_audio_non_keyframe_warnings_,
                        kMaxAudioNonKeyframeWarnings)
          << "Bytestream with audio frame PTS "
          << presentation_timestamp.InMicroseconds() << "us and DTS "
          << decode_timestamp.InMicroseconds()
          << "us indicated the frame is not a random access point (key frame). "
             "All audio frames are expected to be key frames.";
      frame->set_is_key_frame(true);
    }

    // Sanity check the timestamps.
    if (presentation_timestamp == kNoTimestamp) {
      MEDIA_LOG(ERROR, media_log_) << "Unknown PTS for " << frame->GetTypeName()
                                   << " frame";
      return false;
    }
    if (decode_timestamp == kNoDecodeTimestamp()) {
      MEDIA_LOG(ERROR, media_log_) << "Unknown DTS for " << frame->GetTypeName()
                                   << " frame";
      return false;
    }
    if (decode_timestamp.ToPresentationTime() > presentation_timestamp) {
      // TODO(wolenetz): Determine whether DTS>PTS should really be allowed. See
      // http://crbug.com/354518.
      LIMITED_MEDIA_LOG(DEBUG, media_log_, num_dts_beyond_pts_warnings_,
                        kMaxDtsBeyondPtsWarnings)
          << "Parsed " << frame->GetTypeName() << " frame has DTS "
          << decode_timestamp.InMicroseconds()
          << "us, which is after the frame's PTS "
          << presentation_timestamp.InMicroseconds() << "us";
      DVLOG(2) << __func__ << ": WARNING: Frame DTS("
               << decode_timestamp.InSecondsF() << ") > PTS("
               << presentation_timestamp.InSecondsF()
               << "), frame type=" << frame->GetTypeName();
    }

    // All stream parsers must emit valid (non-negative) frame durations.
    // Note that duration of 0 can occur for at least WebM alt-ref frames.
    if (frame_duration == kNoTimestamp) {
      MEDIA_LOG(ERROR, media_log_)
          << "Unknown duration for " << frame->GetTypeName() << " frame at PTS "
          << presentation_timestamp.InMicroseconds() << "us";
      return false;
    }
    if (frame_duration <  base::TimeDelta()) {
      MEDIA_LOG(ERROR, media_log_)
          << "Negative duration " << frame_duration.InMicroseconds()
          << "us for " << frame->GetTypeName() << " frame at PTS "
          << presentation_timestamp.InMicroseconds() << "us";
      return false;
    }

    // 3. If mode equals "sequence" and group start timestamp is set, then run
    //    the following steps:
    if (sequence_mode_ && group_start_timestamp_ != kNoTimestamp) {
      // 3.1. Set timestampOffset equal to group start timestamp -
      //      presentation timestamp.
      *timestamp_offset = group_start_timestamp_ - presentation_timestamp;

      DVLOG(3) << __func__ << ": updated timestampOffset is now "
               << timestamp_offset->InSecondsF();

      // 3.2. Set group end timestamp equal to group start timestamp.
      group_end_timestamp_ = group_start_timestamp_;

      // 3.3. Set the need random access point flag on all track buffers to
      //      true.
      SetAllTrackBuffersNeedRandomAccessPoint();

      // 3.4. Unset group start timestamp.
      group_start_timestamp_ = kNoTimestamp;
    }

    // 4. If timestampOffset is not 0, then run the following steps:
    if (!timestamp_offset->is_zero()) {
      // 4.1. Add timestampOffset to the presentation timestamp.
      // Note: |frame| PTS is only updated if it survives discontinuity
      // processing.
      presentation_timestamp += *timestamp_offset;

      // 4.2. Add timestampOffset to the decode timestamp.
      // Frame DTS is only updated if it survives discontinuity processing.
      decode_timestamp += *timestamp_offset;
    }

    // 5. Let track buffer equal the track buffer that the coded frame will be
    //    added to.
    StreamParser::TrackId track_id = frame->track_id();
    MseTrackBuffer* track_buffer = FindTrack(track_id);
    if (!track_buffer) {
      MEDIA_LOG(ERROR, media_log_)
          << "Unknown track with type " << frame->GetTypeName()
          << ", frame processor track id " << track_id
          << ", and parser track id " << frame->track_id();
      return false;
    }
    if (frame->type() != track_buffer->stream()->type()) {
      MEDIA_LOG(ERROR, media_log_) << "Frame type " << frame->GetTypeName()
                                   << " doesn't match track buffer type "
                                   << track_buffer->stream()->type();
      return false;
    }

    // 6. If last decode timestamp for track buffer is set and decode timestamp
    //    is less than last decode timestamp
    //    OR
    //    If last decode timestamp for track buffer is set and the difference
    //    between decode timestamp and last decode timestamp is greater than 2
    //    times last frame duration:
    DecodeTimestamp track_last_decode_timestamp =
        track_buffer->last_decode_timestamp();
    if (track_last_decode_timestamp != kNoDecodeTimestamp()) {
      base::TimeDelta track_dts_delta =
          decode_timestamp - track_last_decode_timestamp;
      if (track_dts_delta < base::TimeDelta() ||
          track_dts_delta > 2 * track_buffer->last_frame_duration()) {
        DCHECK(!pending_notify_all_group_start_);
        // 6.1. If mode equals "segments": Set group end timestamp to
        //      presentation timestamp.
        //      If mode equals "sequence": Set group start timestamp equal to
        //      the group end timestamp.
        if (!sequence_mode_) {
          group_end_timestamp_ = presentation_timestamp;
          // This triggers a discontinuity so we need to treat the next frames
          // appended within the append window as if they were the beginning of
          // a new coded frame group. |pending_notify_all_group_start_| is reset
          // in Reset(), below, for "segments" mode.
        } else {
          DVLOG(3) << __func__ << " : Sequence mode discontinuity, GETS: "
                   << group_end_timestamp_.InSecondsF();
          // Reset(), below, performs the "Set group start timestamp equal to
          // the group end timestamp" operation for "sequence" mode.
        }

        // 6.2. - 6.5.:
        Reset();

        // 6.6. Jump to the Loop Top step above to restart processing of the
        //      current coded frame.
        DVLOG(3) << __func__ << ": Discontinuity: reprocessing frame";
        continue;
      }
    }

    // 7. Let frame end timestamp equal the sum of presentation timestamp and
    //    frame duration.
    base::TimeDelta frame_end_timestamp =
        presentation_timestamp + frame_duration;

    // 8.  If presentation timestamp is less than appendWindowStart, then set
    //     the need random access point flag to true, drop the coded frame, and
    //     jump to the top of the loop to start processing the next coded
    //     frame.
    // Note: We keep the result of partial discard of a buffer that overlaps
    //       |append_window_start| and does not end after |append_window_end|,
    //       for streams which support partial trimming.
    // 9. If frame end timestamp is greater than appendWindowEnd, then set the
    //    need random access point flag to true, drop the coded frame, and jump
    //    to the top of the loop to start processing the next coded frame.
    // Note: We keep the result of partial discard of a buffer that overlaps
    //       |append_window_end|, for streams which support partial trimming.
    frame->set_timestamp(presentation_timestamp);
    frame->SetDecodeTimestamp(decode_timestamp);
    if (track_buffer->stream()->supports_partial_append_window_trimming() &&
        HandlePartialAppendWindowTrimming(append_window_start,
                                          append_window_end,
                                          frame)) {
      // |frame| has been partially trimmed or had preroll added.  Though
      // |frame|'s duration may have changed, do not update |frame_duration|
      // here, so |track_buffer|'s last frame duration update uses original
      // frame duration and reduces spurious discontinuity detection.
      decode_timestamp = frame->GetDecodeTimestamp();
      presentation_timestamp = frame->timestamp();
      frame_end_timestamp = frame->timestamp() + frame->duration();
    }

    if (presentation_timestamp < append_window_start ||
        frame_end_timestamp > append_window_end) {
      track_buffer->set_needs_random_access_point(true);
      DVLOG(3) << "Dropping frame that is outside append window.";
      return true;
    }

    DCHECK(presentation_timestamp >= base::TimeDelta());
    if (decode_timestamp < DecodeTimestamp()) {
      // B-frames may still result in negative DTS here after being shifted by
      // |timestamp_offset_|.
      // TODO(wolenetz): This is no longer a step in the CFP, since negative DTS
      // are allowed. Remove this parse failure and error log as part of fixing
      // PTS/DTS conflation in SourceBufferStream. See https://crbug.com/398141
      MEDIA_LOG(ERROR, media_log_)
          << frame->GetTypeName() << " frame with PTS "
          << presentation_timestamp.InMicroseconds() << "us has negative DTS "
          << decode_timestamp.InMicroseconds()
          << "us after applying timestampOffset, handling any discontinuity, "
             "and filtering against append window";
      return false;
    }

    // 10. If the need random access point flag on track buffer equals true,
    //     then run the following steps:
    if (track_buffer->needs_random_access_point()) {
      // 10.1. If the coded frame is not a random access point, then drop the
      //       coded frame and jump to the top of the loop to start processing
      //       the next coded frame.
      if (!frame->is_key_frame()) {
        DVLOG(3) << __func__
                 << ": Dropping frame that is not a random access point";
        return true;
      }

      // 10.2. Set the need random access point flag on track buffer to false.
      track_buffer->set_needs_random_access_point(false);
    }

    // We now have a processed buffer to append to the track buffer's stream.
    // If it is the first in a new coded frame group (such as following a
    // segments append mode discontinuity, or following a switch to segments
    // append mode from sequence append mode), notify all the track buffers
    // that a coded frame group is starting.
    //
    // Otherwise, if the buffer's DTS indicates that a new coded frame group
    // needs signalling, signal just the buffer's track buffer. This can
    // happen in both sequence and segments append modes when the first
    // processed track's frame following a discontinuity has a higher DTS than
    // this later processed track's first frame following that discontinuity.
    if (pending_notify_all_group_start_ ||
        track_buffer->last_processed_decode_timestamp() > decode_timestamp) {
      DCHECK(frame->is_key_frame());

      // First, complete the append to track buffer streams of the previous
      // coded frame group's frames, if any.
      if (!FlushProcessedFrames())
        return false;

      if (pending_notify_all_group_start_) {
        // TODO(wolenetz): This should be changed to a presentation timestamp.
        // See http://crbug.com/402502
        NotifyStartOfCodedFrameGroup(decode_timestamp);
        pending_notify_all_group_start_ = false;
      } else {
        // TODO(wolenetz): This should be changed to a presentation timestamp.
        // See http://crbug.com/402502
        track_buffer->NotifyStartOfCodedFrameGroup(decode_timestamp);
      }
    }

    DVLOG(3) << __func__ << ": Sending processed frame to stream, "
             << "PTS=" << presentation_timestamp.InSecondsF()
             << ", DTS=" << decode_timestamp.InSecondsF();

    // Steps 11-16: Note, we optimize by appending groups of contiguous
    // processed frames for each track buffer at end of ProcessFrames() or prior
    // to signalling coded frame group starts.
    track_buffer->EnqueueProcessedFrame(frame);

    // 17. Set last decode timestamp for track buffer to decode timestamp.
    track_buffer->set_last_decode_timestamp(decode_timestamp);

    // 18. Set last frame duration for track buffer to frame duration.
    track_buffer->set_last_frame_duration(frame_duration);

    // 19. If highest presentation timestamp for track buffer is unset or frame
    //     end timestamp is greater than highest presentation timestamp, then
    //     set highest presentation timestamp for track buffer to frame end
    //     timestamp.
    track_buffer->SetHighestPresentationTimestampIfIncreased(
        frame_end_timestamp);

    // 20. If frame end timestamp is greater than group end timestamp, then set
    //     group end timestamp equal to frame end timestamp.
    if (frame_end_timestamp > group_end_timestamp_)
      group_end_timestamp_ = frame_end_timestamp;
    DCHECK(group_end_timestamp_ >= base::TimeDelta());

    // Step 21 is currently handled differently. See SourceBufferState's
    // |auto_update_timestamp_offset_|.
    return true;
  }

  NOTREACHED();
  return false;
}

}  // namespace media
