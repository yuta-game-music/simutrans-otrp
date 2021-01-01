/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "script_generator_frame.h"

#include "../script/script_tool_manager.h"
#include "../dataobj/environment.h"
#include "../dataobj/tabfile.h"
#include "../dataobj/translator.h"
#include "../simdebug.h"
#include "../simworld.h"
#include "../simtool-script-generator.h"
#include "../sys/simsys.h"
#include "../utils/cbuffer_t.h"
#include "../utils/simstring.h"


script_generator_frame_t::script_generator_frame_t(tool_generate_script_t* tl) : savegame_frame_t(NULL, true, NULL, false)
{
	this->tool = tl;
	cbuffer_t dir_str;
	dir_str.printf("%sgenerated-scripts/", env_t::program_dir);
	this->add_path(dir_str);

	set_name(translator::translate("Save generated script"));
	set_focus(NULL);
}


/**
 * Action, started after button pressing.
 */
bool script_generator_frame_t::item_action(const char *fullpath)
{
	tool->save_script(fullpath);
	return true;
}


bool script_generator_frame_t::ok_action(const char *fullpath)
{
	tool->save_script(fullpath);
	return true;
}


const char *script_generator_frame_t::get_info(const char *)
{
	return NULL;
}


bool script_generator_frame_t::check_file( const char *, const char * )
{
	return true;
}
