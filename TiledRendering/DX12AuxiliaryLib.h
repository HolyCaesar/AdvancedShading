#pragma once
#include "stdafx.h"

#include <map>
#include <vector>

namespace DX12Aux
{
	using namespace std;
	using namespace Microsoft::WRL;

	typedef map<string, string > ShaderMacros;

	enum class ShaderType
	{
		UnknownShaderType = 0,
		VertexShader,
		TessellationControlShader,      // Hull Shader in DirectX
		TessellationEvaluationShader,   // Domain Shader in DirectX
		GeometryShader,
		PixelShader,
		ComputeShader
	};

	inline ComPtr<ID3DBlob> LoadShaderFromFile(
		ShaderType type,
		string shaderName,
		string shaderEntryPoint,
		wstring shaderPath,
		const ShaderMacros& shaderMacros,
		std::string shaderHlslVersion)
	{
		HRESULT hr = S_OK;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

		// Load shader macros
		vector<D3D_SHADER_MACRO> vShaderMacro;
		for (auto& iter : shaderMacros)
		{
			string name = iter.first, definition = iter.second;
			char* c_name = new char[name.size() + 1];
			char* c_definition = new char[definition.size() + 1];
			strncpy_s(c_name, name.size() + 1, name.c_str(), name.size());
			strncpy_s(c_definition, definition.size() + 1, definition.c_str(), definition.size());
			vShaderMacro.push_back({ c_name, c_definition });
		}
		vShaderMacro.push_back({ 0, 0 });

		// Load shader
		ComPtr<ID3DBlob> shader;
		ComPtr<ID3DBlob> errorMessages;
#if defined(_DEBUG)
		hr = D3DCompileFromFile(shaderPath.c_str(), vShaderMacro.size() ? vShaderMacro.data() : nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, shaderEntryPoint.c_str(), shaderHlslVersion.c_str(), compileFlags, 0, &shader, &errorMessages);
		if (FAILED(hr) && errorMessages)
		{
			const char* errorMsg = (const char*)errorMessages->GetBufferPointer();
			wstring str;
			for (int i = 0; i < 500; i++) str += errorMsg[i];
			MessageBox(nullptr, str.c_str(), L"Shader Compilation Error", MB_RETRYCANCEL);
			return nullptr;
		}
#else
		ThrowIfFailed(D3DCompileFromFile(shaderPath.c_str(), vShaderMacro.size() ? vShaderMacro.data() : nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, shaderEntryPoint.c_str(), shaderHlslVersion.c_str(), compileFlags, 0, &shader, &errorMessages));
#endif

		return shader;
	}

	inline ComPtr<ID3DBlob> LoadShaderFromString(
		ShaderType type,
		std::string shaderName,
		std::string shaderEntryPoint,
		const std::string& source,
		const ShaderMacros& shaderMacros,
		std::string shaderHlslVersion)
	{
		// TODO
		ComPtr<ID3DBlob> shader;
		ComPtr<ID3DBlob> errorMessages;

		return shader;
	}
}
