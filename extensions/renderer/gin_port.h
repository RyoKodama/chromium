// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_GIN_PORT_H_
#define EXTENSIONS_RENDERER_GIN_PORT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "extensions/common/api/messaging/port_id.h"
#include "gin/wrappable.h"
#include "v8/include/v8.h"

namespace gin {
class Arguments;
}

namespace extensions {
class APIEventHandler;
struct Message;

// A gin::Wrappable implementation of runtime.Port exposed to extensions. This
// provides a means for extensions to communicate with themselves and each
// other. This message-passing usually involves IPCs to the browser; we delegate
// out this responsibility. This class only handles the JS interface.
// TODO(devlin): Expose this class through a native implementation for the
// messaging custom bindings.
class GinPort final : public gin::Wrappable<GinPort> {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Posts a message to the port.
    virtual void PostMessageToPort(const PortId& port_id,
                                   std::unique_ptr<Message> message) = 0;

    // Closes the port.
    virtual void ClosePort(const PortId& port_id) = 0;
  };

  GinPort(const PortId& port_id,
          const std::string& name,
          APIEventHandler* event_handler,
          Delegate* delegate);
  ~GinPort() override;

  static gin::WrapperInfo kWrapperInfo;

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  // Dispatches an event to any listeners of the onMessage event.
  void DispatchOnMessage(v8::Local<v8::Context> context,
                         const Message& message);

  // Dispatches an event to any listeners of the onDisconnect event and closes
  // the port.
  void Disconnect(v8::Local<v8::Context> context);

  // Sets the |sender| property on the port.
  void SetSender(v8::Local<v8::Context> context, v8::Local<v8::Value> sender);

  bool is_closed() const { return is_closed_; }

 private:
  // Handlers for the gin::Wrappable.
  // Port.disconnect()
  void DisconnectHandler(gin::Arguments* arguments);
  // Port.postMessage()
  void PostMessageHandler(gin::Arguments* arguments,
                          v8::Local<v8::Value> v8_message);

  // Port.name
  std::string GetName();
  // Port.onDisconnect
  v8::Local<v8::Value> GetOnDisconnectEvent(gin::Arguments* arguments);
  // Port.onMessage
  v8::Local<v8::Value> GetOnMessageEvent(gin::Arguments* arguments);
  // Port.sender
  v8::Local<v8::Value> GetSender(gin::Arguments* arguments);

  // Helper method to return the event with the given |name| (either
  // onDisconnect or onMessage).
  v8::Local<v8::Object> GetEvent(v8::Local<v8::Context> context,
                                 base::StringPiece event_name);

  // Helper method to dispatch an event.
  void DispatchEvent(v8::Local<v8::Context> context,
                     std::vector<v8::Local<v8::Value>>* args,
                     base::StringPiece event_name);

  // Invalidates the port after it has been disconnected.
  void Invalidate(v8::Local<v8::Context> context);

  // Throws the given |error|.
  void ThrowError(v8::Isolate* isolate, base::StringPiece error);

  // Whether this port has been closed by calling disconnect().
  bool is_closed_ = false;

  // The associated port id.
  PortId port_id_;

  // The port's name.
  std::string name_;

  // The associated APIEventHandler. Guaranteed to outlive this object.
  APIEventHandler* const event_handler_;

  // The delegate for handling the message passing between ports. Guaranteed to
  // outlive this object.
  Delegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(GinPort);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_GIN_PORT_H_
