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
#include "ReSTIRGIPass.h"

namespace
{
    const char kDesc[] = "Path tracer using DXR 1.1 TraceRayInline";

    const std::string kGeneratePathsFilename = "RenderPasses/ReSTIRGIPass/GeneratePaths.cs.slang";
    const std::string kTracePassFilename = "RenderPasses/ReSTIRGIPass/TracePass.cs.slang";
    const std::string kReflectTypesFile = "RenderPasses/ReSTIRGIPass/ReflectTypes.cs.slang";
    const std::string kSpatialReusePassFile = "RenderPasses/ReSTIRGIPass/SpatialReuse.cs.slang";
    const std::string kTemporalReusePassFile = "RenderPasses/ReSTIRGIPass/TemporalReuse.cs.slang";
    const std::string kSpatialPathRetraceFile = "RenderPasses/ReSTIRGIPass/SpatialPathRetrace.cs.slang";
    const std::string kTemporalPathRetraceFile = "RenderPasses/ReSTIRGIPass/TemporalPathRetrace.cs.slang";
    const std::string kComputePathReuseMISWeightsFile = "RenderPasses/ReSTIRGIPass/ComputePathReuseMISWeights.cs.slang";

    // Render pass inputs and outputs.
    const std::string kInputVBuffer = "vbuffer";
    const std::string kInputMotionVectors = "motionVectors";
    const std::string kInputDirectLighting = "directLighting";

    const Falcor::ChannelList kInputChannels =
    {
        { kInputVBuffer,        "gVBuffer",         "Visibility buffer in packed format", false, ResourceFormat::Unknown },
        { kInputMotionVectors,  "gMotionVectors",   "Motion vector buffer (float format)", true /* optional */, ResourceFormat::RG32Float },
        { kInputDirectLighting,    "gDirectLighting",     "Sample count buffer (integer format)", true /* optional */, ResourceFormat::RGBA32Float },
    };

    const std::string kOutputColor = "color";
    const std::string kOutputAlbedo = "albedo";
    const std::string kOutputSpecularAlbedo = "specularAlbedo";
    const std::string kOutputIndirectAlbedo = "indirectAlbedo";
    const std::string kOutputNormal = "normal";
    const std::string kOutputReflectionPosW = "reflectionPosW";
    const std::string kOutputRayCount = "rayCount";
    const std::string kOutputPathLength = "pathLength";
    const std::string kOutputDebug = "debug";
    const std::string kOutputTime = "time";
    const std::string kOutputNRDDiffuseRadianceHitDist = "nrdDiffuseRadianceHitDist";
    const std::string kOutputNRDSpecularRadianceHitDist = "nrdSpecularRadianceHitDist";
    const std::string kOutputNRDResidualRadianceHitDist = "nrdResidualRadianceHitDist";
    const std::string kOutputNRDEmission = "nrdEmission";
    const std::string kOutputNRDDiffuseReflectance = "nrdDiffuseReflectance";
    const std::string kOutputNRDSpecularReflectance = "nrdSpecularReflectance";


    const Falcor::ChannelList kOutputChannels =
    {
        { kOutputColor,                 "gOutputColor",                 "Output color (linear)", true /* optional */ },
        { kOutputAlbedo,                "gOutputAlbedo",                "Output albedo (linear)", true /* optional */, ResourceFormat::RGBA8Unorm },
        { kOutputNormal,                "gOutputNormal",                "Output normal (linear)", true /* optional */, ResourceFormat::RGBA16Float },
        { kOutputRayCount,              "",                             "Per-pixel ray count", true /* optional */, ResourceFormat::R32Uint },
        { kOutputPathLength,            "",                             "Per-pixel path length", true /* optional */, ResourceFormat::R32Uint },
        { kOutputDebug,                 "",                             "Debug output", true /* optional */, ResourceFormat::RGBA32Float },
        { kOutputTime,                  "",                             "Per-pixel time", true /* optional */, ResourceFormat::R32Uint },
        { kOutputSpecularAlbedo,                "gOutputSpecularAlbedo",                "Output specular albedo (linear)", true /* optional */, ResourceFormat::RGBA8Unorm },
        { kOutputIndirectAlbedo,                "gOutputIndirectAlbedo",                "Output indirect albedo (linear)", true /* optional */, ResourceFormat::RGBA8Unorm },
        { kOutputReflectionPosW,                "gOutputReflectionPosW",                "Output reflection pos (world space)", true /* optional */, ResourceFormat::RGBA32Float },
        { kOutputNRDDiffuseRadianceHitDist,     "gOutputNRDDiffuseRadianceHitDist",     "Output demodulated diffuse color (linear) and hit distance", true /* optional */, ResourceFormat::RGBA32Float },
        { kOutputNRDSpecularRadianceHitDist,    "gOutputNRDSpecularRadianceHitDist",    "Output demodulated specular color (linear) and hit distance", true /* optional */, ResourceFormat::RGBA32Float },
        { kOutputNRDResidualRadianceHitDist,    "gOutputNRDResidualRadianceHitDist",    "Output residual color (linear) and hit distance", true /* optional */, ResourceFormat::RGBA32Float },
        { kOutputNRDEmission,                   "gOutputNRDEmission",                   "Output primary surface emission", true /* optional */, ResourceFormat::RGBA32Float },
        { kOutputNRDDiffuseReflectance,         "gOutputNRDDiffuseReflectance",         "Output primary surface diffuse reflectance", true /* optional */, ResourceFormat::RGBA16Float },
        { kOutputNRDSpecularReflectance,        "gOutputNRDSpecularReflectance",        "Output primary surface specular reflectance", true /* optional */, ResourceFormat::RGBA16Float },
    };

