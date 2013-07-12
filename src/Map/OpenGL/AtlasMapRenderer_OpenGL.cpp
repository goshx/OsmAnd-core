#include "AtlasMapRenderer_OpenGL.h"

#include <assert.h>

#include <QtCore/qmath.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

#include <SkBitmap.h>

#include "IMapTileProvider.h"
#include "OsmAndCore/Logging.h"

OsmAnd::AtlasMapRenderer_OpenGL::AtlasMapRenderer_OpenGL()
    : _tilePatchVAO(0)
    , _tilePatchVBO(0)
    , _tilePatchIBO(0)
{
    memset(&_mapStage, 0, sizeof(_mapStage));
    memset(&_skyStage, 0, sizeof(_skyStage));
}

OsmAnd::AtlasMapRenderer_OpenGL::~AtlasMapRenderer_OpenGL()
{
}

void OsmAnd::AtlasMapRenderer_OpenGL::initializeRendering()
{
    MapRenderer_OpenGL::initializeRendering();

    initializeRendering_SkyStage();
    initializeRendering_MapStage();

    AtlasMapRenderer_BaseOpenGL::initializeRendering();
}

void OsmAnd::AtlasMapRenderer_OpenGL::initializeRendering_MapStage()
{
    // Compile vertex shader
    const QString vertexShader_perTileLayerTexCoordsProcessing = QString::fromLatin1(
        "    calculateTextureCoordinates(                                                                                   ""\n"
        "        param_vs_perTileLayer[%layerId%],                                                                          ""\n"
        "        v2f_texCoordsPerLayer[%layerLinearIndex%]);                                                                ""\n"
        "                                                                                                                   ""\n");
    const QString vertexShader = QString::fromLatin1(
        "#version 430 core                                                                                                  ""\n"
        "                                                                                                                   ""\n"
        // Constants
        "const float floatEpsilon = 0.000001;                                                                               ""\n"
        "                                                                                                                   ""\n"
        // Input data
        "in vec2 in_vs_vertexPosition;                                                                                      ""\n"
        "in vec2 in_vs_vertexTexCoords;                                                                                     ""\n"
        "                                                                                                                   ""\n"
        // Output data to next shader stages
        "out vec2 v2f_texCoordsPerLayer[%RasterTileLayersCount%];                                                           ""\n"
        "out float v2f_distanceFromCamera;                                                                                  ""\n"
        "out vec2 v2f_positionRelativeToTarget;                                                                             ""\n"
        "                                                                                                                   ""\n"
        // Parameters: common data
        "uniform mat4 param_vs_mProjectionView;                                                                             ""\n"
        "uniform mat4 param_vs_mView;                                                                                       ""\n"
        "uniform vec2 param_vs_targetInTilePosN;                                                                            ""\n"
        "uniform ivec2 param_vs_targetTile;                                                                                 ""\n"
        "                                                                                                                   ""\n"
        // Parameters: per-tile data
        "uniform ivec2 param_vs_tile;                                                                                       ""\n"
        "uniform float param_vs_elevationData_k;                                                                            ""\n"
        "uniform sampler2D param_vs_elevationData_sampler;                                                                  ""\n"
        "                                                                                                                   ""\n"
        // Parameters: per-layer-in-tile data
        "struct LayerInputPerTile                                                                                           ""\n"
        "{                                                                                                                  ""\n"
        "    float tileSizeN;                                                                                               ""\n"
        "    float tilePaddingN;                                                                                            ""\n"
        "    int slotsPerSide;                                                                                              ""\n"
        "    int slotIndex;                                                                                                 ""\n"
        "};                                                                                                                 ""\n"
        "uniform LayerInputPerTile param_vs_perTileLayer[%TileLayersCount%];                                                ""\n"
        "                                                                                                                   ""\n"
        "void calculateTextureCoordinates(in LayerInputPerTile perTile, out vec2 outTexCoords)                              ""\n"
        "{                                                                                                                  ""\n"
        "    if(perTile.slotIndex >= 0)                                                                                     ""\n"
        "    {                                                                                                              ""\n"
        "        const int rowIndex = perTile.slotIndex / perTile.slotsPerSide;                                             ""\n"
        "        const int colIndex = int(mod(perTile.slotIndex, perTile.slotsPerSide));                                    ""\n"
        "                                                                                                                   ""\n"
        "        const float texCoordRescale = (perTile.tileSizeN - 2.0 * perTile.tilePaddingN) / perTile.tileSizeN;        ""\n"
        "                                                                                                                   ""\n"
        "        outTexCoords.s = float(colIndex) * perTile.tileSizeN;                                                      ""\n"
        "        outTexCoords.s += perTile.tilePaddingN + (in_vs_vertexTexCoords.s * perTile.tileSizeN) * texCoordRescale;  ""\n"
        "                                                                                                                   ""\n"
        "        outTexCoords.t = float(rowIndex) * perTile.tileSizeN;                                                      ""\n"
        "        outTexCoords.t += perTile.tilePaddingN + (in_vs_vertexTexCoords.t * perTile.tileSizeN) * texCoordRescale;  ""\n"
        "    }                                                                                                              ""\n"
        "    else                                                                                                           ""\n"
        "    {                                                                                                              ""\n"
        "        outTexCoords = in_vs_vertexTexCoords;                                                                      ""\n"
        "    }                                                                                                              ""\n"
        "}                                                                                                                  ""\n"
        "                                                                                                                   ""\n"
        "void main()                                                                                                        ""\n"
        "{                                                                                                                  ""\n"
        "    vec4 v = vec4(in_vs_vertexPosition.x, 0.0, in_vs_vertexPosition.y, 1.0);                                       ""\n"
        "                                                                                                                   ""\n"
        //   Shift vertex to it's proper position
        "    float xOffset = float(param_vs_tile.x - param_vs_targetTile.x) - param_vs_targetInTilePosN.x;                  ""\n"
        "    v.x += xOffset * %TileSize3D%.0;                                                                               ""\n"
        "    float yOffset = float(param_vs_tile.y - param_vs_targetTile.y) - param_vs_targetInTilePosN.y;                  ""\n"
        "    v.z += yOffset * %TileSize3D%.0;                                                                               ""\n"
        "                                                                                                                   ""\n"
        //   Process each tile layer texture coordinates (except elevation)
        "%UnrolledPerLayerTexCoordsProcessingCode%                                                                          ""\n"
        "                                                                                                                   ""\n"
        //   If elevation data is active, use it
        "    if(abs(param_vs_elevationData_k) > floatEpsilon)                                                               ""\n"
        "    {                                                                                                              ""\n"
        "        vec2 elevationDataTexCoords;                                                                               ""\n"
        "        calculateTextureCoordinates(                                                                               ""\n"
        "            param_vs_perTileLayer[0],                                                                              ""\n"
        "            elevationDataTexCoords);                                                                               ""\n"
        "                                                                                                                   ""\n"
        "        float height = texture(param_vs_elevationData_sampler, elevationDataTexCoords).r;                          ""\n"
        //TODO: remap meters to units
        //TODO: pixel is point vs pixel is area, and coordinate shift
        //TODO: cap no-data values
        "        v.y = height / 100.0;                                                                                        ""\n"
        "        v.y *= param_vs_elevationData_k;                                                                           ""\n"
        "    }                                                                                                              ""\n"
        "    else                                                                                                           ""\n"
        "    {                                                                                                              ""\n"
        "        v.y = 0.0;                                                                                                 ""\n"
        "    }                                                                                                              ""\n"
        "                                                                                                                   ""\n"
        //   Finally output processed modified vertex
        "    v2f_positionRelativeToTarget = v.xz;                                                                           ""\n"
        "    v2f_distanceFromCamera = length((param_vs_mView * v).xz);                                                      ""\n"
        "    gl_Position = param_vs_mProjectionView * v;                                                                    ""\n"
        "}                                                                                                                  ""\n");
    QString preprocessedVertexShader = vertexShader;
    QString preprocessedVertexShader_UnrolledPerLayerTexCoordsProcessingCode;
    for(int layerId = TileLayerId::RasterMap, linearIdx = 0; layerId < TileLayerId::IdsCount; layerId++, linearIdx++)
    {
        QString preprocessedVertexShader_perTileLayerTexCoordsProcessing = vertexShader_perTileLayerTexCoordsProcessing;
        preprocessedVertexShader_perTileLayerTexCoordsProcessing.replace("%layerId%", QString::number(layerId));
        preprocessedVertexShader_perTileLayerTexCoordsProcessing.replace("%layerLinearIndex%", QString::number(linearIdx));

        preprocessedVertexShader_UnrolledPerLayerTexCoordsProcessingCode +=
            preprocessedVertexShader_perTileLayerTexCoordsProcessing;
    }
    preprocessedVertexShader.replace("%UnrolledPerLayerTexCoordsProcessingCode%",
        preprocessedVertexShader_UnrolledPerLayerTexCoordsProcessingCode);
    preprocessedVertexShader.replace("%TileSize3D%", QString::number(TileSide3D));
    preprocessedVertexShader.replace("%TileLayersCount%", QString::number(TileLayerId::IdsCount));
    preprocessedVertexShader.replace("%RasterTileLayersCount%", QString::number(TileLayerId::IdsCount - TileLayerId::RasterMap));
    preprocessedVertexShader.replace("%Layer_ElevationData%", QString::number(TileLayerId::ElevationData));
    preprocessedVertexShader.replace("%Layer_RasterMap%", QString::number(TileLayerId::RasterMap));
    _mapStage.vs.id = compileShader(GL_VERTEX_SHADER, preprocessedVertexShader.toStdString().c_str());
    assert(_mapStage.vs.id != 0);

    // Compile fragment shader
    const QString fragmentShader_perTileLayer = QString::fromLatin1(
        "    if(param_fs_perTileLayer[%layerLinearIdx%].k > floatEpsilon)                                                   ""\n"
        "    {                                                                                                              ""\n"
        "        vec4 layerColor = textureLod(                                                                              ""\n"
        "            param_fs_perTileLayer[%layerLinearIdx%].sampler,                                                       ""\n"
        "            v2f_texCoordsPerLayer[%layerLinearIdx%], mipmapLod);                                                   ""\n"
        "                                                                                                                   ""\n"
        "        baseColor = mix(baseColor, layerColor, layerColor.a * param_fs_perTileLayer[%layerLinearIdx%].k);          ""\n"
        "    }                                                                                                              ""\n");
    const QString fragmentShader = QString::fromLatin1(
        "#version 430 core                                                                                                  ""\n"
        "                                                                                                                   ""\n"
        // Constants
        "const float floatEpsilon = 0.000001;                                                                               ""\n"
        "                                                                                                                   ""\n"
        // Input data
        "in vec2 v2f_texCoordsPerLayer[%RasterTileLayersCount%];                                                            ""\n"
        "in vec2 v2f_positionRelativeToTarget;                                                                              ""\n"
        "in float v2f_distanceFromCamera;                                                                                   ""\n"
        "                                                                                                                   ""\n"
        // Output data
        "out vec4 out_color;                                                                                                ""\n"
        "                                                                                                                   ""\n"
        // Parameters: common data
        "uniform float param_fs_distanceFromCameraToTarget;                                                                 ""\n"
        "uniform float param_fs_cameraElevationAngle;                                                                       ""\n"
        "uniform vec3 param_fs_fogColor;                                                                                    ""\n"
        "uniform float param_fs_fogDistance;                                                                                ""\n"
        "uniform float param_fs_fogDensity;                                                                                 ""\n"
        "uniform float param_fs_fogOriginFactor;                                                                            ""\n"
        "uniform float param_fs_scaleToRetainProjectedSize;                                                                 ""\n"
        "                                                                                                                   ""\n"
        // Parameters: per-layer data
        "struct LayerInputPerTile                                                                                           ""\n"
        "{                                                                                                                  ""\n"
        "    float k;                                                                                                       ""\n"
        "    sampler2D sampler;                                                                                             ""\n"
        "};                                                                                                                 ""\n"
        "uniform LayerInputPerTile param_fs_perTileLayer[%RasterTileLayersCount%];                                          ""\n"
        "                                                                                                                   ""\n"
        "void main()                                                                                                        ""\n"
        "{                                                                                                                  ""\n"
        //   Calculate normalized camera elevation and recalculate lod0 distance
        "    const float cameraElevationN = param_fs_cameraElevationAngle / 90.0;                                           ""\n"
        "    const float cameraBaseDistance = param_fs_distanceFromCameraToTarget * 1.25;                                   ""\n"
        "    const float zeroLodDistanceShift = 4.0 * (0.2 - cameraElevationN) * param_fs_distanceFromCameraToTarget;       ""\n"
        "    const float cameraDistanceLOD0 = cameraBaseDistance - zeroLodDistanceShift;                                    ""\n"
        "                                                                                                                   ""\n"
        //   Calculate mipmap LOD
        "    float mipmapLod = 0.0;                                                                                         ""\n"
        "    if(v2f_distanceFromCamera > cameraDistanceLOD0)                                                                ""\n"
        "    {                                                                                                              ""\n"
        //       Calculate distance factor that is in range (0.0 ... +inf)
        "        const float distanceF = 1.0 - cameraDistanceLOD0 / v2f_distanceFromCamera;                                 ""\n"
        "        mipmapLod = distanceF * ((1.0 - cameraElevationN) * 10.0);                                                 ""\n"
        "        mipmapLod = clamp(mipmapLod, 0.0, %MipmapLodLevelsMax%.0 - 1.0);                                           ""\n"
        "    }                                                                                                              ""\n"
        "                                                                                                                   ""\n"
        //   Take base color from RasterMap layer
        "    vec4 baseColor = textureLod(                                                                                   ""\n"
        "        param_fs_perTileLayer[0].sampler,                                                                          ""\n"
        "        v2f_texCoordsPerLayer[0], mipmapLod);                                                                      ""\n"
        "    baseColor.a *= param_fs_perTileLayer[0].k;                                                                     ""\n"
        "%UnrolledPerLayerProcessingCode%                                                                                   ""\n"
        "                                                                                                                   ""\n"
        //   Apply fog (square exponential)
        "    const float fogDistanceScaled = param_fs_fogDistance * param_fs_scaleToRetainProjectedSize;                    ""\n"
        "    const float fogStartDistance = fogDistanceScaled * (1.0 - param_fs_fogOriginFactor);                           ""\n"
        "    const float fogLinearFactor = min(max(length(v2f_positionRelativeToTarget) - fogStartDistance, 0.0) /          ""\n"
        "        (fogDistanceScaled - fogStartDistance), 1.0);                                                              ""\n"

        "    const float fogFactorBase = fogLinearFactor * param_fs_fogDensity;                                             ""\n"
        "    const float fogFactor = clamp(exp(-fogFactorBase*fogFactorBase), 0.0, 1.0);                                    ""\n"
        "    out_color = mix(baseColor, vec4(param_fs_fogColor, 1.0), 1.0 - fogFactor);                                     ""\n"
        "                                                                                                                   ""\n"
        //   Remove pixel if it's completely transparent
        "    if(out_color.a < floatEpsilon)                                                                                 ""\n"
        "        discard;                                                                                                   ""\n"
        "}                                                                                                                  ""\n");
    QString preprocessedFragmentShader = fragmentShader;
    QString preprocessedFragmentShader_UnrolledPerLayerProcessingCode;
    for(int layerId = TileLayerId::MapOverlay0; layerId <= TileLayerId::MapOverlay3; layerId++)
    {
        auto linearIdx = layerId - TileLayerId::RasterMap;
        QString preprocessedFragmentShader_perTileLayer = fragmentShader_perTileLayer;
        preprocessedFragmentShader_perTileLayer.replace("%layerLinearIdx%", QString::number(linearIdx));

        preprocessedFragmentShader_UnrolledPerLayerProcessingCode += preprocessedFragmentShader_perTileLayer;
    }
    preprocessedFragmentShader.replace("%UnrolledPerLayerProcessingCode%", preprocessedFragmentShader_UnrolledPerLayerProcessingCode);
    preprocessedFragmentShader.replace("%TileLayersCount%", QString::number(TileLayerId::IdsCount));
    preprocessedFragmentShader.replace("%RasterTileLayersCount%", QString::number(TileLayerId::IdsCount - TileLayerId::RasterMap));
    preprocessedFragmentShader.replace("%Layer_RasterMap%", QString::number(TileLayerId::RasterMap));
    preprocessedFragmentShader.replace("%MipmapLodLevelsMax%", QString::number(MipmapLodLevelsMax));
    _mapStage.fs.id = compileShader(GL_FRAGMENT_SHADER, preprocessedFragmentShader.toStdString().c_str());
    assert(_mapStage.fs.id != 0);

    // Link everything into program object
    GLuint shaders[] = {
        _mapStage.vs.id,
        _mapStage.fs.id
    };
    _mapStage.program = linkProgram(2, shaders);
    assert(_mapStage.program != 0);

    _programVariables.clear();
    findVariableLocation(_mapStage.program, _mapStage.vs.in.vertexPosition, "in_vs_vertexPosition", In);
    findVariableLocation(_mapStage.program, _mapStage.vs.in.vertexTexCoords, "in_vs_vertexTexCoords", In);
    findVariableLocation(_mapStage.program, _mapStage.vs.param.mProjectionView, "param_vs_mProjectionView", Uniform);
    findVariableLocation(_mapStage.program, _mapStage.vs.param.mView, "param_vs_mView", Uniform);
    findVariableLocation(_mapStage.program, _mapStage.vs.param.targetInTilePosN, "param_vs_targetInTilePosN", Uniform);
    findVariableLocation(_mapStage.program, _mapStage.vs.param.targetTile, "param_vs_targetTile", Uniform);
    findVariableLocation(_mapStage.program, _mapStage.vs.param.tile, "param_vs_tile", Uniform);
    findVariableLocation(_mapStage.program, _mapStage.vs.param.elevationData_sampler, "param_vs_elevationData_sampler", Uniform);
    findVariableLocation(_mapStage.program, _mapStage.vs.param.elevationData_k, "param_vs_elevationData_k", Uniform);
    for(int layerId = 0; layerId < TileLayerId::IdsCount; layerId++)
    {
        const auto layerStructName =
            QString::fromLatin1("param_vs_perTileLayer[%layerId%]")
            .replace(QString::fromLatin1("%layerId%"), QString::number(layerId));
        auto& layerStruct = _mapStage.vs.param.perTileLayer[layerId];

        findVariableLocation(_mapStage.program, layerStruct.tileSizeN, layerStructName + ".tileSizeN", Uniform);
        findVariableLocation(_mapStage.program, layerStruct.tilePaddingN, layerStructName + ".tilePaddingN", Uniform);
        findVariableLocation(_mapStage.program, layerStruct.slotsPerSide, layerStructName + ".slotsPerSide", Uniform);
        findVariableLocation(_mapStage.program, layerStruct.slotIndex, layerStructName + ".slotIndex", Uniform);
    }
    findVariableLocation(_mapStage.program, _mapStage.fs.param.distanceFromCameraToTarget, "param_fs_distanceFromCameraToTarget", Uniform);
    findVariableLocation(_mapStage.program, _mapStage.fs.param.cameraElevationAngle, "param_fs_cameraElevationAngle", Uniform);
    findVariableLocation(_mapStage.program, _mapStage.fs.param.fogColor, "param_fs_fogColor", Uniform);
    findVariableLocation(_mapStage.program, _mapStage.fs.param.fogDistance, "param_fs_fogDistance", Uniform);
    findVariableLocation(_mapStage.program, _mapStage.fs.param.fogDensity, "param_fs_fogDensity", Uniform);
    findVariableLocation(_mapStage.program, _mapStage.fs.param.fogOriginFactor, "param_fs_fogOriginFactor", Uniform);
    findVariableLocation(_mapStage.program, _mapStage.fs.param.scaleToRetainProjectedSize, "param_fs_scaleToRetainProjectedSize", Uniform);
    for(int layerId = TileLayerId::RasterMap, linearIdx = 0; layerId < TileLayerId::IdsCount; layerId++, linearIdx++)
    {
        const auto layerStructName =
            QString::fromLatin1("param_fs_perTileLayer[%linearIdx%]")
            .replace(QString::fromLatin1("%linearIdx%"), QString::number(linearIdx));
        auto& layerStruct = _mapStage.fs.param.perTileLayer[linearIdx];

        findVariableLocation(_mapStage.program, layerStruct.k, layerStructName + ".k", Uniform);
        findVariableLocation(_mapStage.program, layerStruct.sampler, layerStructName + ".sampler", Uniform);
    }
    _programVariables.clear();
}

