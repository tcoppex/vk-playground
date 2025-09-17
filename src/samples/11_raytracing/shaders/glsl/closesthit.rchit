#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_buffer_reference2 : require

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
// #extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

// -----------------------------------------------------------------------------

#include <material/interop.h>
#include "../interop.h" //

// -----------------------------------------------------------------------------

hitAttributeEXT vec2 attribs;

layout(location = 0) rayPayloadInEXT HitPayload_t payload;

layout(scalar, set = kDescriptorSet_Internal, binding = kDescriptorSetBinding_MaterialSBO)
buffer RayTracingMaterialSBO_ {
  RayTracingMaterial materials[];
};

layout(set = kDescriptorSet_Scene, binding = kDescriptorSet_Scene_Textures)
uniform sampler2D[] uTextureChannels;

#define TEXTURE_ATLAS(i)  uTextureChannels[nonuniformEXT(i)]

layout(push_constant, scalar) uniform PushConstant_ {
  PushConstant pushConstant;
};

// -----------------------------------------------------------------------------

layout(buffer_reference, scalar) buffer Vertices {
  Vertex v[];
};

layout(buffer_reference, scalar) buffer Indices {
  uint u32[]; // expect uint32 indices
};

// -----------------------------------------------------------------------------

struct ObjBuffers_t {
  uint64_t vertexAddr;
  uint64_t indexAddr;
};

layout(set = kDescriptorSet_RayTracing, binding = kDescriptorSet_RayTracing_InstanceSBO, scalar)
buffer _scene_desc {
  ObjBuffers_t addr[];
} ObjBuffers;

// -----------------------------------------------------------------------------

void buildTangentBasis(in vec3 N, out vec3 T, out vec3 B) {
  if (abs(N.z) < 0.999) {
    T = normalize(cross(N, vec3(0,0,1)));
  } else {
    T = normalize(cross(N, vec3(0,1,0)));
  }
  B = cross(N, T);
}

vec3 cosineSampleHemisphere(vec2 u) {
  float r = sqrt(u.x);
  float theta = 2.0 * 3.14159265 * u.y;
  return vec3(r * cos(theta), r * sin(theta), sqrt(max(0.0, 1.0 - u.x)));
}

float rand() {
  payload.rngState ^= (payload.rngState << 13);
  payload.rngState ^= (payload.rngState >> 17);
  payload.rngState ^= (payload.rngState << 5);
  return float(payload.rngState & 0x00FFFFFFu) / float(0x01000000u);
}

vec2 rand2() {
  return vec2(rand(), rand());
}

void russianRoulette() {
  ++payload.depth;
  if (payload.depth > 3) {
    float p = max(payload.throughput.r,
              max(payload.throughput.g, payload.throughput.b));
    p = clamp(p, 0.05, 0.95);
    if (rand() > p) {
      payload.done = 1;
      return;
    } else {
      payload.throughput /= p;
    }
  }
}

// -----------------------------------------------------------------------------

void main() {
  const uint object_id   = uint(gl_InstanceID); //
  const uint material_id = gl_InstanceCustomIndexEXT;

  // ----------------------------------------

  // GEOMETRY.

  ObjBuffers_t obj = ObjBuffers.addr[nonuniformEXT(object_id)];
  Vertices vertices = Vertices(obj.vertexAddr);
  Indices indices   = Indices(obj.indexAddr);

  uint base_index = gl_PrimitiveID * 3;

  uint i0 = indices.u32[base_index + 0];
  uint i1 = indices.u32[base_index + 1];
  uint i2 = indices.u32[base_index + 2];

  Vertex v0 = vertices.v[i0];
  Vertex v1 = vertices.v[i1];
  Vertex v2 = vertices.v[i2];

  // Barycentric coordinates of the triangle
  const vec3 barycentrics = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);

  // Hit Normal
  vec3 N = v0.normal.xyz * barycentrics.x
         + v1.normal.xyz * barycentrics.y
         + v2.normal.xyz * barycentrics.z;
  N = normalize(vec3(N.xyz * gl_WorldToObjectEXT));

  // Hit Position
  vec3 P = v0.position.xyz * barycentrics.x
         + v1.position.xyz * barycentrics.y
         + v2.position.xyz * barycentrics.z;
  P = vec3(gl_ObjectToWorldEXT * vec4(P, 1.0));

  vec2 uv = v0.texcoord * barycentrics.x
          + v1.texcoord * barycentrics.y
          + v2.texcoord * barycentrics.z;


  // ----------------------------------------

  // MATERIAL.

  const uint kInvalidIndexU24 = 0x00FFFFFF;
  uint material_type = kRayTracingMaterialType_Diffuse;
  vec3 emissive = vec3(0.0f);
  vec4 color = vec4(1.0f);
  float alpha_cutoff = 0.5f;

  if (material_id != kInvalidIndexU24)
  {
    RayTracingMaterial mat = materials[nonuniformEXT(material_id)];

    vec4 emissive_base = texture(TEXTURE_ATLAS(mat.emissive_texture_id), uv);
    emissive = mix(vec3(1.0f), emissive_base.rgb, emissive_base.a)
             * mat.emissive_factor
             ;

    color = texture(TEXTURE_ATLAS(mat.diffuse_texture_id), uv)
          * mat.diffuse_factor
          ;
    alpha_cutoff = mat.alpha_cutoff;

    const vec3 orm = texture(TEXTURE_ATLAS(mat.orm_texture_id), uv).xyz;
    const float roughness = max(orm.y, 1e-3f) * mat.roughness_factor;
    const float metallic = orm.z * mat.metallic_factor;

    if (any(greaterThan(emissive, vec3(1.e-2f))))
    {
      material_type = kRayTracingMaterialType_Emissive;
    }
    else if ((metallic > 0.0f) && (roughness < 0.05f))
    {
      material_type = kRayTracingMaterialType_Mirror;
    }
    else
    {
      material_type = kRayTracingMaterialType_Diffuse;
    }
  }

  // ----------------------------------------

  // SHADING.

  if (material_type == kRayTracingMaterialType_Diffuse) {
    if (color.a >= alpha_cutoff) {
      // Cosine-weighted hemisphere sampling
      vec3 tangent, bitangent;
      buildTangentBasis(N, tangent, bitangent);
      vec2 u = rand2();
      vec3 localDir = cosineSampleHemisphere(u);
      vec3 newDir = normalize(localDir.x * tangent +
                              localDir.y * bitangent +
                              localDir.z * N);

      payload.origin = P + N * 1e-3;
      payload.direction = newDir;
      payload.throughput *= color.rgb;
      russianRoulette();
    } else {
      // This create crack between alpha geometry and colliders.
      // We should probably separate the geometry for that reasons.
      float eps = 1e-5 * max(1.0f, length(P));
      payload.origin = P + payload.direction * eps;
      payload.done = -1;
    }

    return;
  }

  if (material_type == kRayTracingMaterialType_Mirror)
  {
    vec3 reflDir = reflect(normalize(gl_WorldRayDirectionEXT), N);
    payload.origin = P + N * 1e-3;
    payload.direction = reflDir;
    return;
  }

  if (material_type == kRayTracingMaterialType_Emissive)
  {
    payload.radiance += payload.throughput
                      * emissive * pushConstant.light_intensity
                      ;
    payload.done = 1;
    return;
  }
}

// -----------------------------------------------------------------------------
