#include "WebGpu.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "EngineCore.h"
#include "EngineDebug.h"
#include "EngineMemory.h"
#include "Macros.h"
#include "platform/Platform.h"

#include <windows.h>

namespace
{

    // wgpu takes explicit (pointer, length) strings rather than null-terminated
    // ones, so every literal has to be wrapped.
    WGPUStringView Str(const char* s) { return WGPUStringView{s, s ? strlen(s) : 0}; }

    // One shader for every pass. Untextured geometry samples a 1x1 white texture,
    // which keeps this to two pipelines (3D and 2D) instead of four that differ
    // only in whether a texture is bound.
    const char* const kShaderSource = R"WGSL(
struct Uniforms {
    viewProj : mat4x4<f32>,
};
@group(0) @binding(0) var<uniform> u : Uniforms;
@group(1) @binding(0) var tex : texture_2d<f32>;
@group(1) @binding(1) var smp : sampler;

struct VsOut {
    @builtin(position) clipPos : vec4<f32>,
    @location(0) uv : vec2<f32>,
    @location(1) color : vec4<f32>,
};

@vertex
fn vs_main(@location(0) pos : vec3<f32>,
           @location(1) normal : vec3<f32>,
           @location(2) uv : vec2<f32>,
           @location(3) color : vec4<f32>) -> VsOut {
    var out : VsOut;
    out.clipPos = u.viewProj * vec4<f32>(pos, 1.0);
    out.uv = uv;

    // Fixed headlight term, matching the flat look the PS2 backends produce.
    // Normals are zero for 2D geometry, which falls through to full brightness.
    let n = length(normal);
    var shade = 1.0;
    if (n > 0.0001) {
        let l = normalize(vec3<f32>(0.4, 0.8, 0.45));
        shade = 0.35 + 0.65 * max(dot(normalize(normal), l), 0.0);
    }
    out.color = vec4<f32>(color.rgb * shade, color.a);
    return out;
}

@fragment
fn fs_main(in : VsOut) -> @location(0) vec4<f32> {
    let t = textureSample(tex, smp, in.uv);
    return vec4<f32>(t.rgb * in.color.rgb, t.a * in.color.a);
}
)WGSL";

    struct AdapterRequest
    {
        WGPUAdapter adapter;
        bool done;
    };

    struct DeviceRequest
    {
        WGPUDevice device;
        bool done;
    };

    void OnAdapter(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* user1, void* user2)
    {
        UNUSED_VAR(user2);
        AdapterRequest* req = static_cast<AdapterRequest*>(user1);
        if (status == WGPURequestAdapterStatus_Success)
            req->adapter = adapter;
        else
            Engine_LogError("WebGpu: no adapter (%.*s)", static_cast<int>(message.length), message.data ? message.data : "");
        req->done = true;
    }

    void OnDevice(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* user1, void* user2)
    {
        UNUSED_VAR(user2);
        DeviceRequest* req = static_cast<DeviceRequest*>(user1);
        if (status == WGPURequestDeviceStatus_Success)
            req->device = device;
        else
            Engine_LogError("WebGpu: no device (%.*s)", static_cast<int>(message.length), message.data ? message.data : "");
        req->done = true;
    }

    // Validation errors are programmer errors and must be loud, per the engine's
    // crash-loudly-on-programming-error rule.
    void OnUncapturedError(WGPUDevice const* device, WGPUErrorType type, WGPUStringView message, void* user1, void* user2)
    {
        UNUSED_VAR(device);
        UNUSED_VAR(user1);
        UNUSED_VAR(user2);
        Engine_LogError("WebGpu: uncaptured error (type %d): %.*s", static_cast<int>(type), static_cast<int>(message.length), message.data ? message.data : "");
    }

    // TIM2 stores alpha with 0x80 meaning fully opaque, not 0xFF. Expanding it
    // here keeps textures from looking half-transparent the moment blending is
    // switched on.
    inline uint8_t ExpandPs2Alpha(uint8_t a) { return (a >= 0x80u) ? 0xFFu : static_cast<uint8_t>(a * 2u); }

} // namespace

WebGpuRenderer::WebGpuRenderer(const EngineConfig& config) :
    m_instance(nullptr), m_adapter(nullptr), m_device(nullptr), m_queue(nullptr), m_surface(nullptr), m_surfaceFormat(WGPUTextureFormat_BGRA8Unorm), m_pipeline3D(nullptr), m_pipeline2D(nullptr),
    m_uniformLayout(nullptr), m_textureLayout(nullptr), m_bindGroup3D(nullptr), m_bindGroup2D(nullptr), m_uniformBuffer3D(nullptr), m_uniformBuffer2D(nullptr), m_vertexBuffer(nullptr),
    m_vertexBufferCapacity(0), m_sampler(nullptr), m_depthTexture(nullptr), m_depthView(nullptr), m_whiteTexture{}, m_clearColor{0.0f, 0.0f, 0.0f}, m_width(0), m_height(0), m_frameStats{},
    m_initialized(false)
{
    UNUSED_VAR(config);
    memset(m_textures, 0, sizeof(m_textures));

    Platform* platform = Engine_GetPlatform();
    platform->GetFramebufferSize(&m_width, &m_height);
    Engine_LogInfo("WebGpuRenderer: initializing (%ux%u)", m_width, m_height);

    // The primitive geometry tables live in the renderer arena, same as the PS2
    // backends: the draw lists own the de-interleaved arrays and every path reads
    // them, so nothing about that is platform-specific.
    float* arena = static_cast<float*>(Engine_GetSlot(ARENA_RENDERER, 0));
    if (!arena)
    {
        Engine_LogError("WebGpuRenderer: failed to retrieve ARENA_RENDERER slot 0");
        return;
    }
    m_drawLists.Init(arena);

    if (!InitDevice())
        return;
    if (!CreatePipelines())
        return;
    if (!CreateWhiteTexture())
        return;
    if (!ConfigureSurface(m_width, m_height))
        return;

    m_initialized = true;
    Engine_LogInfo("WebGpuRenderer: ready");
}