    // UI variables.
    const Gui::DropdownList kColorFormatList =
    {
        { (uint32_t)ColorFormat::RGBA32F, "RGBA32F (128bpp)" },
        { (uint32_t)ColorFormat::LogLuvHDR, "LogLuvHDR (32bpp)" },
    };

    const Gui::DropdownList kMISHeuristicList =
    {
        { (uint32_t)MISHeuristic::Balance, "Balance heuristic" },
        { (uint32_t)MISHeuristic::PowerTwo, "Power heuristic (exp=2)" },
        { (uint32_t)MISHeuristic::PowerExp, "Power heuristic" },
    };

    const Gui::DropdownList kShiftMappingList =
    {
        { (uint32_t)ShiftMapping::Reconnection, "Reconnection" },
        { (uint32_t)ShiftMapping::RandomReplay, "Random Replay" },
        { (uint32_t)ShiftMapping::Hybrid, "Hybrid" },
    };

    const Gui::DropdownList kReSTIRMISList =
    {
        { (uint32_t)ReSTIRMISKind::Constant , "Constant resampling MIS (with balance-heuristic contribution MIS)" },
        { (uint32_t)ReSTIRMISKind::Talbot, "Talbot resampling MIS" },
        { (uint32_t)ReSTIRMISKind::Pairwise, "Pairwise resampling MIS" },
        { (uint32_t)ReSTIRMISKind::ConstantBinary, "Constant resampling MIS (with 1/|Z| contribution MIS)" },
        { (uint32_t)ReSTIRMISKind::ConstantBiased, "Constant resampling MIS (constant contribution MIS, biased)" },
    };

    const Gui::DropdownList kReSTIRMISList2 =
    {
        { (uint32_t)ReSTIRMISKind::Constant , "Constant resampling MIS (with balance-heuristic contribution MIS)" },
        { (uint32_t)ReSTIRMISKind::Talbot, "Talbot resampling MIS" },
        { (uint32_t)ReSTIRMISKind::ConstantBinary, "Constant resampling MIS (with 1/|Z| contribution MIS)" },
        { (uint32_t)ReSTIRMISKind::ConstantBiased, "Constant resampling MIS (constant contribution MIS, biased)" },
    };

    const Gui::DropdownList kPathReusePatternList =
    {
        { (uint32_t)PathReusePattern::Block, std::string("Block")},
        { (uint32_t)PathReusePattern::NRooks, std::string("N-Rooks")},
        { (uint32_t)PathReusePattern::NRooksShift, std::string("N-Rooks Shift")},
    };

    const Gui::DropdownList kSpatialReusePatternList =
    {
        { (uint32_t)SpatialReusePattern::Default, std::string("Default")},
        { (uint32_t)SpatialReusePattern::SmallWindow, std::string("Small Window")},
    };

    const Gui::DropdownList kEmissiveSamplerList =
    {
        { (uint32_t)EmissiveLightSamplerType::Uniform, "Uniform" },
        { (uint32_t)EmissiveLightSamplerType::LightBVH, "LightBVH" },
        { (uint32_t)EmissiveLightSamplerType::Power, "Power" },
    };

    const Gui::DropdownList kLODModeList =
    {
        { (uint32_t)TexLODMode::Mip0, "Mip0" },
        { (uint32_t)TexLODMode::RayDiffs, "Ray Diffs" }
    };

    const Gui::DropdownList kPathSamplingModeList =
    {
        { (uint32_t)PathSamplingMode::ReSTIR, "ReSTIR PT" },
        { (uint32_t)PathSamplingMode::PathReuse, "Bekaert-style Path Reuse" },
        { (uint32_t)PathSamplingMode::PathTracing, "Path Tracing" }
    };

