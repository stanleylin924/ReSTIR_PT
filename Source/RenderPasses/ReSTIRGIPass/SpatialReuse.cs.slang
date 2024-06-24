/***************************************************************************
 # Copyright (c) 2022, Daqi Lin.  All rights reserved.
 **************************************************************************/
import Params;
import Shift;
import Scene.HitInfo;
import PathReservoir;
import PathTracer;
import Scene.Scene;
import Utils.Debug.PixelDebug;
import Rendering.Utils.PixelStats;
import Utils.Sampling.TinyUniformSampleGenerator;
import Utils.Math.Ray;
import Utils.Math.MathHelpers;
import LoadShadingData;
import Rendering.Materials.MaterialShading;

struct PathReusePass
{
    static const uint kNeighborOffsetCount = NEIGHBOR_OFFSET_COUNT;
    static const uint kNeighborOffsetMask = kNeighborOffsetCount - 1;

    // Resources
    RestirPathTracerParams params;                        ///< Runtime parameters.

    Texture2D<PackedHitInfo> vbuffer;                     ///< Fullscreen V-buffer for the primary hits.
    Texture1D<float2> neighborOffsets;

    RWTexture2D<float4> outputColor;                      ///< Output resolved color.
    StructuredBuffer<PathReservoir> outputReservoirs;     // reservoir from previous pass
    RWStructuredBuffer<PathReservoir> temporalReservoirs; // resulting reservoir for next frame

    int gSpatialRoundId;
    int  gNumSpatialRounds;
    int gNeighborCount;
    float gGatherRadius;

    bool gIsLastRound;

    bool gFeatureBasedRejection;

    uint gSppId;

    Texture2D<float4> directLighting;  
    bool useDirectLighting;

    bool isValidPackedHitInfo(PackedHitInfo packed)
    {
        return packed.x != 0;
    }

    bool isValidScreenRegion(int2 pixel) { return all(pixel >= 0 && pixel < params.frameDim); }

    bool isValidGeometry(ShadingData centralSd, ShadingData neighborSd)
    {
        if (!gFeatureBasedRejection) return true;
        float3 camPos = gScene.camera.data.posW;
        float centralDist = distance(camPos, centralSd.posW);
        float neighborDist = distance(camPos, neighborSd.posW);
        return dot(centralSd.N, neighborSd.N) >= 0.5f && abs(centralDist - neighborDist) < 0.1f * centralDist;
    }

    ShadingData getPixelShadingData(int2 pixel, out PackedHitInfo PrimaryHitPacked)
    {
        ShadingData sd = {};
        Ray ray = gScene.camera.computeRayPinhole(pixel, params.frameDim);
        PrimaryHitPacked = vbuffer[pixel];
        if (isValidPackedHitInfo(PrimaryHitPacked))
        {
            HitInfo PrimaryHit; PrimaryHit.unpack(PrimaryHitPacked);

            TriangleHit th = PrimaryHit.getTriangleHit();
            sd = loadShadingData(PrimaryHit, -ray.dir, true);
        }
        return sd;
    }

    int getNeighborCount()
    {
        return gNeighborCount; // does not include self
    }

    int2 getNextNeighborPixel(const uint startIndex, int2 pixel, int i)
    {
        int2 neighborPixel = int2(0,0);

        {
            uint neighborIndex = (startIndex + i) & kNeighborOffsetMask;
            neighborPixel = pixel + int2(neighborOffsets[neighborIndex] * gGatherRadius);
        }
 
        return neighborPixel;
    }

    void ReSTIR(const uint2 pixel)
    {
        var sg = TinyUniformSampleGenerator(pixel, (kCandidateSamples + 1 + gNumSpatialRounds) * params.seed + kCandidateSamples + 1 + gSpatialRoundId);

        // Compute offset into per-sample buffers. All samples are stored consecutively at this offset.
        const uint centralOffset = params.getReservoirOffset(pixel);
        PathReservoir dstReservoir = outputReservoirs[centralOffset];
        PathReservoir centralReservoir = dstReservoir;

        float3 initialColor = dstReservoir.F * dstReservoir.weight;

        float centralM = dstReservoir.M;
        PackedHitInfo centralPrimaryHitPacked;
        ShadingData centralPrimarySd = getPixelShadingData(pixel, centralPrimaryHitPacked);

        if (!isValidPackedHitInfo(centralPrimaryHitPacked)) return;

        {
            dstReservoir.prepareMerging();
        }

        float3 color = 0.f;

        int neighborCount = getNeighborCount();
        const uint startIndex = sampleNext1D(sg) * kNeighborOffsetCount;

        /////////////////////
        /// CONSTANT RMIS ///
        /////////////////////
        {
            uint2 chosenPixel = pixel;
            int chosen_i = -1;
            float chosen_Jacobian = 1.f;

            for (uint i = 0; i < neighborCount; ++i)
            {
                int2 neighborPixel = getNextNeighborPixel(startIndex, pixel, i);
                if (!isValidScreenRegion(neighborPixel)) continue;

                PathReservoir neighborReservoir = outputReservoirs[params.getReservoirOffset(neighborPixel)];
                PackedHitInfo neighborPrimaryHitPacked;
                ShadingData neighborPrimarySd = getPixelShadingData(neighborPixel, neighborPrimaryHitPacked);
                if (!isValidPackedHitInfo(neighborPrimaryHitPacked)) continue;
                if (!isValidGeometry(centralPrimarySd, neighborPrimarySd)) continue;

                float dstJacobian;

                bool selected = shiftAndMergeReservoir(params, false, dstJacobian, centralPrimaryHitPacked, centralPrimarySd, dstReservoir,
                    neighborPrimarySd, neighborReservoir, true, sg, true);

                if (selected)
                {
                    chosen_i = i;
                    chosenPixel = neighborPixel;
                    chosen_Jacobian = dstJacobian;
                }
            }

            if (dstReservoir.weight > 0)
            {
                float count = 0;

                // Evaluate the MIS weight by shifting back.

                PackedHitInfo chosenPrimaryHitPacked;
                ShadingData chosenPrimarySd = getPixelShadingData(chosenPixel, chosenPrimaryHitPacked);
                PathReservoir chosenReservoir = outputReservoirs[params.getReservoirOffset(chosenPixel)];

                float chosen_approxPdf = 0.f;
                float sum_approxPdf = 0.f;

                dstReservoir.finalizeRIS();

                color = dstReservoir.F * dstReservoir.weight;
            }
        }

        if (dstReservoir.weight < 0.f) dstReservoir.weight = 0.f;
        if (isnan(dstReservoir.weight) || isinf(dstReservoir.weight)) dstReservoir.weight = 0.f;

        temporalReservoirs[centralOffset] = dstReservoir;

        if (any(isnan(color) || isinf(color) || color < 0.f)) color = 0.f;
        if (gIsLastRound)
        {
            if (useDirectLighting)
            {
                color += directLighting[pixel].rgb;
                if (any(isnan(color) || isinf(color))) color = 0.f;
            }

            outputColor[pixel] += float4(color / kSamplesPerPixel, 1.f);
        }

    }

    /** Entry point for merge pass.
        \param[in] pixel Pixel coordinates.
    */
    void execute(const uint2 _pixel)
    {
        const uint2 pixel = _pixel;

        if (any(pixel >= params.frameDim)) return;

        printSetPixel(pixel);
        logSetPixel(pixel);

        ReSTIR(pixel);
    }
};

cbuffer CB
{
    PathReusePass gPathReusePass;
}


[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    gPathReusePass.execute(dispatchThreadId.xy);
}
