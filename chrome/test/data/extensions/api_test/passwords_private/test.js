// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This just tests the interface. It does not test for specific results, only
// that callbacks are correctly invoked, expected parameters are correct,
// and failures are detected.

var availableTests = [
  function removeSavedPassword() {
    var numCalls = 0;
    var numSavedPasswords;
    var callback = function(savedPasswordsList) {
      numCalls++;

      if (numCalls == 1) {
        numSavedPasswords = savedPasswordsList.length;
        chrome.passwordsPrivate.removeSavedPassword(
            savedPasswordsList[0].index);
      } else if (numCalls == 2) {
        chrome.test.assertEq(
            savedPasswordsList.length, numSavedPasswords - 1);
        chrome.test.succeed();
      } else {
        chrome.test.fail();
      }
    };

    chrome.passwordsPrivate.onSavedPasswordsListChanged.addListener(callback);
    chrome.passwordsPrivate.getSavedPasswordList(callback);
  },

  function removePasswordException() {
    var numCalls = 0;
    var numPasswordExceptions;
    var callback = function(passwordExceptionsList) {
      numCalls++;

      if (numCalls == 1) {
        numPasswordExceptions = passwordExceptionsList.length;
        chrome.passwordsPrivate.removePasswordException(
            passwordExceptionsList[0].index);
      } else if (numCalls == 2) {
        chrome.test.assertEq(
            passwordExceptionsList.length, numPasswordExceptions - 1);
        chrome.test.succeed();
      } else {
        chrome.test.fail();
      }
    };

    chrome.passwordsPrivate.onPasswordExceptionsListChanged.addListener(
        callback);
    chrome.passwordsPrivate.getPasswordExceptionList(callback);
  },

  function requestPlaintextPassword() {
    var callback = function() {
      // Ensure that the callback is invoked.
      chrome.test.succeed();
    };

    chrome.passwordsPrivate.onPlaintextPasswordRetrieved.addListener(callback);
    chrome.passwordsPrivate.requestPlaintextPassword(0);
  },

  function getSavedPasswordList() {
    var callback = function(list) {
      chrome.test.assertTrue(!!list);
      chrome.test.assertTrue(list.length > 0);

      for (var i = 0; i < list.length; ++i) {
        var entry = list[i];
        chrome.test.assertTrue(!!entry.loginPair);
        chrome.test.assertTrue(!!entry.loginPair.urls.origin);
        chrome.test.assertTrue(!!entry.loginPair.urls.shown);
        chrome.test.assertTrue(!!entry.loginPair.urls.link);
        chrome.test.assertEq(entry.index, i);
      }

      // Ensure that the callback is invoked.
      chrome.test.succeed();
    };

    chrome.passwordsPrivate.getSavedPasswordList(callback);
  },

  function getPasswordExceptionList() {
    var callback = function(list) {
      chrome.test.assertTrue(!!list);
      chrome.test.assertTrue(list.length > 0);

      for (var i = 0; i < list.length; ++i) {
        var exception = list[i];
        chrome.test.assertTrue(!!exception.urls.origin);
        chrome.test.assertTrue(!!exception.urls.shown);
        chrome.test.assertTrue(!!exception.urls.link);
        chrome.test.assertEq(exception.index, i);
      }

      // Ensure that the callback is invoked.
      chrome.test.succeed();
    };

    chrome.passwordsPrivate.getPasswordExceptionList(callback);
  },
];

var testToRun = window.location.search.substring(1);
chrome.test.runTests(availableTests.filter(function(op) {
  return op.name == testToRun;
}));
