// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Bundle;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowApplication;

import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.test.WebApkTestHelper;

/** Unit tests for {@link MainActivity}.
 *
 * Note: In real word, |loggedIntentUrlParam| is set to be nonempty iff intent url is outside of the
 * scope specified in the Android manifest, so in the test we always have these two conditions
 * together.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, packageName = WebApkUtilsTest.WEBAPK_PACKAGE_NAME)
public final class MainActivityTest {
    /**
     * Test that MainActivity uses the manifest start URL and appends the intent url as a paramater,
     * if intent URL scheme does not match the scope url.
     */
    @Test
    public void testIntentUrlOutOfScopeBecauseOfScheme() {
        final String intentStartUrl = "http://www.google.com/search_results?q=eh#cr=countryCA";
        final String manifestStartUrl = "https://www.google.com/index.html";
        final String manifestScope = "https://www.google.com/";
        final String expectedStartUrl =
                "https://www.google.com/index.html?originalUrl=http%253A%252F%252Fwww.google.com%252Fsearch_results%253Fq%253Deh%2523cr%253DcountryCA";
        final String browserPackageName = "com.android.chrome";

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, manifestStartUrl);
        bundle.putString(WebApkMetaDataKeys.SCOPE, manifestScope);
        bundle.putString(WebApkMetaDataKeys.RUNTIME_HOST, browserPackageName);
        bundle.putString(WebApkMetaDataKeys.LOGGED_INTENT_URL_PARAM, "originalUrl");
        WebApkTestHelper.registerWebApkWithMetaData(WebApkUtilsTest.WEBAPK_PACKAGE_NAME, bundle);

        installBrowser(browserPackageName);

        Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(intentStartUrl));
        Robolectric.buildActivity(MainActivity.class).withIntent(launchIntent).create();

        Intent startActivityIntent = ShadowApplication.getInstance().getNextStartedActivity();
        Assert.assertEquals(MainActivity.ACTION_START_WEBAPK, startActivityIntent.getAction());
        Assert.assertEquals(
                expectedStartUrl, startActivityIntent.getStringExtra(WebApkConstants.EXTRA_URL));
    }

    /**
     * Test that MainActivity uses the manifest start URL and appends the intent url as a paramater,
     * if intent URL path is outside of the scope specified in the Android Manifest.
     */
    @Test
    public void testIntentUrlOutOfScopeBecauseOfPath() {
        final String intentStartUrl = "https://www.google.com/maps/";
        final String manifestStartUrl = "https://www.google.com/maps/contrib/startUrl";
        final String manifestScope = "https://www.google.com/maps/contrib/";
        final String expectedStartUrl =
                "https://www.google.com/maps/contrib/startUrl?originalUrl=https%253A%252F%252Fwww.google.com%252Fmaps%252F";
        final String browserPackageName = "com.android.chrome";

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, manifestStartUrl);
        bundle.putString(WebApkMetaDataKeys.SCOPE, manifestScope);
        bundle.putString(WebApkMetaDataKeys.RUNTIME_HOST, browserPackageName);
        bundle.putString(WebApkMetaDataKeys.LOGGED_INTENT_URL_PARAM, "originalUrl");
        WebApkTestHelper.registerWebApkWithMetaData(WebApkUtilsTest.WEBAPK_PACKAGE_NAME, bundle);

        installBrowser(browserPackageName);

        Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(intentStartUrl));
        Robolectric.buildActivity(MainActivity.class).withIntent(launchIntent).create();

        Intent startActivityIntent = ShadowApplication.getInstance().getNextStartedActivity();
        Assert.assertEquals(MainActivity.ACTION_START_WEBAPK, startActivityIntent.getAction());
        Assert.assertEquals(
                expectedStartUrl, startActivityIntent.getStringExtra(WebApkConstants.EXTRA_URL));
    }

    /**
     * Test that the intent URL is not rewritten if it is inside the scope specified in the Android
     * Manifest.
     */
    @Test
    public void testNotRewriteStartUrlInsideScope() {
        final String intentStartUrl = "https://www.google.com/maps/address?A=a";
        final String manifestStartUrl = "https://www.google.com/maps/startUrl";
        final String manifestScope = "https://www.google.com/maps";
        final String browserPackageName = "com.android.chrome";

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, manifestStartUrl);
        bundle.putString(WebApkMetaDataKeys.SCOPE, manifestScope);
        bundle.putString(WebApkMetaDataKeys.RUNTIME_HOST, browserPackageName);
        bundle.putString(WebApkMetaDataKeys.LOGGED_INTENT_URL_PARAM, "originalUrl");
        WebApkTestHelper.registerWebApkWithMetaData(WebApkUtilsTest.WEBAPK_PACKAGE_NAME, bundle);

        installBrowser(browserPackageName);

        Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(intentStartUrl));
        Robolectric.buildActivity(MainActivity.class).withIntent(launchIntent).create();

        Intent startActivityIntent = ShadowApplication.getInstance().getNextStartedActivity();
        Assert.assertEquals(MainActivity.ACTION_START_WEBAPK, startActivityIntent.getAction());
        Assert.assertEquals(
                intentStartUrl, startActivityIntent.getStringExtra(WebApkConstants.EXTRA_URL));
    }

    /**
     * Test that MainActivity uses the manifest start URL and appends the intent url as a paramater,
     * if intent URL host includes unicode characters, and the host name is different from the scope
     * url host specified in the Android Manifest. In particular, MainActivity should not escape
     * unicode characters.
     */
    @Test
    public void testRewriteUnicodeHost() {
        final String intentStartUrl = "https://www.google.com/";
        final String manifestStartUrl = "https://www.☺.com/";
        final String scope = "https://www.☺.com/";
        final String expectedStartUrl =
                "https://www.☺.com/?originalUrl=https%253A%252F%252Fwww.google.com%252F";
        final String browserPackageName = "com.android.chrome";

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, manifestStartUrl);
        bundle.putString(WebApkMetaDataKeys.SCOPE, scope);
        bundle.putString(WebApkMetaDataKeys.RUNTIME_HOST, browserPackageName);
        bundle.putString(WebApkMetaDataKeys.LOGGED_INTENT_URL_PARAM, "originalUrl");
        WebApkTestHelper.registerWebApkWithMetaData(WebApkUtilsTest.WEBAPK_PACKAGE_NAME, bundle);

        installBrowser(browserPackageName);

        Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(intentStartUrl));
        Robolectric.buildActivity(MainActivity.class).withIntent(launchIntent).create();

        Intent startActivityIntent = ShadowApplication.getInstance().getNextStartedActivity();
        Assert.assertEquals(MainActivity.ACTION_START_WEBAPK, startActivityIntent.getAction());
        Assert.assertEquals(
                expectedStartUrl, startActivityIntent.getStringExtra(WebApkConstants.EXTRA_URL));
    }

    private void installBrowser(String browserPackageName) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://"));
        RuntimeEnvironment.getRobolectricPackageManager().addResolveInfoForIntent(
                intent, newResolveInfo(browserPackageName));
    }

    private static ResolveInfo newResolveInfo(String packageName) {
        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = packageName;
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = activityInfo;
        return resolveInfo;
    }
}
