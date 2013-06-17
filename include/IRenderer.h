/**
 * @file
 *
 * @section LICENSE
 *
 * OsmAnd - Android navigation software based on OSM maps.
 * Copyright (C) 2010-2013  OsmAnd Authors listed in AUTHORS file
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __I_RENDERER_H_
#define __I_RENDERER_H_

#include <stdint.h>
#include <memory>
#include <functional>
#include <array>

#include <QQueue>
#include <QSet>
#include <QMap>
#include <QMutex>

#include <SkBitmap.h>

#include <OsmAndCore.h>
#include <CommonTypes.h>
#include <TileZoomCache.h>

namespace OsmAnd {

    class IMapTileProvider;

    class OSMAND_CORE_API IRenderer
    {
    public:
        enum TextureDepth {
            _16bits,
            _32bits,
        };
        typedef std::function<void ()> RedrawRequestCallback;

        struct OSMAND_CORE_API Configuration
        {
            Configuration();

            std::shared_ptr<IMapTileProvider> tileProvider;
            PointI windowSize;
            AreaI viewport;
            float fieldOfView;
            float fogDistance;
            float distanceFromTarget;
            float azimuth;
            float elevationAngle;
            PointI target31;
            uint32_t zoom;
            TextureDepth preferredTextureDepth;
        };

    private:
        void tileReadyCallback(const TileId& tileId, uint32_t zoom, const std::shared_ptr<SkBitmap>& tile);
        bool _viewIsDirty;
        bool _tilesCacheInvalidated;
    protected:
        IRenderer();

        virtual void invalidateConfiguration();
        QMutex _pendingToActiveConfigMutex;
        bool _configInvalidated;
        Configuration _pendingConfig;
        Configuration _activeConfig;

        QSet<TileId> _visibleTiles;
        PointD _targetInTile;
        
        enum {
            TileSide3D = 100,
        };

        virtual void computeMatrices() = 0;
        virtual void refreshVisibleTileset() = 0;
        
        QMutex _tilesCacheMutex;
        struct OSMAND_CORE_API CachedTile : TileZoomCache::Tile
        {
            CachedTile(const uint32_t& zoom, const TileId& id, const size_t& usedMemory);
            virtual ~CachedTile();
        };
        TileZoomCache _tilesCache;
        virtual void purgeTilesCache();
        void updateTilesCache();
        virtual void cacheTile(const TileId& tileId, uint32_t zoom, const std::shared_ptr<SkBitmap>& tileBitmap) = 0;

        QMutex _tilesPendingToCacheMutex;
        struct OSMAND_CORE_API TilePendingToCache
        {
            TileId tileId;
            uint32_t zoom;
            std::shared_ptr<SkBitmap> tileBitmap;
        };
        QQueue< TilePendingToCache > _tilesPendingToCacheQueue;
        std::array< QSet< TileId >, 32 > _tilesPendingToCache;

        bool _isRenderingInitialized;

        const bool& viewIsDirty;
        virtual void requestRedraw();
        
        const bool& tilesCacheInvalidated;
        virtual void invalidateTileCache();

        virtual void updateConfiguration();
    public:
        virtual ~IRenderer();

        RedrawRequestCallback redrawRequestCallback;
                
        const Configuration& configuration;
        const QSet<TileId>& visibleTiles;
        
        virtual int getCachedTilesCount() const;

        virtual void setTileProvider(const std::shared_ptr<IMapTileProvider>& tileProvider);
        virtual void setPreferredTextureDepth(TextureDepth depth);
        virtual void updateViewport(const PointI& windowSize, const AreaI& viewport, float fieldOfView, float viewDepth);
        virtual void updateCamera(float distanceFromTarget, float azimuth, float elevationAngle);
        virtual void updateMap(const PointI& target31, uint32_t zoom);

        const bool& isRenderingInitialized;
        virtual void initializeRendering() = 0;
        virtual void performRendering() = 0;
        virtual void releaseRendering() = 0;
    };

#if defined(OSMAND_OPENGL_RENDERER_SUPPORTED)
    OSMAND_CORE_API std::shared_ptr<OsmAnd::IRenderer> OSMAND_CORE_CALL createRenderer_OpenGL();
#endif // OSMAND_OPENGL_RENDERER_SUPPORTED
}

#endif // __I_RENDERER_H_