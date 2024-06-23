/***************************************************************************
 # Copyright (c) 2022, Daqi Lin.  All rights reserved.
 **************************************************************************/

import LoadShadingData;
import Utils.Debug.PixelDebug;
import Utils.Geometry.GeometryHelpers;
import Utils.Sampling.SampleGenerator;
import Scene.HitInfoType;
import Utils.Math.FormatConversion;
import Utils.Math.Ray;
import Params;
import Utils.Math.PackedFormats;

static const int kRcAttrCount = 1;

struct ReSTIRPathFlags
{
    int flags = 0;

    // this struct can store bounce type information for two vertices
    [mutating]
    void insertIsDeltaEvent(bool isDeltaEvent, bool beforeRcVertex)
    {
        flags &= (beforeRcVertex ? ~(0x100) : ~(0x200));
        if (isDeltaEvent) flags |= 1 << (beforeRcVertex ? 8 : 9);
    }

    [mutating]
    void insertIsTransmissionEvent(bool isTransmissionEvent, bool beforeRcVertex)
    {
        flags &= (beforeRcVertex ? ~(0x400) : ~(0x800));
        if (isTransmissionEvent) flags |= 1 << (beforeRcVertex ? 10 : 11);
    }

    // currently doesn't support a material with both "SpecularReflectionMicrofacet" and "SpecularReflectionTransmissionMicrofacet" components
    [mutating]
    void insertIsSpecularBounce(bool isSpecularBounce, bool beforeRcVertex)
    {
        flags &= (beforeRcVertex ? ~(0x4000000) : ~(0x8000000));
        if (isSpecularBounce) flags |= 1 << (beforeRcVertex ? 26 : 27);
    }

    [mutating]
    void transferDeltaInformation(ReSTIRPathFlags srcPathFlags)
    {
        // transfer delta information for half vector reuse (TODO: refactor)
        flags &= ~(0x300);
        flags |= ((srcPathFlags.flags >> 8) & 3) << 8;
    }

    [mutating]
    void transferTransmissionInformation(ReSTIRPathFlags srcPathFlags)
    {
        // transfer delta information for half vector reuse (TODO: refactor)
        flags &= ~(0xc00);
        flags |= ((srcPathFlags.flags >> 10) & 3) << 10;
    }

    [mutating]
    void transferSpecularBounceInformation(ReSTIRPathFlags srcPathFlags)
    {
        // transfer delta information for half vector reuse (TODO: refactor)
        flags &= ~(0xc000000);
        flags |= ((srcPathFlags.flags >> 26) & 3) << 26;
    }

    bool decodeIsDeltaEvent(bool beforeRcVertex)
    {
        if (beforeRcVertex) return (flags >> 8) & 1;
        else return (flags >> 9) & 1;
    }

    bool decodeIsTransmissionEvent(bool beforeRcVertex)
    {
        if (beforeRcVertex) return (flags >> 10) & 1;
        else return (flags >> 11) & 1;
    }

    bool decodeIsSpecularBounce(bool beforeRcVertex)
    {
        if (beforeRcVertex) return (flags >> 26) & 1;
        else return (flags >> 27) & 1;
    }

    // maximum length: 15
    [mutating]
    void insertPathLength(int pathLength)
    {
        flags &= ~0xF;
        flags |= pathLength & 0xF;
    }

    int pathLength()
    {
        return flags & 0xF;
    }

    [mutating]
    void insertLastVertexNEE(bool isNEE)
    {
        flags &= ~0x10000;
        flags |= (int(isNEE) & 1) << 16;
    }

    bool lastVertexNEE()
    {
        return (flags >> 16) & 1;
    }

    [mutating]
    void insertLightType(uint lightType)
    {
        flags &= ~0xc0000;
        flags |= ((int(lightType) & 3) << 18);
    }

    uint lightType()
    {
        return (flags >> 18) & 3;
    }

};


struct TriMeshHitInfo
{
    uint instanceID;
    uint primitiveIndex;
    //uint barycentrics; // lossy compression of barycentrics
    float2 barycentrics;

    HitInfo getHitInfo()
    {
        if (instanceID == 0xffffffff)
        {
            return HitInfo();
        }
        else
        {
            TriangleHit hit;
            //hit.type = InstanceType::TriangleMesh;
            hit.instanceID = GeometryInstanceID(instanceID, 0);
            hit.primitiveIndex = primitiveIndex;
            hit.barycentrics = barycentrics;
            //hit.barycentrics = unpackUnorm2x16(barycentrics); // lossy compression of barycentrics
            return HitInfo(hit);
        }
    }

    [mutating]
    void initFromHitInfo(HitInfo hitInfo)
    {
        if (!hitInfo.isValid()) instanceID = 0xffffffff;
        else
        {
            TriangleHit hit = hitInfo.getTriangleHit();
            instanceID = hit.instanceID.index;
            primitiveIndex = hit.primitiveIndex;
            barycentrics = hit.barycentrics;
            //barycentrics = packUnorm2x16_unsafe(hit.barycentrics); // lossy compression of barycentrics
        }
    }
};

struct PathReuseMISWeight
{
    float rcBSDFMISWeight;
    float rcNEEMISWeight;
}

