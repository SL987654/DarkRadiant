/*
Copyright (C) 2001-2006, William Joseph.
All Rights Reserved.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "plugin.h"

#include "EntityCreator.h"
#include "itextstream.h"

extern "C" void DARKRADIANT_DLLEXPORT RegisterModule(IModuleRegistry& registry) {
	registry.registerModule(entity::Doom3EntityCreatorPtr(
			new entity::Doom3EntityCreator
	));
	
	// Initialise the streams
	const ApplicationContext& ctx = registry.getApplicationContext();
	GlobalOutputStream::instance().setOutputStream(ctx.getOutputStream());
	GlobalErrorStream::instance().setOutputStream(ctx.getOutputStream());
	
	// Remember the reference to the ModuleRegistry
	module::RegistryReference::Instance().setRegistry(registry);
}
