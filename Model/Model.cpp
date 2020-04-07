
#include "../TiledRendering/stdafx.h"
#include "Model.h"
#include "OBJ_Loader.h"
#include <algorithm>
#include <limits>



Model::Model()
{
	Destroy();
}

Model::~Model()
{
	Destroy();
}

void Model::Destroy()
{
	m_vMeshes.clear();
	m_Header.meshCount = 0;

	m_vMaterials.clear();
	m_Header.materialCount = 0;

	m_Header.meshCount = 0;
	m_Header.materialCount = 0;
	m_Header.vertexDataByteSize = 0;
	m_Header.indexDataByteSize = 0;
	m_Header.boundingBox.minPt = Scalar(0);
	m_Header.boundingBox.maxPt = Scalar(0);

	ReleaseTextures();

	m_vecVertexData.clear();
	m_vecIndexData.clear();
}

void Model::ComputeMeshBoundingBox(unsigned int meshIndex, BoundingBox& bbox) const
{
	Mesh meshInfo = m_vMeshes[meshIndex];

	if (meshInfo.vertexCount > 0)
	{
		unsigned int vertexStride = meshInfo.vertexStride;

		bbox.minPt = Scalar((numeric_limits<float>::max)());
		bbox.maxPt = Scalar((numeric_limits<float>::min)());

		// TODO
		int start(0), end(0);
		for (unsigned int i = 0; i < meshIndex; i++) start += m_vMeshes[i].vertexDataByteOffset / m_vMeshes[i].vertexStride;
		end = start + meshInfo.vertexDataByteOffset / meshInfo.vertexStride;

		while (start < end)
		{
			Vertex bv = m_vecVertexData[start];
			Vector3 tmpPt(bv.position.x, bv.position.y, bv.position.z);

			bbox.minPt = Min(bbox.minPt, tmpPt);
			bbox.maxPt = Max(bbox.maxPt, tmpPt);
			++start;
		}
	}
}

void Model::ComputeGlobalBoundingBox(BoundingBox& bbox) const
{
	// TODO;
	for (unsigned int i = 0; i < m_Header.meshCount; i++)
	{
		bbox.minPt = Scalar(FLT_MAX);
		bbox.maxPt = Scalar(-FLT_MAX);
		for (unsigned int meshIndex = 0; meshIndex < m_Header.meshCount; meshIndex++)
		{
			Mesh meshInfo = m_vMeshes[meshIndex];

			bbox.minPt = Min(bbox.minPt, meshInfo.boundingBox.minPt);
			bbox.maxPt = Max(bbox.maxPt, meshInfo.boundingBox.maxPt);
		}
	}
}

void Model::ComputeAllBoundingBoxes()
{
	//TODO
}

bool Model::LoadObjModel(const string model_file_path)
{
	// Reset m_Header
	m_Header.meshCount = 0;
	m_Header.materialCount = 0;
	m_Header.vertexDataByteSize = 0;
	m_Header.indexDataByteSize = 0;
	m_Header.boundingBox.minPt = Scalar(0);
	m_Header.boundingBox.maxPt = Scalar(0);

	objl::Loader m_loader;
	bool loadRes = m_loader.LoadFile(model_file_path);
	if (!loadRes)
	{
		cerr << "Failed to load the Obj file from " << model_file_path << endl;
		cerr << __FILE__ << " at line " << __LINE__ << endl;
		return false;
	}

	XMFLOAT3 AABBMin = XMFLOAT3((numeric_limits<float>::max)(), (numeric_limits<float>::max)(), (numeric_limits<float>::max)());
	XMFLOAT3 AABBMax = XMFLOAT3((numeric_limits<float>::min)(), (numeric_limits<float>::min)(), (numeric_limits<float>::min)());
	vector<Vertex> vertices;
	vector<uint32_t> indices;

	for (int i = 0; i < m_loader.LoadedMeshes.size(); i++)
	{
		objl::Mesh curMesh = m_loader.LoadedMeshes[i];
		Mesh meshInfo;
		meshInfo.boundingBox.minPt = Scalar((numeric_limits<float>::max)());
		meshInfo.boundingBox.maxPt = Scalar((numeric_limits<float>::min)());

		// Load Vertex
		for (auto vex : curMesh.Vertices)
		{
			Vertex bv;
			bv.position = XMFLOAT3(vex.Position.X, vex.Position.Y, vex.Position.Z);
			bv.normal = XMFLOAT3(vex.Normal.X, vex.Normal.Y, vex.Normal.Z);
			bv.uv = XMFLOAT2(vex.TextureCoordinate.X, vex.TextureCoordinate.Y);
			vertices.push_back(bv);
			AABBMin.x = min(AABBMin.x, bv.position.x);
			AABBMin.y = min(AABBMin.y, bv.position.y);
			AABBMin.z = min(AABBMin.z, bv.position.z);
			AABBMax.x = max(AABBMax.x, bv.position.x);
			AABBMax.y = max(AABBMax.y, bv.position.y);
			AABBMax.z = max(AABBMax.z, bv.position.z);
		}

		float cX = (AABBMax.x + AABBMin.x) / 2.0f;
		float cY = (AABBMax.y + AABBMin.y) / 2.0f;
		float cZ = (AABBMax.z + AABBMin.z) / 2.0f;
		// Move Object Bounding Box Center to the world coordinate origin 
		for (auto& v : vertices)
		{
			v.position.x -= cX;
			v.position.y -= cY;
			v.position.z -= cZ;
		}
		AABBMax.x -= cX, AABBMax.y -= cY, AABBMax.z -= cZ;
		AABBMin.x -= cX, AABBMin.y -= cY, AABBMin.z -= cZ;

		// Load Indices
		for (int i = 0; i < curMesh.Indices.size(); i += 3)
		{
			indices.push_back(curMesh.Indices[i]);
			indices.push_back(curMesh.Indices[i + 1]);
			indices.push_back(curMesh.Indices[i + 2]);
		}

		meshInfo.boundingBox.minPt.SetX(AABBMin.x);
		meshInfo.boundingBox.minPt.SetY(AABBMin.y);
		meshInfo.boundingBox.minPt.SetZ(AABBMin.z);
		meshInfo.boundingBox.maxPt.SetX(AABBMax.x);
		meshInfo.boundingBox.maxPt.SetY(AABBMax.y);
		meshInfo.boundingBox.maxPt.SetZ(AABBMax.z);
		
		// meshInfo.materialIndex = ??;
		meshInfo.vertexDataByteOffset = sizeof(Vertex) * vertices.size();
		meshInfo.vertexCount = vertices.size();
		meshInfo.indexDataByteOffset = sizeof(uint32_t) * indices.size();
		meshInfo.indexCount = indices.size();
		meshInfo.vertexStride = sizeof(Vertex);

		m_vMeshes.push_back(meshInfo);


		// TODO Load material
	}

	// Copied
	m_vecVertexData = move(vertices);
	m_vecIndexData = move(indices);

	m_Header.meshCount = m_vMeshes.size();
	m_Header.materialCount = m_vMaterials.size();
	m_Header.vertexDataByteSize = m_vecVertexData.size() * sizeof(Vertex);
	m_Header.indexDataByteSize = m_vecIndexData.size() * sizeof(uint32_t);
	BoundingBox bbox;
	ComputeGlobalBoundingBox(bbox);
	m_Header.boundingBox.minPt = bbox.minPt;
	m_Header.boundingBox.maxPt = bbox.maxPt;

	return true;
}
