#pragma once

#include "icommandsystem.h"
#include "imainframelayout.h"

namespace ui 
{

class FloatingCamWnd;
typedef std::shared_ptr<FloatingCamWnd> FloatingCamWndPtr;

#define FLOATING_LAYOUT_NAME "Floating"

class FloatingLayout;
typedef std::shared_ptr<FloatingLayout> FloatingLayoutPtr;

class FloatingLayout :
	public IMainFrameLayout
{
	// The floating camera window
	FloatingCamWndPtr _floatingCamWnd;

public:
	// IMainFrameLayout implementation
	virtual std::string getName();
	virtual void activate();
	virtual void deactivate();
	virtual void toggleFullscreenCameraView();

	// The creation function, needed by the mainframe layout manager
	static FloatingLayoutPtr CreateInstance();
};

} // namespace ui
