// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/track/CueTimeline.h"

#include "core/dom/events/Event.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/HTMLTrackElement.h"
#include "core/html/track/LoadableTextTrack.h"
#include "core/html/track/TextTrack.h"
#include "core/html/track/TextTrackCue.h"
#include "core/html/track/TextTrackCueList.h"
#include "platform/wtf/NonCopyingSort.h"

namespace blink {

CueTimeline::CueTimeline(HTMLMediaElement& media_element)
    : media_element_(&media_element),
      last_update_time_(-1),
      ignore_update_(0) {}

void CueTimeline::AddCues(TextTrack* track, const TextTrackCueList* cues) {
  DCHECK_NE(track->mode(), TextTrack::DisabledKeyword());
  for (size_t i = 0; i < cues->length(); ++i)
    AddCueInternal(cues->AnonymousIndexedGetter(i));
  UpdateActiveCues(MediaElement().currentTime());
}

void CueTimeline::AddCue(TextTrack* track, TextTrackCue* cue) {
  DCHECK_NE(track->mode(), TextTrack::DisabledKeyword());
  AddCueInternal(cue);
  UpdateActiveCues(MediaElement().currentTime());
}

void CueTimeline::AddCueInternal(TextTrackCue* cue) {
  // Negative duration cues need be treated in the interval tree as
  // zero-length cues.
  double end_time = std::max(cue->startTime(), cue->endTime());

  CueInterval interval =
      cue_tree_.CreateInterval(cue->startTime(), end_time, cue);
  if (!cue_tree_.Contains(interval))
    cue_tree_.Add(interval);
}

void CueTimeline::RemoveCues(TextTrack*, const TextTrackCueList* cues) {
  for (size_t i = 0; i < cues->length(); ++i)
    RemoveCueInternal(cues->AnonymousIndexedGetter(i));
  UpdateActiveCues(MediaElement().currentTime());
}

void CueTimeline::RemoveCue(TextTrack*, TextTrackCue* cue) {
  RemoveCueInternal(cue);
  UpdateActiveCues(MediaElement().currentTime());
}

void CueTimeline::RemoveCueInternal(TextTrackCue* cue) {
  // Negative duration cues need to be treated in the interval tree as
  // zero-length cues.
  double end_time = std::max(cue->startTime(), cue->endTime());

  CueInterval interval =
      cue_tree_.CreateInterval(cue->startTime(), end_time, cue);
  cue_tree_.Remove(interval);

  size_t index = currently_active_cues_.Find(interval);
  if (index != kNotFound) {
    DCHECK(cue->IsActive());
    currently_active_cues_.erase(index);
    cue->SetIsActive(false);
    // Since the cue will be removed from the media element and likely the
    // TextTrack might also be destructed, notifying the region of the cue
    // removal shouldn't be done.
    cue->RemoveDisplayTree(TextTrackCue::kDontNotifyRegion);
  }
}

void CueTimeline::HideCues(TextTrack*, const TextTrackCueList* cues) {
  for (size_t i = 0; i < cues->length(); ++i)
    cues->AnonymousIndexedGetter(i)->RemoveDisplayTree();
}

static bool TrackIndexCompare(TextTrack* a, TextTrack* b) {
  return a->TrackIndex() - b->TrackIndex() < 0;
}

static bool EventTimeCueCompare(const std::pair<double, TextTrackCue*>& a,
                                const std::pair<double, TextTrackCue*>& b) {
  // 12 - Sort the tasks in events in ascending time order (tasks with earlier
  // times first).
  if (a.first != b.first)
    return a.first - b.first < 0;

  // If the cues belong to different text tracks, it doesn't make sense to
  // compare the two tracks by the relative cue order, so return the relative
  // track order.
  if (a.second->track() != b.second->track())
    return TrackIndexCompare(a.second->track(), b.second->track());

  // 12 - Further sort tasks in events that have the same time by the
  // relative text track cue order of the text track cues associated
  // with these tasks.
  return a.second->CueIndex() < b.second->CueIndex();
}

static Event* CreateEventWithTarget(const AtomicString& event_name,
                                    EventTarget* event_target) {
  Event* event = Event::Create(event_name);
  event->SetTarget(event_target);
  return event;
}

void CueTimeline::UpdateActiveCues(double movie_time) {
  // 4.8.10.8 Playing the media resource

  //  If the current playback position changes while the steps are running,
  //  then the user agent must wait for the steps to complete, and then must
  //  immediately rerun the steps.
  if (IgnoreUpdateRequests())
    return;

  HTMLMediaElement& media_element = this->MediaElement();

  // Don't run the "time marches on" algorithm if the document has been
  // detached. This primarily guards against dispatch of events w/
  // HTMLTrackElement targets.
  if (media_element.GetDocument().IsDetached())
    return;

  // https://html.spec.whatwg.org/#time-marches-on

  // 1 - Let current cues be a list of cues, initialized to contain all the
  // cues of all the hidden, showing, or showing by default text tracks of the
  // media element (not the disabled ones) whose start times are less than or
  // equal to the current playback position and whose end times are greater
  // than the current playback position.
  CueList current_cues;

  // The user agent must synchronously unset [the text track cue active] flag
  // whenever ... the media element's readyState is changed back to
  // kHaveNothing.
  if (media_element.getReadyState() != HTMLMediaElement::kHaveNothing &&
      media_element.GetWebMediaPlayer())
    current_cues =
        cue_tree_.AllOverlaps(cue_tree_.CreateInterval(movie_time, movie_time));

  CueList previous_cues;
  CueList missed_cues;

  // 2 - Let other cues be a list of cues, initialized to contain all the cues
  // of hidden, showing, and showing by default text tracks of the media
  // element that are not present in current cues.
  previous_cues = currently_active_cues_;

  // 3 - Let last time be the current playback position at the time this
  // algorithm was last run for this media element, if this is not the first
  // time it has run.
  double last_time = last_update_time_;
  double last_seek_time = media_element.LastSeekTime();

  // 4 - If the current playback position has, since the last time this
  // algorithm was run, only changed through its usual monotonic increase
  // during normal playback, then let missed cues be the list of cues in other
  // cues whose start times are greater than or equal to last time and whose
  // end times are less than or equal to the current playback position.
  // Otherwise, let missed cues be an empty list.
  if (last_time >= 0 && last_seek_time < movie_time) {
    CueList potentially_skipped_cues =
        cue_tree_.AllOverlaps(cue_tree_.CreateInterval(last_time, movie_time));

    for (CueInterval cue : potentially_skipped_cues) {
      // Consider cues that may have been missed since the last seek time.
      if (cue.Low() > std::max(last_seek_time, last_time) &&
          cue.High() < movie_time)
        missed_cues.push_back(cue);
    }
  }

  last_update_time_ = movie_time;

  // 5 - If the time was reached through the usual monotonic increase of the
  // current playback position during normal playback, and if the user agent
  // has not fired a timeupdate event at the element in the past 15 to 250ms
  // and is not still running event handlers for such an event, then the user
  // agent must queue a task to fire a simple event named timeupdate at the
  // element. (In the other cases, such as explicit seeks, relevant events get
  // fired as part of the overall process of changing the current playback
  // position.)
  if (!media_element.seeking() && last_seek_time < last_time)
    media_element.ScheduleTimeupdateEvent(true);

  // Explicitly cache vector sizes, as their content is constant from here.
  size_t missed_cues_size = missed_cues.size();
  size_t previous_cues_size = previous_cues.size();

  // 6 - If all of the cues in current cues have their text track cue active
  // flag set, none of the cues in other cues have their text track cue active
  // flag set, and missed cues is empty, then abort these steps.
  bool active_set_changed = missed_cues_size;

  for (size_t i = 0; !active_set_changed && i < previous_cues_size; ++i) {
    if (!current_cues.Contains(previous_cues[i]) &&
        previous_cues[i].Data()->IsActive())
      active_set_changed = true;
  }

  for (CueInterval current_cue : current_cues) {
    // Notify any cues that are already active of the current time to mark
    // past and future nodes. Any inactive cues have an empty display state;
    // they will be notified of the current time when the display state is
    // updated.
    if (current_cue.Data()->IsActive())
      current_cue.Data()->UpdatePastAndFutureNodes(movie_time);
    else
      active_set_changed = true;
  }

  if (!active_set_changed)
    return;

  // 7 - If the time was reached through the usual monotonic increase of the
  // current playback position during normal playback, and there are cues in
  // other cues that have their text track cue pause-on-exi flag set and that
  // either have their text track cue active flag set or are also in missed
  // cues, then immediately pause the media element.
  for (size_t i = 0; !media_element.paused() && i < previous_cues_size; ++i) {
    if (previous_cues[i].Data()->pauseOnExit() &&
        previous_cues[i].Data()->IsActive() &&
        !current_cues.Contains(previous_cues[i]))
      media_element.pause();
  }

  for (size_t i = 0; !media_element.paused() && i < missed_cues_size; ++i) {
    if (missed_cues[i].Data()->pauseOnExit())
      media_element.pause();
  }

  // 8 - Let events be a list of tasks, initially empty. Each task in this
  // list will be associated with a text track, a text track cue, and a time,
  // which are used to sort the list before the tasks are queued.
  HeapVector<std::pair<double, Member<TextTrackCue>>> event_tasks;

  // 8 - Let affected tracks be a list of text tracks, initially empty.
  HeapVector<Member<TextTrack>> affected_tracks;

  for (const auto& missed_cue : missed_cues) {
    // 9 - For each text track cue in missed cues, prepare an event named enter
    // for the TextTrackCue object with the text track cue start time.
    event_tasks.push_back(
        std::make_pair(missed_cue.Data()->startTime(), missed_cue.Data()));

    // 10 - For each text track [...] in missed cues, prepare an event
    // named exit for the TextTrackCue object with the  with the later of
    // the text track cue end time and the text track cue start time.

    // Note: An explicit task is added only if the cue is NOT a zero or
    // negative length cue. Otherwise, the need for an exit event is
    // checked when these tasks are actually queued below. This doesn't
    // affect sorting events before dispatch either, because the exit
    // event has the same time as the enter event.
    if (missed_cue.Data()->startTime() < missed_cue.Data()->endTime()) {
      event_tasks.push_back(
          std::make_pair(missed_cue.Data()->endTime(), missed_cue.Data()));
    }
  }

  for (const auto& previous_cue : previous_cues) {
    // 10 - For each text track cue in other cues that has its text
    // track cue active flag set prepare an event named exit for the
    // TextTrackCue object with the text track cue end time.
    if (!current_cues.Contains(previous_cue)) {
      event_tasks.push_back(
          std::make_pair(previous_cue.Data()->endTime(), previous_cue.Data()));
    }
  }

  for (const auto& current_cue : current_cues) {
    // 11 - For each text track cue in current cues that does not have its
    // text track cue active flag set, prepare an event named enter for the
    // TextTrackCue object with the text track cue start time.
    if (!previous_cues.Contains(current_cue)) {
      event_tasks.push_back(
          std::make_pair(current_cue.Data()->startTime(), current_cue.Data()));
    }
  }

  // 12 - Sort the tasks in events in ascending time order (tasks with earlier
  // times first).
  NonCopyingSort(event_tasks.begin(), event_tasks.end(), EventTimeCueCompare);

  for (const auto& task : event_tasks) {
    if (!affected_tracks.Contains(task.second->track()))
      affected_tracks.push_back(task.second->track());

    // 13 - Queue each task in events, in list order.

    // Each event in eventTasks may be either an enterEvent or an exitEvent,
    // depending on the time that is associated with the event. This
    // correctly identifies the type of the event, if the startTime is
    // less than the endTime in the cue.
    if (task.second->startTime() >= task.second->endTime()) {
      media_element.ScheduleEvent(
          CreateEventWithTarget(EventTypeNames::enter, task.second.Get()));
      media_element.ScheduleEvent(
          CreateEventWithTarget(EventTypeNames::exit, task.second.Get()));
    } else {
      bool is_enter_event = task.first == task.second->startTime();
      AtomicString event_name =
          is_enter_event ? EventTypeNames::enter : EventTypeNames::exit;
      media_element.ScheduleEvent(
          CreateEventWithTarget(event_name, task.second.Get()));
    }
  }

  // 14 - Sort affected tracks in the same order as the text tracks appear in
  // the media element's list of text tracks, and remove duplicates.
  NonCopyingSort(affected_tracks.begin(), affected_tracks.end(),
                 TrackIndexCompare);

  // 15 - For each text track in affected tracks, in the list order, queue a
  // task to fire a simple event named cuechange at the TextTrack object, and,
  // ...
  for (const auto& track : affected_tracks) {
    media_element.ScheduleEvent(
        CreateEventWithTarget(EventTypeNames::cuechange, track.Get()));

    // ... if the text track has a corresponding track element, to then fire a
    // simple event named cuechange at the track element as well.
    if (track->TrackType() == TextTrack::kTrackElement) {
      HTMLTrackElement* track_element =
          ToLoadableTextTrack(track.Get())->TrackElement();
      DCHECK(track_element);
      media_element.ScheduleEvent(
          CreateEventWithTarget(EventTypeNames::cuechange, track_element));
    }
  }

  // 16 - Set the text track cue active flag of all the cues in the current
  // cues, and unset the text track cue active flag of all the cues in the
  // other cues.
  for (const auto& cue : current_cues)
    cue.Data()->SetIsActive(true);

  for (const auto& previous_cue : previous_cues) {
    if (!current_cues.Contains(previous_cue)) {
      TextTrackCue* cue = previous_cue.Data();
      cue->SetIsActive(false);
      cue->RemoveDisplayTree();
    }
  }

  // Update the current active cues.
  currently_active_cues_ = current_cues;
  media_element.UpdateTextTrackDisplay();
}

void CueTimeline::BeginIgnoringUpdateRequests() {
  ++ignore_update_;
}

void CueTimeline::EndIgnoringUpdateRequests() {
  DCHECK(ignore_update_);
  --ignore_update_;
  if (!ignore_update_)
    UpdateActiveCues(MediaElement().currentTime());
}

DEFINE_TRACE(CueTimeline) {
  visitor->Trace(media_element_);
}

}  // namespace blink
