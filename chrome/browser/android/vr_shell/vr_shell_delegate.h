// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_DELEGATE_H_

#include <jni.h>

#include <map>
#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "chrome/browser/android/vr_shell/vr_core_info.h"
#include "chrome/browser/android/vr_shell/vr_usage_monitor.h"
#include "device/vr/android/gvr/gvr_delegate_provider.h"
#include "device/vr/vr_service.mojom.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"

namespace content {
class RenderWidgetHost;
}

namespace device {
class VRDevice;
}

namespace vr_shell {

class DelegateWebContentsObserver;
class VrShell;

class VrShellDelegate : public device::GvrDelegateProvider {
 public:
  VrShellDelegate(JNIEnv* env, jobject obj);
  ~VrShellDelegate() override;

  static device::GvrDelegateProvider* CreateVrShellDelegate();

  static VrShellDelegate* GetNativeVrShellDelegate(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jdelegate);

  void SetDelegate(VrShell* vr_shell, gvr::ViewerType viewer_type);
  void RemoveDelegate();

  void SetPresentResult(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        jboolean success);
  void DisplayActivate(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj);
  void OnPause(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void OnResume(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  bool IsClearActivatePending(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& obj);
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  void OnWebContentsFocused(content::RenderWidgetHost* host);
  void OnWebContentsLostFocus(content::RenderWidgetHost* host);

  device::VRDevice* GetDevice();

  // device::GvrDelegateProvider implementation.
  void ExitWebVRPresent() override;

 private:
  // device::GvrDelegateProvider implementation.
  void SetDeviceId(unsigned int device_id) override;
  void RequestWebVRPresent(device::mojom::VRSubmitFrameClientPtr submit_client,
                           device::mojom::VRPresentationProviderRequest request,
                           device::mojom::VRDisplayInfoPtr display_info,
                           const base::Callback<void(bool)>& callback) override;
  void OnDisplayAdded(device::VRDisplayImpl* display) override;
  void OnDisplayRemoved(device::VRDisplayImpl* display) override;
  void OnListeningForActivateChanged(device::VRDisplayImpl* display) override;
  void GetNextMagicWindowPose(
      gvr::GvrApi* gvr_api,
      device::VRDisplayImpl* display,
      device::mojom::VRDisplay::GetNextMagicWindowPoseCallback callback)
      override;

  void OnActivateDisplayHandled(bool will_not_present);
  void OnFocusedAndActivatable(device::VRDisplayImpl* display);
  void OnLostFocusedAndActivatable();
  void SetListeningForActivate(bool listening);
  void SetPresentResult(bool success);

  std::unique_ptr<VrCoreInfo> MakeVrCoreInfo(JNIEnv* env);

  base::android::ScopedJavaGlobalRef<jobject> j_vr_shell_delegate_;
  unsigned int device_id_ = 0;
  VrShell* vr_shell_ = nullptr;
  base::Callback<void(bool)> present_callback_;
  device::mojom::VRSubmitFrameClientPtr submit_client_;
  device::mojom::VRPresentationProviderRequest presentation_provider_request_;
  device::mojom::VRDisplayInfoPtr display_info_;
  bool pending_successful_present_request_ = false;

  std::map<content::RenderWidgetHost*, device::VRDisplayImpl*> displays_;
  std::map<device::VRDisplayImpl*, std::unique_ptr<DelegateWebContentsObserver>>
      observers_;
  device::VRDisplayImpl* activatable_display_ = nullptr;
  base::CancelableClosure clear_activate_task_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtrFactory<VrShellDelegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VrShellDelegate);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_DELEGATE_H_