    // Scripting options.
    const std::string kSamplesPerPixel = "samplesPerPixel";
    const std::string kMaxSurfaceBounces = "maxSurfaceBounces";
    const std::string kMaxDiffuseBounces = "maxDiffuseBounces";
    const std::string kMaxSpecularBounces = "maxSpecularBounces";
    const std::string kMaxTransmissionBounces = "maxTransmissionBounces";
    const std::string kAdjustShadingNormals = "adjustShadingNormals";
    const std::string kLODBias = "lodBias";
    const std::string kSampleGenerator = "sampleGenerator";
    const std::string kUseBSDFSampling = "useBSDFSampling";
    const std::string kUseNEE = "useNEE";
    const std::string kUseMIS = "useMIS";
    const std::string kUseRussianRoulette = "useRussianRoulette";
    const std::string kScreenSpaceReSTIROptions = "screenSpaceReSTIROptions";
    const std::string kUseAlphaTest = "useAlphaTest";
    const std::string kMaxNestedMaterials = "maxNestedMaterials";
    const std::string kUseLightsInDielectricVolumes = "useLightsInDielectricVolumes";
    const std::string kLimitTransmission = "limitTransmission";
    const std::string kMaxTransmissionReflectionDepth = "maxTransmissionReflectionDepth";
    const std::string kMaxTransmissionRefractionDepth = "maxTransmissionRefractionDepth";
    const std::string kDisableCaustics = "disableCaustics";
    const std::string kSpecularRoughnessThreshold = "specularRoughnessThreshold";
    const std::string kDisableDirectIllumination = "disableDirectIllumination";
    const std::string kColorFormat = "colorFormat";
    const std::string kMISHeuristic = "misHeuristic";
    const std::string kMISPowerExponent = "misPowerExponent";
    const std::string kFixedSeed = "fixedSeed";
    const std::string kEmissiveSampler = "emissiveSampler";
    const std::string kLightBVHOptions = "lightBVHOptions";
    const std::string kPrimaryLodMode = "primaryLodMode";
    const std::string kUseNRDDemodulation = "useNRDDemodulation";

    const std::string kSpatialMisKind = "spatialMisKind";
    const std::string kTemporalMisKind = "temporalMisKind";
    const std::string kShiftStrategy = "shiftStrategy";
    const std::string kRejectShiftBasedOnJacobian = "rejectShiftBasedOnJacobian";
    const std::string kJacobianRejectionThreshold = "jacobianRejectionThreshold";
    const std::string kNearFieldDistance = "nearFieldDistance";
    const std::string kLocalStrategyType = "localStrategyType";

    const std::string kTemporalHistoryLength = "temporalHistoryLength";
    const std::string kUseMaxHistory = "useMaxHistory";
    const std::string kSeedOffset = "seedOffset";
    const std::string kEnableTemporalReuse = "enableTemporalReuse";
    const std::string kEnableSpatialReuse = "enableSpatialReuse";
    const std::string kNumSpatialRounds = "numSpatialRounds";
    const std::string kPathSamplingMode = "pathSamplingMode";
    const std::string kEnableTemporalReprojection = "enableTemporalReprojection";
    const std::string kNoResamplingForTemporalReuse = "noResamplingForTemporalReuse";
    const std::string kSpatialNeighborCount = "spatialNeighborCount";
    const std::string kFeatureBasedRejection = "featureBasedRejection";
    const std::string kSpatialReusePattern = "spatialReusePattern";
    const std::string kSmallWindowRestirWindowRadius = "smallWindowRestirWindowRadius";
    const std::string kSpatialReuseRadius = "spatialReuseRadius";
    const std::string kUseDirectLighting = "useDirectLighting";
    const std::string kSeparatePathBSDF = "separatePathBSDF";
    const std::string kCandidateSamples = "candidateSamples";
    const std::string kTemporalUpdateForDynamicScene = "temporalUpdateForDynamicScene";
    const std::string kEnableRayStats = "enableRayStats";

    const uint32_t kNeighborOffsetCount = 8192;
}

// Don't remove this. it's required for hot-reload to function properly
extern "C" __declspec(dllexport) const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" __declspec(dllexport) void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerClass("ReSTIRGIPass", kDesc, ReSTIRGIPass::create);
}


void ReSTIRGIPass::updateDict(const Dictionary& dict)
{
}

void ReSTIRGIPass::initDict()
{
    Init();
    mParams.frameCount = 0;
}

ReSTIRGIPass::SharedPtr ReSTIRGIPass::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    logInfo(__FUNCTION__);

    return SharedPtr(new ReSTIRGIPass(dict));
}

std::string ReSTIRGIPass::getDesc() { return kDesc; }

Dictionary ReSTIRGIPass::getScriptingDictionary()
{
    return Dictionary();
}

ReSTIRGIPass::ReSTIRGIPass(const Dictionary& dict)
{
    // Create programs.
    auto defines = mStaticParams.getDefines(*this);

    mpGeneratePaths = ComputePass::create(kGeneratePathsFilename, "main", defines, false);
    mpReflectTypes = ComputePass::create(kReflectTypesFile, "main", defines, false);
}

