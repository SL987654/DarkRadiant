#pragma once

#include <functional>

#include "event/SingleIdleCallback.h"

namespace wxutil
{

/**
 * A class accumulating mouse motion delta calls. When GTK is idle, 
 * the attached function object is called with the stored x,y delta values.
 */
class DeferredMotionDelta :
	private wxutil::SingleIdleCallback
{
public:
	typedef std::function<void(int, int)> MotionDeltaFunction;

private:
	int _deltaX;
	int _deltaY;

	MotionDeltaFunction _function;

public:
	DeferredMotionDelta(const MotionDeltaFunction& function) : 
		_deltaX(0), 
		_deltaY(0),
		_function(function)
	{}

	void flush()
	{
		flushIdleCallback();
	}

	void onMouseMotionDelta(int x, int y, unsigned int state)
	{
		_deltaX += x;
		_deltaY += y;

		requestIdleCallback();
	}

private:
	void onIdle()
	{
		_function(_deltaX, _deltaY);

		_deltaX = 0;
		_deltaY = 0;
	}
};

} // namespace