bool WebGpuRenderer::InitDevice()
{
    m_instance = wgpuCreateInstance(nullptr);
    if (!m_instance)
    {
        Engine_LogError("WebGpu: wgpuCreateInstance failed");
        return false;
    }

    // Surface first: the adapter is chosen for compatibility with it, so a
    // machine with several GPUs picks the one that can actually present here.
    Platform* platform = Engine_GetPlatform();
    void* hwnd = platform->GetNativeWindowHandle();
    if (!hwnd)
    {
        Engine_LogError("WebGpu: platform has no native window handle");
        return false;
    }

    WGPUSurfaceSourceWindowsHWND fromHwnd;
    memset(&fromHwnd, 0, sizeof(fromHwnd));
    fromHwnd.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
    fromHwnd.hinstance = GetModuleHandleA(nullptr);
    fromHwnd.hwnd = hwnd;

    WGPUSurfaceDescriptor surfaceDesc;
    memset(&surfaceDesc, 0, sizeof(surfaceDesc));
    surfaceDesc.nextInChain = &fromHwnd.chain;
    surfaceDesc.label = Str("engine-surface");

    m_surface = wgpuInstanceCreateSurface(m_instance, &surfaceDesc);
    if (!m_surface)
    {
        Engine_LogError("WebGpu: could not create a surface for the window");
        return false;
    }

    AdapterRequest adapterReq;
    adapterReq.adapter = nullptr;
    adapterReq.done = false;

    WGPURequestAdapterOptions options;
    memset(&options, 0, sizeof(options));
    options.compatibleSurface = m_surface;
    options.powerPreference = WGPUPowerPreference_HighPerformance;

    WGPURequestAdapterCallbackInfo adapterCb;
    memset(&adapterCb, 0, sizeof(adapterCb));
    adapterCb.mode = WGPUCallbackMode_AllowProcessEvents;
    adapterCb.callback = &OnAdapter;
    adapterCb.userdata1 = &adapterReq;

    wgpuInstanceRequestAdapter(m_instance, &options, adapterCb);
    while (!adapterReq.done)
        wgpuInstanceProcessEvents(m_instance);

    if (!adapterReq.adapter)
        return false; // OnAdapter already said why
    m_adapter = adapterReq.adapter;

    DeviceRequest deviceReq;
    deviceReq.device = nullptr;
    deviceReq.done = false;

    WGPUDeviceDescriptor deviceDesc;
    memset(&deviceDesc, 0, sizeof(deviceDesc));
    deviceDesc.label = Str("engine-device");
    deviceDesc.uncapturedErrorCallbackInfo.callback = &OnUncapturedError;

    WGPURequestDeviceCallbackInfo deviceCb;
    memset(&deviceCb, 0, sizeof(deviceCb));
    deviceCb.mode = WGPUCallbackMode_AllowProcessEvents;
    deviceCb.callback = &OnDevice;
    deviceCb.userdata1 = &deviceReq;

    wgpuAdapterRequestDevice(m_adapter, &deviceDesc, deviceCb);
    while (!deviceReq.done)
        wgpuInstanceProcessEvents(m_instance);

    if (!deviceReq.device)
        return false;
    m_device = deviceReq.device;
    m_queue = wgpuDeviceGetQueue(m_device);

    // Prefer a NON-sRGB surface format. The engine hands renderers raw 0-1
    // colour components and the PS2 writes them to the GS untouched; an sRGB
    // surface would apply an encode on top and make every desktop frame visibly
    // lighter than the console, which is the opposite of what a preview target
    // is for. Fall back to whatever the surface prefers if none is offered.
    WGPUSurfaceCapabilities caps;
    memset(&caps, 0, sizeof(caps));
    if (wgpuSurfaceGetCapabilities(m_surface, m_adapter, &caps) == WGPUStatus_Success && caps.formatCount > 0)
    {
        m_surfaceFormat = caps.formats[0];
        for (size_t i = 0; i < caps.formatCount; ++i)
        {
            const WGPUTextureFormat f = caps.formats[i];
            if (f == WGPUTextureFormat_BGRA8Unorm || f == WGPUTextureFormat_RGBA8Unorm)
            {
                m_surfaceFormat = f;
                break;
            }
        }
    }

    return true;
}

