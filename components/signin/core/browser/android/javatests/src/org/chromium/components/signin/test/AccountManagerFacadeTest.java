// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test;

import android.accounts.Account;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.SmallTest;
import android.support.test.rule.UiThreadTestRule;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.ProfileDataSource;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;
import org.chromium.components.signin.test.util.SimpleFuture;

/**
 * Test class for {@link AccountManagerFacade}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class AccountManagerFacadeTest {
    @Rule
    public UiThreadTestRule mRule = new UiThreadTestRule();

    private FakeAccountManagerDelegate mDelegate;
    private AccountManagerFacade mHelper;

    @Before
    public void setUp() throws Exception {
        mDelegate = new FakeAccountManagerDelegate(
                FakeAccountManagerDelegate.ENABLE_PROFILE_DATA_SOURCE);
        Assert.assertFalse(mDelegate.isRegisterObserversCalled());
        AccountManagerFacade.overrideAccountManagerFacadeForTests(mDelegate);
        Assert.assertTrue(mDelegate.isRegisterObserversCalled());
        mHelper = AccountManagerFacade.get();
    }

    @After
    public void tearDown() throws Exception {
        AccountManagerFacade.resetAccountManagerFacadeForTests();
    }

    @Test
    @SmallTest
    public void testCanonicalAccount() throws InterruptedException {
        addTestAccount("test@gmail.com", "password");

        Assert.assertTrue(hasAccountForName("test@gmail.com"));
        Assert.assertTrue(hasAccountForName("Test@gmail.com"));
        Assert.assertTrue(hasAccountForName("te.st@gmail.com"));
    }

    // If this test starts flaking, please re-open crbug.com/568636 and make sure there is some sort
    // of stack trace or error message in that bug BEFORE disabling the test.
    @Test
    @SmallTest
    public void testNonCanonicalAccount() throws InterruptedException {
        addTestAccount("test.me@gmail.com", "password");

        Assert.assertTrue(hasAccountForName("test.me@gmail.com"));
        Assert.assertTrue(hasAccountForName("testme@gmail.com"));
        Assert.assertTrue(hasAccountForName("Testme@gmail.com"));
        Assert.assertTrue(hasAccountForName("te.st.me@gmail.com"));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testProfileDataSource() throws InterruptedException {
        String accountName = "test@gmail.com";
        addTestAccount(accountName, "password");
        ProfileDataSource.ProfileData profileData = new ProfileDataSource.ProfileData(
                accountName, null, "Test Full Name", "Test Given Name");

        ProfileDataSource profileDataSource = mDelegate.getProfileDataSource();
        Assert.assertNotNull(profileDataSource);
        mDelegate.setProfileData(accountName, profileData);
        Assert.assertArrayEquals(profileDataSource.getProfileDataMap().values().toArray(),
                new ProfileDataSource.ProfileData[] {profileData});

        mDelegate.setProfileData(accountName, null);
        Assert.assertArrayEquals(profileDataSource.getProfileDataMap().values().toArray(),
                new ProfileDataSource.ProfileData[0]);
    }

    private Account addTestAccount(String accountName, String password) {
        Account account = AccountManagerFacade.createAccountFromName(accountName);
        AccountHolder.Builder accountHolder =
                AccountHolder.builder(account).password(password).alwaysAccept(true);
        mDelegate.addAccountHolderExplicitly(accountHolder.build());
        return account;
    }

    private boolean hasAccountForName(final String accountName) throws InterruptedException {
        final SimpleFuture<Boolean> result = new SimpleFuture<Boolean>();
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                mHelper.hasAccountForName(accountName, result.createCallback());
            }
        });
        return result.get();
    }
}