RenderPassReflection ReSTIRGIPass::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;
    addRenderPassInputs(reflector, kInputChannels);
    addRenderPassOutputs(reflector, kOutputChannels);
    return reflector;
}

void ReSTIRGIPass::compile(RenderContext* pContext, const CompileData& compileData)
{
    logInfo(__FUNCTION__);

    mParams.frameDim = compileData.defaultTexDims;
    if (mParams.frameDim.x > kMaxFrameDimension || mParams.frameDim.y > kMaxFrameDimension)
    {
        logError("Frame dimensions up to " + std::to_string(kMaxFrameDimension) + " pixels width/height are supported.");
    }

    // Tile dimensions have to be powers-of-two.
    assert(isPowerOf2(kScreenTileDim.x) && isPowerOf2(kScreenTileDim.y));
    assert(kScreenTileDim.x == (1 << kScreenTileBits.x) && kScreenTileDim.y == (1 << kScreenTileBits.y));
    mParams.screenTiles = div_round_up(mParams.frameDim, kScreenTileDim);
}

void ReSTIRGIPass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{
    logDebug(__FUNCTION__);

    mpScene = pScene;
    mParams.frameCount = 0;

    // resetLighting();

    if (mpScene)
    {
        if (is_set(pScene->getPrimitiveTypes(), PrimitiveTypeFlags::Custom)) logError("This render pass does not support custom primitives.");

        // check if the scene is dynamic
        bool enableRobustSettingsByDefault = mpScene->hasAnimation() && mpScene->isAnimated();
        mParams.rejectShiftBasedOnJacobian = enableRobustSettingsByDefault;
        mStaticParams.temporalUpdateForDynamicScene = enableRobustSettingsByDefault;

        // Prepare our programs for the scene.
        Shader::DefineList defines = mpScene->getSceneDefines();

        mpGeneratePaths->getProgram()->addDefines(defines);
        // mpTracePass->getProgram()->addDefines(defines);
        mpReflectTypes->getProgram()->addDefines(defines);

        // mpSpatialPathRetracePass->getProgram()->addDefines(defines);
        // mpTemporalPathRetracePass->getProgram()->addDefines(defines);

        // mpSpatialReusePass->getProgram()->addDefines(defines);
        // mpTemporalReusePass->getProgram()->addDefines(defines);
        // mpComputePathReuseMISWeightsPass->getProgram()->addDefines(defines);

        // validateOptions();

        mRecompile = true;
    }

    mOptionsChanged = true;
}