void OsmAnd::AtlasMapRenderer_OpenGL::performRendering()
{
    AtlasMapRenderer_BaseOpenGL::performRendering();
    GL_CHECK_RESULT;
 
    // Setup viewport
    GLint oldViewport[4];
    glGetIntegerv(GL_VIEWPORT, oldViewport);
    GL_CHECK_RESULT;
    glViewport(
        _activeConfig.viewport.left,
        _activeConfig.windowSize.y - _activeConfig.viewport.bottom,
        _activeConfig.viewport.width(),
        _activeConfig.viewport.height());
    GL_CHECK_RESULT;

    performRendering_SkyStage();
    performRendering_MapStage();

    // Revert viewport
    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
    GL_CHECK_RESULT;
}

void OsmAnd::AtlasMapRenderer_OpenGL::performRendering_MapStage()
{
    // Set tile patch VAO
    assert(glBindVertexArray);
    glBindVertexArray(_tilePatchVAO);
    GL_CHECK_RESULT;

    // Activate program
    assert(glUseProgram);
    glUseProgram(_mapStage.program);
    GL_CHECK_RESULT;

    // Set matrices
    auto mProjectionView = _mProjection * _mView;
    glUniformMatrix4fv(_mapStage.vs.param.mProjectionView, 1, GL_FALSE, glm::value_ptr(mProjectionView));
    GL_CHECK_RESULT;
    glUniformMatrix4fv(_mapStage.vs.param.mView, 1, GL_FALSE, glm::value_ptr(_mView));
    GL_CHECK_RESULT;

    // Set center offset
    glUniform2f(_mapStage.vs.param.targetInTilePosN, _targetInTilePosN.x, _targetInTilePosN.y);
    GL_CHECK_RESULT;

    // Set target tile
    glUniform2i(_mapStage.vs.param.targetTile, _targetTile.x, _targetTile.y);
    GL_CHECK_RESULT;

    // Set distance to camera from target
    glUniform1f(_mapStage.fs.param.distanceFromCameraToTarget, _distanceFromCameraToTarget);
    GL_CHECK_RESULT;

    // Set camera elevation angle
    glUniform1f(_mapStage.fs.param.cameraElevationAngle, _activeConfig.elevationAngle);
    GL_CHECK_RESULT;

    // Set fog parameters
    glUniform3f(_mapStage.fs.param.fogColor, _activeConfig.fogColor[0], _activeConfig.fogColor[1], _activeConfig.fogColor[2]);
    GL_CHECK_RESULT;
    glUniform1f(_mapStage.fs.param.fogDistance, _activeConfig.fogDistance);
    GL_CHECK_RESULT;
    glUniform1f(_mapStage.fs.param.fogDensity, _activeConfig.fogDensity);
    GL_CHECK_RESULT;
    glUniform1f(_mapStage.fs.param.fogOriginFactor, _activeConfig.fogOriginFactor);
    GL_CHECK_RESULT;
    glUniform1f(_mapStage.fs.param.scaleToRetainProjectedSize, _scaleToRetainProjectedSize);
    GL_CHECK_RESULT;

    // Set samplers
    glUniform1i(_mapStage.vs.param.elevationData_sampler, TileLayerId::ElevationData);
    GL_CHECK_RESULT;
    for(int layerId = TileLayerId::RasterMap; layerId < TileLayerId::IdsCount; layerId++)
    {
        glUniform1i(_mapStage.fs.param.perTileLayer[layerId - TileLayerId::RasterMap].sampler, layerId);
        GL_CHECK_RESULT;
    }

    // For each visible tile, render it
    const auto maxTileIndex = static_cast<signed>(1u << _activeConfig.zoomBase);
    for(auto itTileId = _visibleTiles.begin(); itTileId != _visibleTiles.end(); ++itTileId)
    {
        const auto& tileId = *itTileId;

        // Get normalized tile index
        TileId tileIdN = tileId;
        while(tileIdN.x < 0)
            tileIdN.x += maxTileIndex;
        while(tileIdN.y < 0)
            tileIdN.y += maxTileIndex;
        if(_activeConfig.zoomBase < 31)
        {
            while(tileIdN.x >= maxTileIndex)
                tileIdN.x -= maxTileIndex;
            while(tileIdN.y >= maxTileIndex)
                tileIdN.y -= maxTileIndex;
        }

        // Set tile id
        glUniform2i(_mapStage.vs.param.tile, tileId.x, tileId.y);
        GL_CHECK_RESULT;

        // Set elevation data
        {
            auto& tileLayer = _tileLayers[TileLayerId::ElevationData];

            QMutexLocker scopeLock(&tileLayer._cacheModificationMutex);

            std::shared_ptr<TileZoomCache::Tile> cachedTile_;
            bool cacheHit = tileLayer._cache.getTile(_activeConfig.zoomBase, tileIdN, cachedTile_);
            if(cacheHit)
            {
                auto cachedTile = static_cast<CachedTile*>(cachedTile_.get());

                glUniform1f(_mapStage.vs.param.elevationData_k, 1.0f);
                GL_CHECK_RESULT;

                glActiveTexture(GL_TEXTURE0 + TileLayerId::ElevationData);
                GL_CHECK_RESULT;

                glEnable(GL_TEXTURE_2D);
                GL_CHECK_RESULT;

                glBindTexture(GL_TEXTURE_2D, reinterpret_cast<GLuint>(cachedTile->textureRef));
                GL_CHECK_RESULT;

                const auto& perTile_vs = _mapStage.vs.param.perTileLayer[TileLayerId::ElevationData];
                glUniform1i(perTile_vs.slotIndex, cachedTile->atlasSlotIndex);
                GL_CHECK_RESULT;
                if(cachedTile->atlasSlotIndex >= 0)
                {
                    const auto& atlas = tileLayer._atlasTexturePools[cachedTile->atlasPoolId];
                    glUniform1f(perTile_vs.tileSizeN, atlas._tileSizeN);
                    GL_CHECK_RESULT;
                    glUniform1f(perTile_vs.tilePaddingN, atlas._tilePaddingN);
                    GL_CHECK_RESULT;
                    glUniform1i(perTile_vs.slotsPerSide, atlas._slotsPerSide);
                    GL_CHECK_RESULT;

                    glBindSampler(TileLayerId::ElevationData, _textureSampler_ElevationData_Atlas);
                    GL_CHECK_RESULT;
                }
                else
                {
                    glBindSampler(TileLayerId::ElevationData, _textureSampler_ElevationData_NoAtlas);
                    GL_CHECK_RESULT;
                }
            }
            else
            {
                glUniform1f(_mapStage.vs.param.elevationData_k, 0.0f);
                GL_CHECK_RESULT;
            }
        }

        // We need to pass each layer of this tile to shader
        for(int layerId = TileLayerId::RasterMap; layerId < TileLayerId::IdsCount; layerId++)
        {
            auto& tileLayer = _tileLayers[layerId];
            const auto& perTile_vs = _mapStage.vs.param.perTileLayer[layerId];
            const auto& perTile_fs = _mapStage.fs.param.perTileLayer[layerId - TileLayerId::RasterMap];

            QMutexLocker scopeLock(&tileLayer._cacheModificationMutex);

            std::shared_ptr<TileZoomCache::Tile> cachedTile_;
            bool cacheHit = tileLayer._cache.getTile(_activeConfig.zoomBase, tileIdN, cachedTile_);
            if(cacheHit)
            {
                auto cachedTile = static_cast<CachedTile*>(cachedTile_.get());

                if(cachedTile->textureRef == nullptr)
                {
                    //TODO: render "not available" stub
                    glUniform1f(perTile_fs.k, 0.0f);
                    GL_CHECK_RESULT;
                }
                else
                {
                    glUniform1f(perTile_fs.k, 1.0f);
                    GL_CHECK_RESULT;

                    glActiveTexture(GL_TEXTURE0 + layerId);
                    GL_CHECK_RESULT;

                    glBindTexture(GL_TEXTURE_2D, reinterpret_cast<GLuint>(cachedTile->textureRef));
                    GL_CHECK_RESULT;

                    glUniform1i(perTile_vs.slotIndex, cachedTile->atlasSlotIndex);
                    GL_CHECK_RESULT;
                    if(cachedTile->atlasSlotIndex >= 0)
                    {
                        const auto& atlas = tileLayer._atlasTexturePools[cachedTile->atlasPoolId];

                        glUniform1f(perTile_vs.tileSizeN, atlas._tileSizeN);
                        GL_CHECK_RESULT;
                        glUniform1f(perTile_vs.tilePaddingN, atlas._tilePaddingN);
                        GL_CHECK_RESULT;
                        glUniform1i(perTile_vs.slotsPerSide, atlas._slotsPerSide);
                        GL_CHECK_RESULT;

                        glBindSampler(layerId, _textureSampler_Bitmap_Atlas);
                        GL_CHECK_RESULT;
                    }
                    else
                    {
                        glBindSampler(layerId, _textureSampler_Bitmap_NoAtlas);
                        GL_CHECK_RESULT;
                    }
                }
            }
            else
            {
                //TODO: render "in-progress" stub
                glUniform1f(perTile_fs.k, 0.0f);
                GL_CHECK_RESULT;
            }
        }

        const auto verticesCount = _activeConfig.tileProviders[TileLayerId::ElevationData]
        ? (_activeConfig.heightmapPatchesPerSide * _activeConfig.heightmapPatchesPerSide) * 4 * 3
            : 6;
        glDrawElements(GL_TRIANGLES, verticesCount, GL_UNSIGNED_SHORT, nullptr);
        GL_CHECK_RESULT;
    }

    // Disable textures
    for(int layerId = 0; layerId < TileLayerId::IdsCount; layerId++)
    {
        glActiveTexture(GL_TEXTURE0 + layerId);
        GL_CHECK_RESULT;

        glBindTexture(GL_TEXTURE_2D, 0);
        GL_CHECK_RESULT;
    }

    // Deactivate program
    glUseProgram(0);
    GL_CHECK_RESULT;

    // Deselect VAO
    glBindVertexArray(0);
    GL_CHECK_RESULT;
}

