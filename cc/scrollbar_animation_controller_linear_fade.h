// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCScrollbarAnimationControllerLinearFade_h
#define CCScrollbarAnimationControllerLinearFade_h

#include "cc/scrollbar_animation_controller.h"

namespace cc {

class ScrollbarAnimationControllerLinearFade : public ScrollbarAnimationController {
public:
    static scoped_ptr<ScrollbarAnimationControllerLinearFade> create(LayerImpl* scrollLayer, double fadeoutDelay, double fadeoutLength);

    virtual ~ScrollbarAnimationControllerLinearFade();

    virtual bool animate(double monotonicTime) OVERRIDE;

    virtual void didPinchGestureUpdateAtTime(double monotonicTime) OVERRIDE;
    virtual void didPinchGestureEndAtTime(double monotonicTime) OVERRIDE;
    virtual void updateScrollOffsetAtTime(LayerImpl* scrollLayer, double monotonicTime) OVERRIDE;

protected:
    ScrollbarAnimationControllerLinearFade(LayerImpl* scrollLayer, double fadeoutDelay, double fadeoutLength);

private:
    float opacityAtTime(double monotonicTime);

    double m_lastAwakenTime;
    bool m_pinchGestureInEffect;

    double m_fadeoutDelay;
    double m_fadeoutLength;
};

} // namespace cc

#endif // CCScrollbarAnimationControllerLinearFade_h
