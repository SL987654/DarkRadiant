#pragma once

#include "gtkutil/PanedPosition.h"
#include "imainframelayout.h"

class CamWnd;
typedef boost::shared_ptr<CamWnd> CamWndPtr;

namespace ui
{

#define REGULAR_LAYOUT_NAME "Regular"
#define REGULAR_LEFT_LAYOUT_NAME "RegularLeft"

class RegularLayout;
typedef boost::shared_ptr<RegularLayout> RegularLayoutPtr;

class RegularLayout :
	public IMainFrameLayout
{
	// The camera view
	CamWndPtr _camWnd;

	struct RegularView
	{
		wxSplitterWindow* horizPane;
		wxSplitterWindow* texCamPane;

		wxutil::PanedPosition posHPane;
		wxutil::PanedPosition posTexCamPane;
	} _regular;

	// Whether the cam is left or right
	bool _regularLeft;

	// Pass the exact type (left/right) to the constructor
	RegularLayout(bool regularLeft);

public:
	// IMainFrameLayout implementation
	virtual std::string getName();
	virtual void activate();
	virtual void deactivate();
	virtual void toggleFullscreenCameraView();

	// The creation function, needed by the mainframe layout manager
	static RegularLayoutPtr CreateRegularLeftInstance();
	static RegularLayoutPtr CreateRegularInstance();

private:
	void maximiseCameraSize();
	void restorePanePositions();

	// Saves the state of this window layout to the given XMLRegistry path (without trailing slash)
	void restoreStateFromPath(const std::string& path);
	void saveStateToPath(const std::string& path);
};

} // namespace ui
