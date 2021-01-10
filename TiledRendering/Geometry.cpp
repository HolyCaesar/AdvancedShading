#include "Geometry.h"

Square::Square()
{
	m_name = "Square";
	m_fEdgeLen = 1.0f;
	GenerateVertices();
}

Square::~Square()
{

}

void Square::GenerateVertices()
{
	m_vertices.clear();
	m_indices.clear();
	m_texCoords.clear();

	XMFLOAT3 center(0.0f, 0.0f, 0.0f);
	float halfEdge = m_fEdgeLen * 0.5f;

	XMFLOAT3 upperLeft(center.x - halfEdge, center.y + halfEdge, center.z);
	XMFLOAT3 bottomLeft(center.x - halfEdge, center.y - halfEdge, center.z);
	XMFLOAT3 upperRight(center.x + halfEdge, center.y + halfEdge, center.z);
	XMFLOAT3 bottomRight(center.x + halfEdge, center.y - halfEdge, center.z);

	// Add vertices and add texCoords
	m_vertices.emplace_back(upperLeft);
	m_texCoords.emplace_back(XMFLOAT2(0.0f, 0.0f));

	m_vertices.emplace_back(upperRight);
	m_texCoords.emplace_back(XMFLOAT2(1.0f, 0.0f));
	
	m_vertices.emplace_back(bottomRight);
	m_texCoords.emplace_back(XMFLOAT2(1.0f, 1.0f));

	m_vertices.emplace_back(bottomLeft);
	m_texCoords.emplace_back(XMFLOAT2(0.0f, 1.0f));

	// Add indices
	m_indices.push_back(0);
	m_indices.push_back(1);
	m_indices.push_back(2);

	m_indices.push_back(0);
	m_indices.push_back(2);
	m_indices.push_back(3);
}
