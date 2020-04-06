#pragma once

#include "../TiledRendering/VectorMath.h"
#include <string>
#include <vector>
#include <memory>

using namespace std;
using namespace IMath;

class Model
{
public:
	Model();
	~Model();

	void Destroy();

	struct Vertex
	{
		XMFLOAT3 position;
		XMFLOAT3 normal;
		XMFLOAT2 uv;
	};

	struct BoundingBox
	{
		Vector3 minPt;
		Vector3 maxPt;
	};

	struct Header
	{
		uint32_t meshCount;
		uint32_t materialCount;
		uint32_t vertexDataByteSize;
		uint32_t indexDataByteSize;
		BoundingBox boundingBox;
	};
	Header m_Header;

	struct Mesh
	{
		BoundingBox boundingBox;

		unsigned int materialIndex;
		
		unsigned int vertexDataByteOffset;
		unsigned int vertexCount;
		unsigned int indexDataByteOffset;
		unsigned int indexCount;

		unsigned int vertexStride;
	};
	vector<Mesh> m_vMeshes;

	struct Material
	{
		Vector3 diffuse;
		Vector3 specular;
		Vector3 ambient;
		Vector3 emissive;
		Vector3 transparent;  // light passing through a transparent surface is multiplied by this filter color

		float opacity;
		// Specular exponent
		float shininess; 
		// Specular power
		float specularStrength;

		enum { maxMaterialName = 128 };
		char name[maxMaterialName];
	};
	vector<Material> m_vMaterials;

	//unsigned char* m_pVertexData;
	//unsigned char* m_pIndexData;

	vector<Vertex> m_vecVertexData;
	vector<uint32_t> m_vecIndexData;

	virtual bool Load(const std::string filename)
	{
		std::string suffix = filename.substr(filename.find('.'));
		if (suffix == ".obj") return LoadObjModel(filename);
		return false;
	}

	const BoundingBox& getBoundingBox() const
	{
		return m_Header.boundingBox;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GetSRV(uint32_t materialIdx) const
	{
		return m_SRVs[materialIdx];
	}

protected:
	bool LoadObjModel(const std::string filename);
	bool SaveObjModel(const std::string filename) const {}

	void ComputeMeshBoundingBox(unsigned int meshIndex, BoundingBox& bbox) const;
	void ComputeGlobalBoundingBox(BoundingBox& bbox) const;
	void ComputeAllBoundingBoxes();

	void ReleaseTextures() {}
	void LoadTextures() {}
	vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_SRVs;
};