void OsmAnd::AtlasMapRenderer_OpenGL::releaseRendering()
{
    releaseRendering_MapStage();
    releaseRendering_SkyStage();

    AtlasMapRenderer_BaseOpenGL::releaseRendering();
    MapRenderer_OpenGL::releaseRendering();
}

void OsmAnd::AtlasMapRenderer_OpenGL::releaseRendering_MapStage()
{
    if(_mapStage.program)
    {
        assert(glDeleteProgram);
        glDeleteProgram(_mapStage.program);
        GL_CHECK_RESULT;
    }
    if(_mapStage.fs.id)
    {
        assert(glDeleteShader);
        glDeleteShader(_mapStage.fs.id);
        GL_CHECK_RESULT;
    }
    if(_mapStage.vs.id)
    {
        glDeleteShader(_mapStage.vs.id);
        GL_CHECK_RESULT;
    }
    memset(&_mapStage, 0, sizeof(_mapStage));
}

void OsmAnd::AtlasMapRenderer_OpenGL::allocateTilePatch( MapTileVertex* vertices, size_t verticesCount, GLushort* indices, size_t indicesCount )
{
    // Create Vertex Array Object
    assert(glGenVertexArrays);
    glGenVertexArrays(1, &_tilePatchVAO);
    GL_CHECK_RESULT;
    assert(glBindVertexArray);
    glBindVertexArray(_tilePatchVAO);
    GL_CHECK_RESULT;

    // Create vertex buffer and associate it with VAO
    assert(glGenBuffers);
    glGenBuffers(1, &_tilePatchVBO);
    GL_CHECK_RESULT;
    assert(glBindBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, _tilePatchVBO);
    GL_CHECK_RESULT;
    assert(glBufferData);
    glBufferData(GL_ARRAY_BUFFER, verticesCount * sizeof(MapTileVertex), vertices, GL_STATIC_DRAW);
    GL_CHECK_RESULT;
    assert(glEnableVertexAttribArray);
    glEnableVertexAttribArray(_mapStage.vs.in.vertexPosition);
    GL_CHECK_RESULT;
    assert(glVertexAttribPointer);
    glVertexAttribPointer(_mapStage.vs.in.vertexPosition, 2, GL_FLOAT, GL_FALSE, sizeof(MapTileVertex), reinterpret_cast<GLvoid*>(offsetof(MapTileVertex, position)));
    GL_CHECK_RESULT;
    assert(glEnableVertexAttribArray);
    glEnableVertexAttribArray(_mapStage.vs.in.vertexTexCoords);
    GL_CHECK_RESULT;
    assert(glVertexAttribPointer);
    glVertexAttribPointer(_mapStage.vs.in.vertexTexCoords, 2, GL_FLOAT, GL_FALSE, sizeof(MapTileVertex), reinterpret_cast<GLvoid*>(offsetof(MapTileVertex, uv)));
    GL_CHECK_RESULT;

    // Create index buffer and associate it with VAO
    assert(glGenBuffers);
    glGenBuffers(1, &_tilePatchIBO);
    GL_CHECK_RESULT;
    assert(glBindBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _tilePatchIBO);
    GL_CHECK_RESULT;
    assert(glBufferData);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indicesCount * sizeof(GLushort), indices, GL_STATIC_DRAW);
    GL_CHECK_RESULT;

    glBindVertexArray(0);
    GL_CHECK_RESULT;
}

