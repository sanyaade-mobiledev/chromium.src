// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/math_util.h"

#include "FloatSize.h"
#include "cc/test/geometry_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_f.h"
#include <public/WebTransformationMatrix.h>

using namespace cc;
using WebKit::WebTransformationMatrix;

namespace {

TEST(MathUtilTest, verifyBackfaceVisibilityBasicCases)
{
    WebTransformationMatrix transform;

    transform.makeIdentity();
    EXPECT_FALSE(transform.isBackFaceVisible());

    transform.makeIdentity();
    transform.rotate3d(0, 80, 0);
    EXPECT_FALSE(transform.isBackFaceVisible());

    transform.makeIdentity();
    transform.rotate3d(0, 100, 0);
    EXPECT_TRUE(transform.isBackFaceVisible());

    // Edge case, 90 degree rotation should return false.
    transform.makeIdentity();
    transform.rotate3d(0, 90, 0);
    EXPECT_FALSE(transform.isBackFaceVisible());
}

TEST(MathUtilTest, verifyBackfaceVisibilityForPerspective)
{
    WebTransformationMatrix layerSpaceToProjectionPlane;

    // This tests if isBackFaceVisible works properly under perspective transforms.
    // Specifically, layers that may have their back face visible in orthographic
    // projection, may not actually have back face visible under perspective projection.

    // Case 1: Layer is rotated by slightly more than 90 degrees, at the center of the
    //         prespective projection. In this case, the layer's back-side is visible to
    //         the camera.
    layerSpaceToProjectionPlane.makeIdentity();
    layerSpaceToProjectionPlane.applyPerspective(1);
    layerSpaceToProjectionPlane.translate3d(0, 0, 0);
    layerSpaceToProjectionPlane.rotate3d(0, 100, 0);
    EXPECT_TRUE(layerSpaceToProjectionPlane.isBackFaceVisible());

    // Case 2: Layer is rotated by slightly more than 90 degrees, but shifted off to the
    //         side of the camera. Because of the wide field-of-view, the layer's front
    //         side is still visible.
    //
    //                       |<-- front side of layer is visible to perspective camera
    //                    \  |            /
    //                     \ |           /
    //                      \|          /
    //                       |         /
    //                       |\       /<-- camera field of view
    //                       | \     /
    // back side of layer -->|  \   /
    //                           \./ <-- camera origin
    //
    layerSpaceToProjectionPlane.makeIdentity();
    layerSpaceToProjectionPlane.applyPerspective(1);
    layerSpaceToProjectionPlane.translate3d(-10, 0, 0);
    layerSpaceToProjectionPlane.rotate3d(0, 100, 0);
    EXPECT_FALSE(layerSpaceToProjectionPlane.isBackFaceVisible());

    // Case 3: Additionally rotating the layer by 180 degrees should of course show the
    //         opposite result of case 2.
    layerSpaceToProjectionPlane.rotate3d(0, 180, 0);
    EXPECT_TRUE(layerSpaceToProjectionPlane.isBackFaceVisible());
}

TEST(MathUtilTest, verifyProjectionOfPerpendicularPlane)
{
    // In this case, the m33() element of the transform becomes zero, which could cause a
    // divide-by-zero when projecting points/quads.

    WebTransformationMatrix transform;
    transform.makeIdentity();
    transform.setM33(0);

    gfx::RectF rect = gfx::RectF(0, 0, 1, 1);
    gfx::RectF projectedRect = MathUtil::projectClippedRect(transform, rect);

    EXPECT_EQ(0, projectedRect.x());
    EXPECT_EQ(0, projectedRect.y());
    EXPECT_TRUE(projectedRect.IsEmpty());
}

TEST(MathUtilTest, verifyEnclosingClippedRectUsesCorrectInitialBounds)
{
    HomogeneousCoordinate h1(-100, -100, 0, 1);
    HomogeneousCoordinate h2(-10, -10, 0, 1);
    HomogeneousCoordinate h3(10, 10, 0, -1);
    HomogeneousCoordinate h4(100, 100, 0, -1);

    // The bounds of the enclosing clipped rect should be -100 to -10 for both x and y.
    // However, if there is a bug where the initial xmin/xmax/ymin/ymax are initialized to
    // numeric_limits<float>::min() (which is zero, not -flt_max) then the enclosing
    // clipped rect will be computed incorrectly.
    gfx::RectF result = MathUtil::computeEnclosingClippedRect(h1, h2, h3, h4);

    EXPECT_FLOAT_RECT_EQ(gfx::RectF(gfx::PointF(-100, -100), gfx::SizeF(90, 90)), result);
}

TEST(MathUtilTest, verifyEnclosingRectOfVerticesUsesCorrectInitialBounds)
{
    gfx::PointF vertices[3];
    int numVertices = 3;

    vertices[0] = gfx::PointF(-10, -100);
    vertices[1] = gfx::PointF(-100, -10);
    vertices[2] = gfx::PointF(-30, -30);

    // The bounds of the enclosing rect should be -100 to -10 for both x and y. However,
    // if there is a bug where the initial xmin/xmax/ymin/ymax are initialized to
    // numeric_limits<float>::min() (which is zero, not -flt_max) then the enclosing
    // clipped rect will be computed incorrectly.
    gfx::RectF result = MathUtil::computeEnclosingRectOfVertices(vertices, numVertices);

    EXPECT_FLOAT_RECT_EQ(gfx::RectF(gfx::PointF(-100, -100), gfx::SizeF(90, 90)), result);
}

TEST(MathUtilTest, smallestAngleBetweenVectors)
{
    FloatSize x(1, 0);
    FloatSize y(0, 1);
    FloatSize testVector(0.5, 0.5);

    // Orthogonal vectors are at an angle of 90 degress.
    EXPECT_EQ(90, MathUtil::smallestAngleBetweenVectors(x, y));

    // A vector makes a zero angle with itself.
    EXPECT_EQ(0, MathUtil::smallestAngleBetweenVectors(x, x));
    EXPECT_EQ(0, MathUtil::smallestAngleBetweenVectors(y, y));
    EXPECT_EQ(0, MathUtil::smallestAngleBetweenVectors(testVector, testVector));

    // Parallel but reversed vectors are at 180 degrees.
    EXPECT_FLOAT_EQ(180, MathUtil::smallestAngleBetweenVectors(x, -x));
    EXPECT_FLOAT_EQ(180, MathUtil::smallestAngleBetweenVectors(y, -y));
    EXPECT_FLOAT_EQ(180, MathUtil::smallestAngleBetweenVectors(testVector, -testVector));

    // The test vector is at a known angle.
    EXPECT_FLOAT_EQ(45, floor(MathUtil::smallestAngleBetweenVectors(testVector, x)));
    EXPECT_FLOAT_EQ(45, floor(MathUtil::smallestAngleBetweenVectors(testVector, y)));
}

TEST(MathUtilTest, vectorProjection)
{
    FloatSize x(1, 0);
    FloatSize y(0, 1);
    FloatSize testVector(0.3f, 0.7f);

    // Orthogonal vectors project to a zero vector.
    EXPECT_EQ(FloatSize(0, 0), MathUtil::projectVector(x, y));
    EXPECT_EQ(FloatSize(0, 0), MathUtil::projectVector(y, x));

    // Projecting a vector onto the orthonormal basis gives the corresponding component of the
    // vector.
    EXPECT_EQ(FloatSize(testVector.width(), 0), MathUtil::projectVector(testVector, x));
    EXPECT_EQ(FloatSize(0, testVector.height()), MathUtil::projectVector(testVector, y));

    // Finally check than an arbitrary vector projected to another one gives a vector parallel to
    // the second vector.
    FloatSize targetVector(0.5, 0.2f);
    FloatSize projectedVector = MathUtil::projectVector(testVector, targetVector);
    EXPECT_EQ(projectedVector.width() / targetVector.width(),
              projectedVector.height() / targetVector.height());
}

} // namespace
