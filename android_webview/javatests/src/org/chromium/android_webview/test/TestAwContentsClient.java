// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.graphics.Bitmap;
import android.graphics.Picture;
import android.net.http.SslError;

import org.chromium.android_webview.AwConsoleMessage;
import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;
import org.chromium.android_webview.AwWebResourceResponse;
import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageCommitVisibleHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageStartedHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnReceivedErrorHelper;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

/**
 * AwContentsClient subclass used for testing.
 */
public class TestAwContentsClient extends NullContentsClient {
    private boolean mAllowSslError;
    private final OnPageStartedHelper mOnPageStartedHelper;
    private final OnPageFinishedHelper mOnPageFinishedHelper;
    private final OnPageCommitVisibleHelper mOnPageCommitVisibleHelper;
    private final OnReceivedErrorHelper mOnReceivedErrorHelper;
    private final OnReceivedError2Helper mOnReceivedError2Helper;
    private final OnReceivedHttpErrorHelper mOnReceivedHttpErrorHelper;
    private final CallbackHelper mOnReceivedSslErrorHelper;
    private final OnDownloadStartHelper mOnDownloadStartHelper;
    private final OnReceivedLoginRequestHelper mOnReceivedLoginRequestHelper;
    private final OnEvaluateJavaScriptResultHelper mOnEvaluateJavaScriptResultHelper;
    private final AddMessageToConsoleHelper mAddMessageToConsoleHelper;
    private final OnScaleChangedHelper mOnScaleChangedHelper;
    private final OnReceivedTitleHelper mOnReceivedTitleHelper;
    private final PictureListenerHelper mPictureListenerHelper;
    private final ShouldOverrideUrlLoadingHelper mShouldOverrideUrlLoadingHelper;
    private final DoUpdateVisitedHistoryHelper mDoUpdateVisitedHistoryHelper;
    private final OnCreateWindowHelper mOnCreateWindowHelper;
    private final FaviconHelper mFaviconHelper;
    private final TouchIconHelper mTouchIconHelper;

    public TestAwContentsClient() {
        super(ThreadUtils.getUiThreadLooper());
        mOnPageStartedHelper = new OnPageStartedHelper();
        mOnPageFinishedHelper = new OnPageFinishedHelper();
        mOnPageCommitVisibleHelper = new OnPageCommitVisibleHelper();
        mOnReceivedErrorHelper = new OnReceivedErrorHelper();
        mOnReceivedError2Helper = new OnReceivedError2Helper();
        mOnReceivedHttpErrorHelper = new OnReceivedHttpErrorHelper();
        mOnReceivedSslErrorHelper = new CallbackHelper();
        mOnDownloadStartHelper = new OnDownloadStartHelper();
        mOnReceivedLoginRequestHelper = new OnReceivedLoginRequestHelper();
        mOnEvaluateJavaScriptResultHelper = new OnEvaluateJavaScriptResultHelper();
        mAddMessageToConsoleHelper = new AddMessageToConsoleHelper();
        mOnScaleChangedHelper = new OnScaleChangedHelper();
        mOnReceivedTitleHelper = new OnReceivedTitleHelper();
        mPictureListenerHelper = new PictureListenerHelper();
        mShouldOverrideUrlLoadingHelper = new ShouldOverrideUrlLoadingHelper();
        mDoUpdateVisitedHistoryHelper = new DoUpdateVisitedHistoryHelper();
        mOnCreateWindowHelper = new OnCreateWindowHelper();
        mFaviconHelper = new FaviconHelper();
        mTouchIconHelper = new TouchIconHelper();
        mAllowSslError = true;
    }

    public OnPageStartedHelper getOnPageStartedHelper() {
        return mOnPageStartedHelper;
    }

    public OnPageCommitVisibleHelper getOnPageCommitVisibleHelper() {
        return mOnPageCommitVisibleHelper;
    }

    public OnPageFinishedHelper getOnPageFinishedHelper() {
        return mOnPageFinishedHelper;
    }

    public OnReceivedErrorHelper getOnReceivedErrorHelper() {
        return mOnReceivedErrorHelper;
    }

    public OnReceivedError2Helper getOnReceivedError2Helper() {
        return mOnReceivedError2Helper;
    }

    public OnReceivedHttpErrorHelper getOnReceivedHttpErrorHelper() {
        return mOnReceivedHttpErrorHelper;
    }

    public CallbackHelper getOnReceivedSslErrorHelper() {
        return mOnReceivedSslErrorHelper;
    }

    public OnDownloadStartHelper getOnDownloadStartHelper() {
        return mOnDownloadStartHelper;
    }

