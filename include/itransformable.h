#pragma once

#include <memory>

enum TransformModifierType
{
	TRANSFORM_PRIMITIVE,
	TRANSFORM_COMPONENT,
};

// Forward-declare the math objects
template<typename Element> class BasicVector3;
typedef BasicVector3<double> Vector3;
template<typename Element> class BasicVector4;
typedef BasicVector4<double> Vector4;
class Quaternion;

class ITransformable
{
public:
    virtual ~ITransformable() {}
	virtual void setType(TransformModifierType type) = 0;
	virtual void setTranslation(const Vector3& value) = 0;
	virtual void setRotation(const Quaternion& value) = 0;
	virtual void setScale(const Vector3& value) = 0;
	virtual void freezeTransform() = 0;
	virtual void revertTransform() = 0;
};
typedef std::shared_ptr<ITransformable> ITransformablePtr;

namespace scene
{
	class INode;
	typedef std::shared_ptr<INode> INodePtr;
}

inline ITransformablePtr Node_getTransformable(const scene::INodePtr& node)
{
    return std::dynamic_pointer_cast<ITransformable>(node);
}
