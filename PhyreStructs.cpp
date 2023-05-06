#include "PhyreStructs.h"
#include <iomanip>
#include <sstream>

Phyre::Mesh::Mesh() :
    _components{
    &pos_meta,
    &norm_meta,
    &uv_meta,
    &tang_meta,
    &bitang_meta,
    &color_meta,
    &weight_meta,
    &boneID_meta
} {}

Phyre::Mesh::VertexComponentMetadata& Phyre::Mesh::component(size_t index)
{
	return *_components[index];
}

const Phyre::Mesh::VertexComponentMetadata& Phyre::Mesh::component(size_t index) const
{
	return *_components[index];
}