    public OnReceivedLoginRequestHelper getOnReceivedLoginRequestHelper() {
        return mOnReceivedLoginRequestHelper;
    }

    public OnEvaluateJavaScriptResultHelper getOnEvaluateJavaScriptResultHelper() {
        return mOnEvaluateJavaScriptResultHelper;
    }

    public ShouldOverrideUrlLoadingHelper getShouldOverrideUrlLoadingHelper() {
        return mShouldOverrideUrlLoadingHelper;
    }

    public AddMessageToConsoleHelper getAddMessageToConsoleHelper() {
        return mAddMessageToConsoleHelper;
    }

    public DoUpdateVisitedHistoryHelper getDoUpdateVisitedHistoryHelper() {
        return mDoUpdateVisitedHistoryHelper;
    }

    public OnCreateWindowHelper getOnCreateWindowHelper() {
        return mOnCreateWindowHelper;
    }

    public FaviconHelper getFaviconHelper() {
        return mFaviconHelper;
    }

    public TouchIconHelper getTouchIconHelper() {
        return mTouchIconHelper;
    }

    /**
     * Callback helper for onScaleChangedScaled.
     */
    public static class OnScaleChangedHelper extends CallbackHelper {
        private float mPreviousScale;
        private float mCurrentScale;
        public void notifyCalled(float oldScale, float newScale) {
            mPreviousScale = oldScale;
            mCurrentScale = newScale;
            super.notifyCalled();
        }

        public float getOldScale() {
            return mPreviousScale;
        }

        public float getNewScale() {
            return mCurrentScale;
        }

        public float getLastScaleRatio() {
            assert getCallCount() > 0;
            return mCurrentScale / mPreviousScale;
        }
    }

    public OnScaleChangedHelper getOnScaleChangedHelper() {
        return mOnScaleChangedHelper;
    }

    public PictureListenerHelper getPictureListenerHelper() {
        return mPictureListenerHelper;
    }

    /**
     * Callback helper for onReceivedTitle.
     */
    public static class OnReceivedTitleHelper extends CallbackHelper {
        private String mTitle;

        public void notifyCalled(String title) {
            mTitle = title;
            super.notifyCalled();
        }
        public String getTitle() {
            return mTitle;
        }
    }

    public OnReceivedTitleHelper getOnReceivedTitleHelper() {
        return mOnReceivedTitleHelper;
    }

    @Override
    public void onReceivedTitle(String title) {
        mOnReceivedTitleHelper.notifyCalled(title);
    }

    public String getUpdatedTitle() {
        return mOnReceivedTitleHelper.getTitle();
    }

    @Override
    public void onPageStarted(String url) {
        mOnPageStartedHelper.notifyCalled(url);
    }

    @Override
    public void onPageCommitVisible(String url) {
        mOnPageCommitVisibleHelper.notifyCalled(url);
    }

    @Override
    public void onPageFinished(String url) {
        mOnPageFinishedHelper.notifyCalled(url);
    }

    @Override
    public void onReceivedError(int errorCode, String description, String failingUrl) {
        mOnReceivedErrorHelper.notifyCalled(errorCode, description, failingUrl);
    }

    @Override
    public void onReceivedError2(AwWebResourceRequest request, AwWebResourceError error) {
        mOnReceivedError2Helper.notifyCalled(request, error);
    }

    @Override
    public void onReceivedSslError(Callback<Boolean> callback, SslError error) {
        callback.onResult(mAllowSslError);
        mOnReceivedSslErrorHelper.notifyCalled();
    }

    public void setAllowSslError(boolean allow) {
        mAllowSslError = allow;
    }

    /**
     * CallbackHelper for OnDownloadStart.
     */
    public static class OnDownloadStartHelper extends CallbackHelper {
        private String mUrl;
        private String mUserAgent;
        private String mContentDisposition;
        private String mMimeType;
        long mContentLength;

        public String getUrl() {
            assert getCallCount() > 0;
            return mUrl;
        }

        public String getUserAgent() {
            assert getCallCount() > 0;
            return mUserAgent;
        }

        public String getContentDisposition() {
            assert getCallCount() > 0;
            return mContentDisposition;
        }

        public String getMimeType() {
            assert getCallCount() > 0;
            return mMimeType;
        }

        public long getContentLength() {
            assert getCallCount() > 0;
            return mContentLength;
        }

        public void notifyCalled(String url, String userAgent, String contentDisposition,
                String mimeType, long contentLength) {
            mUrl = url;
            mUserAgent = userAgent;
            mContentDisposition = contentDisposition;
            mMimeType = mimeType;
            mContentLength = contentLength;
            notifyCalled();
        }
    }