bool WebGpuRenderer::CreatePipelines()
{
    WGPUShaderSourceWGSL wgsl;
    memset(&wgsl, 0, sizeof(wgsl));
    wgsl.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgsl.code = Str(kShaderSource);

    WGPUShaderModuleDescriptor shaderDesc;
    memset(&shaderDesc, 0, sizeof(shaderDesc));
    shaderDesc.nextInChain = &wgsl.chain;
    shaderDesc.label = Str("engine-shader");

    WGPUShaderModule shader = wgpuDeviceCreateShaderModule(m_device, &shaderDesc);
    if (!shader)
    {
        Engine_LogError("WebGpu: shader module creation failed");
        return false;
    }

    // --- group 0: the view-projection uniform -------------------------------
    WGPUBindGroupLayoutEntry uniformEntry;
    memset(&uniformEntry, 0, sizeof(uniformEntry));
    uniformEntry.binding = 0;
    uniformEntry.visibility = WGPUShaderStage_Vertex;
    uniformEntry.buffer.type = WGPUBufferBindingType_Uniform;
    uniformEntry.buffer.minBindingSize = sizeof(Uniforms);

    WGPUBindGroupLayoutDescriptor uniformLayoutDesc;
    memset(&uniformLayoutDesc, 0, sizeof(uniformLayoutDesc));
    uniformLayoutDesc.entryCount = 1;
    uniformLayoutDesc.entries = &uniformEntry;
    m_uniformLayout = wgpuDeviceCreateBindGroupLayout(m_device, &uniformLayoutDesc);

    // --- group 1: texture + sampler -----------------------------------------
    WGPUBindGroupLayoutEntry texEntries[2];
    memset(texEntries, 0, sizeof(texEntries));
    texEntries[0].binding = 0;
    texEntries[0].visibility = WGPUShaderStage_Fragment;
    texEntries[0].texture.sampleType = WGPUTextureSampleType_Float;
    texEntries[0].texture.viewDimension = WGPUTextureViewDimension_2D;
    texEntries[1].binding = 1;
    texEntries[1].visibility = WGPUShaderStage_Fragment;
    texEntries[1].sampler.type = WGPUSamplerBindingType_Filtering;

    WGPUBindGroupLayoutDescriptor texLayoutDesc;
    memset(&texLayoutDesc, 0, sizeof(texLayoutDesc));
    texLayoutDesc.entryCount = 2;
    texLayoutDesc.entries = texEntries;
    m_textureLayout = wgpuDeviceCreateBindGroupLayout(m_device, &texLayoutDesc);

    WGPUSamplerDescriptor samplerDesc;
    memset(&samplerDesc, 0, sizeof(samplerDesc));
    samplerDesc.addressModeU = WGPUAddressMode_Repeat;
    samplerDesc.addressModeV = WGPUAddressMode_Repeat;
    samplerDesc.addressModeW = WGPUAddressMode_Repeat;
    samplerDesc.magFilter = WGPUFilterMode_Linear;
    samplerDesc.minFilter = WGPUFilterMode_Linear;
    samplerDesc.mipmapFilter = WGPUMipmapFilterMode_Linear;
    samplerDesc.lodMaxClamp = 32.0f;
    samplerDesc.maxAnisotropy = 1;
    m_sampler = wgpuDeviceCreateSampler(m_device, &samplerDesc);

    WGPUBufferDescriptor uniformDesc;
    memset(&uniformDesc, 0, sizeof(uniformDesc));
    uniformDesc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    uniformDesc.size = sizeof(Uniforms);
    m_uniformBuffer3D = wgpuDeviceCreateBuffer(m_device, &uniformDesc);
    m_uniformBuffer2D = wgpuDeviceCreateBuffer(m_device, &uniformDesc);

    // Two bind groups rather than one rebound mid-pass: the 3D and 2D passes want
    // different matrices, and swapping the group is cheaper than a buffer write
    // between them.
    WGPUBindGroupEntry bindEntry;
    memset(&bindEntry, 0, sizeof(bindEntry));
    bindEntry.binding = 0;
    bindEntry.size = sizeof(Uniforms);

    WGPUBindGroupDescriptor bindDesc;
    memset(&bindDesc, 0, sizeof(bindDesc));
    bindDesc.layout = m_uniformLayout;
    bindDesc.entryCount = 1;
    bindDesc.entries = &bindEntry;

    bindEntry.buffer = m_uniformBuffer3D;
    m_bindGroup3D = wgpuDeviceCreateBindGroup(m_device, &bindDesc);
    bindEntry.buffer = m_uniformBuffer2D;
    m_bindGroup2D = wgpuDeviceCreateBindGroup(m_device, &bindDesc);

    WGPUBindGroupLayout layouts[2] = {m_uniformLayout, m_textureLayout};
    WGPUPipelineLayoutDescriptor pipelineLayoutDesc;
    memset(&pipelineLayoutDesc, 0, sizeof(pipelineLayoutDesc));
    pipelineLayoutDesc.bindGroupLayoutCount = 2;
    pipelineLayoutDesc.bindGroupLayouts = layouts;
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(m_device, &pipelineLayoutDesc);

    // --- vertex layout ------------------------------------------------------
    WGPUVertexAttribute attributes[4];
    memset(attributes, 0, sizeof(attributes));
    attributes[0].format = WGPUVertexFormat_Float32x3; // position
    attributes[0].offset = 0;
    attributes[0].shaderLocation = 0;
    attributes[1].format = WGPUVertexFormat_Float32x3; // normal
    attributes[1].offset = sizeof(float) * 3;
    attributes[1].shaderLocation = 1;
    attributes[2].format = WGPUVertexFormat_Float32x2; // uv
    attributes[2].offset = sizeof(float) * 6;
    attributes[2].shaderLocation = 2;
    attributes[3].format = WGPUVertexFormat_Float32x4; // colour
    attributes[3].offset = sizeof(float) * 8;
    attributes[3].shaderLocation = 3;

    WGPUVertexBufferLayout vertexLayout;
    memset(&vertexLayout, 0, sizeof(vertexLayout));
    vertexLayout.arrayStride = sizeof(DesktopGeometry::Vertex);
    vertexLayout.stepMode = WGPUVertexStepMode_Vertex;
    vertexLayout.attributeCount = 4;
    vertexLayout.attributes = attributes;

    WGPUColorTargetState colorTarget;
    memset(&colorTarget, 0, sizeof(colorTarget));
    colorTarget.format = m_surfaceFormat;
    colorTarget.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragment;
    memset(&fragment, 0, sizeof(fragment));
    fragment.module = shader;
    fragment.entryPoint = Str("fs_main");
    fragment.targetCount = 1;
    fragment.targets = &colorTarget;

    WGPUDepthStencilState depthState;
    memset(&depthState, 0, sizeof(depthState));
    depthState.format = WGPUTextureFormat_Depth24Plus;
    depthState.depthWriteEnabled = WGPUOptionalBool_True;
    depthState.depthCompare = WGPUCompareFunction_Less;
    depthState.stencilFront.compare = WGPUCompareFunction_Always;
    depthState.stencilBack.compare = WGPUCompareFunction_Always;

    WGPURenderPipelineDescriptor pipelineDesc;
    memset(&pipelineDesc, 0, sizeof(pipelineDesc));
    pipelineDesc.label = Str("engine-3d");
    pipelineDesc.layout = pipelineLayout;
    pipelineDesc.vertex.module = shader;
    pipelineDesc.vertex.entryPoint = Str("vs_main");
    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers = &vertexLayout;
    pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
    // No back-face culling: the baked level and model geometry does not carry a
    // guaranteed winding, and dropping triangles is worse than drawing extra.
    pipelineDesc.primitive.cullMode = WGPUCullMode_None;
    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = 0xFFFFFFFFu;
    pipelineDesc.depthStencil = &depthState;
    pipelineDesc.fragment = &fragment;

    m_pipeline3D = wgpuDeviceCreateRenderPipeline(m_device, &pipelineDesc);

    // 2D shares everything but depth: HUD quads are submitted in draw order and
    // must not be discarded by Z.
    WGPUDepthStencilState depth2D = depthState;
    depth2D.depthWriteEnabled = WGPUOptionalBool_False;
    depth2D.depthCompare = WGPUCompareFunction_Always;

    pipelineDesc.label = Str("engine-2d");
    pipelineDesc.depthStencil = &depth2D;
    m_pipeline2D = wgpuDeviceCreateRenderPipeline(m_device, &pipelineDesc);

    if (!m_pipeline3D || !m_pipeline2D)
    {
        Engine_LogError("WebGpu: render pipeline creation failed");
        return false;
    }
    return true;
}

