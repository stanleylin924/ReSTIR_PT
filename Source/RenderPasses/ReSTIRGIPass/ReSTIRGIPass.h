/***************************************************************************
 # Copyright (c) 2015-21, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#pragma once
#include "Falcor.h"
#include "Utils/Debug/PixelDebug.h"
#include "Utils/Sampling/SampleGenerator.h"
#include "Rendering/Lights/EnvMapSampler.h"
#include "Rendering/Lights/EnvMapLighting.h"
#include "Rendering/Lights/EmissiveLightSampler.h"
#include "Rendering/Lights/EmissiveUniformSampler.h"
#include "Rendering/Lights/EmissivePowerSampler.h"
#include "Rendering/Lights/LightBVHSampler.h"
#include "Rendering/Volumes/GridVolumeSampler.h"
#include "Rendering/Utils/PixelStats.h"
#include "Rendering/Materials/TexLODTypes.slang"
#include "Params.slang"
#include <fstream>

using namespace Falcor;

class ReSTIRGIPass : public RenderPass
{
public:
    using SharedPtr = std::shared_ptr<ReSTIRGIPass>;

    /** Create a new render pass object.
        \param[in] pRenderContext The render context.
        \param[in] dict Dictionary of serialized parameters.
        \return A new object, or an exception is thrown if creation failed.
    */
    static SharedPtr create(RenderContext* pRenderContext = nullptr, const Dictionary& dict = {});

    virtual std::string getDesc() override;
    virtual Dictionary getScriptingDictionary() override;
    virtual RenderPassReflection reflect(const CompileData& compileData) override;
    virtual void compile(RenderContext* pRenderContext, const CompileData& compileData) override;
    virtual void execute(RenderContext* pRenderContext, const RenderData& renderData) override;
    virtual void renderUI(Gui::Widgets& widget) override;
    virtual void setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) override;
    virtual bool onMouseEvent(const MouseEvent& mouseEvent) override { return false; }
    virtual bool onKeyEvent(const KeyboardEvent& keyEvent) override { return false; }

    void updateDict(const Dictionary& dict) override;
    void initDict() override;

private:
    ReSTIRGIPass(const Dictionary& dict);
    bool beginFrame(RenderContext* pRenderContext, const RenderData& renderData);
    void endFrame(RenderContext* pRenderContext, const RenderData& renderData);

    /** Static configuration. Changing any of these options require shader recompilation.
    */
    struct StaticParams
    {
        // Rendering parameters
        uint32_t    samplesPerPixel = 1;                        ///< Number of samples (paths) per pixel, unless a sample density map is used.
        uint32_t    candidateSamples = 1;
        uint32_t    maxSurfaceBounces = 9;                      ///< Max number of surface bounces (diffuse + specular + transmission), up to kMaxPathLenth.
        uint32_t    maxDiffuseBounces = -1;                     ///< Max number of diffuse bounces (0 = direct only), up to kMaxBounces. This will be initialized at startup.
        uint32_t    maxSpecularBounces = -1;                    ///< Max number of specular bounces (0 = direct only), up to kMaxBounces. This will be initialized at startup.
        uint32_t    maxTransmissionBounces = -1;                ///< Max number of transmission bounces (0 = none), up to kMaxBounces. This will be initialized at startup.
        uint32_t    sampleGenerator = SAMPLE_GENERATOR_TINY_UNIFORM; ///< Pseudorandom sample generator type.
        bool        adjustShadingNormals = false;               ///< Adjust shading normals on secondary hits.
        bool        useBSDFSampling = true;                     ///< Use BRDF importance sampling, otherwise cosine-weighted hemisphere sampling.
        bool        useNEE = true;                              ///< Use next-event estimation (NEE). This enables shadow ray(s) from each path vertex.
        bool        useMIS = true;                              ///< Use multiple importance sampling (MIS) when NEE is enabled.
        bool        useRussianRoulette = false;                 ///< Use russian roulette to terminate low throughput paths.
        bool        useAlphaTest = true;                        ///< Use alpha testing on non-opaque triangles.
        uint32_t    maxNestedMaterials = 2;                     ///< Maximum supported number of nested materials.
        bool        useLightsInDielectricVolumes = false;       ///< Use lights inside of volumes (transmissive materials). We typically don't want this because lights are occluded by the interface.
        bool        limitTransmission = false;                  ///< Limit specular transmission by handling reflection/refraction events only up to a given transmission depth.
        uint32_t    maxTransmissionReflectionDepth = 0;         ///< Maximum transmission depth at which to sample specular reflection.
        uint32_t    maxTransmissionRefractionDepth = 0;         ///< Maximum transmission depth at which to sample specular refraction (after that, IoR is set to 1).
        bool        disableCaustics = false;                    ///< Disable sampling of caustics.
        bool        disableDirectIllumination = true;          ///< Disable all direct illumination.
        TexLODMode  primaryLodMode = TexLODMode::Mip0;          ///< Use filtered texture lookups at the primary hit.
        ColorFormat colorFormat = ColorFormat::LogLuvHDR;       ///< Color format used for internal per-sample color and denoiser buffers.
        MISHeuristic misHeuristic = MISHeuristic::Balance;      ///< MIS heuristic.
        float       misPowerExponent = 2.f;                     ///< MIS exponent for the power heuristic. This is only used when 'PowerExp' is chosen.
        EmissiveLightSamplerType emissiveSampler = EmissiveLightSamplerType::Power;  ///< Emissive light sampler to use for NEE.

        bool        useDeterministicBSDF = true;                    ///< Evaluate all compatible lobes at BSDF sampling time.

        ReSTIRMISKind    spatialMisKind = ReSTIRMISKind::Pairwise;
        ReSTIRMISKind    temporalMisKind = ReSTIRMISKind::Talbot;

        ShiftMapping    shiftStrategy = ShiftMapping::Hybrid;
        bool            temporalUpdateForDynamicScene = false;

        PathSamplingMode pathSamplingMode = PathSamplingMode::ReSTIR;

        bool            separatePathBSDF = true;

        bool            rcDataOfflineMode = false;

        // Denoising parameters
        bool        useNRDDemodulation = true;                  ///< Global switch for NRD demodulation.

        Program::DefineList getDefines(const ReSTIRGIPass& owner) const;
    };


    void Init()
    {
        mStaticParams = StaticParams();
        mParams = RestirPathTracerParams();
    }

    // Configuration
    RestirPathTracerParams          mParams;                    ///< Runtime path tracer parameters.
    StaticParams                    mStaticParams;              ///< Static parameters. These are set as compile-time constants in the shaders.

    // Internal state
    Scene::SharedPtr                mpScene;                    ///< The current scene, or nullptr if no scene loaded.

    ComputePass::SharedPtr          mpGeneratePaths;                ///< Fullscreen compute pass generating paths starting at primary hits.
};
