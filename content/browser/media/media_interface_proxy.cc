// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_interface_proxy.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/service_manager_connection.h"
#include "media/mojo/interfaces/constants.mojom.h"
#include "media/mojo/interfaces/media_service.mojom.h"
#include "media/mojo/services/media_interface_provider.h"
#include "services/service_manager/public/cpp/connector.h"

#if BUILDFLAG(ENABLE_MOJO_CDM)
#include "content/public/browser/browser_context.h"
#include "content/public/browser/provision_fetcher_impl.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "net/url_request/url_request_context_getter.h"
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "content/public/browser/cdm_registry.h"
#include "content/public/common/cdm_info.h"
#include "media/base/key_system_names.h"
#endif

namespace content {

MediaInterfaceProxy::MediaInterfaceProxy(
    RenderFrameHost* render_frame_host,
    media::mojom::InterfaceFactoryRequest request,
    const base::Closure& error_handler)
    : render_frame_host_(render_frame_host),
      binding_(this, std::move(request)) {
  DVLOG(1) << __func__;
  DCHECK(render_frame_host_);
  DCHECK(!error_handler.is_null());

  binding_.set_connection_error_handler(error_handler);

  // |interface_factory_ptr_| and |cdm_interface_factory_ptr_| will be lazily
  // connected in GetMediaInterfaceFactory() and GetCdmInterfaceFactory().
}

MediaInterfaceProxy::~MediaInterfaceProxy() {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
}

void MediaInterfaceProxy::CreateAudioDecoder(
    media::mojom::AudioDecoderRequest request) {
  DCHECK(thread_checker_.CalledOnValidThread());
  media::mojom::InterfaceFactory* factory = GetMediaInterfaceFactory();
  if (factory)
    factory->CreateAudioDecoder(std::move(request));
}

void MediaInterfaceProxy::CreateVideoDecoder(
    media::mojom::VideoDecoderRequest request) {
  DCHECK(thread_checker_.CalledOnValidThread());
  media::mojom::InterfaceFactory* factory = GetMediaInterfaceFactory();
  if (factory)
    factory->CreateVideoDecoder(std::move(request));
}

void MediaInterfaceProxy::CreateRenderer(
    const std::string& audio_device_id,
    media::mojom::RendererRequest request) {
  DCHECK(thread_checker_.CalledOnValidThread());
  media::mojom::InterfaceFactory* factory = GetMediaInterfaceFactory();
  if (factory)
    factory->CreateRenderer(audio_device_id, std::move(request));
}

void MediaInterfaceProxy::CreateCdm(
    const std::string& key_system,
    media::mojom::ContentDecryptionModuleRequest request) {
  DCHECK(thread_checker_.CalledOnValidThread());
  media::mojom::InterfaceFactory* factory = GetCdmInterfaceFactory(key_system);
  if (factory)
    factory->CreateCdm(key_system, std::move(request));
}

media::mojom::InterfaceFactory*
MediaInterfaceProxy::GetMediaInterfaceFactory() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!interface_factory_ptr_)
    ConnectToMediaService();

  return interface_factory_ptr_.get();
}

media::mojom::InterfaceFactory* MediaInterfaceProxy::GetCdmInterfaceFactory(
    const std::string& key_system) {
  DCHECK(thread_checker_.CalledOnValidThread());
#if !BUILDFLAG(ENABLE_STANDALONE_CDM_SERVICE)
  return GetMediaInterfaceFactory();
#else
  if (!cdm_interface_factory_ptr_)
    ConnectToCdmService(key_system);

  return cdm_interface_factory_ptr_.get();
#endif
}

void MediaInterfaceProxy::OnMediaServiceConnectionError() {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  interface_factory_ptr_.reset();
}

void MediaInterfaceProxy::OnCdmServiceConnectionError() {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  cdm_interface_factory_ptr_.reset();
}

