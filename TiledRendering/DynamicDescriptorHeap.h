#pragma once
#include "DescriptorHeap.h"
#include "DX12RootSignature.h"
#include "GraphicsCore.h"
#include <vector>
#include <queue>

// This class is a linear allocation system for dynamically generated descriptor tables.  It internally caches
// CPU descriptor handles so that when not enough space is available in the current heap, necessary descriptors
// can be re-copied to the new heap.
class DynamicDescriptorHeap
{
public:
	DynamicDescriptorHeap();
};

