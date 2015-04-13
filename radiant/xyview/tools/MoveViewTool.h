#pragma once

#include "i18n.h"
#include "imousetool.h"
#include "iorthoview.h"

namespace ui
{

/**
* The MoveViewTool implements the drag view functionality used in
* the orthoviews. The mouse pointer is captured during its active
* phase and only the move deltas are processed to translate the view origin.
*/
class MoveViewTool :
    public MouseTool
{
public:
    const std::string& getName()
    {
        static std::string name("MoveViewTool");
        return name;
    }

    const std::string& getDisplayName()
    {
        static std::string displayName(_("Drag View"));
        return displayName;
    }

    unsigned int getPointerMode()
    {
        return PointerMode::Capture | PointerMode::Freeze | 
            PointerMode::Hidden | PointerMode::MotionDeltas;
    }

    Result onMouseDown(Event& ev)
    {
        try
        {
            dynamic_cast<XYMouseToolEvent&>(ev);
            return Result::Activated;
        }
        catch (std::bad_cast&)
        {
        }

        return Result::Ignored; // not handled
    }

    Result onMouseMove(Event& ev)
    {
        try
        {
            // We use capture mode, so xy event will contain the delta only
            XYMouseToolEvent& xyEvent = dynamic_cast<XYMouseToolEvent&>(ev);

            // Scroll the view
            xyEvent.getView().scroll(-xyEvent.getDeviceDelta().x(), xyEvent.getDeviceDelta().y());

            return Result::Continued;
        }
        catch (std::bad_cast&)
        {
        }

        return Result::Ignored;
    }

    Result onMouseUp(Event& ev)
    {
        try
        {
            // We only operate on XY view events, so attempt to cast
            dynamic_cast<XYMouseToolEvent&>(ev);
            return Result::Finished;
        }
        catch (std::bad_cast&)
        {
        }

        return Result::Ignored;
    }
};

}