service_manager::mojom::InterfaceProviderPtr
MediaInterfaceProxy::GetFrameServices() {
  // Register frame services.
  service_manager::mojom::InterfaceProviderPtr interfaces;

  // TODO(xhwang): Replace this InterfaceProvider with a dedicated media host
  // interface. See http://crbug.com/660573
  auto provider = std::make_unique<media::MediaInterfaceProvider>(
      mojo::MakeRequest(&interfaces));

#if BUILDFLAG(ENABLE_MOJO_CDM)
  // TODO(slan): Wrap these into a RenderFrame specific ProvisionFetcher impl.
  net::URLRequestContextGetter* context_getter =
      BrowserContext::GetDefaultStoragePartition(
          render_frame_host_->GetProcess()->GetBrowserContext())
          ->GetURLRequestContext();
  provider->registry()->AddInterface(base::Bind(
      &ProvisionFetcherImpl::Create, base::RetainedRef(context_getter)));
#endif  // BUILDFLAG(ENABLE_MOJO_CDM)

  GetContentClient()->browser()->ExposeInterfacesToMediaService(
      provider->registry(), render_frame_host_);

  media_registries_.push_back(std::move(provider));

  return interfaces;
}

void MediaInterfaceProxy::ConnectToMediaService() {
  DVLOG(1) << __func__;
  DCHECK(!interface_factory_ptr_);

  media::mojom::MediaServicePtr media_service;

  // TODO(slan): Use the BrowserContext Connector instead. See crbug.com/638950.
  service_manager::Connector* connector =
      ServiceManagerConnection::GetForProcess()->GetConnector();
  connector->BindInterface(media::mojom::kMediaServiceName, &media_service);

  media_service->CreateInterfaceFactory(MakeRequest(&interface_factory_ptr_),
                                        GetFrameServices());

  interface_factory_ptr_.set_connection_error_handler(
      base::BindOnce(&MediaInterfaceProxy::OnMediaServiceConnectionError,
                     base::Unretained(this)));
}

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
static std::unique_ptr<CdmInfo> GetCdmInfoForKeySystem(
    const std::string& key_system) {
  DVLOG(2) << __func__ << ": key_system = " << key_system;
  for (const auto& cdm : CdmRegistry::GetInstance()->GetAllRegisteredCdms()) {
    if (cdm.supported_key_system == key_system ||
        (cdm.supports_sub_key_systems &&
         media::IsChildKeySystemOf(key_system, cdm.supported_key_system))) {
      return std::make_unique<CdmInfo>(cdm);
    }
  }

  return nullptr;
}
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

void MediaInterfaceProxy::ConnectToCdmService(const std::string& key_system) {
  DVLOG(1) << __func__ << ": key_system = " << key_system;
  DCHECK(!cdm_interface_factory_ptr_);

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  // The |key_system| comes from the render process which we cannot fully trust.
  // Handle failure cases gracefully!
  auto cdm_info = GetCdmInfoForKeySystem(key_system);
  if (!cdm_info) {
    NOTREACHED() << "Key system " << key_system << " not registered.";
    return;
  }
  base::FilePath cdm_path = cdm_info->path;
  if (cdm_path.empty()) {
    NOTREACHED() << "CDM path for " << key_system << " is empty.";
    return;
  }
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

  // TODO(slan): Use the BrowserContext Connector instead. See crbug.com/638950.
  service_manager::Connector* connector =
      ServiceManagerConnection::GetForProcess()->GetConnector();

  // TODO(crbug.com/510604): Use different "User ID" for different CDM types to
  // run each type of CDM in its own process.
  media::mojom::MediaServicePtr media_service;
  connector->BindInterface(media::mojom::kCdmServiceName, &media_service);

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  // LoadCdm() should always be called before CreateInterfaceFactory().
  media_service->LoadCdm(cdm_path);
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

  media_service->CreateInterfaceFactory(
      MakeRequest(&cdm_interface_factory_ptr_), GetFrameServices());

  cdm_interface_factory_ptr_.set_connection_error_handler(
      base::BindOnce(&MediaInterfaceProxy::OnCdmServiceConnectionError,
                     base::Unretained(this)));
}

}  // namespace content