bool WebGpuRenderer::CreateWhiteTexture()
{
    WGPUTextureDescriptor desc;
    memset(&desc, 0, sizeof(desc));
    desc.label = Str("engine-white");
    desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    desc.dimension = WGPUTextureDimension_2D;
    desc.size.width = 1;
    desc.size.height = 1;
    desc.size.depthOrArrayLayers = 1;
    desc.format = WGPUTextureFormat_RGBA8Unorm;
    desc.mipLevelCount = 1;
    desc.sampleCount = 1;

    m_whiteTexture.texture = wgpuDeviceCreateTexture(m_device, &desc);
    if (!m_whiteTexture.texture)
        return false;

    const uint32_t white = 0xFFFFFFFFu;
    WGPUTexelCopyTextureInfo dst;
    memset(&dst, 0, sizeof(dst));
    dst.texture = m_whiteTexture.texture;
    dst.aspect = WGPUTextureAspect_All;

    WGPUTexelCopyBufferLayout layout;
    memset(&layout, 0, sizeof(layout));
    layout.bytesPerRow = 4;
    layout.rowsPerImage = 1;

    WGPUExtent3D extent = {1, 1, 1};
    wgpuQueueWriteTexture(m_queue, &dst, &white, sizeof(white), &layout, &extent);

    m_whiteTexture.view = wgpuTextureCreateView(m_whiteTexture.texture, nullptr);

    WGPUBindGroupEntry entries[2];
    memset(entries, 0, sizeof(entries));
    entries[0].binding = 0;
    entries[0].textureView = m_whiteTexture.view;
    entries[1].binding = 1;
    entries[1].sampler = m_sampler;

    WGPUBindGroupDescriptor bindDesc;
    memset(&bindDesc, 0, sizeof(bindDesc));
    bindDesc.layout = m_textureLayout;
    bindDesc.entryCount = 2;
    bindDesc.entries = entries;
    m_whiteTexture.bindGroup = wgpuDeviceCreateBindGroup(m_device, &bindDesc);

    return m_whiteTexture.bindGroup != nullptr;
}

// ---------------------------------------------------------------------------
// Textures
// ---------------------------------------------------------------------------

