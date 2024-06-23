/***************************************************************************
 # Copyright (c) 2022, Daqi Lin.  All rights reserved.
 **************************************************************************/
#include "Scene/Material/MaterialDefines.slangh"

import Rendering.Materials.MaterialShading;
import LoadShadingData;
import Utils.Debug.PixelDebug;
import Utils.Math.HashUtils;
import Params;
import PathReservoir;
import Rendering.Utils.PixelStats;
import Scene.RaytracingInline;
import Rendering.Materials.MaterialShading;
import Utils.Sampling.TinyUniformSampleGenerator;

static const int kRcWiCount = 1;

/// Builds a Path and a PathPrefix.
struct PathBuilder
{
    TinyUniformSampleGenerator sg;
    ReSTIRPathFlags pathFlags;
    float3 cachedJacobian = 1.f;

    [mutating]
    void init<S : ISampleGenerator>(inout S sg)
    {
        this.sg = TinyUniformSampleGenerator(jenkinsHash(sg.next()));
    }

    void finalize(inout PathReservoir pathReservoir)
    {
        pathReservoir.M = 1;
    }

    [mutating]
    bool addNeeVertex(RestirPathTracerParams params, uint pathLength, float3 wi, float3 pathWeight, float3 postfixWeight,
        float russianroulettePDF, float misWeight, float lightPdf, int lightType, inout PathReservoir pathReservoir, bool forceAdd = false)
    {
        bool selected = false;

        if (pathLength >= 1)
        {
            if (forceAdd || pathReservoir.add(pathWeight, russianroulettePDF, sg))
            {
                selected = true;
                pathReservoir.pathFlags = pathFlags;
                pathReservoir.pathFlags.insertPathLength(pathLength); // excluding the NEE vertex
                pathReservoir.pathFlags.insertLastVertexNEE(true);
                pathReservoir.F = pathWeight;

                pathReservoir.cachedJacobian = cachedJacobian;

                pathReservoir.lightPdf = lightPdf;
                pathReservoir.pathFlags.insertLightType(lightType);
            }
        }

        return selected;
    }
};

