#pragma once
#include "stdafx.h"

/***********************************/
/***The base class of all objects***/
/***********************************/

class Object
{
public:
    virtual bool operator==(const Object& rhs) const;

protected:
    // Objects should not be created or destroyed unless explicitly stated
    // by overriding these methods
    Object();
    virtual ~Object();

    uint64_t m_objID;
    uint64_t GetObjectID() const;
private:
    // Objects should not be copied or assigned unless explicitly stated.
    Object(const Object& copy) = delete;
    Object(Object&& copy) = delete;
    Object& operator=(const Object& other) = delete;
};