uint32_t WebGpuRenderer::UploadTexture(const TextureUpload& upload)
{
    if (!m_initialized && !m_device)
        return 0;

    int slot = -1;
    for (int i = 0; i < WGPU_MAX_TEXTURES; ++i)
    {
        if (!m_textures[i].texture)
        {
            slot = i;
            break;
        }
    }
    if (slot < 0)
    {
        Engine_LogError("WebGpuRenderer: texture registry full (%d)", WGPU_MAX_TEXTURES);
        return 0;
    }

    const uint32_t width = static_cast<uint32_t>(upload.width);
    const uint32_t height = static_cast<uint32_t>(upload.height);
    const size_t texels = static_cast<size_t>(width) * height;
    if (width == 0 || height == 0)
        return 0;

    // Every source format is expanded to RGBA8 here. A desktop GPU has no reason
    // to carry the PS2 storage modes, and the resource manager's byte budget is
    // computed against the same assumption (see Win32Platform::GetTextureFootprintBytes).
    uint32_t* rgba = static_cast<uint32_t*>(malloc(texels * 4));
    if (!rgba)
    {
        Engine_LogError("WebGpuRenderer: out of memory expanding a %ux%u texture", width, height);
        return 0;
    }

    if (upload.format == PixelFormat::RGBA32)
    {
        const uint8_t* src = static_cast<const uint8_t*>(upload.levelPtr[0]);
        uint8_t* dst = reinterpret_cast<uint8_t*>(rgba);
        for (size_t i = 0; i < texels; ++i)
        {
            dst[i * 4 + 0] = src[i * 4 + 0];
            dst[i * 4 + 1] = src[i * 4 + 1];
            dst[i * 4 + 2] = src[i * 4 + 2];
            dst[i * 4 + 3] = ExpandPs2Alpha(src[i * 4 + 3]);
        }
    }
    else if (upload.format == PixelFormat::RGBA16)
    {
        // A1B5G5R5 packed little-endian: 5 bits each, alpha in the top bit.
        const uint16_t* src = static_cast<const uint16_t*>(upload.levelPtr[0]);
        uint8_t* dst = reinterpret_cast<uint8_t*>(rgba);
        for (size_t i = 0; i < texels; ++i)
        {
            const uint16_t p = src[i];
            dst[i * 4 + 0] = static_cast<uint8_t>(((p >> 0) & 0x1Fu) * 255u / 31u);
            dst[i * 4 + 1] = static_cast<uint8_t>(((p >> 5) & 0x1Fu) * 255u / 31u);
            dst[i * 4 + 2] = static_cast<uint8_t>(((p >> 10) & 0x1Fu) * 255u / 31u);
            dst[i * 4 + 3] = (p & 0x8000u) ? 0xFFu : 0x00u;
        }
    }
    else // PAL8
    {
        const uint8_t* idx = static_cast<const uint8_t*>(upload.levelPtr[0]);
        const uint8_t* clut = static_cast<const uint8_t*>(upload.clut);
        uint8_t* dst = reinterpret_cast<uint8_t*>(rgba);
        for (size_t i = 0; i < texels; ++i)
        {
            if (!clut)
            {
                rgba[i] = 0xFFFFFFFFu;
                continue;
            }
            const uint8_t* e = clut + static_cast<size_t>(idx[i]) * 4u;
            dst[i * 4 + 0] = e[0];
            dst[i * 4 + 1] = e[1];
            dst[i * 4 + 2] = e[2];
            dst[i * 4 + 3] = ExpandPs2Alpha(e[3]);
        }
    }

    WGPUTextureDescriptor desc;
    memset(&desc, 0, sizeof(desc));
    desc.label = Str("engine-texture");
    desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    desc.dimension = WGPUTextureDimension_2D;
    desc.size.width = width;
    desc.size.height = height;
    desc.size.depthOrArrayLayers = 1;
    desc.format = WGPUTextureFormat_RGBA8Unorm;
    // Only level 0 is uploaded: the baked mip chain is in the PS2 storage layout,
    // and expanding every level would cost more than it buys before there is a
    // measured need for it.
    desc.mipLevelCount = 1;
    desc.sampleCount = 1;

    WGPUTexture texture = wgpuDeviceCreateTexture(m_device, &desc);
    if (!texture)
    {
        free(rgba);
        Engine_LogError("WebGpuRenderer: wgpuDeviceCreateTexture failed for %ux%u", width, height);
        return 0;
    }

    WGPUTexelCopyTextureInfo dst;
    memset(&dst, 0, sizeof(dst));
    dst.texture = texture;
    dst.aspect = WGPUTextureAspect_All;

    WGPUTexelCopyBufferLayout layout;
    memset(&layout, 0, sizeof(layout));
    layout.bytesPerRow = width * 4u;
    layout.rowsPerImage = height;

    WGPUExtent3D extent = {width, height, 1};
    wgpuQueueWriteTexture(m_queue, &dst, rgba, texels * 4, &layout, &extent);
    free(rgba);

    WGPUTextureView view = wgpuTextureCreateView(texture, nullptr);

    WGPUBindGroupEntry entries[2];
    memset(entries, 0, sizeof(entries));
    entries[0].binding = 0;
    entries[0].textureView = view;
    entries[1].binding = 1;
    entries[1].sampler = m_sampler;

    WGPUBindGroupDescriptor bindDesc;
    memset(&bindDesc, 0, sizeof(bindDesc));
    bindDesc.layout = m_textureLayout;
    bindDesc.entryCount = 2;
    bindDesc.entries = entries;

    m_textures[slot].texture = texture;
    m_textures[slot].view = view;
    m_textures[slot].bindGroup = wgpuDeviceCreateBindGroup(m_device, &bindDesc);

    // index+1, so 0 stays the universal "invalid handle".
    return static_cast<uint32_t>(slot) + 1u;
}

