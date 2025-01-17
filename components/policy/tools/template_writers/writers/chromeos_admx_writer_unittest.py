#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Unittests for writers.chromeos_admx_writer."""


import os
import sys
import unittest
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../../../..'))


from writers import chromeos_admx_writer
from writers import admx_writer_unittest


class ChromeOsAdmxWriterUnittest(
  admx_writer_unittest.AdmxWriterUnittest):

  # Overridden.
  def _GetWriter(self, config):
    return chromeos_admx_writer.GetWriter(config)

  # Overridden.
  def _GetKey(self):
    return "CrOSTest"

  # Overridden.
  def _GetCategory(self):
    return "cros_test_category";

  # Overridden.
  def _GetCategoryRec(self):
    return "cros_test_recommended_category";

  # Overridden.
  def _GetNamespace(self):
    return "ADMXWriter.Test.Namespace.ChromeOS";

  # Overridden.
  def testPlatform(self):
    # Test that the writer correctly chooses policies of platform Chrome OS.
    self.assertTrue(self.writer.IsPolicySupported({
      'supported_on': [
        {'platforms': ['chrome_os', 'zzz']}, {'platforms': ['aaa']}
      ]
    }))
    self.assertFalse(self.writer.IsPolicySupported({
      'supported_on': [
        {'platforms': ['win', 'mac', 'linux']}, {'platforms': ['aaa']}
      ]
    }))

  def testUserPolicy(self):
    self.doTestUserOrDevicePolicy(False);

  def testDevicePolicy(self):
    self.doTestUserOrDevicePolicy(True);

  def doTestUserOrDevicePolicy(self, is_device_only):
    # Tests whether CLASS attribute is 'User' for user policies and 'Machine'
    # for device policies.
    main_policy = {
      'name': 'DummyMainPolicy',
      'type': 'main',
      'device_only': is_device_only,
    }

    expected_class = 'Machine' if is_device_only else 'User';

    self._initWriterForPolicy(self.writer, main_policy)
    self.writer.WritePolicy(main_policy)

    output = self.GetXMLOfChildren(self._GetPoliciesElement(self.writer._doc))
    expected_output = (
        '<policy class="' + expected_class + '"'
        ' displayName="$(string.DummyMainPolicy)"'
        ' explainText="$(string.DummyMainPolicy_Explain)"'
        ' key="Software\\Policies\\' + self._GetKey() + '"'
        ' name="DummyMainPolicy"'
        ' presentation="$(presentation.DummyMainPolicy)"'
        ' valueName="DummyMainPolicy">\n'
        '  <parentCategory ref="PolicyGroup"/>\n'
        '  <supportedOn ref="SUPPORTED_TESTOS"/>\n'
        '  <enabledValue>\n'
        '    <decimal value="1"/>\n'
        '  </enabledValue>\n'
        '  <disabledValue>\n'
        '    <decimal value="0"/>\n'
        '  </disabledValue>\n'
        '</policy>')

    self.AssertXMLEquals(output, expected_output)


if __name__ == '__main__':
  unittest.main()
