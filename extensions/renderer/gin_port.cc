// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/gin_port.h"

#include <cstring>
#include <vector>

#include "extensions/common/api/messaging/message.h"
#include "extensions/renderer/bindings/api_event_handler.h"
#include "extensions/renderer/bindings/event_emitter.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/object_template_builder.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"

namespace extensions {

namespace {

constexpr char kSenderKey[] = "sender";
constexpr char kOnMessageEvent[] = "onMessage";
constexpr char kOnDisconnectEvent[] = "onDisconnect";

}  // namespace

GinPort::GinPort(const PortId& port_id,
                 const std::string& name,
                 APIEventHandler* event_handler,
                 Delegate* delegate)
    : port_id_(port_id),
      name_(name),
      event_handler_(event_handler),
      delegate_(delegate) {}

GinPort::~GinPort() {}

gin::WrapperInfo GinPort::kWrapperInfo = {gin::kEmbedderNativeGin};

gin::ObjectTemplateBuilder GinPort::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return Wrappable<GinPort>::GetObjectTemplateBuilder(isolate)
      .SetMethod("disconnect", &GinPort::DisconnectHandler)
      .SetMethod("postMessage", &GinPort::PostMessageHandler)
      .SetProperty("name", &GinPort::GetName)
      .SetProperty("onDisconnect", &GinPort::GetOnDisconnectEvent)
      .SetProperty("onMessage", &GinPort::GetOnMessageEvent)
      .SetProperty("sender", &GinPort::GetSender);
}

void GinPort::DispatchOnMessage(v8::Local<v8::Context> context,
                                const Message& message) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context);

  v8::Local<v8::String> v8_message_string =
      gin::StringToV8(isolate, message.data);
  v8::Local<v8::Value> parsed_message;
  {
    v8::TryCatch try_catch(isolate);
    if (!v8::JSON::Parse(context, v8_message_string).ToLocal(&parsed_message)) {
      NOTREACHED();
      return;
    }
  }
  v8::Local<v8::Object> self = GetWrapper(isolate).ToLocalChecked();
  std::vector<v8::Local<v8::Value>> args = {parsed_message, self};
  DispatchEvent(context, &args, kOnMessageEvent);
}

void GinPort::Disconnect(v8::Local<v8::Context> context) {
  DCHECK(!is_closed_);

  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Object> self = GetWrapper(isolate).ToLocalChecked();
  std::vector<v8::Local<v8::Value>> args = {self};
  DispatchEvent(context, &args, kOnDisconnectEvent);

  Invalidate(context);
}

void GinPort::SetSender(v8::Local<v8::Context> context,
                        v8::Local<v8::Value> sender) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Object> wrapper = GetWrapper(isolate).ToLocalChecked();
  v8::Local<v8::Private> key =
      v8::Private::ForApi(isolate, gin::StringToSymbol(isolate, kSenderKey));
  v8::Maybe<bool> set_result = wrapper->SetPrivate(context, key, sender);
  DCHECK(set_result.IsJust() && set_result.FromJust());
}

void GinPort::DisconnectHandler(gin::Arguments* arguments) {
  if (is_closed_)
    return;

  v8::Local<v8::Context> context = arguments->GetHolderCreationContext();
  Invalidate(context);
}

void GinPort::PostMessageHandler(gin::Arguments* arguments,
                                 v8::Local<v8::Value> v8_message) {
  v8::Isolate* isolate = arguments->isolate();
  if (is_closed_) {
    ThrowError(isolate, "Attempting to use a disconnected port object");
    return;
  }

  // TODO(devlin): For some reason, we don't use the signature for
  // Port.postMessage when evaluating the parameters. We probably should, but
  // we don't know how many extensions that may break. It would be good to
  // investigate, and, ideally, use the signature.

  if (v8_message->IsUndefined()) {
    // JSON.stringify won't serialized undefined (it returns undefined), but it
    // will serialized null. We've always converted undefined to null in JS
    // bindings, so preserve this behavior for now.
    v8_message = v8::Null(isolate);
  }

  bool success = false;
  v8::Local<v8::String> stringified;
  {
    v8::TryCatch try_catch(isolate);
    success =
        v8::JSON::Stringify(arguments->GetHolderCreationContext(), v8_message)
            .ToLocal(&stringified);
  }

  std::string message;
  if (success) {
    message = gin::V8ToString(stringified);
    // JSON.stringify can either fail (with unserializable objects) or can
    // return undefined. If it returns undefined, the v8 API then coerces it to
    // the string value "undefined". Throw an error if we were passed
    // unserializable objects.
    success = message != "undefined";
  }

  if (!success) {
    ThrowError(isolate, "Illegal argument to Port.postMessage");
    return;
  }

  delegate_->PostMessageToPort(
      port_id_,
      std::make_unique<Message>(
          message, blink::WebUserGestureIndicator::IsProcessingUserGesture()));
}

