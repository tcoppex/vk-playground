#version 460

// ----------------------------------------------------------------------------

out gl_PerVertex {
  vec4 gl_Position;
};

out layout(location = 0) vec2 vTexCoord;

// ----------------------------------------------------------------------------

// Render a NDC-mapped quad using a generated triangle with texcoords mapping the screen
// from 3 inputs vertices.
void main() {
  vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
  vTexCoord = vec2(uv.x, 1.0 - uv.y);
  gl_Position = vec4(2.0 * uv - 1.0f, 0.0f, 1.0f);
}

// ----------------------------------------------------------------------------