void OsmAnd::AtlasMapRenderer_OpenGL::releaseTilePatch()
{
    if(_tilePatchIBO)
    {
        assert(glDeleteBuffers);
        glDeleteBuffers(1, &_tilePatchIBO);
        GL_CHECK_RESULT;
        _tilePatchIBO = 0;
    }

    if(_tilePatchVBO)
    {
        assert(glDeleteBuffers);
        glDeleteBuffers(1, &_tilePatchVBO);
        GL_CHECK_RESULT;
        _tilePatchVBO = 0;
    }

    if(_tilePatchVAO)
    {
        assert(glDeleteVertexArrays);
        glDeleteVertexArrays(1, &_tilePatchVAO);
        GL_CHECK_RESULT;
        _tilePatchVAO = 0;
    }
}

void OsmAnd::AtlasMapRenderer_OpenGL::initializeRendering_SkyStage()
{
    // Vertex data (x,y)
    float vertices[4][2] =
    {
        {-1.0f,-1.0f},
        {-1.0f, 1.0f},
        { 1.0f, 1.0f},
        { 1.0f,-1.0f}
    };
    const auto verticesCount = 4;

    // Index data
    GLushort indices[6] =
    {
        0, 1, 2,
        0, 2, 3
    };
    const auto indicesCount = 6;

    // Create Vertex Array Object
    assert(glGenVertexArrays);
    glGenVertexArrays(1, &_skyStage.vao);
    GL_CHECK_RESULT;
    assert(glBindVertexArray);
    glBindVertexArray(_skyStage.vao);
    GL_CHECK_RESULT;

    // Create vertex buffer and associate it with VAO
    assert(glGenBuffers);
    glGenBuffers(1, &_skyStage.vbo);
    GL_CHECK_RESULT;
    assert(glBindBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, _skyStage.vbo);
    GL_CHECK_RESULT;
    assert(glBufferData);
    glBufferData(GL_ARRAY_BUFFER, verticesCount * sizeof(float) * 2, vertices, GL_STATIC_DRAW);
    GL_CHECK_RESULT;
    assert(glEnableVertexAttribArray);
    glEnableVertexAttribArray(_skyStage.vs.in.vertexPosition);
    GL_CHECK_RESULT;
    assert(glVertexAttribPointer);
    glVertexAttribPointer(_skyStage.vs.in.vertexPosition, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, nullptr);
    GL_CHECK_RESULT;
        
    // Create index buffer and associate it with VAO
    assert(glGenBuffers);
    glGenBuffers(1, &_skyStage.ibo);
    GL_CHECK_RESULT;
    assert(glBindBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _skyStage.ibo);
    GL_CHECK_RESULT;
    assert(glBufferData);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indicesCount * sizeof(GLushort), indices, GL_STATIC_DRAW);
    GL_CHECK_RESULT;

    glBindVertexArray(0);
    GL_CHECK_RESULT;

    // Compile vertex shader
    const QString vertexShader = QString::fromLatin1(
        "#version 430 core                                                                                                  ""\n"
        "                                                                                                                   ""\n"
        // Constants
        "const float floatEpsilon = 0.000001;                                                                               ""\n"
        "                                                                                                                   ""\n"
        // Input data
        "in vec2 in_vs_vertexPosition;                                                                                      ""\n"
        "                                                                                                                   ""\n"
        // Output data
        "out float v2f_horizonOffsetN;                                                                                      ""\n"
        "                                                                                                                   ""\n"
        // Parameters: common data
        "uniform mat4 param_vs_mProjectionViewModel;                                                                        ""\n"
        "uniform vec2 param_vs_halfSize;                                                                                    ""\n"
        "                                                                                                                   ""\n"
        "void main()                                                                                                        ""\n"
        "{                                                                                                                  ""\n"
        "    vec4 v = vec4(in_vs_vertexPosition.x * param_vs_halfSize.x,                                                    ""\n"
        "        in_vs_vertexPosition.y * param_vs_halfSize.y, 0.0, 1.0);                                                   ""\n"
        "                                                                                                                   ""\n"
        //   Horizon offset is in range [-1.0 ... +1.0], what is the same as input vertex data
        "    v2f_horizonOffsetN = in_vs_vertexPosition.y;                                                                   ""\n"
        "    gl_Position = param_vs_mProjectionViewModel * v;                                                               ""\n"
        "}                                                                                                                  ""\n");
    QString preprocessedVertexShader = vertexShader;
    _skyStage.vs.id = compileShader(GL_VERTEX_SHADER, preprocessedVertexShader.toStdString().c_str());
    assert(_skyStage.vs.id != 0);

    // Compile fragment shader
    const QString fragmentShader = QString::fromLatin1(
        "#version 430 core                                                                                                  ""\n"
        "                                                                                                                   ""\n"
        // Constants
        "const float floatEpsilon = 0.000001;                                                                               ""\n"
        "                                                                                                                   ""\n"
        // Input data
        "in float v2f_horizonOffsetN;                                                                                       ""\n"
        "                                                                                                                   ""\n"
        // Output data
        "out vec4 out_color;                                                                                                ""\n"
        "                                                                                                                   ""\n"
        // Parameters: common data
        "uniform vec3 param_fs_skyColor;                                                                                    ""\n"
        "uniform vec3 param_fs_fogColor;                                                                                    ""\n"
        "uniform float param_fs_fogDensity;                                                                                 ""\n"
        "uniform float param_fs_fogOriginFactor;                                                                            ""\n"
        "uniform float param_fs_scaleToRetainProjectedSize;                                                                 ""\n"
        "                                                                                                                   ""\n"
        "void main()                                                                                                        ""\n"
        "{                                                                                                                  ""\n"
        "    const float fogHeight = 1.0 * param_fs_scaleToRetainProjectedSize / param_fs_scaleToRetainProjectedSize;                                             ""\n"
        "    const float fogStartHeight = fogHeight * (1.0 - param_fs_fogOriginFactor);                                     ""\n"
        "    const float fragmentHeight = 1.0 - v2f_horizonOffsetN;                                                         ""\n"
        //   Fog linear is factor in range [0.0 ... 1.0]
        "    const float fogLinearFactor = min(max(fragmentHeight - fogStartHeight, 0.0) /                                  ""\n"
        "        (fogHeight - fogStartHeight), 1.0);                                                                        ""\n"
        "    const float fogFactorBase = fogLinearFactor * param_fs_fogDensity;                                             ""\n"
        "    const float fogFactor = clamp(exp(-fogFactorBase*fogFactorBase), 0.0, 1.0);                                    ""\n"
        "    const vec3 mixedColor = mix(param_fs_skyColor, param_fs_fogColor, 1.0 - fogFactor);                            ""\n"
        "    out_color.rgba = vec4(mixedColor, 1.0);                                                                        ""\n"
        "}                                                                                                                  ""\n");
    QString preprocessedFragmentShader = fragmentShader;
    QString preprocessedFragmentShader_UnrolledPerLayerProcessingCode;
    _skyStage.fs.id = compileShader(GL_FRAGMENT_SHADER, preprocessedFragmentShader.toStdString().c_str());
    assert(_skyStage.fs.id != 0);

    // Link everything into program object
    GLuint shaders[] = {
        _skyStage.vs.id,
        _skyStage.fs.id
    };
    _skyStage.program = linkProgram(2, shaders);
    assert(_skyStage.program != 0);

    _programVariables.clear();
    findVariableLocation(_skyStage.program, _skyStage.vs.in.vertexPosition, "in_vs_vertexPosition", In);
    findVariableLocation(_skyStage.program, _skyStage.vs.param.mProjectionViewModel, "param_vs_mProjectionViewModel", Uniform);
    findVariableLocation(_skyStage.program, _skyStage.vs.param.halfSize, "param_vs_halfSize", Uniform);
    findVariableLocation(_skyStage.program, _skyStage.fs.param.fogColor, "param_fs_fogColor", Uniform);
    findVariableLocation(_skyStage.program, _skyStage.fs.param.skyColor, "param_fs_skyColor", Uniform);
    findVariableLocation(_skyStage.program, _skyStage.fs.param.fogDensity, "param_fs_fogDensity", Uniform);
    findVariableLocation(_skyStage.program, _skyStage.fs.param.fogOriginFactor, "param_fs_fogOriginFactor", Uniform);
    findVariableLocation(_skyStage.program, _skyStage.fs.param.scaleToRetainProjectedSize, "param_fs_scaleToRetainProjectedSize", Uniform);
    _programVariables.clear();
}