    @Override
    public void onDownloadStart(String url,
            String userAgent,
            String contentDisposition,
            String mimeType,
            long contentLength) {
        getOnDownloadStartHelper().notifyCalled(url, userAgent, contentDisposition, mimeType,
                contentLength);
    }

    /**
     * Callback helper for onCreateWindow.
     */
    public static class OnCreateWindowHelper extends CallbackHelper {
        private boolean mIsDialog;
        private boolean mIsUserGesture;
        private boolean mReturnValue;

        public boolean getIsDialog() {
            assert getCallCount() > 0;
            return mIsDialog;
        }

        public boolean getUserAgent() {
            assert getCallCount() > 0;
            return mIsUserGesture;
        }

        public void setReturnValue(boolean returnValue) {
            mReturnValue = returnValue;
        }

        public boolean notifyCalled(boolean isDialog, boolean isUserGesture) {
            mIsDialog = isDialog;
            mIsUserGesture = isUserGesture;
            boolean returnValue = mReturnValue;
            notifyCalled();
            return returnValue;
        }
    }

    @Override
    public boolean onCreateWindow(boolean isDialog, boolean isUserGesture) {
        return mOnCreateWindowHelper.notifyCalled(isDialog, isUserGesture);
    }

    /**
     * CallbackHelper for OnReceivedLoginRequest.
     */
    public static class OnReceivedLoginRequestHelper extends CallbackHelper {
        private String mRealm;
        private String mAccount;
        private String mArgs;

        public String getRealm() {
            assert getCallCount() > 0;
            return mRealm;
        }

        public String getAccount() {
            assert getCallCount() > 0;
            return mAccount;
        }

        public String getArgs() {
            assert getCallCount() > 0;
            return mArgs;
        }

        public void notifyCalled(String realm, String account, String args) {
            mRealm = realm;
            mAccount = account;
            mArgs = args;
            notifyCalled();
        }
    }

    @Override
    public void onReceivedLoginRequest(String realm, String account, String args) {
        getOnReceivedLoginRequestHelper().notifyCalled(realm, account, args);
    }

    @Override
    public boolean onConsoleMessage(AwConsoleMessage consoleMessage) {
        mAddMessageToConsoleHelper.notifyCalled(consoleMessage);
        return false;
    }

    /**
     * Callback helper for AddMessageToConsole.
     */
    public static class AddMessageToConsoleHelper extends CallbackHelper {
        private List<AwConsoleMessage> mMessages = new ArrayList<AwConsoleMessage>();

        public void clearMessages() {
            mMessages.clear();
        }

        public List<AwConsoleMessage> getMessages() {
            return mMessages;
        }

        public int getLevel() {
            assert getCallCount() > 0;
            return getLastMessage().messageLevel();
        }

        public String getMessage() {
            assert getCallCount() > 0;
            return getLastMessage().message();
        }

        public int getLineNumber() {
            assert getCallCount() > 0;
            return getLastMessage().lineNumber();
        }

        public String getSourceId() {
            assert getCallCount() > 0;
            return getLastMessage().sourceId();
        }

        private AwConsoleMessage getLastMessage() {
            return mMessages.get(mMessages.size() - 1);
        }

        void notifyCalled(AwConsoleMessage message) {
            mMessages.add(message);
            notifyCalled();
        }
    }

    @Override
    public void onScaleChangedScaled(float oldScale, float newScale) {
        mOnScaleChangedHelper.notifyCalled(oldScale, newScale);
    }

    /**
     * Callback helper for PictureListener.
     */
    public static class PictureListenerHelper extends CallbackHelper {
        // Generally null, depending on |invalidationOnly| in enableOnNewPicture()
        private Picture mPicture;

        public Picture getPicture() {
            assert getCallCount() > 0;
            return mPicture;
        }

        void notifyCalled(Picture picture) {
            mPicture = picture;
            notifyCalled();
        }
    }

    @Override
    public void onNewPicture(Picture picture) {
        mPictureListenerHelper.notifyCalled(picture);
    }

