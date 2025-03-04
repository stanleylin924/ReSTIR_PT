from falcor import *
import os
import sys

# Add the path of the helper script file to the system path
sys.path.append('D:/3D_Scene/script')
import framecapture

def render_graph_ReSTIRPT():
    g = RenderGraph("ReSTIRPTPass")
    loadRenderPassLibrary("AccumulatePass.dll")
    loadRenderPassLibrary("GBuffer.dll")
    loadRenderPassLibrary("ReSTIRPTPass.dll")
    loadRenderPassLibrary("ToneMapper.dll")
    loadRenderPassLibrary("ScreenSpaceReSTIRPass.dll")
    loadRenderPassLibrary("ErrorMeasurePass.dll")
    loadRenderPassLibrary("ImageLoader.dll")

    ReSTIRGIPlusPass = createPass("ReSTIRPTPass", {'samplesPerPixel': 1})
    g.addPass(ReSTIRGIPlusPass, "ReSTIRPTPass")
    VBufferRT = createPass("VBufferRT", {'samplePattern': SamplePattern.Center, 'sampleCount': 1, 'texLOD': TexLODMode.Mip0, 'useAlphaTest': True})
    g.addPass(VBufferRT, "VBufferRT")
    AccumulatePass = createPass("AccumulatePass", {'enableAccumulation': False, 'precisionMode': AccumulatePrecision.Single})
    g.addPass(AccumulatePass, "AccumulatePass")
    ToneMapper = createPass("ToneMapper", {'autoExposure': False, 'exposureCompensation': 0.0, 'operator': ToneMapOp.Linear})
    g.addPass(ToneMapper, "ToneMapper")
    ScreenSpaceReSTIRPass = createPass("ScreenSpaceReSTIRPass")    
    g.addPass(ScreenSpaceReSTIRPass, "ScreenSpaceReSTIRPass")
    
    g.addEdge("VBufferRT.vbuffer", "ReSTIRPTPass.vbuffer")   
    g.addEdge("VBufferRT.mvec", "ReSTIRPTPass.motionVectors")    
    
    g.addEdge("VBufferRT.vbuffer", "ScreenSpaceReSTIRPass.vbuffer")   
    g.addEdge("VBufferRT.mvec", "ScreenSpaceReSTIRPass.motionVectors")    
    g.addEdge("ScreenSpaceReSTIRPass.color", "ReSTIRPTPass.directLighting")    
    
    g.addEdge("ReSTIRPTPass.color", "AccumulatePass.input")
    g.addEdge("AccumulatePass.output", "ToneMapper.src")
    
    g.markOutput("ToneMapper.dst")
    # g.markOutput("AccumulatePass.output")  

    return g

graph_ReSTIRPT = render_graph_ReSTIRPT()

m.addGraph(graph_ReSTIRPT)

# Scene
m.loadScene('D:/3D_Scene/ReSTIR-FG/Kitchen_ReSTIRFG/KitchenReSTIRFG_v1.3.pyscene')
m.scene.cameraSpeed = 1.0

# Window Configuration
m.resizeSwapChain(1280, 800)
m.ui = True

# Clock Settings
m.clock.time = 0
m.clock.framerate = 30
# If framerate is not zero, you can use the frame property to set the start frame
# m.clock.frame = 0
# m.clock.exitFrame = 250

# Frame Capture
m.frameCapture.outputDir = 'D:/Temp/FrameCapture'
m.frameCapture.baseFilename = 'Mogwai'

# framecapture.capture_cameras(m, 30)
framecapture.capture_frames(m, 179, 229)
# m.timingCapture.captureFrameTime("D:/Temp/FrameCapture/timecapture.csv")

# Profiler: 效能分析
m.profiler.enabled = True
for frame in range(250):
    m.renderFrame()
    # Profiler: 打印 183~196 幀的效能數據
    if m.clock.frame in range(183, 197):
        print(f"Frame ID: {m.clock.frame}", flush=True)
        cputime = m.profiler.events["/onFrameRender/cpuTime"]["value"]
        gputime = m.profiler.events["/onFrameRender/gpuTime"]["value"]
        print(f"Frame time: {cputime}/{gputime} ms")
m.profiler.enabled = False
