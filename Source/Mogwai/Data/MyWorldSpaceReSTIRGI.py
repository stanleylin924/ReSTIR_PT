from falcor import *

def render_graph_WorldSpaceReSTIRGI():
    g = RenderGraph("WorldSpaceReSTIRGI")
    loadRenderPassLibrary("GBuffer.dll")
    loadRenderPassLibrary("WorldSpaceReSTIRGIPass.dll")

    VBufferRT = createPass("VBufferRT")
    g.addPass(VBufferRT, "VBufferRT")

    WorldSpaceReSTIRGIPass = createPass("WorldSpaceReSTIRGIPass")
    g.addPass(WorldSpaceReSTIRGIPass, "WorldSpaceReSTIRGIPass")

    # g.addEdge("ReSTIRGIGBuffer.vPosW", "WorldSpaceReSTIRGIPass.vPosW")
    # g.addEdge("ReSTIRGIGBuffer.vNormW", "WorldSpaceReSTIRGIPass.vNormW")
    g.addEdge("VBufferRT.viewW", "WorldSpaceReSTIRGIPass.vNormW")

    g.addEdge("VBufferRT.vbuffer", "WorldSpaceReSTIRGIPass.vbuffer")
    g.addEdge('VBufferRT.viewW', 'WorldSpaceReSTIRGIPass.vDepth')

    g.markOutput("WorldSpaceReSTIRGIPass.outputColor")

    return g

WorldSpaceReSTIRGI = render_graph_WorldSpaceReSTIRGI()
try: m.addGraph(WorldSpaceReSTIRGI)
except NameError: None

# m.loadScene('Arcade\Arcade.pyscene')
# m.loadScene('../Data/pink_room/pink_room.pyscene')
# m.loadScene('../Data/VeachAjar/VeachAjar.pyscene')
# m.loadScene('../Data/VeachAjar/VeachAjarAnimated.pyscene')