    /**
     * Callback helper for ShouldOverrideUrlLoading.
     */
    public static class ShouldOverrideUrlLoadingHelper extends CallbackHelper {
        private String mShouldOverrideUrlLoadingUrl;
        private boolean mShouldOverrideUrlLoadingReturnValue;
        private boolean mIsRedirect;
        private boolean mHasUserGesture;
        private boolean mIsMainFrame;
        void setShouldOverrideUrlLoadingUrl(String url) {
            mShouldOverrideUrlLoadingUrl = url;
        }
        void setShouldOverrideUrlLoadingReturnValue(boolean value) {
            mShouldOverrideUrlLoadingReturnValue = value;
        }
        public String getShouldOverrideUrlLoadingUrl() {
            assert getCallCount() > 0;
            return mShouldOverrideUrlLoadingUrl;
        }
        public boolean getShouldOverrideUrlLoadingReturnValue() {
            return mShouldOverrideUrlLoadingReturnValue;
        }
        public boolean isRedirect() {
            return mIsRedirect;
        }
        public boolean hasUserGesture() {
            return mHasUserGesture;
        }
        public boolean isMainFrame() {
            return mIsMainFrame;
        }
        public void notifyCalled(
                String url, boolean isRedirect, boolean hasUserGesture, boolean isMainFrame) {
            mShouldOverrideUrlLoadingUrl = url;
            mIsRedirect = isRedirect;
            mHasUserGesture = hasUserGesture;
            mIsMainFrame = isMainFrame;
            notifyCalled();
        }
    }

    @Override
    public boolean shouldOverrideUrlLoading(AwWebResourceRequest request) {
        super.shouldOverrideUrlLoading(request);
        boolean returnValue =
                mShouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingReturnValue();
        mShouldOverrideUrlLoadingHelper.notifyCalled(
                request.url, request.isRedirect, request.hasUserGesture, request.isMainFrame);
        return returnValue;
    }


    /**
     * Callback helper for doUpdateVisitedHistory.
     */
    public static class DoUpdateVisitedHistoryHelper extends CallbackHelper {
        String mUrl;
        boolean mIsReload;

        public String getUrl() {
            assert getCallCount() > 0;
            return mUrl;
        }

        public boolean getIsReload() {
            assert getCallCount() > 0;
            return mIsReload;
        }

        public void notifyCalled(String url, boolean isReload) {
            mUrl = url;
            mIsReload = isReload;
            notifyCalled();
        }
    }

    @Override
    public void doUpdateVisitedHistory(String url, boolean isReload) {
        getDoUpdateVisitedHistoryHelper().notifyCalled(url, isReload);
    }

    /**
     * CallbackHelper for OnReceivedError2.
     */
    public static class OnReceivedError2Helper extends CallbackHelper {
        private AwWebResourceRequest mRequest;
        private AwWebResourceError mError;
        public void notifyCalled(AwWebResourceRequest request, AwWebResourceError error) {
            mRequest = request;
            mError = error;
            notifyCalled();
        }
        public AwWebResourceRequest getRequest() {
            assert getCallCount() > 0;
            return mRequest;
        }
        public AwWebResourceError getError() {
            assert getCallCount() > 0;
            return mError;
        }
    }

    /**
     * CallbackHelper for OnReceivedHttpError.
     */
    public static class OnReceivedHttpErrorHelper extends CallbackHelper {
        private AwWebResourceRequest mRequest;
        private AwWebResourceResponse mResponse;

        public void notifyCalled(AwWebResourceRequest request, AwWebResourceResponse response) {
            mRequest = request;
            mResponse = response;
            notifyCalled();
        }
        public AwWebResourceRequest getRequest() {
            assert getCallCount() > 0;
            return mRequest;
        }
        public AwWebResourceResponse getResponse() {
            assert getCallCount() > 0;
            return mResponse;
        }
    }

    @Override
    public void onReceivedHttpError(AwWebResourceRequest request, AwWebResourceResponse response) {
        super.onReceivedHttpError(request, response);
        mOnReceivedHttpErrorHelper.notifyCalled(request, response);
    }

    /**
     * CallbackHelper for onReceivedIcon.
     */
    public static class FaviconHelper extends CallbackHelper {
        private Bitmap mIcon;

        public void notifyFavicon(Bitmap icon) {
            mIcon = icon;
            super.notifyCalled();
        }

        public Bitmap getIcon() {
            assert getCallCount() > 0;
            return mIcon;
        }
    }

    @Override
    public void onReceivedIcon(Bitmap bitmap) {
        // We don't inform the API client about the URL of the icon.
        mFaviconHelper.notifyFavicon(bitmap);
    }

    /**
     * CallbackHelper for onReceivedTouchIconUrl.
     */
    public static class TouchIconHelper extends CallbackHelper {
        private HashMap<String, Boolean> mTouchIcons = new HashMap<String, Boolean>();

        public void notifyTouchIcon(String url, boolean precomposed) {
            mTouchIcons.put(url, precomposed);
            super.notifyCalled();
        }

        public int getTouchIconsCount() {
            assert getCallCount() > 0;
            return mTouchIcons.size();
        }

        public boolean hasTouchIcon(String url) {
            return mTouchIcons.get(url);
        }
    }

    @Override
    public void onReceivedTouchIconUrl(String url, boolean precomposed) {
        mTouchIconHelper.notifyTouchIcon(url, precomposed);
    }
}
