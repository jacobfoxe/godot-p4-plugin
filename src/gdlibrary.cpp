#include "p4_plugin.h"

#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/godot.hpp"
#include "godot_cpp/classes/global_constants.hpp"
using namespace godot;

void initialize_p4_plugin_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_EDITOR) {
		return;
	}

	ClassDB::register_class<P4Plugin>();
}

void uninitialize_p4_plugin_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_EDITOR) {
		return;
	}
}

extern "C" {

GDExtensionBool GDE_EXPORT p4_plugin_init(const GDExtensionInterfaceGetProcAddress p_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	GDExtensionBinding::InitObject init_obj(p_address, p_library, r_initialization);

	init_obj.register_initializer(initialize_p4_plugin_module);
	init_obj.register_terminator(uninitialize_p4_plugin_module);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_EDITOR);

	return init_obj.init();
}
}
