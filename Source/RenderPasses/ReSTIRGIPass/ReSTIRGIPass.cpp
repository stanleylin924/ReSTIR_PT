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
    const char kDesc[] = "Insert pass description here";    

    const std::string kGeneratePathsFilename = "RenderPasses/ReSTIRGIPass/GeneratePaths.cs.slang";

    // Render pass inputs and outputs.
    const std::string kInputVBuffer = "vbuffer";
    const std::string kInputMotionVectors = "motionVectors";
    const std::string kInputDirectLighting = "directLighting";

    const Falcor::ChannelList kInputChannels =
    {
        { kInputVBuffer,        "gVBuffer",        "Visibility buffer in packed format",   false,               ResourceFormat::Unknown },
        { kInputMotionVectors,  "gMotionVectors",  "Motion vector buffer (float format)",  true /* optional */, ResourceFormat::RG32Float },
        { kInputDirectLighting, "gDirectLighting", "Sample count buffer (integer format)", true /* optional */, ResourceFormat::RGBA32Float },
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

ReSTIRGIPass::SharedPtr ReSTIRGIPass::create(RenderContext* pRenderContext, const Dictionary& dict)
{
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
    Program::DefineList defines;

    mpGeneratePaths = ComputePass::create(kGeneratePathsFilename, "main", defines, false);
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
}

void ReSTIRGIPass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{
    mpScene = pScene;

    if (mpScene)
    {
        if (is_set(pScene->getPrimitiveTypes(), PrimitiveTypeFlags::Custom)) logError("This render pass does not support custom primitives.");

        // Prepare our programs for the scene.
        Shader::DefineList defines = mpScene->getSceneDefines();

        mpGeneratePaths->getProgram()->addDefines(defines);
        mpGeneratePaths->setVars(nullptr);
    }
}

void ReSTIRGIPass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (!beginFrame(pRenderContext, renderData)) return;

    // Bind resources.
    auto var = mpGeneratePaths->getRootVar()["CB"]["gPathGenerator"];
    var["outputColor"] = renderData[kOutputColor]->asTexture();

    auto frameDim = renderData.getDefaultTextureDims();
    mpGeneratePaths->execute(pRenderContext, uint3(frameDim, 1u));

    endFrame(pRenderContext, renderData);
}

void ReSTIRGIPass::renderUI(Gui::Widgets& widget)
{
}

bool ReSTIRGIPass::beginFrame(RenderContext* pRenderContext, const RenderData& renderData)
{
    const auto& pOutputColor = renderData[kOutputColor]->asTexture();
    assert(pOutputColor);
    pRenderContext->clearUAV(pOutputColor->getUAV().get(), float4(0.f, 0.3f, 0.f, 0.f));

    return true;
}

void ReSTIRGIPass::endFrame(RenderContext* pRenderContext, const RenderData& renderData)
{
}
