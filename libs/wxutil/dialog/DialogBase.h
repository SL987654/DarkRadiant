#pragma once

#include <wx/dialog.h>
#include <wx/frame.h>
#include <wx/display.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include "imainframe.h"

namespace wxutil
{

/**
 * Base class for many DarkRadiant dialogs.
 * It comes with a panel/sizer combination pre-populated, use
 * the _mainPanel member to add more stuff.
 */
class DialogBase :
	public wxDialog
{
private:
	void _onDelete(wxCloseEvent& ev)
	{
		if (_onDeleteEvent())
		{
			ev.Veto();
		}
		else
		{
			EndModal(wxID_CANCEL);
		}
	}

public:
	DialogBase(const std::string& title, wxWindow* parent = NULL) :
		wxDialog(parent != NULL ? parent : GlobalMainFrame().getWxTopLevelWindow(), 
			wxID_ANY, title, wxDefaultPosition, wxDefaultSize, 
			wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
	{
		Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(DialogBase::_onDelete), NULL, this);
	}

	/**
	 * Adjust this window to fit the display DarkRadiant is currently (mainly) active on.
	 * Set the xProp and yProp factors to control how much space this window should use.
	 * The factors should be in the range (0.0..1.0], where 1.0 takes the entire space.
	 */
	void FitToScreen(float xProp, float yProp)
	{
		int curDisplayIdx = 0;

		if (GlobalMainFrame().getWxTopLevelWindow() != NULL)
		{
			curDisplayIdx = wxDisplay::GetFromWindow(GlobalMainFrame().getWxTopLevelWindow());
		}

		wxDisplay curDisplay(curDisplayIdx);

		wxRect rect = curDisplay.GetGeometry();
		int newWidth = static_cast<int>(rect.GetWidth() * xProp);
		int newHeight = static_cast<int>(rect.GetHeight() * yProp);

		SetSize(newWidth, newHeight);
		CenterOnScreen();
	}

protected:
	// Overrideable: return true to prevent the window from being deleted
	virtual bool _onDeleteEvent()
	{
		return false;
	}
};

} // namespace