void ReSTIRGIPass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    logInfo(__FUNCTION__);

    if (!beginFrame(pRenderContext, renderData)) return;
    renderData.getDictionary()["enableScreenSpaceReSTIR"] = mUseDirectLighting;

    bool skipTemporalReuse = mReservoirFrameCount == 0;
    if (mStaticParams.pathSamplingMode != PathSamplingMode::ReSTIR) mStaticParams.candidateSamples = 1;
    if (mStaticParams.pathSamplingMode == PathSamplingMode::PathReuse)
    {
        mStaticParams.shiftStrategy = ShiftMapping::Reconnection;
        mEnableSpatialReuse = true;
    }
    if (mStaticParams.shiftStrategy == ShiftMapping::Hybrid)
    {
        // the ray tracing pass happens before spatial/temporal reuse,
        // so currently hybrid shift is only implemented for Pairwise and Talbot
        mStaticParams.spatialMisKind = ReSTIRMISKind::Pairwise;
        mStaticParams.temporalMisKind = ReSTIRMISKind::Talbot;
    }

    uint32_t numPasses = mStaticParams.pathSamplingMode == PathSamplingMode::PathTracing ? 1 : mStaticParams.samplesPerPixel;

    for (uint32_t restir_i = 0; restir_i < numPasses; restir_i++)
    {
        {
            // Update shader program specialization.
            updatePrograms();

            // Prepare resources.
            prepareResources(pRenderContext, renderData);

            // Prepare the path tracer parameter block.
            // This should be called after all resources have been created.
            // preparePathTracer(renderData);

            // Reset atomic counters.

            // Clear time output texture.

            // if (const auto& texture = renderData[kOutputTime])
            // {
                // pRenderContext->clearUAV(texture->getUAV().get(), uint4(0));
            // }

            {
                // assert(mpCounters);
                // pRenderContext->clearUAV(mpCounters->getUAV().get(), uint4(0));

                // mpPathTracerBlock->getRootVar()["gSppId"] = restir_i;
                // mpPathTracerBlock->getRootVar()["gNumSpatialRounds"] = mNumSpatialRounds;

                if (restir_i == 0)
                    // Generate paths at primary hits.
                    generatePaths(pRenderContext, renderData, 0);

                // Launch main trace pass.
                // tracePass(pRenderContext, renderData, mpTracePass, "tracePass", 0);
            }
        }

        if (mStaticParams.pathSamplingMode != PathSamplingMode::PathTracing)
        {
            // Launch restir merge pass.
            /*
            if (mStaticParams.pathSamplingMode == PathSamplingMode::ReSTIR)
            {
                if (mEnableTemporalReuse && !skipTemporalReuse)
                {
                    if (mStaticParams.shiftStrategy == ShiftMapping::Hybrid)
                        PathRetracePass(pRenderContext, restir_i, renderData, true, 0);
                    // a separate pass to trace rays for hybrid shift/random number replay
                    PathReusePass(pRenderContext, restir_i, renderData, true, 0, !mEnableSpatialReuse);
                }
            }
            else if (mStaticParams.pathSamplingMode == PathSamplingMode::PathReuse)
            {
                PathReusePass(pRenderContext, restir_i, renderData, false, -1, false);
            }
            */

            /*
            if (mEnableSpatialReuse)
            {
                // multiple rounds?
                for (int spatialRoundId = 0; spatialRoundId < mNumSpatialRounds; spatialRoundId++)
                {
                    // a separate pass to trace rays for hybrid shift/random number replay
                    if (mStaticParams.shiftStrategy == ShiftMapping::Hybrid)
                        PathRetracePass(pRenderContext, restir_i, renderData, false, spatialRoundId);
                    PathReusePass(pRenderContext, restir_i, renderData, false, spatialRoundId, spatialRoundId == mNumSpatialRounds - 1);
                }
            }
            */

            if (restir_i == numPasses - 1)
                mReservoirFrameCount++; // mark as at least one temporally reused frame
            /*
            if (mEnableTemporalReuse && mStaticParams.pathSamplingMode == PathSamplingMode::ReSTIR)
            {
                if ((!mEnableSpatialReuse || mNumSpatialRounds % 2 == 0))
                    pRenderContext->copyResource(mpTemporalReservoirs[restir_i].get(), mpOutputReservoirs.get());
                if (restir_i == numPasses - 1)
                    pRenderContext->copyResource(mpTemporalVBuffer.get(), renderData[kInputVBuffer].get());
            }
            */
        }
        mParams.seed++;
    }

    mParams.frameCount++;

    endFrame(pRenderContext, renderData);
}
/*
    std::cout << "ReSTIRGIPass::execute()++\n";

    if (!beginFrame(pRenderContext, renderData)) return;

    // auto defines = mStaticParams.getDefines(*this);
    // mpGeneratePaths->getProgram()->addDefines(defines);

    // Bind resources.
    auto var = mpGeneratePaths->getRootVar()["CB"]["gPathGenerator"];
    var["params"].setBlob(mParams);
    var["vbuffer"] = renderData[kInputVBuffer]->asTexture();
    var["outputColor"] = renderData[kOutputColor]->asTexture();

    auto frameDim = renderData.getDefaultTextureDims();
    // mpGeneratePaths->execute(pRenderContext, uint3(frameDim, 1u));
    const uint32_t tileSize = kScreenTileDim.x * kScreenTileDim.y;
    // mpGeneratePaths->execute(pRenderContext, { mParams.screenTiles.x * tileSize, mParams.screenTiles.y, 1u });
    generatePaths(pRenderContext, renderData, 0);

    endFrame(pRenderContext, renderData);
}
*/
void ReSTIRGIPass::renderUI(Gui::Widgets& widget)
{
    logInfo(__FUNCTION__);
}

bool ReSTIRGIPass::onMouseEvent(const MouseEvent& mouseEvent)
{
    // return mpPixelDebug->onMouseEvent(mouseEvent);
    return true;
}

void ReSTIRGIPass::updatePrograms()
{
    logInfo(__FUNCTION__);

    if (mRecompile == false) return;

    mStaticParams.rcDataOfflineMode = mSpatialNeighborCount > 3 && mStaticParams.shiftStrategy == ShiftMapping::Hybrid; // ?

    auto defines = mStaticParams.getDefines(*this);

    // Update program specialization. This is done through defines in lieu of specialization constants.
    mpGeneratePaths->getProgram()->addDefines(defines);
    // mpTracePass->getProgram()->addDefines(defines);
    mpReflectTypes->getProgram()->addDefines(defines);
    // mpSpatialPathRetracePass->getProgram()->addDefines(defines);
    // mpTemporalPathRetracePass->getProgram()->addDefines(defines);
    // mpSpatialReusePass->getProgram()->addDefines(defines);
    // mpTemporalReusePass->getProgram()->addDefines(defines);
    // mpComputePathReuseMISWeightsPass->getProgram()->addDefines(defines);

    // Recreate program vars. This may trigger recompilation if needed.
    // Note that program versions are cached, so switching to a previously used specialization is faster.
    mpGeneratePaths->setVars(nullptr);
    // mpTracePass->setVars(nullptr);
    mpReflectTypes->setVars(nullptr);
    // mpSpatialPathRetracePass->setVars(nullptr);
    // mpTemporalPathRetracePass->setVars(nullptr);
    // mpSpatialReusePass->setVars(nullptr);
    // mpTemporalReusePass->setVars(nullptr);
    // mpComputePathReuseMISWeightsPass->setVars(nullptr);

    mVarsChanged = true;
    mRecompile = false;
}

