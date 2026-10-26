#ifndef RETAIL_BODY_SHAPE_MESH_H
#define RETAIL_BODY_SHAPE_MESH_H

#include <cstdint>

// Breast / rear mesh deformation for the audited female Retail meshes. The profile data (BodyShapeData/) comes
// from the WXL body-shape extension; it lives in one translation unit (Model2/BodyShapeMesh.cpp) because it is
// several megabytes of constants.
//
// Vertex arrays here hold 6 floats per vertex: bind-pose position, then normal.
namespace BodyShapeMesh {

// The profile whose bind-pose bounds and vertex mask match this model, or nullptr (males, unaudited meshes).
const void* FindProfile(const float* bind, uint32_t count);

const char* Name(const void* profile);

// Writes the deformed array into `out` (same layout and size as `bind`). Returns false if nothing moved.
bool Apply(const void* profile, const float* bind, uint32_t count, float breast, float butt, float* out);

} // namespace BodyShapeMesh

#endif
