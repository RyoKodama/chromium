// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_sync_message.h"

#include <stdint.h>

#include <stack>

#include "base/atomic_sequence_num.h"
#include "base/logging.h"
#include "build/build_config.h"

namespace {

base::AtomicSequenceNumber g_next_id;

}  // namespace

namespace IPC {

#define kSyncMessageHeaderSize 4

SyncMessage::SyncMessage(int32_t routing_id,
                         uint32_t type,
                         PriorityValue priority,
                         MessageReplyDeserializer* deserializer)
    : Message(routing_id, type, priority),
      deserializer_(deserializer) {
  set_sync();
  set_unblock(true);

  // Add synchronous message data before the message payload.
  SyncHeader header;
  header.message_id = g_next_id.GetNext();
  WriteSyncHeader(this, header);
}

SyncMessage::~SyncMessage() {
}

MessageReplyDeserializer* SyncMessage::GetReplyDeserializer() {
  DCHECK(deserializer_.get());
  return deserializer_.release();
}

bool SyncMessage::IsMessageReplyTo(const Message& msg, int request_id) {
  if (!msg.is_reply())
    return false;

  return GetMessageId(msg) == request_id;
}

base::PickleIterator SyncMessage::GetDataIterator(const Message* msg) {
  base::PickleIterator iter(*msg);
  if (!iter.SkipBytes(kSyncMessageHeaderSize))
    return base::PickleIterator();
  else
    return iter;
}

int SyncMessage::GetMessageId(const Message& msg) {
  if (!msg.is_sync() && !msg.is_reply())
    return 0;

  SyncHeader header;
  if (!ReadSyncHeader(msg, &header))
    return 0;

  return header.message_id;
}

Message* SyncMessage::GenerateReply(const Message* msg) {
  DCHECK(msg->is_sync());

  Message* reply = new Message(msg->routing_id(), IPC_REPLY_ID,
                               msg->priority());
  reply->set_reply();

  SyncHeader header;

  // use the same message id, but this time reply bit is set
  header.message_id = GetMessageId(*msg);
  WriteSyncHeader(reply, header);

  return reply;
}

bool SyncMessage::ReadSyncHeader(const Message& msg, SyncHeader* header) {
  DCHECK(msg.is_sync() || msg.is_reply());

  base::PickleIterator iter(msg);
  bool result = iter.ReadInt(&header->message_id);
  if (!result) {
    NOTREACHED();
    return false;
  }

  return true;
}

bool SyncMessage::WriteSyncHeader(Message* msg, const SyncHeader& header) {
  DCHECK(msg->is_sync() || msg->is_reply());
  DCHECK(msg->payload_size() == 0);
  msg->WriteInt(header.message_id);

  // Note: if you add anything here, you need to update kSyncMessageHeaderSize.
  DCHECK(kSyncMessageHeaderSize == msg->payload_size());

  return true;
}


bool MessageReplyDeserializer::SerializeOutputParameters(const Message& msg) {
  return SerializeOutputParameters(msg, SyncMessage::GetDataIterator(&msg));
}

}  // namespace IPC