void WebGpuRenderer::ReleaseTexture(uint32_t handle)
{
    if (handle == 0 || handle > WGPU_MAX_TEXTURES)
        return;

    TextureEntry& e = m_textures[handle - 1u];
    if (e.bindGroup)
        wgpuBindGroupRelease(e.bindGroup);
    if (e.view)
        wgpuTextureViewRelease(e.view);
    if (e.texture)
    {
        wgpuTextureDestroy(e.texture);
        wgpuTextureRelease(e.texture);
    }
    memset(&e, 0, sizeof(e));
}

WGPUBindGroup WebGpuRenderer::BindGroupFor(uint32_t handle) const
{
    if (handle == 0 || handle > WGPU_MAX_TEXTURES)
        return m_whiteTexture.bindGroup;
    return m_textures[handle - 1u].bindGroup ? m_textures[handle - 1u].bindGroup : m_whiteTexture.bindGroup;
}


// ---------------------------------------------------------------------------
// Surface and depth
// ---------------------------------------------------------------------------

bool WebGpuRenderer::ConfigureSurface(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return true; // minimised; keep the old configuration

    WGPUSurfaceConfiguration config;
    memset(&config, 0, sizeof(config));
    config.device = m_device;
    config.format = m_surfaceFormat;
    config.usage = WGPUTextureUsage_RenderAttachment;
    config.width = width;
    config.height = height;
    config.alphaMode = WGPUCompositeAlphaMode_Auto;
    config.presentMode = WGPUPresentMode_Fifo; // vsync

    wgpuSurfaceConfigure(m_surface, &config);

    m_width = width;
    m_height = height;
    return EnsureDepthTexture(width, height);
}

bool WebGpuRenderer::EnsureDepthTexture(uint32_t width, uint32_t height)
{
    ReleaseDepthTexture();

    WGPUTextureDescriptor desc;
    memset(&desc, 0, sizeof(desc));
    desc.label = Str("engine-depth");
    desc.usage = WGPUTextureUsage_RenderAttachment;
    desc.dimension = WGPUTextureDimension_2D;
    desc.size.width = width;
    desc.size.height = height;
    desc.size.depthOrArrayLayers = 1;
    desc.format = WGPUTextureFormat_Depth24Plus;
    desc.mipLevelCount = 1;
    desc.sampleCount = 1;

    m_depthTexture = wgpuDeviceCreateTexture(m_device, &desc);
    if (!m_depthTexture)
    {
        Engine_LogError("WebGpu: depth texture creation failed");
        return false;
    }
    m_depthView = wgpuTextureCreateView(m_depthTexture, nullptr);
    return m_depthView != nullptr;
}

void WebGpuRenderer::ReleaseDepthTexture()
{
    if (m_depthView)
    {
        wgpuTextureViewRelease(m_depthView);
        m_depthView = nullptr;
    }
    if (m_depthTexture)
    {
        wgpuTextureDestroy(m_depthTexture);
        wgpuTextureRelease(m_depthTexture);
        m_depthTexture = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Draw-list submission
// ---------------------------------------------------------------------------

void WebGpuRenderer::AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale)
{
    AddPrimitiveToDrawList(primitive, position, rotation, scale, Color3{1.0f, 1.0f, 1.0f}, -1);
}

void WebGpuRenderer::AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, Color3 color)
{
    AddPrimitiveToDrawList(primitive, position, rotation, scale, color, -1);
}

void WebGpuRenderer::AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, int32_t textureId)
{
    AddPrimitiveToDrawList(primitive, position, rotation, scale, Color3{1.0f, 1.0f, 1.0f}, textureId);
}

void WebGpuRenderer::AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, Color3 color, int32_t textureId)
{
    PrimitiveDrawEntry entry;
    entry.transform = Transform3D(position, rotation, scale);
    entry.color = color;
    entry.type = primitive;
    entry.textureId = textureId;
    m_drawLists.AddPrimitive(entry);
}

void WebGpuRenderer::AddUIToDrawList(const UI& ui, const Vector2& offset, const Vector2& scale)
{
    UIDrawEntry entry;
    entry.ui = ui;
    entry.offset = offset;
    entry.scale = scale.x;
    m_drawLists.AddUIDraw(entry);
}

// Level geometry is pulled from the sector manager at render time rather than
// queued here, matching the PS2 backends: the resident ring is the source of
// truth and re-queueing it every frame would duplicate that state.
void WebGpuRenderer::AddLevelToDrawList(const Level& level) { UNUSED_VAR(level); }

void WebGpuRenderer::AddModelToDrawList(int32_t modelId, const Vector3& position, const Vector3& rotation, const Vector3& scale)
{
    ModelDrawEntry entry;
    entry.resourceId = modelId;
    entry.transform = Transform3D(position, rotation, scale);
    m_drawLists.AddModel(entry);
}

void WebGpuRenderer::AddSkyToDrawList(int32_t resourceId) { m_drawLists.SetSkyboxTexture(resourceId); }

