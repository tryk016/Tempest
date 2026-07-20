#include <metal_stdlib>

using namespace metal;

struct TempestOfflineVertexIn {
  float3 position [[attribute(0)]];
  float2 uv       [[attribute(1)]];
  float4 color    [[attribute(2)]];
};

struct TempestOfflineColorVertexOut {
  float4 position [[position]];
  float4 color    [[user(locn0)]];
};

struct TempestOfflineTextureVertexOut {
  float4 position [[position]];
  float4 color    [[user(locn0)]];
  float2 uv       [[user(locn1)]];
};

vertex TempestOfflineColorVertexOut tempestOfflineColorVertex(
    TempestOfflineVertexIn in [[stage_in]]) {
  TempestOfflineColorVertexOut out;
  out.position = float4(in.position,1.0f);
  out.position.y = -out.position.y;
  out.color = in.color;
  return out;
}

fragment float4 tempestOfflineColorFragment(
    TempestOfflineColorVertexOut in [[stage_in]]) {
  return in.color;
}

vertex TempestOfflineTextureVertexOut tempestOfflineTextureVertex(
    TempestOfflineVertexIn in [[stage_in]]) {
  TempestOfflineTextureVertexOut out;
  out.position = float4(in.position,1.0f);
  out.position.y = -out.position.y;
  out.color = in.color;
  out.uv = in.uv;
  return out;
}

fragment float4 tempestOfflineTextureFragment(
    TempestOfflineTextureVertexOut in [[stage_in]],
    texture2d<float,access::sample> texture [[texture(0)]],
    sampler textureSampler [[sampler(0)]]) {
  return in.color*texture.sample(textureSampler,in.uv);
}

vertex TempestOfflineColorVertexOut tempestOfflineWrongFragmentStage(
    TempestOfflineVertexIn in [[stage_in]]) {
  TempestOfflineColorVertexOut out;
  out.position = float4(in.position,1.0f);
  out.color = in.color;
  return out;
}
