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
    TriMeshHitInfo rcVertexHit;
    float3 rcVertexWi[kRcWiCount];
    uint cachedRandomSeed;
    TinyUniformSampleGenerator sg;
    int rcVertexLength = kMaximumPathLength;
    ReSTIRPathFlags pathFlags;
    float3 cachedJacobian = 1.f;

    [mutating]
    void init<S : ISampleGenerator>(inout S sg)
    {
        this.sg = TinyUniformSampleGenerator(jenkinsHash(sg.next()));
        rcVertexWi[0] = float3(0.f);
        rcVertexHit.instanceID = 0xffffffff;
    }

    void finalize(inout PathReservoir pathReservoir)
    {
        pathReservoir.M = 1;
    }

    [mutating]
    bool addEscapeVertex(RestirPathTracerParams params, uint pathLength, float3 wi, float3 pathWeight, float3 postfixWeight,
        float russianroulettePDF, float misWeight, float lightPdf, int lightType, inout PathReservoir pathReservoir, bool forceAdd = false)
    {
        bool selected = false;

        if (pathLength >= 1)
        {
            bool is_rcVertex = pathLength == rcVertexLength;

            if (forceAdd || pathReservoir.add(pathWeight, russianroulettePDF, sg))
            {
                selected = true;
                pathReservoir.pathFlags = pathFlags;
                pathReservoir.pathFlags.insertPathLength(pathLength);
                pathReservoir.pathFlags.insertRcVertexLength(rcVertexLength);
                pathReservoir.pathFlags.insertLastVertexNEE(false);
                pathReservoir.rcVertexIrradiance[0] = postfixWeight;
                pathReservoir.F = pathWeight;
                pathReservoir.rcVertexWi[0] = rcVertexWi[0];
                pathReservoir.rcVertexHit = rcVertexHit;

                pathReservoir.cachedJacobian = cachedJacobian;

                pathReservoir.rcRandomSeed = cachedRandomSeed;

                if (is_rcVertex) // at rcVertex
                {
                    pathReservoir.rcVertexWi[0] = wi;
                    pathReservoir.rcVertexIrradiance[0] /= misWeight; // exclude last vertex dependent mis weight
                }
                pathReservoir.lightPdf = lightPdf;
                pathReservoir.pathFlags.insertLightType(lightType);
            }
        }

        return selected;
    }

    [mutating]
    bool addNeeVertex(RestirPathTracerParams params, uint pathLength, float3 wi, float3 pathWeight, float3 postfixWeight,
        float russianroulettePDF, float misWeight, float lightPdf, int lightType, inout PathReservoir pathReservoir, bool forceAdd = false)
    {
        bool selected = false;

        if (pathLength >= 1)
        {
            bool is_rcVertex = pathLength == rcVertexLength;

            if (forceAdd || pathReservoir.add(pathWeight, russianroulettePDF, sg))
            {
                selected = true;
                pathReservoir.pathFlags = pathFlags;
                pathReservoir.pathFlags.insertPathLength(pathLength); // excluding the NEE vertex
                pathReservoir.pathFlags.insertRcVertexLength(rcVertexLength);
                pathReservoir.pathFlags.insertLastVertexNEE(true);
                pathReservoir.rcVertexIrradiance[0] = postfixWeight;
                pathReservoir.F = pathWeight;
                pathReservoir.rcVertexWi[0] = rcVertexWi[0];
                pathReservoir.rcVertexHit = rcVertexHit;

                pathReservoir.cachedJacobian = cachedJacobian;

                pathReservoir.rcRandomSeed = cachedRandomSeed;

                if (is_rcVertex) // at rcVertex
                {
                    pathReservoir.rcVertexIrradiance[0] = pathReservoir.rcVertexIrradiance[0] * lightPdf / misWeight;
                    pathReservoir.rcVertexWi[0] = wi;
                }
                pathReservoir.lightPdf = lightPdf;
                pathReservoir.pathFlags.insertLightType(lightType);
            }
        }

        return selected;
    }

    [mutating] void markEscapeVertexAsRcVertex(RestirPathTracerParams params, uint pathLength, inout PathReservoir pathReservoir, HitInfo hit, bool isDelta,
        bool isTransmission, bool isSpecularBounce, float lightPdf, uint lightType, float3 rcVertexIrradiance, float3 rcVertexWi, float prevScatterPdf, float geometryFactor)
    {
        pathReservoir.pathFlags.insertRcVertexLength(pathLength);

        pathReservoir.rcVertexHit.initFromHitInfo(hit);
        pathReservoir.pathFlags.insertIsDeltaEvent(isDelta, true);
        pathReservoir.pathFlags.insertIsTransmissionEvent(isTransmission, true);
        pathReservoir.pathFlags.insertIsSpecularBounce(isSpecularBounce, true);
        pathReservoir.lightPdf = lightPdf;
        pathReservoir.cachedJacobian.x = prevScatterPdf;
        pathReservoir.pathFlags.insertLightType(lightType);
        pathReservoir.rcVertexIrradiance[0] = rcVertexIrradiance;
        pathReservoir.rcVertexWi[0] = rcVertexWi;
        pathReservoir.cachedJacobian.z = geometryFactor;
    }
};