void OsmAnd::AtlasMapRenderer_OpenGL::performRendering_SkyStage()
{
#if 0
    {
        const auto mFogTranslate = glm::translate(0.0f, 0.0f, -_correctedFogDistance);
        const auto mModel = _mAzimuthInv * mFogTranslate;

        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(glm::value_ptr(_mProjection));
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(glm::value_ptr(_mView * mModel));
        glColor4f(0.0f, 0.0f, 1.0f, 1.0f);
        glBegin(GL_LINES);
            glVertex3f(-100.0f, 0.5f, 0.0f);
            glVertex3f(+100.0f, 0.5f, 0.0f);
        glEnd();
    }
#endif

    // Set tile patch VAO
    assert(glBindVertexArray);
    glBindVertexArray(_skyStage.vao);
    GL_CHECK_RESULT;

    // Activate program
    assert(glUseProgram);
    glUseProgram(_skyStage.program);
    GL_CHECK_RESULT;

    // Set projection*view*model matrix:
    const auto mFogTranslate = glm::translate(0.0f, 0.0f, -_correctedFogDistance);
    const auto mModel = _mAzimuthInv * mFogTranslate;
    const auto mProjectionViewModel = _mProjection * _mView * mModel;
    glUniformMatrix4fv(_skyStage.vs.param.mProjectionViewModel, 1, GL_FALSE, glm::value_ptr(mProjectionViewModel));
    GL_CHECK_RESULT;

    // Set halfsize
    glUniform2f(_skyStage.vs.param.halfSize, _skyplaneHalfSize.x, _skyplaneHalfSize.y);
    GL_CHECK_RESULT;

    // Set fog and sky parameters
    glUniform3f(_skyStage.fs.param.skyColor, _activeConfig.skyColor[0], _activeConfig.skyColor[1], _activeConfig.skyColor[2]);
    GL_CHECK_RESULT;
    glUniform3f(_skyStage.fs.param.fogColor, _activeConfig.fogColor[0], _activeConfig.fogColor[1], _activeConfig.fogColor[2]);
    GL_CHECK_RESULT;
    glUniform1f(_skyStage.fs.param.fogDensity, _activeConfig.fogDensity);
    GL_CHECK_RESULT;
    glUniform1f(_skyStage.fs.param.fogOriginFactor, _activeConfig.fogOriginFactor);
    GL_CHECK_RESULT;
    glUniform1f(_skyStage.fs.param.scaleToRetainProjectedSize, _scaleToRetainProjectedSize);
    GL_CHECK_RESULT;

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
    GL_CHECK_RESULT;

    // Deactivate program
    glUseProgram(0);
    GL_CHECK_RESULT;

    // Deselect VAO
    glBindVertexArray(0);
    GL_CHECK_RESULT;
}

void OsmAnd::AtlasMapRenderer_OpenGL::releaseRendering_SkyStage()
{
    if(_skyStage.ibo)
    {
        assert(glDeleteBuffers);
        glDeleteBuffers(1, &_skyStage.ibo);
        GL_CHECK_RESULT;
    }

    if(_skyStage.vbo)
    {
        assert(glDeleteBuffers);
        glDeleteBuffers(1, &_skyStage.vbo);
        GL_CHECK_RESULT;
    }

    if(_skyStage.vao)
    {
        assert(glDeleteVertexArrays);
        glDeleteVertexArrays(1, &_skyStage.vao);
        GL_CHECK_RESULT;
    }

    if(_skyStage.program)
    {
        assert(glDeleteProgram);
        glDeleteProgram(_skyStage.program);
        GL_CHECK_RESULT;
    }
    if(_skyStage.fs.id)
    {
        assert(glDeleteShader);
        glDeleteShader(_skyStage.fs.id);
        GL_CHECK_RESULT;
    }
    if(_skyStage.vs.id)
    {
        glDeleteShader(_skyStage.vs.id);
        GL_CHECK_RESULT;
    }
    memset(&_skyStage, 0, sizeof(_skyStage));
}