void WebGpuRenderer::ClearDrawLists() { m_drawLists.Reset(false); }

void WebGpuRenderer::ClearFrame(const Color3& color) { m_clearColor = color; }

void WebGpuRenderer::DrawRect2D(int32_t x, int32_t y, int32_t width, int32_t height, const Color3& color) { m_geometry.AddRect2D(x, y, width, height, color); }

void WebGpuRenderer::DrawGrid(int32_t slices, float spacing)
{
    UNUSED_VAR(slices);
    UNUSED_VAR(spacing);
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------

void WebGpuRenderer::BeginFrame()
{
    // A resize invalidates the swapchain and the depth buffer together.
    Platform* platform = Engine_GetPlatform();
    uint32_t w = 0, h = 0;
    platform->GetFramebufferSize(&w, &h);
    if ((w != m_width || h != m_height) && w > 0 && h > 0)
        ConfigureSurface(w, h);

    m_geometry.BeginFrame();
    m_frameStats = DrawStats{};
}

void WebGpuRenderer::Render() { m_geometry.BuildFrame(m_drawLists, &m_frameStats); }

void WebGpuRenderer::EndFrame()
{
    if (!m_initialized)
        return;

    WGPUSurfaceTexture surfaceTexture;
    memset(&surfaceTexture, 0, sizeof(surfaceTexture));
    wgpuSurfaceGetCurrentTexture(m_surface, &surfaceTexture);

    // Lost/outdated is normal after a resize or a mode switch: reconfigure and
    // skip this frame rather than treating it as an error.
    if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal && surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal)
    {
        if (surfaceTexture.texture)
            wgpuTextureRelease(surfaceTexture.texture);
        ConfigureSurface(m_width, m_height);
        m_geometry.EndFrame();
        m_drawLists.Reset(false);
        return;
    }

    WGPUTextureView backbuffer = wgpuTextureCreateView(surfaceTexture.texture, nullptr);

    // Upload both ranges into one buffer, 3D first, so each pass draws a
    // contiguous slice and only the pipeline and bind group change between them.
    const uint32_t count3D = m_geometry.Count3D();
    const uint32_t count2D = m_geometry.Count2D();
    const uint32_t totalVerts = count3D + count2D;
    if (totalVerts > 0)
    {
        const uint64_t needed = static_cast<uint64_t>(totalVerts) * sizeof(DesktopGeometry::Vertex);
        if (needed > m_vertexBufferCapacity)
        {
            if (m_vertexBuffer)
            {
                wgpuBufferDestroy(m_vertexBuffer);
                wgpuBufferRelease(m_vertexBuffer);
            }
            WGPUBufferDescriptor desc;
            memset(&desc, 0, sizeof(desc));
            desc.label = Str("engine-vertices");
            desc.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
            desc.size = needed * 2u; // headroom, so a growing scene does not realloc every frame
            m_vertexBuffer = wgpuDeviceCreateBuffer(m_device, &desc);
            m_vertexBufferCapacity = desc.size;
        }

        if (count3D > 0)
            wgpuQueueWriteBuffer(m_queue, m_vertexBuffer, 0, m_geometry.Vertices3D(), static_cast<size_t>(count3D) * sizeof(DesktopGeometry::Vertex));
        if (count2D > 0)
            wgpuQueueWriteBuffer(m_queue, m_vertexBuffer, static_cast<uint64_t>(count3D) * sizeof(DesktopGeometry::Vertex), m_geometry.Vertices2D(),
                                 static_cast<size_t>(count2D) * sizeof(DesktopGeometry::Vertex));
    }

    Uniforms uniforms;
    // WebGPU clips Z to [0,1]; the shared builder takes that as a flag.
    DesktopGeometry::BuildViewProjection(m_drawLists.GetCamera3D(), m_width, m_height, true, uniforms.viewProj);
    wgpuQueueWriteBuffer(m_queue, m_uniformBuffer3D, 0, &uniforms, sizeof(uniforms));
    DesktopGeometry::BuildOrtho2D(m_width, m_height, true, uniforms.viewProj);
    wgpuQueueWriteBuffer(m_queue, m_uniformBuffer2D, 0, &uniforms, sizeof(uniforms));

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_device, nullptr);

    WGPURenderPassColorAttachment colorAttachment;
    memset(&colorAttachment, 0, sizeof(colorAttachment));
    colorAttachment.view = backbuffer;
    colorAttachment.loadOp = WGPULoadOp_Clear;
    colorAttachment.storeOp = WGPUStoreOp_Store;
    colorAttachment.clearValue = WGPUColor{m_clearColor.r, m_clearColor.g, m_clearColor.b, 1.0};
    colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

    WGPURenderPassDepthStencilAttachment depthAttachment;
    memset(&depthAttachment, 0, sizeof(depthAttachment));
    depthAttachment.view = m_depthView;
    depthAttachment.depthLoadOp = WGPULoadOp_Clear;
    depthAttachment.depthStoreOp = WGPUStoreOp_Store;
    depthAttachment.depthClearValue = 1.0f;

    WGPURenderPassDescriptor passDesc;
    memset(&passDesc, 0, sizeof(passDesc));
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments = &colorAttachment;
    passDesc.depthStencilAttachment = m_depthView ? &depthAttachment : nullptr;

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &passDesc);

    if (totalVerts > 0)
    {
        wgpuRenderPassEncoderSetVertexBuffer(pass, 0, m_vertexBuffer, 0, static_cast<uint64_t>(totalVerts) * sizeof(DesktopGeometry::Vertex));

        // 3D: one draw per texture run.
        if (count3D > 0)
        {
            wgpuRenderPassEncoderSetPipeline(pass, m_pipeline3D);
            wgpuRenderPassEncoderSetBindGroup(pass, 0, m_bindGroup3D, 0, nullptr);
            const DesktopGeometry::DrawRun* runs = m_geometry.Runs();
            for (uint32_t i = 0; i < m_geometry.RunCount(); ++i)
            {
                wgpuRenderPassEncoderSetBindGroup(pass, 1, BindGroupFor(runs[i].texture), 0, nullptr);
                wgpuRenderPassEncoderDraw(pass, runs[i].count, 1, runs[i].first, 0);
            }
        }

        // 2D: always the white texture, so one draw covers the whole HUD.
        if (count2D > 0)
        {
            wgpuRenderPassEncoderSetPipeline(pass, m_pipeline2D);
            wgpuRenderPassEncoderSetBindGroup(pass, 0, m_bindGroup2D, 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(pass, 1, m_whiteTexture.bindGroup, 0, nullptr);
            wgpuRenderPassEncoderDraw(pass, count2D, 1, count3D, 0);
        }
    }

    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(m_queue, 1, &commands);

    wgpuCommandBufferRelease(commands);
    wgpuCommandEncoderRelease(encoder);
    wgpuTextureViewRelease(backbuffer);

    wgpuSurfacePresent(m_surface);
    wgpuTextureRelease(surfaceTexture.texture);

    // 2D is consumed here, not at BeginFrame: this is when the game has finished
    // submitting it.
    m_geometry.EndFrame();

    m_drawLists.SetLastStats(m_frameStats);
    m_drawLists.Reset(false);
}

