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
import LoadShadingData;
import Rendering.Materials.MaterialShading;

/** TODO.
*/
struct PathReusePass
{
    static const uint kNeighborOffsetCount = NEIGHBOR_OFFSET_COUNT;
    static const uint kNeighborOffsetMask = kNeighborOffsetCount - 1;

    // Resources
    RestirPathTracerParams params;                        ///< Runtime parameters.

    Texture2D<PackedHitInfo> vbuffer;                     ///< Fullscreen V-buffer for the primary hits.
    Texture2D<PackedHitInfo> temporalVbuffer;
    Texture2D<float2> motionVectors;

    RWTexture2D<float4> outputColor;                      ///< Output resolved color.
    RWStructuredBuffer<PathReservoir> outputReservoirs;    // reservoir for next pass
    RWStructuredBuffer<PathReservoir> temporalReservoirs;  // reservoir from previous frame

    Texture2D<float4> directLighting;                  
    bool useDirectLighting;

    int  gNumSpatialRounds;

    bool gIsLastRound;
    bool gEnableTemporalReprojection;

    float gTemporalHistoryLength;

    bool isValidPackedHitInfo(PackedHitInfo packed)
    {
        return packed.x != 0;
    }

    bool isValidScreenRegion(int2 pixel) { return all(pixel >= 0 && pixel < params.frameDim); }

    ShadingData getPixelShadingData(int2 pixel, out PackedHitInfo PrimaryHitPacked)
    {
        ShadingData sd = {};
        Ray ray = gScene.camera.computeRayPinhole(pixel, params.frameDim);
        PrimaryHitPacked = vbuffer[pixel];
        if (isValidPackedHitInfo(PrimaryHitPacked))
        {
            HitInfo PrimaryHit; PrimaryHit.unpack(PrimaryHitPacked);
            sd = loadShadingData(PrimaryHit, -ray.dir, true);
        }
        return sd;
    }

    ShadingData getPixelTemporalShadingData(int2 pixel, out PackedHitInfo PrimaryHitPacked)
    {
        ShadingData sd = {};
        Ray ray = gScene.camera.computeRayPinholePrevFrame(pixel, params.frameDim);
        PrimaryHitPacked = temporalVbuffer[pixel];
        if (isValidPackedHitInfo(PrimaryHitPacked))
        {
            HitInfo PrimaryHit; PrimaryHit.unpack(PrimaryHitPacked);
            sd = loadShadingData(PrimaryHit, -ray.dir, true);
        }
        return sd;
    }

    /*
    * Build a reservoir of paths whose suffix can be merged to a pixel's prefix.
    * This method is optimized for small windows such that we can run the entire
    * algorithm in one pass.
    */
    void ReSTIR(const uint2 pixel)
    {
        var sg = TinyUniformSampleGenerator(pixel, (kCandidateSamples + 1 + gNumSpatialRounds) * params.seed + kCandidateSamples);

        // Compute offset into per-sample buffers. All samples are stored consecutively at this offset.
        const uint centralOffset = params.getReservoirOffset(pixel);
        PathReservoir dstReservoir = outputReservoirs[centralOffset];
        PathReservoir centralReservoir = dstReservoir;
        float currentM = dstReservoir.M;
        float3 initialColor = dstReservoir.F * dstReservoir.weight;

        {
            dstReservoir.prepareMerging();
        }

        PackedHitInfo centralPrimaryHitPacked;
        ShadingData centralPrimarySd = getPixelShadingData(pixel, centralPrimaryHitPacked);
        if (!isValidPackedHitInfo(centralPrimaryHitPacked)) return;

        float3 color = 0.f;
        ReconnectionData dummyRcData;
        dummyRcData.Init();

        {
            bool chooseCurrent = true;

            // color averaging test

            // fetch temporal reservoir
            float2 motionVector = motionVectors[pixel];
            int2 prevPixel = pixel;

            bool foundTemporalSurface = true;

            if (gEnableTemporalReprojection)
            {
                prevPixel = pixel + motionVector * params.frameDim + (sampleNext2D(sg) * 1.f - 0.f);
            }

            if (!isValidScreenRegion(prevPixel) || !foundTemporalSurface) return;

            PackedHitInfo temporalPrimaryHitPacked;
            ShadingData temporalPrimarySd = getPixelTemporalShadingData(prevPixel, temporalPrimaryHitPacked);
            if (!isValidPackedHitInfo(temporalPrimaryHitPacked)) return;

            bool doTemporalUpdateForDynamicScene = kTemporalUpdateForDynamicScene;

            PathReservoir temporalReservoir = temporalReservoirs[params.getReservoirOffset(prevPixel)];

            temporalReservoir.M = min(gTemporalHistoryLength * currentM, temporalReservoir.M);

            float dstJacobian;

            /////////////////////
            /// CONSTANT RMIS ///
            /////////////////////
            {
                float chosen_Jacobian = 1.f;

                bool selected;

                selected = mergeReservoirNoResampling(params, dstReservoir, temporalReservoir, sg);

                if (selected)
                {
                    chooseCurrent = false;
                    chosen_Jacobian = dstJacobian;
                }

                dstReservoir.finalizeRIS();
            }

            if (dstReservoir.weight < 0.f || isinf(dstReservoir.weight) || isnan(dstReservoir.weight)) dstReservoir.weight = 0.f;
            outputReservoirs[centralOffset] = dstReservoir;
            color = dstReservoir.F * dstReservoir.weight;

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
