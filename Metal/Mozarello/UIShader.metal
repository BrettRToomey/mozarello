#include <metal_stdlib>
#include <metal_pack>
#include <simd/simd.h>

using namespace metal;

typedef struct {
    packed_float2 position;
    unsigned color;
} Vertex;

enum QuadKind {
    QuadKind_Normal,
    QuadKind_Dashed,
    QuadKind_Circle
};

typedef struct {
    float4 clipSpacePosition [[position]];
    float4 color;
    unsigned char isDashed;
    unsigned char isCircle;
} RasterizerData;

vertex RasterizerData vertexShader(
    uint vertexID [[vertex_id]],
    constant Vertex *vertices [[buffer(0)]],
    constant float4x2 *m [[buffer(1)]],
    constant QuadKind *kind [[buffer(2)]]
) {
    RasterizerData out;
    out.clipSpacePosition = vector_float4(0.0, 0.0, 0.0, 1.0);
    float4 p1 = float4(vertices[vertexID].position, 1.0, 1.0);
    out.clipSpacePosition.xy = (*m) * p1;
    out.color = unpack_unorm4x8_to_float(vertices[vertexID].color);
    out.isDashed = *kind == QuadKind_Dashed;
    out.isCircle = *kind == QuadKind_Circle;

    return out;
}

fragment float4 fragmentShader(RasterizerData in [[stage_in]]) {
    if (in.isDashed) {
        if (step(sin(in.clipSpacePosition.y*100), 0.5)) {
            discard_fragment();
        }
    } else if (in.isCircle) {
        // TODO: use radius and discard fragments outside
    }

    return in.color;
}

typedef struct {
    packed_float2 position;
    packed_float2 uv;
} TexturedVertex;

typedef struct {
    float4 clipSpacePosition [[position]];
    float4 color;
    float2 uv;
} TexturedRasterizerData;

vertex TexturedRasterizerData texturedVertShader(
    uint vertexID [[vertex_id]],
    constant TexturedVertex *vertices [[buffer(0)]],
    constant float4x2 *m [[buffer(1)]]
) {
    TexturedRasterizerData out;
    out.clipSpacePosition = vector_float4(0.0, 0.0, 0.0, 1.0);

    float4 p1 = float4(vertices[vertexID].position, 1.0, 1.0);
    out.clipSpacePosition.xy = (*m) * p1;
    out.uv = vertices[vertexID].uv;

    return out;
}

fragment float4 texturedFragmentShader(
    TexturedRasterizerData in [[stage_in]],
    texture2d<float> texture [[texture(0)]],
    constant unsigned *color [[buffer(0)]]
) {
    constexpr sampler texSampler (
        mag_filter::nearest,
        min_filter::nearest
    );
    float sample = texture.sample(texSampler, in.uv).r;
    return float4(unpack_unorm4x8_to_float(*color).rgb, sample*2);
}
