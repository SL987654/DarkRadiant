#pragma once

#include "itransformable.h"
#include "math/Matrix4.h"
#include "math/Quaternion.h"

const Vector3 c_translation_identity(0, 0, 0);
const Quaternion c_rotation_identity(Quaternion::Identity());
const Vector3 c_scale_identity(1, 1, 1);

/**
 * Base implementation of the ITransformable interface.
 */
class Transformable :
	public ITransformable
{
protected:
    // Flags to signal which type of transformation this is about
    enum TransformationType
    {
        NoTransform = 0,
        Translation = 1 << 0,
        Rotation    = 1 << 1,
        Scale       = 1 << 2,
    };

private:
	Vector3 _translation;
	Quaternion _rotation;
	Vector3 _scale;

	TransformModifierType _type;

    unsigned int _transformationType;

public:

	Transformable() :
		_translation(c_translation_identity),
		_rotation(Quaternion::Identity()),
		_scale(c_scale_identity),
		_type(TRANSFORM_PRIMITIVE),
        _transformationType(NoTransform)
	{}

	void setType(TransformModifierType type)
	{
		_type = type;
	}

	TransformModifierType getType() const
	{
		return _type;
	}

	void setTranslation(const Vector3& value)
	{
		_translation = value;
        _transformationType |= Translation;

		_onTransformationChanged();
	}

	void setRotation(const Quaternion& value)
	{
		_rotation = value;
        _transformationType |= Rotation;

		_onTransformationChanged();
	}

	void setScale(const Vector3& value)
	{
		_scale = value;
        _transformationType |= Scale;

		_onTransformationChanged();
	}

	void freezeTransform()
	{
		if (_translation != c_translation_identity ||
			_rotation != c_rotation_identity ||
			_scale != c_scale_identity)
		{
			_applyTransformation();

			_translation = c_translation_identity;
			_rotation = c_rotation_identity;
			_scale = c_scale_identity;
            _transformationType = NoTransform;

			_onTransformationChanged();
		}
	}

	/* greebo: This reverts the currently active transformation
	* by setting the scale/rotation/translation to identity.
	* It's enough to call _onTransformationChanged() as this
	* usually marks the node's geometry as "needs re-evaluation",
	* and during next rendering turn everything will be updated.
	*/
	void revertTransform()
	{
		_translation = c_translation_identity;
		_rotation = c_rotation_identity;
		_scale = c_scale_identity;
        _transformationType = NoTransform;

		_onTransformationChanged();
	}

	const Vector3& getTranslation() const
	{
		return _translation;
	}

	const Quaternion& getRotation() const
	{
		return _rotation;
	}

	const Vector3& getScale() const
	{
		return _scale;
	}

	Matrix4 calculateTransform() const
	{
		return getMatrixForComponents(getTranslation(), getRotation(), getScale());
	}

protected:
    /**
     * Returns a bitmask indicating which transformation classes we're dealing with
     * to allow subclasses to specialise their math on the requested transformation.
     * See TransformationType enum for bit values.
     */
    unsigned int getTransformationType() const
    {
        return _transformationType;
    }

	/**
	 * greebo: Signal method for subclasses. This gets called
	 * as soon as anything (translation, scale, rotation) is changed.
	 *
	 * To be implemented by subclasses
	 */
    virtual void _onTransformationChanged() = 0;

	/**
	 * greebo: Signal method to be implemented by subclasses.
	 * Is invoked whenever the transformation is reverted or frozen.
	 */
    virtual void _applyTransformation() = 0;

private:
	static Matrix4 getMatrixForComponents(const Vector3& translation, const Quaternion& rotation, const Vector3& scale)
	{
		Matrix4 result(Matrix4::getRotationQuantised(rotation));
		result.x().getVector3() *= scale.x();
		result.y().getVector3() *= scale.y();
		result.z().getVector3() *= scale.z();
		result.tx() = translation.x();
		result.ty() = translation.y();
		result.tz() = translation.z();
		return result;
	} 
};