void WebGpuRenderer::DrawDebugOverlay() {}

// ---------------------------------------------------------------------------
// Cameras and lifecycle
// ---------------------------------------------------------------------------

void WebGpuRenderer::SetCamera3D(CameraID id, const Camera3D& camera) { m_drawLists.SetCamera3D(id, camera); }
void WebGpuRenderer::SetActiveCamera3D(CameraID id) { m_drawLists.SetActiveCamera3D(id); }
void WebGpuRenderer::SetActiveCamera2D(const Camera2D& camera) { m_drawLists.SetActiveCamera2D(camera); }

bool WebGpuRenderer::IsInitialized() const { return m_initialized; }

void WebGpuRenderer::Shutdown()
{
    if (!m_initialized)
        return;

    for (uint32_t i = 0; i < WGPU_MAX_TEXTURES; ++i)
        ReleaseTexture(i + 1u);

    if (m_whiteTexture.bindGroup)
        wgpuBindGroupRelease(m_whiteTexture.bindGroup);
    if (m_whiteTexture.view)
        wgpuTextureViewRelease(m_whiteTexture.view);
    if (m_whiteTexture.texture)
        wgpuTextureRelease(m_whiteTexture.texture);

    ReleaseDepthTexture();
    if (m_sampler)
        wgpuSamplerRelease(m_sampler);
    if (m_vertexBuffer)
        wgpuBufferRelease(m_vertexBuffer);
    if (m_uniformBuffer3D)
        wgpuBufferRelease(m_uniformBuffer3D);
    if (m_uniformBuffer2D)
        wgpuBufferRelease(m_uniformBuffer2D);
    if (m_bindGroup3D)
        wgpuBindGroupRelease(m_bindGroup3D);
    if (m_bindGroup2D)
        wgpuBindGroupRelease(m_bindGroup2D);
    if (m_uniformLayout)
        wgpuBindGroupLayoutRelease(m_uniformLayout);
    if (m_textureLayout)
        wgpuBindGroupLayoutRelease(m_textureLayout);
    if (m_pipeline3D)
        wgpuRenderPipelineRelease(m_pipeline3D);
    if (m_pipeline2D)
        wgpuRenderPipelineRelease(m_pipeline2D);
    if (m_surface)
        wgpuSurfaceRelease(m_surface);
    if (m_device)
        wgpuDeviceRelease(m_device);
    if (m_adapter)
        wgpuAdapterRelease(m_adapter);
    if (m_instance)
        wgpuInstanceRelease(m_instance);

    m_initialized = false;
}

RendererType WebGpuRenderer::GetRendererType() const { return RendererType::WebGpu; }
DrawStats WebGpuRenderer::GetLastStats() const { return m_drawLists.GetLastStats(); }
Camera3D WebGpuRenderer::GetActiveCamera3D() const { return m_drawLists.GetCamera3D(); }

// The Renderer interface exposes these as separate phases; this backend builds
// everything in Render() and submits once in EndFrame(), so they stay empty.
void WebGpuRenderer::RenderSkybox(const DrawLists& lists) { UNUSED_VAR(lists); }
void WebGpuRenderer::RenderPrimitives(DrawLists& lists) { UNUSED_VAR(lists); }
void WebGpuRenderer::RenderModels(const DrawLists& lists) { UNUSED_VAR(lists); }
void WebGpuRenderer::RenderUI(const DrawLists& lists) { UNUSED_VAR(lists); }
