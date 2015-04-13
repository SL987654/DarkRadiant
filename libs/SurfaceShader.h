#pragma once

#include <string>
#include "debugging/debugging.h"
#include "moduleobservers.h"
#include <boost/noncopyable.hpp>
#include "irender.h"
#include "shaderlib.h"

/**
 * Encapsulates a GL ShaderPtr and keeps track whether this
 * shader is actually in use in the map or not. The shader
 * is captured and released based on whether there is a
 * shadersystem reference available.
 */
class SurfaceShader :
    public ModuleObserver,
    public boost::noncopyable
{
public:
    // Observer classes can be attached to SurfaceShader to get notified
    // on realisation/unrealisation
    class Observer
    {
    public:
        virtual ~Observer() {}
        virtual void realiseShader() = 0;
        virtual void unrealiseShader() = 0;
    };

private:
    // greebo: The name of the material
    std::string _materialName;

    RenderSystemPtr _renderSystem;

    ShaderPtr _glShader;

    // In-use flag
    bool _inUse;

    bool _realised;

    typedef std::set<Observer*> Observers;
    Observers _observers;

public:
    // Constructor. The renderSystem reference will be kept internally as reference
    // The SurfaceShader will try to de-reference it when capturing shaders.
    SurfaceShader(const std::string& materialName, const RenderSystemPtr& renderSystem = RenderSystemPtr()) :
        _materialName(materialName),
        _renderSystem(renderSystem),
        _inUse(false),
        _realised(false)
    {
        captureShader();
    }

    // Destructor
    virtual ~SurfaceShader()
    {
        releaseShader();
    }

    /**
    * Indicates whether this Shader is actually in use in the scene or not.
    * The shader is not in use if the owning Patch resides on the UndoStack, forex.
    */
    void setInUse(bool isUsed)
    {
        _inUse = isUsed;

        if (!_glShader) return;

        // Update the shader's use count
        if (_inUse)
        {
            _glShader->incrementUsed();
        }
        else
        {
            _glShader->decrementUsed();
        }
    }

    /**
    * \brief
    * Get the material name.
    */
    const std::string& getMaterialName() const
    {
        return _materialName;
    }

    /**
    * \brief
    * Set the material name.
    */
    void setMaterialName(const std::string& name)
    {
        // return, if the shader is the same as the currently used
        if (shader_equal(_materialName, name)) return;

        releaseShader();

        _materialName = name;

        captureShader();
    }

    /**
    * \brief
    * Return the Shader for rendering.
    */
    const ShaderPtr& getGLShader() const
    {
        return _glShader;
    }

    // Return the dimensions of the editorimage of the contained material
    std::size_t getWidth() const
    {
        if (_realised)
        {
            return _glShader->getMaterial()->getEditorImage()->getWidth();
        }

        return 1;
    }

    std::size_t getHeight() const
    {
        if (_realised)
        {
            return _glShader->getMaterial()->getEditorImage()->getHeight();
        }

        return 1;
    }

    void setRenderSystem(const RenderSystemPtr& renderSystem)
    {
        _renderSystem = renderSystem;

        captureShader();
    }

    void attachObserver(Observer& observer)
    {
        // Insert the observer into our observer set
        std::pair<Observers::iterator, bool> result = _observers.insert(&observer);

        ASSERT_MESSAGE(result.second, "SurfaceShader::attachObserver(): Observer already attached.");

        if (_realised)
        {
            observer.realiseShader();
        }
    }

    void detachObserver(Observer& observer)
    {
        if (_realised)
        {
            observer.unrealiseShader();
        }

        ASSERT_MESSAGE(_observers.find(&observer) != _observers.end(), "SurfaceShader::detachObserver(): Cannot detach non-existing observer.");

        // Remove after unrealising
        _observers.erase(&observer);
    }

    // ModuleObserver methods
    void realise() override
    {
        assert(!_realised);
        _realised = true;

        for (auto i : _observers) i->realiseShader();
    }

    void unrealise() override
    {
        assert(_realised);

        for (auto i : _observers) i->unrealiseShader();

        _realised = false;
    }

private:
    // Shader capture and release
    void captureShader()
    {
        // Check if we have a rendersystem - can we capture already?
        if (_renderSystem)
        {
            releaseShader();

            _glShader = _renderSystem->capture(_materialName);
            assert(_glShader);

            _glShader->attach(*this);

            if (_inUse)
            {
                _glShader->incrementUsed();
            }
        }
        else
        {
            releaseShader();
        }
    }

    void releaseShader()
    {
        if (_glShader)
        {
            if (_inUse)
            {
                _glShader->decrementUsed();
            }

            _glShader->detach(*this);

            _glShader.reset();
        }
    }
};
