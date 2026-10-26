#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <span>

namespace Sunshine::RHI {

enum class BufferUsage : uint32_t {
    Vertex  = 1 << 0,
    Index   = 1 << 1,
    Uniform = 1 << 2,
    TransferSrc = 1 << 3,
    TransferDst = 1 << 4
};

inline BufferUsage operator|(BufferUsage a, BufferUsage b) {
    return static_cast<BufferUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool operator&(BufferUsage a, BufferUsage b) {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

enum class BufferMemoryType {
    GPUOnly,    // Optimal device local
    CPUVisible  // Host visible and coherent for frequent updates
};

struct BufferDesc {
    size_t size{0};
    BufferUsage usage{BufferUsage::Vertex};
    BufferMemoryType memoryType{BufferMemoryType::CPUVisible};
    std::string debugName;
};

enum class IndexType {
    Uint16,
    Uint32
};

enum class ShaderStage : uint32_t {
    Vertex   = 1 << 0,
    Fragment = 1 << 1
};

inline ShaderStage operator|(ShaderStage a, ShaderStage b) {
    return static_cast<ShaderStage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool operator&(ShaderStage a, ShaderStage b) {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

struct ShaderDesc {
    ShaderStage stage;
    std::span<const uint32_t> spirvCode;
    std::string entryPoint{"main"};
};

enum class VertexFormat {
    Float,
    Float2,
    Float3,
    Float4
};

inline uint32_t GetVertexFormatSize(VertexFormat format) {
    switch (format) {
        case VertexFormat::Float:  return sizeof(float);
        case VertexFormat::Float2: return sizeof(float) * 2;
        case VertexFormat::Float3: return sizeof(float) * 3;
        case VertexFormat::Float4: return sizeof(float) * 4;
        default: return 0;
    }
}

enum class VertexInputRate {
    PerVertex,
    PerInstance
};

struct VertexBindingDesc {
    uint32_t binding{0};
    uint32_t stride{0};
    VertexInputRate inputRate{VertexInputRate::PerVertex};
};

struct VertexAttribute {
    uint32_t location{0};
    VertexFormat format{VertexFormat::Float3};
    uint32_t offset{0};
    uint32_t binding{0};
};

struct VertexLayout {
    uint32_t stride{0};
    std::vector<VertexBindingDesc> bindings;
    std::vector<VertexAttribute> attributes;
};

enum class PrimitiveTopology {
    TriangleList,
    TriangleStrip,
    LineList,
    PointList
};

enum class CullMode {
    None,
    Front,
    Back
};

enum class FrontFace {
    CounterClockwise,
    Clockwise
};

enum class PolygonMode {
    Fill,
    Line,
    Point
};

struct RasterizerDesc {
    CullMode cullMode{CullMode::Back};
    FrontFace frontFace{FrontFace::CounterClockwise};
    PolygonMode polygonMode{PolygonMode::Fill};
    bool depthBiasEnable{false};
    float depthBiasConstantFactor{0.0f};
    float depthBiasClamp{0.0f};
    float depthBiasSlopeFactor{0.0f};
};

struct BlendDesc {
    bool blendEnable{false};
};

enum class CompareOp {
    Never,
    Less,
    Equal,
    LessOrEqual,
    Greater,
    NotEqual,
    GreaterOrEqual,
    Always
};

struct DepthStencilDesc {
    bool depthTestEnable{true};
    bool depthWriteEnable{true};
    CompareOp depthCompareOp{CompareOp::Less};
};

enum class TextureFormat {
    RGBA8_UNORM,
    RGBA8_SRGB,
    D32_FLOAT,
    D24_UNORM_S8_UINT
};

struct TextureDesc {
    uint32_t width{1};
    uint32_t height{1};
    TextureFormat format{TextureFormat::RGBA8_UNORM};
    std::string debugName;
};

enum class FilterMode {
    Nearest,
    Linear
};

enum class AddressMode {
    Repeat,
    MirroredRepeat,
    ClampToEdge,
    ClampToBorder
};

enum class BorderColor {
    FloatOpaqueBlack,
    IntOpaqueBlack,
    FloatOpaqueWhite,
    IntOpaqueWhite,
    FloatTransparentBlack,
    IntTransparentBlack
};

struct SamplerDesc {
    FilterMode minFilter{FilterMode::Linear};
    FilterMode magFilter{FilterMode::Linear};
    AddressMode addressModeU{AddressMode::Repeat};
    AddressMode addressModeV{AddressMode::Repeat};
    AddressMode addressModeW{AddressMode::Repeat};
    BorderColor borderColor{BorderColor::FloatOpaqueBlack};
};

enum class DescriptorType {
    UniformBuffer,
    CombinedImageSampler
};

struct DescriptorBindingDesc {
    uint32_t binding{0};
    DescriptorType type{DescriptorType::UniformBuffer};
    ShaderStage stageFlags{ShaderStage::Vertex};
};

} // namespace Sunshine::RHI
