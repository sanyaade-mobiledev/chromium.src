# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import urlparse

SOURCE_WHITELIST = [
  'https://commondatastorage.googleapis.com/nativeclient-mirror/nacl/nacl_sdk',
]

def IsSourceValid(url):
  scheme, host, path, _, _, _ = urlparse.urlparse(url)
  for allowed_url in SOURCE_WHITELIST:
    allowed_scheme, allowed_host, allowed_path_prefix, _, _, _ = \
        urlparse.urlparse(allowed_url)
    if (scheme == allowed_scheme and host == allowed_host and
        path.startswith(allowed_path_prefix)):
      return True
  return False


class Config(dict):
  def __init__(self, data=None):
    dict.__init__(self)
    if data:
      self.update(data)
    else:
      self.sources = []

  def ToJson(self):
    return json.dumps(self, sort_keys=False, indent=2)

  def __getattr__(self, name):
    return self.__getitem__(name)

  def __setattr__(self, name, value):
    return self.__setitem__(name, value)

  def AddSource(self, source):
    if not IsSourceValid(source):
      logging.warn('Only whitelisted sources are allowed. Ignoring \"%s\".' % (
          source,))
      return

    if source in self.sources:
      logging.info('Source \"%s\" already in Config.' % (source,))
      return
    self.sources.append(source)

  def RemoveSource(self, source):
    if source not in self.sources:
      logging.warn('Source \"%s\" not in Config.' % (source,))
      return
    self.sources.remove(source)

  def RemoveAllSources(self):
    if not self.sources:
      logging.info('No sources to remove.')
      return
    self.sources = []
