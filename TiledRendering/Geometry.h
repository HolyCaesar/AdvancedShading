#pragma once

#include "stdafx.h"
#include "Object.h"

using namespace DirectX;

class Geometry : public Object
{
public:
	Geometry() : m_name("Geometry") {}
	virtual ~Geometry() {}

	std::vector<XMFLOAT3>& GetVertices() { return m_vertices; }
	std::vector<UINT>& GetIndices() { return m_indices; }
	std::vector<XMFLOAT2>& GetTexCoords() { return m_texCoords; }

protected:
	std::string m_name;

	std::vector<XMFLOAT3>	m_vertices;
	std::vector<UINT>				m_indices;
	std::vector<XMFLOAT2>	m_texCoords;

	virtual void GenerateVertices() = 0;
};

class Square : public Geometry
{
public:
	Square();
	virtual ~Square();

	void SetLength(float len)
	{
		m_fEdgeLen = len;
		GenerateVertices();
	}
	float GetLength() { return m_fEdgeLen; }
	
private:
	float m_fEdgeLen;

	// Generate a square at the center of the screen
	void GenerateVertices();
};

