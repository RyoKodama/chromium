// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/background_fetch/mock_background_fetch_delegate.h"
#include "content/public/browser/background_fetch_response.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_response_headers.h"

namespace content {

MockBackgroundFetchDelegate::TestResponse::TestResponse() = default;

MockBackgroundFetchDelegate::TestResponse::~TestResponse() = default;

MockBackgroundFetchDelegate::TestResponseBuilder::TestResponseBuilder(
    int response_code)
    : response_(base::MakeUnique<TestResponse>()) {
  response_->succeeded_ = (response_code >= 200 && response_code < 300);
  response_->headers = make_scoped_refptr(new net::HttpResponseHeaders(
      "HTTP/1.1 " + std::to_string(response_code)));
}

MockBackgroundFetchDelegate::TestResponseBuilder::~TestResponseBuilder() =
    default;

MockBackgroundFetchDelegate::TestResponseBuilder&
MockBackgroundFetchDelegate::TestResponseBuilder::AddResponseHeader(
    const std::string& name,
    const std::string& value) {
  DCHECK(response_);
  response_->headers->AddHeader(name + ": " + value);
  return *this;
}

MockBackgroundFetchDelegate::TestResponseBuilder&
MockBackgroundFetchDelegate::TestResponseBuilder::SetResponseData(
    std::string data) {
  DCHECK(response_);
  response_->data.swap(data);
  return *this;
}

std::unique_ptr<MockBackgroundFetchDelegate::TestResponse>
MockBackgroundFetchDelegate::TestResponseBuilder::Build() {
  return std::move(response_);
}

MockBackgroundFetchDelegate::MockBackgroundFetchDelegate() {}

MockBackgroundFetchDelegate::~MockBackgroundFetchDelegate() {}

void MockBackgroundFetchDelegate::DownloadUrl(
    const std::string& guid,
    const std::string& method,
    const GURL& url,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const net::HttpRequestHeaders& headers) {
  // TODO(delphick): Currently we just disallow re-using GUIDs but later when we
  // use the DownloadService, we should signal StartResult::UNEXPECTED_GUID.
  DCHECK(seen_guids_.find(guid) == seen_guids_.end());

  std::unique_ptr<TestResponse> test_response;
  scoped_refptr<const net::HttpResponseHeaders> response_headers;

  auto url_iter = url_responses_.find(url);
  if (url_iter != url_responses_.end()) {
    test_response = std::move(url_iter->second);
    url_responses_.erase(url_iter);
  } else {
    // TODO(delphick): When we use the DownloadService, we should signal
    // StartResult::INTERNAL_ERROR to say the URL wasn't registered rather than
    // assuming 404.
    test_response = TestResponseBuilder(404).Build();
  }

  response_headers = test_response->headers;

  std::unique_ptr<BackgroundFetchResponse> response =
      std::make_unique<BackgroundFetchResponse>(std::vector<GURL>({url}),
                                                response_headers);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&BackgroundFetchDelegate::Client::OnDownloadStarted,
                     client(), guid, std::move(response)));

  if (test_response->succeeded_) {
    base::FilePath response_path;
    if (!temp_directory_.IsValid()) {
      CHECK(temp_directory_.CreateUniqueTempDir());
    }

    // Write the |response|'s data to a temporary file.
    CHECK(base::CreateTemporaryFileInDir(temp_directory_.GetPath(),
                                         &response_path));

    CHECK_NE(-1 /* error */,
             base::WriteFile(response_path, test_response->data.c_str(),
                             test_response->data.size()));

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &BackgroundFetchDelegate::Client::OnDownloadComplete, client(),
            guid,
            std::make_unique<BackgroundFetchResult>(
                base::Time::Now(), response_path, test_response->data.size())));
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &BackgroundFetchDelegate::Client::OnDownloadComplete, client(),
            guid, std::make_unique<BackgroundFetchResult>(base::Time::Now())));
  }

  seen_guids_.insert(guid);
}

void MockBackgroundFetchDelegate::RegisterResponse(
    const GURL& url,
    std::unique_ptr<TestResponse> response) {
  DCHECK_EQ(0u, url_responses_.count(url));
  url_responses_[url] = std::move(response);
}

}  // namespace content
