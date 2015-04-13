#pragma once

#include "i18n.h"
#include "imousetool.h"
#include "math/Vector3.h"
#include "iorthoview.h"
#include "camera/GlobalCamera.h"

namespace ui
{

/**
* The CameraAngleTool re-orients the camera such that 
* the clicked location is in its view.
*/
class CameraAngleTool :
    public MouseTool
{
public:
    const std::string& getName()
    {
        static std::string name("CameraAngleTool");
        return name;
    }

    const std::string& getDisplayName()
    {
        static std::string displayName(_("Point Camera"));
        return displayName;
    }

    Result onMouseDown(Event& ev)
    {
        try
        {
            // Set the camera angle
            orientCamera(dynamic_cast<XYMouseToolEvent&>(ev));

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
            orientCamera(dynamic_cast<XYMouseToolEvent&>(ev));

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

private:
    void orientCamera(XYMouseToolEvent& xyEvent)
    {
        CamWndPtr cam = GlobalCamera().getActiveCamWnd();

        if (!cam)
        {
            return;
        }

        Vector3 point = xyEvent.getWorldPos();
        xyEvent.getView().snapToGrid(point);

        point -= cam->getCameraOrigin();

        int n1 = (xyEvent.getViewType() == XY) ? 1 : 2;
        int n2 = (xyEvent.getViewType() == YZ) ? 1 : 0;

        int nAngle = (xyEvent.getViewType() == XY) ? CAMERA_YAW : CAMERA_PITCH;

        if (point[n1] || point[n2])
        {
            Vector3 angles(cam->getCameraAngles());

            angles[nAngle] = static_cast<float>(radians_to_degrees(atan2(point[n1], point[n2])));

            cam->setCameraAngles(angles);
        }

        xyEvent.getView().queueDraw();
    }
};

} // namespace