// 88/128 B
struct PathReservoir
{
    float M = 0.f; // this is a float, because temporal history length is allowed to be a fraction. 
    float weight = 0.f; // during RIS and when used as a "RisState", this is w_sum; during RIS when used as an incoming reseroivr or after RIS, this is 1/p(y) * 1/M * w_sum
    ReSTIRPathFlags pathFlags; // this is a path type indicator, see the struct definition for details
    uint rcRandomSeed; // saved random seed after rcVertex (due to the need of blending half-vector reuse and random number replay)
    float3 F = 0.f; // cached integrand (always updated after a new path is chosen in RIS)
    float lightPdf; // NEE light pdf (might change after shift if transmission is included since light sampling considers "upperHemisphere" of the previous bounce)
    float3 cachedJacobian; // saved previous vertex scatter PDF, scatter PDF, and geometry term at rcVertex (used when rcVertex is not v2)
    uint initRandomSeed; // saved random seed at the first bounce (for recovering the random distance threshold for hybrid shift)
    TriMeshHitInfo rcVertexHit; // hitinfo of the reconnection vertex
    float3 rcVertexWi[kRcAttrCount]; // incident direction on reconnection vertex
    float3 rcVertexIrradiance[kRcAttrCount]; // sampled irradiance on reconnection vertex

    // [0] -- (the rcVertex at x2, secondary hit point)
    // [1] -- (the rcVertex at a bounce chosen by the hybrid shift heuristics)

    [mutating]
    void init()
    {
        M = 0.f;
        weight = 0.f;
        pathFlags.flags = 0;
        F = 0.f;

        // set rcVertex as invalid
        rcVertexHit.instanceID = 0xffffffff;
    }

    static float toScalar(float3 color)
    {
        return dot(color, float3(0.299, 0.587, 0.114)); // luminance
    }

    static float computeWeight(float3 color, bool binarize = false)
    {
        float weight = toScalar(color);
        if (binarize && weight > 0.f) weight = 1.f;
        return weight;
    }

    [mutating]
    bool add(float3 in_F, float p, inout SampleGenerator sg)
    {
        M += 1.f;

        float w = toScalar(in_F) / p;

        if (isnan(w) || w == 0.f) return false;

        weight += w;

        // Accept?
        if (sampleNext1D(sg) * weight <= w)
        {
            F = in_F;
            //p_hat = _p_hat; // because we are using primary sample space
            return true;
        }

        return false;
    }

    [mutating]
    bool addPathReuseSample(float3 in_F, float in_Jacobian, PathReservoir inReservoir, float misWeight = 1.f)
    {
        float3 w = in_F * in_Jacobian * inReservoir.weight * misWeight;
        F += w;
        return any(F > 0.f);
    }

    [mutating]
    bool merge(float3 in_F, float in_Jacobian, PathReservoir inReservoir, inout SampleGenerator sg, float misWeight = 1.f, bool forceAdd = false)
    {
        float w = toScalar(in_F) * in_Jacobian * inReservoir.M * inReservoir.weight * misWeight;

        M += inReservoir.M;

        if (isnan(w) || w == 0.f) return false;

        weight += w;

        // Accept?
        if (forceAdd || sampleNext1D(sg) * weight <= w)
        {
            pathFlags = inReservoir.pathFlags;
            cachedJacobian = inReservoir.cachedJacobian;
            lightPdf = inReservoir.lightPdf;
            rcVertexWi[0] = inReservoir.rcVertexWi[0];
            rcVertexHit = inReservoir.rcVertexHit;
            rcVertexIrradiance[0] = inReservoir.rcVertexIrradiance[0];

            F = in_F;
            return true;
        }

        return false;
    }

    [mutating]
    bool mergeWithResamplingMIS(float3 in_F, float in_Jacobian, PathReservoir inReservoir, inout SampleGenerator sg, float misWeight = 1.f, bool forceAdd = false)
    {
        float w = toScalar(in_F) * in_Jacobian * inReservoir.weight * misWeight;

        M += inReservoir.M;

        if (isnan(w) || w == 0.f) return false;

        weight += w;

        // Accept?
        if (forceAdd || sampleNext1D(sg) * weight <= w)
        {
            pathFlags = inReservoir.pathFlags;
            cachedJacobian = inReservoir.cachedJacobian;
            lightPdf = inReservoir.lightPdf;
            rcVertexWi[0] = inReservoir.rcVertexWi[0];
            rcVertexHit = inReservoir.rcVertexHit;
            rcVertexIrradiance[0] = inReservoir.rcVertexIrradiance[0];

            F = in_F;
            return true;
        }

        return false;
    }



    [mutating]
    bool mergeInSamplePixel(PathReservoir inReservoir, inout SampleGenerator sg)
    {
        float w = inReservoir.weight;

        M += inReservoir.M;

        if (isnan(w) || w == 0.f) return false;

        weight += w;

        // Accept?
        if (sampleNext1D(sg) * weight <= w)
        {
            pathFlags = inReservoir.pathFlags;
            cachedJacobian = inReservoir.cachedJacobian;
            lightPdf = inReservoir.lightPdf;
            rcVertexWi[0] = inReservoir.rcVertexWi[0];
            rcVertexHit = inReservoir.rcVertexHit;
            rcVertexIrradiance[0] = inReservoir.rcVertexIrradiance[0];

            F = inReservoir.F;
            return true;
        }

        return false;
    }



    [mutating]
    void prepareMerging()
    {
        weight *= toScalar(F) * M;
    }

    [mutating]
    void finalizeRIS()
    {
        float p_hat = toScalar(F);
        if (p_hat == 0.f || M == 0.f) weight = 0.f;
        else weight = weight / (p_hat * M);
    }

    // assuming using proper resampling MIS weight, no need to divide by M
    [mutating]
    void finalizeGRIS()
    {
        float p_hat = toScalar(F);
        if (p_hat == 0.f) weight = 0.f;
        else weight = weight / p_hat;
    }

};