void ReSTIRGIPass::prepareResources(RenderContext* pRenderContext, const RenderData& renderData)
{
    logInfo(__FUNCTION__);
}

void ReSTIRGIPass::setNRDData(const ShaderVar& var, const RenderData& renderData) const
{
    var["primaryHitEmission"] = renderData[kOutputNRDEmission]->asTexture();
    var["primaryHitDiffuseReflectance"] = renderData[kOutputNRDDiffuseReflectance]->asTexture();
    var["primaryHitSpecularReflectance"] = renderData[kOutputNRDSpecularReflectance]->asTexture();
}

bool ReSTIRGIPass::prepareLighting(RenderContext* pRenderContext)
{
    logInfo(__FUNCTION__);

    bool lightingChanged = false;

    if (is_set(mpScene->getUpdates(), Scene::UpdateFlags::RenderSettingsChanged))
    {
        lightingChanged = true;
        mRecompile = true;
    }

    if (is_set(mpScene->getUpdates(), Scene::UpdateFlags::EnvMapChanged))
    {
        mpEnvMapSampler = nullptr;
        lightingChanged = true;
        mRecompile = true;
    }

    if (mpScene->useEnvLight())
    {
        if (!mpEnvMapSampler)
        {
            mpEnvMapSampler = EnvMapSampler::create(pRenderContext, mpScene->getEnvMap());
            lightingChanged = true;
            mRecompile = true;
        }
    }
    else
    {
        if (mpEnvMapSampler)
        {
            mpEnvMapSampler = nullptr;
            lightingChanged = true;
            mRecompile = true;
        }
    }

    // Request the light collection if emissive lights are enabled.
    if (mpScene->getRenderSettings().useEmissiveLights)
    {
        mpScene->getLightCollection(pRenderContext);
    }

    if (mpScene->useEmissiveLights())
    {
        if (!mpEmissiveSampler)
        {
            const auto& pLights = mpScene->getLightCollection(pRenderContext);
            assert(pLights && pLights->getActiveLightCount() > 0);
            assert(!mpEmissiveSampler);

            switch (mStaticParams.emissiveSampler)
            {
            case EmissiveLightSamplerType::Uniform:
                mpEmissiveSampler = EmissiveUniformSampler::create(pRenderContext, mpScene);
                break;
            case EmissiveLightSamplerType::LightBVH:
                mpEmissiveSampler = LightBVHSampler::create(pRenderContext, mpScene, mLightBVHOptions);
                break;
            case EmissiveLightSamplerType::Power:
                mpEmissiveSampler = EmissivePowerSampler::create(pRenderContext, mpScene);
                break;
            default:
                logError("Unknown emissive light sampler type");
            }
            lightingChanged = true;
            mRecompile = true;
        }
    }
    else
    {
        if (mpEmissiveSampler)
        {
            // Retain the options for the emissive sampler.
            if (auto lightBVHSampler = std::dynamic_pointer_cast<LightBVHSampler>(mpEmissiveSampler))
            {
                mLightBVHOptions = lightBVHSampler->getOptions();
            }

            mpEmissiveSampler = nullptr;
            lightingChanged = true;
            mRecompile = true;
        }
    }

    if (mpEmissiveSampler)
    {
        lightingChanged |= mpEmissiveSampler->update(pRenderContext);
        auto defines = mpEmissiveSampler->getDefines();
        // if (mpTracePass->getProgram()->addDefines(defines)) mRecompile = true;
        // if (mpSpatialPathRetracePass->getProgram()->addDefines(defines)) mRecompile = true;
        // if (mpTemporalPathRetracePass->getProgram()->addDefines(defines)) mRecompile = true;
        // if (mpSpatialReusePass->getProgram()->addDefines(defines)) mRecompile = true;
        // if (mpTemporalReusePass->getProgram()->addDefines(defines)) mRecompile = true;
        // if (mpComputePathReuseMISWeightsPass->getProgram()->addDefines(defines)) mRecompile = true;
    }

    return lightingChanged;
}