std::string GinPort::GetName() {
  return name_;
}

v8::Local<v8::Value> GinPort::GetOnDisconnectEvent(gin::Arguments* arguments) {
  return GetEvent(arguments->GetHolderCreationContext(), kOnDisconnectEvent);
}

v8::Local<v8::Value> GinPort::GetOnMessageEvent(gin::Arguments* arguments) {
  return GetEvent(arguments->GetHolderCreationContext(), kOnMessageEvent);
}

v8::Local<v8::Value> GinPort::GetSender(gin::Arguments* arguments) {
  v8::Isolate* isolate = arguments->isolate();
  v8::Local<v8::Object> wrapper = GetWrapper(isolate).ToLocalChecked();
  v8::Local<v8::Private> key =
      v8::Private::ForApi(isolate, gin::StringToSymbol(isolate, kSenderKey));
  v8::Local<v8::Value> sender;
  if (!wrapper->GetPrivate(arguments->GetHolderCreationContext(), key)
           .ToLocal(&sender)) {
    NOTREACHED();
    return v8::Undefined(isolate);
  }

  return sender;
}

v8::Local<v8::Object> GinPort::GetEvent(v8::Local<v8::Context> context,
                                        base::StringPiece event_name) {
  DCHECK(event_name == kOnMessageEvent || event_name == kOnDisconnectEvent);
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::Object> wrapper = GetWrapper(isolate).ToLocalChecked();
  v8::Local<v8::Private> key =
      v8::Private::ForApi(isolate, gin::StringToSymbol(isolate, event_name));
  v8::Local<v8::Value> event_val;
  if (!wrapper->GetPrivate(context, key).ToLocal(&event_val)) {
    NOTREACHED();
    return v8::Local<v8::Object>();
  }

  DCHECK(!event_val.IsEmpty());
  v8::Local<v8::Object> event_object;
  if (event_val->IsUndefined()) {
    event_object = event_handler_->CreateAnonymousEventInstance(context);
    v8::Maybe<bool> set_result =
        wrapper->SetPrivate(context, key, event_object);
    if (!set_result.IsJust() || !set_result.FromJust()) {
      NOTREACHED();
      return v8::Local<v8::Object>();
    }
  } else {
    event_object = event_val.As<v8::Object>();
  }
  return event_object;
}

void GinPort::DispatchEvent(v8::Local<v8::Context> context,
                            std::vector<v8::Local<v8::Value>>* args,
                            base::StringPiece event_name) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::Value> on_message = GetEvent(context, event_name);
  EventEmitter* emitter = nullptr;
  gin::Converter<EventEmitter*>::FromV8(isolate, on_message, &emitter);
  CHECK(emitter);

  emitter->Fire(context, args, nullptr);
}

void GinPort::Invalidate(v8::Local<v8::Context> context) {
  is_closed_ = true;
  event_handler_->InvalidateCustomEvent(context,
                                        GetEvent(context, kOnMessageEvent));
  event_handler_->InvalidateCustomEvent(context,
                                        GetEvent(context, kOnDisconnectEvent));
  delegate_->ClosePort(port_id_);
}

void GinPort::ThrowError(v8::Isolate* isolate, base::StringPiece error) {
  isolate->ThrowException(
      v8::Exception::Error(gin::StringToV8(isolate, error)));
}

}  // namespace extensions
