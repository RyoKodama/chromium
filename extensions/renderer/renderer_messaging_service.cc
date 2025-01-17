// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/renderer_messaging_service.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "content/public/child/v8_value_converter.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/extension_bindings_system.h"
#include "extensions/renderer/extension_port.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScopedUserGesture.h"
#include "third_party/WebKit/public/web/WebScopedWindowFocusAllowedIndicator.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "v8/include/v8.h"

namespace extensions {

RendererMessagingService::RendererMessagingService(
    ExtensionBindingsSystem* bindings_system)
    : bindings_system_(bindings_system) {}
RendererMessagingService::~RendererMessagingService() {}

void RendererMessagingService::ValidateMessagePort(
    const ScriptContextSet& context_set,
    const PortId& port_id,
    content::RenderFrame* render_frame) {
  int routing_id = render_frame->GetRoutingID();

  bool has_port = false;
  // The base::Unretained() below is safe since ScriptContextSet::ForEach is
  // synchronous.
  context_set.ForEach(
      render_frame,
      base::Bind(&RendererMessagingService::ValidateMessagePortInContext,
                 base::Unretained(this), port_id, &has_port));

  // A reply is only sent if the port is missing, because the port is assumed to
  // exist unless stated otherwise.
  if (!has_port) {
    content::RenderThread::Get()->Send(
        new ExtensionHostMsg_CloseMessagePort(routing_id, port_id, false));
  }
}

void RendererMessagingService::DispatchOnConnect(
    const ScriptContextSet& context_set,
    const PortId& target_port_id,
    const std::string& channel_name,
    const ExtensionMsg_TabConnectionInfo& source,
    const ExtensionMsg_ExternalConnectionInfo& info,
    const std::string& tls_channel_id,
    content::RenderFrame* restrict_to_render_frame) {
  DCHECK(!target_port_id.is_opener);
  int routing_id = restrict_to_render_frame
                       ? restrict_to_render_frame->GetRoutingID()
                       : MSG_ROUTING_NONE;
  bool port_created = false;
  context_set.ForEach(
      info.target_id, restrict_to_render_frame,
      base::Bind(&RendererMessagingService::DispatchOnConnectToScriptContext,
                 base::Unretained(this), target_port_id, channel_name, &source,
                 info, tls_channel_id, &port_created));
  // Note: |restrict_to_render_frame| may have been deleted at this point!

  if (port_created) {
    content::RenderThread::Get()->Send(
        new ExtensionHostMsg_OpenMessagePort(routing_id, target_port_id));
  } else {
    content::RenderThread::Get()->Send(new ExtensionHostMsg_CloseMessagePort(
        routing_id, target_port_id, false));
  }
}

void RendererMessagingService::DeliverMessage(
    const ScriptContextSet& context_set,
    const PortId& target_port_id,
    const Message& message,
    content::RenderFrame* restrict_to_render_frame) {
  context_set.ForEach(
      restrict_to_render_frame,
      base::Bind(&RendererMessagingService::DeliverMessageToScriptContext,
                 base::Unretained(this), message, target_port_id));
}

void RendererMessagingService::DispatchOnDisconnect(
    const ScriptContextSet& context_set,
    const PortId& port_id,
    const std::string& error_message,
    content::RenderFrame* restrict_to_render_frame) {
  context_set.ForEach(
      restrict_to_render_frame,
      base::Bind(&RendererMessagingService::DispatchOnDisconnectToScriptContext,
                 base::Unretained(this), port_id, error_message));
}

void RendererMessagingService::ValidateMessagePortInContext(
    const PortId& port_id,
    bool* has_port,
    ScriptContext* script_context) {
  if (*has_port)
    return;  // Stop checking if the port was found.

  // No need for |=; we know this is false right now from above.
  *has_port = ContextHasMessagePort(script_context, port_id);
}

void RendererMessagingService::DispatchOnConnectToScriptContext(
    const PortId& target_port_id,
    const std::string& channel_name,
    const ExtensionMsg_TabConnectionInfo* source,
    const ExtensionMsg_ExternalConnectionInfo& info,
    const std::string& tls_channel_id,
    bool* port_created,
    ScriptContext* script_context) {
  // If the channel was opened by this same context, ignore it. This should only
  // happen when messages are sent to an entire process (rather than a single
  // frame) as an optimization; otherwise the browser process filters this out.
  if (script_context->context_id() == target_port_id.context_id)
    return;

  // First, determine the event we'll use to connect.
  std::string target_extension_id = script_context->GetExtensionID();
  bool is_external = info.source_id != target_extension_id;
  std::string event_name;
  if (channel_name == "chrome.extension.sendRequest") {
    event_name =
        is_external ? "extension.onRequestExternal" : "extension.onRequest";
  } else if (channel_name == "chrome.runtime.sendMessage") {
    event_name =
        is_external ? "runtime.onMessageExternal" : "runtime.onMessage";
  } else {
    event_name =
        is_external ? "runtime.onConnectExternal" : "runtime.onConnect";
  }

  // If there are no listeners for the given event, then we know the port won't
  // be used in this context.
  if (!bindings_system_->HasEventListenerInContext(event_name,
                                                   script_context)) {
    return;
  }
  *port_created = true;

  DispatchOnConnectToListeners(script_context, target_port_id,
                               target_extension_id, channel_name, source, info,
                               tls_channel_id, event_name);
}

void RendererMessagingService::DeliverMessageToScriptContext(
    const Message& message,
    const PortId& target_port_id,
    ScriptContext* script_context) {
  if (!ContextHasMessagePort(script_context, target_port_id))
    return;

  std::unique_ptr<blink::WebScopedUserGesture> web_user_gesture;
  std::unique_ptr<blink::WebScopedWindowFocusAllowedIndicator>
      allow_window_focus;
  if (message.user_gesture) {
    web_user_gesture = std::make_unique<blink::WebScopedUserGesture>(
        script_context->web_frame());

    if (script_context->web_frame()) {
      blink::WebDocument document = script_context->web_frame()->GetDocument();
      allow_window_focus =
          std::make_unique<blink::WebScopedWindowFocusAllowedIndicator>(
              &document);
    }
  }

  DispatchOnMessageToListeners(script_context, message, target_port_id);
}

void RendererMessagingService::DispatchOnDisconnectToScriptContext(
    const PortId& port_id,
    const std::string& error_message,
    ScriptContext* script_context) {
  if (!ContextHasMessagePort(script_context, port_id))
    return;

  DispatchOnDisconnectToListeners(script_context, port_id, error_message);
}

}  // namespace extensions
