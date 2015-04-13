#pragma once

#include <string>
#include <memory>
#include <list>
#include "imousetoolevent.h"

namespace ui
{

/** 
 * A Tool class represents an operator which can be "used" 
 * in DarkRadiant's Ortho- and Camera views by using the mouse.
 */
class MouseTool
{
public:
    typedef MouseToolEvent Event;

    enum class Result
    {
        Ignored,    // event does not apply for this tool
        Activated,  // event handled, tool is now active
        Continued,  // event handled, tool continues to be active
        Finished,   // event handled, tool no longer active
    };

    // Returns the name of this operation. This name is only used
    // internally and should be unique.
    virtual const std::string& getName() = 0;

    // The display name, which is also used in the status bar
    virtual const std::string& getDisplayName() = 0;

    virtual Result onMouseDown(Event& ev) = 0;
    virtual Result onMouseMove(Event& ev) = 0;
    virtual Result onMouseUp(Event& ev) = 0;

    // During an active operation the user may hit ESC,
    // in which case the cancel event will be fired.
    // This should not be ignored by the tool, which should
    // seek to shut down any ongoing operation safely.
    virtual void onCancel()
    {}

    // A tool using pointer mode Capture might want to get notified
    // when the mouse capture of the window has been lost due to 
    // the user alt-tabbing out of the app or something else.
    virtual void onMouseCaptureLost()
    {}

    // Some tools might want to receive mouseMove events even when they
    // are not active, to send feedback to the user before the buttons
    // are pressed. The Clipper tool uses this to change the mouse cursor
    // to a crosshair when moved over a manipulatable clip point.
    virtual bool alwaysReceivesMoveEvents()
    {
        return false;
    }

    // By default, when the user is dragging the mouse to the borders of
    // the view, the viewport will be moved along. For some tools this might
    // not be desirable, in which case they need to override this method to
    // return false.
    virtual bool allowChaseMouse()
    {
        return true;
    }

    // A tool can request special mouse capture modes during its active phase
    // All flags can be combined, use Normal to indicate that no capturing is needed.
    struct PointerMode
    {
        enum Flags
        {
            Normal       = 0,   // no capturing, absolute coordinates are sent to onMouseMove, pointer will be shown
            Capture      = 1,   // capture mouse (tools should implement onMouseCaptureLost), see also MotionDeltas
            Freeze       = 2,   // pointer will be frozen and kept at the same position
            Hidden       = 4,   // pointer will be hidden
            MotionDeltas = 8,   // onMouseMove will receive delta values relative to the capture position
        };
    };

    // Some tools might want to capture the pointer after onMouseDown
    // Override this method to return "Capture" instead of "Normal".
    virtual unsigned int getPointerMode()
    {
        return PointerMode::Normal;
    }

    // Optional render routine that is invoked after the scene
    // has been drawn on the interactive window. The projection
    // and modelview matrix have already been set up for  
    // overlay rendering (glOrtho).
    virtual void renderOverlay()
    {}
};
typedef std::shared_ptr<MouseTool> MouseToolPtr;

} // namespace
