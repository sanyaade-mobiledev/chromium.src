// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCTiledLayerTestCommon_h
#define CCTiledLayerTestCommon_h

#include "Region.h"
#include "cc/layer_updater.h"
#include "cc/prioritized_texture.h"
#include "cc/resource_provider.h"
#include "cc/resource_update_queue.h"
#include "cc/texture_copier.h"
#include "cc/texture_uploader.h"
#include "cc/tiled_layer.h"
#include "cc/tiled_layer_impl.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace WebKitTests {

class FakeTiledLayer;

class FakeLayerUpdater : public cc::LayerUpdater {
public:
    class Resource : public cc::LayerUpdater::Resource {
    public:
        Resource(FakeLayerUpdater*, scoped_ptr<cc::PrioritizedTexture>);
        virtual ~Resource();

        virtual void update(cc::ResourceUpdateQueue&, const gfx::Rect&, const gfx::Vector2d&, bool, cc::RenderingStats&) OVERRIDE;

    private:
        FakeLayerUpdater* m_layer;
        SkBitmap m_bitmap;
    };

    FakeLayerUpdater();

    virtual scoped_ptr<cc::LayerUpdater::Resource> createResource(cc::PrioritizedTextureManager*) OVERRIDE;

    virtual void prepareToUpdate(const gfx::Rect& contentRect, const gfx::Size&, float, float, gfx::Rect& resultingOpaqueRect, cc::RenderingStats&) OVERRIDE;
    // Sets the rect to invalidate during the next call to prepareToUpdate(). After the next
    // call to prepareToUpdate() the rect is reset.
    void setRectToInvalidate(const gfx::Rect&, FakeTiledLayer*);
    // Last rect passed to prepareToUpdate().
    const gfx::Rect& lastUpdateRect()  const { return m_lastUpdateRect; }

    // Number of times prepareToUpdate has been invoked.
    int prepareCount() const { return m_prepareCount; }
    void clearPrepareCount() { m_prepareCount = 0; }

    // Number of times update() has been invoked on a texture.
    int updateCount() const { return m_updateCount; }
    void clearUpdateCount() { m_updateCount = 0; }
    void update() { m_updateCount++; }

    void setOpaquePaintRect(const gfx::Rect& opaquePaintRect) { m_opaquePaintRect = opaquePaintRect; }

protected:
    virtual ~FakeLayerUpdater();

private:
    int m_prepareCount;
    int m_updateCount;
    gfx::Rect m_rectToInvalidate;
    gfx::Rect m_lastUpdateRect;
    gfx::Rect m_opaquePaintRect;
    scoped_refptr<FakeTiledLayer> m_layer;
};

class FakeTiledLayerImpl : public cc::TiledLayerImpl {
public:
    explicit FakeTiledLayerImpl(int id);
    virtual ~FakeTiledLayerImpl();

    using cc::TiledLayerImpl::hasTileAt;
    using cc::TiledLayerImpl::hasResourceIdForTileAt;
};

class FakeTiledLayer : public cc::TiledLayer {
public:
    explicit FakeTiledLayer(cc::PrioritizedTextureManager*);

    static gfx::Size tileSize() { return gfx::Size(100, 100); }

    using cc::TiledLayer::invalidateContentRect;
    using cc::TiledLayer::needsIdlePaint;
    using cc::TiledLayer::skipsDraw;
    using cc::TiledLayer::numPaintedTiles;
    using cc::TiledLayer::idlePaintRect;

    virtual void setNeedsDisplayRect(const gfx::RectF&) OVERRIDE;
    const gfx::RectF& lastNeedsDisplayRect() const { return m_lastNeedsDisplayRect; }

    virtual void setTexturePriorities(const cc::PriorityCalculator&) OVERRIDE;

    virtual cc::PrioritizedTextureManager* textureManager() const OVERRIDE;
    FakeLayerUpdater* fakeLayerUpdater() { return m_fakeUpdater.get(); }
    gfx::RectF updateRect() { return m_updateRect; }

protected:
    virtual cc::LayerUpdater* updater() const OVERRIDE;
    virtual void createUpdaterIfNeeded() OVERRIDE { }
    virtual ~FakeTiledLayer();

private:
    scoped_refptr<FakeLayerUpdater> m_fakeUpdater;
    cc::PrioritizedTextureManager* m_textureManager;
    gfx::RectF m_lastNeedsDisplayRect;
};

class FakeTiledLayerWithScaledBounds : public FakeTiledLayer {
public:
    explicit FakeTiledLayerWithScaledBounds(cc::PrioritizedTextureManager*);

    void setContentBounds(const gfx::Size& contentBounds) { m_forcedContentBounds = contentBounds; }
    virtual gfx::Size contentBounds() const OVERRIDE;
    virtual float contentsScaleX() const OVERRIDE;
    virtual float contentsScaleY() const OVERRIDE;
    virtual void setContentsScale(float) OVERRIDE;

protected:
    virtual ~FakeTiledLayerWithScaledBounds();
    gfx::Size m_forcedContentBounds;
};

}
#endif // CCTiledLayerTestCommon_h