void ReSTIRGIPass::setShaderData(const ShaderVar& var, const RenderData& renderData, bool isPathTracer, bool isPathGenerator) const
{
    logInfo(__FUNCTION__);

    // Bind static resources that don't change per frame.
    if (mVarsChanged)
    {
        if (isPathTracer && mpEnvMapSampler) mpEnvMapSampler->setShaderData(var["envMapSampler"]);
    }

    // Bind runtime data.
    var["params"].setBlob(mParams);
    var["vbuffer"] = renderData[kInputVBuffer]->asTexture();
    var["outputColor"] = renderData[kOutputColor]->asTexture();


    if (mOutputNRDData && isPathTracer)
    {
        setNRDData(var["outputNRD"], renderData);
        var["outputNRDDiffuseRadianceHitDist"] = renderData[kOutputNRDDiffuseRadianceHitDist]->asTexture();    ///< Output resolved diffuse color in .rgb and hit distance in .a for NRD. Only valid if kOutputNRDData == true.
        var["outputNRDSpecularRadianceHitDist"] = renderData[kOutputNRDSpecularRadianceHitDist]->asTexture();  ///< Output resolved specular color in .rgb and hit distance in .a for NRD. Only valid if kOutputNRDData == true.
        var["outputNRDResidualRadianceHitDist"] = renderData[kOutputNRDResidualRadianceHitDist]->asTexture();///< Output resolved residual color in .rgb and hit distance in .a for NRD. Only valid if kOutputNRDData == true.
    }

    if (isPathTracer)
    {
        var["isLastRound"] = !mEnableSpatialReuse && !mEnableTemporalReuse;
        var["useDirectLighting"] = mUseDirectLighting;
        var["kUseEnvLight"] = mpScene->useEnvLight();
        var["kUseEmissiveLights"] = mpScene->useEmissiveLights();
        var["kUseAnalyticLights"] = mpScene->useAnalyticLights();
    }
    else if (isPathGenerator)
    {
        var["kUseEnvBackground"] = mpScene->useEnvBackground();
    }

    if (auto outputDebug = var.findMember("outputDebug"); outputDebug.isValid())
    {
        outputDebug = renderData[kOutputDebug]->asTexture(); // Can be nullptr
    }
    if (auto outputTime = var.findMember("outputTime"); outputTime.isValid())
    {
        outputTime = renderData[kOutputTime]->asTexture(); // Can be nullptr
    }

    if (isPathTracer && mpEmissiveSampler)
    {
        // TODO: Do we have to bind this every frame?
        bool success = mpEmissiveSampler->setShaderData(var["emissiveSampler"]);
        if (!success) throw std::exception("Failed to bind emissive light sampler");
    }
}

bool ReSTIRGIPass::beginFrame(RenderContext* pRenderContext, const RenderData& renderData)
{
    logInfo(__FUNCTION__);

    const auto& pOutputColor = renderData[kOutputColor]->asTexture();
    assert(pOutputColor);
    pRenderContext->clearUAV(pOutputColor->getUAV().get(), float4(0.f, 0.3f, 0.f, 0.f));

    // Update the env map and emissive sampler to the current frame.
    bool lightingChanged = prepareLighting(pRenderContext);

    return true;
}

void ReSTIRGIPass::endFrame(RenderContext* pRenderContext, const RenderData& renderData)
{
    logInfo(__FUNCTION__);
}

void ReSTIRGIPass::generatePaths(RenderContext* pRenderContext, const RenderData& renderData, int sampleId)
{
    logInfo(__FUNCTION__);

    PROFILE("generatePaths");

    // Check shader assumptions.
    // We launch one thread group per screen tile, with threads linearly indexed.
    const uint32_t tileSize = kScreenTileDim.x * kScreenTileDim.y;
    assert(kScreenTileDim.x == 16 && kScreenTileDim.y == 16); // TODO: Remove this temporary limitation when Slang bug has been fixed, see comments in shader.
    assert(kScreenTileBits.x <= 4 && kScreenTileBits.y <= 4); // Since we use 8-bit deinterleave.
    assert(mpGeneratePaths->getThreadGroupSize().x == tileSize);
    assert(mpGeneratePaths->getThreadGroupSize().y == 1 && mpGeneratePaths->getThreadGroupSize().z == 1);

    // Additional specialization. This shouldn't change resource declarations.
    mpGeneratePaths->addDefine("OUTPUT_TIME", mOutputTime ? "1" : "0");
    mpGeneratePaths->addDefine("OUTPUT_NRD_DATA", mOutputNRDData ? "1" : "0");

    // Bind resources.
    auto var = mpGeneratePaths->getRootVar()["CB"]["gPathGenerator"];
    setShaderData(var, renderData, false, true);

    mpGeneratePaths["gScene"] = mpScene->getParameterBlock();
    var["gSampleId"] = sampleId;

    // Launch one thread per pixel.
    // The dimensions are padded to whole tiles to allow re-indexing the threads in the shader.
    mpGeneratePaths->execute(pRenderContext, { mParams.screenTiles.x * tileSize, mParams.screenTiles.y, 1u });
}

