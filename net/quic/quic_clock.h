// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CLOCK_H_
#define NET_QUIC_QUIC_CLOCK_H_

#include "base/basictypes.h"
#include "net/base/net_export.h"

namespace net {

typedef double WallTime;

// Clock to efficiently retrieve an approximately accurate time from an
// EpollServer.
class NET_EXPORT_PRIVATE QuicClock {
 public:
  QuicClock();
  virtual ~QuicClock();

  // Returns the approximate current time as the number of microseconds
  // since the Unix epoch.
  virtual uint64 NowInUsec();
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CLOCK_H_
