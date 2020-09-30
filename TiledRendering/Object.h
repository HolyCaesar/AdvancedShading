#pragma once

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

    uint64_t GetObjectID() const { return m_objID; }
private:
    // Objects should not be copied or assigned unless explicitly stated.
    Object(const Object& copy);
    Object(Object&& copy);
    Object& operator=(const Object& other);

};