Program::DefineList ReSTIRGIPass::StaticParams::getDefines(const ReSTIRGIPass& owner) const
{
    Program::DefineList defines;

    // Path tracer configuration.
    defines.add("SAMPLES_PER_PIXEL", std::to_string(samplesPerPixel)); // 0 indicates a variable sample count
    defines.add("CANDIDATE_SAMPLES", std::to_string(candidateSamples)); // 0 indicates a variable sample count    
    defines.add("MAX_SURFACE_BOUNCES", std::to_string(maxSurfaceBounces));
    defines.add("MAX_DIFFUSE_BOUNCES", std::to_string(maxDiffuseBounces));
    defines.add("MAX_SPECULAR_BOUNCES", std::to_string(maxSpecularBounces));
    defines.add("MAX_TRANSMISSON_BOUNCES", std::to_string(maxTransmissionBounces));
    defines.add("ADJUST_SHADING_NORMALS", adjustShadingNormals ? "1" : "0");
    defines.add("USE_BSDF_SAMPLING", useBSDFSampling ? "1" : "0");
    defines.add("USE_NEE", useNEE ? "1" : "0");
    defines.add("USE_MIS", useMIS ? "1" : "0");
    defines.add("USE_RUSSIAN_ROULETTE", useRussianRoulette ? "1" : "0");
    defines.add("USE_ALPHA_TEST", useAlphaTest ? "1" : "0");
    defines.add("USE_LIGHTS_IN_DIELECTRIC_VOLUMES", useLightsInDielectricVolumes ? "1" : "0");
    defines.add("LIMIT_TRANSMISSION", limitTransmission ? "1" : "0");
    defines.add("MAX_TRANSMISSION_REFLECTION_DEPTH", std::to_string(maxTransmissionReflectionDepth));
    defines.add("MAX_TRANSMISSION_REFRACTION_DEPTH", std::to_string(maxTransmissionRefractionDepth));
    defines.add("DISABLE_CAUSTICS", disableCaustics ? "1" : "0");
    defines.add("DISABLE_DIRECT_ILLUMINATION", disableDirectIllumination ? "1" : "0");
    defines.add("PRIMARY_LOD_MODE", std::to_string((uint32_t)primaryLodMode));
    defines.add("USE_NRD_DEMODULATION", useNRDDemodulation ? "1" : "0");
    defines.add("COLOR_FORMAT", std::to_string((uint32_t)colorFormat));
    defines.add("MIS_HEURISTIC", std::to_string((uint32_t)misHeuristic));
    defines.add("MIS_POWER_EXPONENT", std::to_string(misPowerExponent));
    defines.add("_USE_DETERMINISTIC_BSDF", useDeterministicBSDF ? "1" : "0");
    defines.add("NEIGHBOR_OFFSET_COUNT", std::to_string(kNeighborOffsetCount));
    defines.add("SHIFT_STRATEGY", std::to_string((uint32_t)shiftStrategy));
    defines.add("PATH_SAMPLING_MODE", std::to_string((uint32_t)pathSamplingMode));

    // We don't use the legacy shading code anymore (MaterialShading.slang).
    defines.add("_USE_LEGACY_SHADING_CODE", "0");

    defines.add("INTERIOR_LIST_SLOT_COUNT", std::to_string(maxNestedMaterials));

    defines.add("GBUFFER_ADJUST_SHADING_NORMALS", owner.mGBufferAdjustShadingNormals ? "1" : "0");

    // Scene-specific configuration.
    const auto& scene = owner.mpScene;

    // Set default (off) values for additional features.
    defines.add("OUTPUT_GUIDE_DATA", "0");
    defines.add("OUTPUT_TIME", "0");
    defines.add("OUTPUT_NRD_DATA", "0");
    defines.add("OUTPUT_NRD_ADDITIONAL_DATA", "0");

    defines.add("SPATIAL_RESTIR_MIS_KIND", std::to_string((uint32_t)spatialMisKind));
    defines.add("TEMPORAL_RESTIR_MIS_KIND", std::to_string((uint32_t)temporalMisKind));

    defines.add("TEMPORAL_UPDATE_FOR_DYNAMIC_SCENE", temporalUpdateForDynamicScene ? "1" : "0");

    defines.add("BPR", pathSamplingMode == PathSamplingMode::PathReuse ? "1" : "0");

    defines.add("SEPARATE_PATH_BSDF", separatePathBSDF ? "1" : "0");

    defines.add("RCDATA_PATH_NUM", rcDataOfflineMode ? "12" : "6");
    defines.add("RCDATA_PAD_SIZE", rcDataOfflineMode ? "2" : "1");

    return defines;
}
