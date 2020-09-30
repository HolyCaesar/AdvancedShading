#include "Object.h"
#include "stdafx.h"

namespace ObjAux
{
    uint64_t g_objID = 0;
}

Object::Object()
{
    m_objID = ObjAux::g_objID;
    ObjAux::g_objID++;
}

Object::~Object()
{
}

bool Object::operator==(const Object& rhs) const
{
    return m_objID == rhs.m_objID;
}
