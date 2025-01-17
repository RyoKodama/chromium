#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import base64

from writers import admx_writer


def GetWriter(config):
  '''Factory method for creating ADMXWriter objects for the Chrome OS platform
  See the constructor of TemplateWriter for description of arguments.
  '''
  return ChromeOSADMXWriter(['chrome_os'], config)


class ChromeOSADMXWriter(admx_writer.ADMXWriter):
  '''Class for generating Chrome OS policy templates in the ADMX format.
  It is used by PolicyTemplateGenerator to write ADMX files.
  '''

  # Overridden.
  def GetClass(self, policy):
    is_device_only = 'device_only' in policy and policy['device_only']
    return 'Machine' if is_device_only else 'User'
