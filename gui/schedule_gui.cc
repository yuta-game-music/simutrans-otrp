/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "../simline.h"
#include "../simcolor.h"
#include "../simhalt.h"
#include "../simworld.h"
#include "../simmenu.h"
#include "../simconvoi.h"
#include "../display/simgraph.h"
#include "../display/viewport.h"

#include "../utils/simstring.h"
#include "../utils/cbuffer_t.h"

#include "../boden/grund.h"

#include "../obj/zeiger.h"

#include "../dataobj/schedule.h"
#include "../dataobj/loadsave.h"
#include "../dataobj/translator.h"
#include "../dataobj/environment.h"

#include "../player/simplay.h"

#include "../tpl/vector_tpl.h"

#include "depot_frame.h"
#include "schedule_gui.h"
#include "line_item.h"

#include "components/gui_button.h"
#include "components/gui_image.h"
#include "components/gui_textarea.h"
#include "minimap.h"

static karte_ptr_t welt;

/**
 * One entry in the list of schedule entries.
 */
class gui_schedule_entry_t : public gui_aligned_container_t, public gui_action_creator_t
{
	schedule_entry_t entry;
	bool is_current;
	uint number;
	player_t* player;
	gui_image_t arrow;
	gui_label_buf_t stop;

public:
	gui_schedule_entry_t(player_t* pl, schedule_entry_t e, uint n)
	{
		player = pl;
		entry  = e;
		number = n;
		is_current = false;
		set_table_layout(2,1);

		add_component(&arrow);
		arrow.set_image(gui_theme_t::pos_button_img[0], true);

		add_component(&stop);
		update_label();
	}

	void update_label()
	{
		stop.buf().printf("%i) ", number+1);
		schedule_t::gimme_stop_name(stop.buf(), welt, player, entry, -1);
		stop.set_color(is_current ? SYSCOL_TEXT_HIGHLIGHT : SYSCOL_TEXT);
		stop.update();
	}

	void draw(scr_coord offset) OVERRIDE
	{
		update_label();
		if (is_current) {
			display_fillbox_wh_clip_rgb(pos.x + offset.x, pos.y + offset.y, size.w, size.h, SYSCOL_LIST_BACKGROUND_SELECTED_F, false);
		}
		gui_aligned_container_t::draw(offset);
	}

	void set_active(bool yesno)
	{
		is_current = yesno;
		arrow.set_image(gui_theme_t::pos_button_img[yesno ? 1: 0], true);
	}

	bool infowin_event(const event_t *ev) OVERRIDE
	{
		if( ev->ev_class == EVENT_CLICK ) {
			if(  IS_RIGHTCLICK(ev)  ||  ev->mx < stop.get_pos().x) {
				// just center on it
				welt->get_viewport()->change_world_position( entry.pos );
			}
			else {
				call_listeners(number);
			}
			return true;
		}
		return false;
	}
};


schedule_gui_stats_t::schedule_gui_stats_t()
{
	set_table_layout(1,0);
	last_schedule = NULL;

	current_stop_mark = new zeiger_t(koord3d::invalid, NULL );
	current_stop_mark->set_image( tool_t::general_tool[TOOL_SCHEDULE_ADD]->cursor );
	
	empty_message = "Please click on the map to add\nwaypoints or stops to this\nschedule.";
}


schedule_gui_stats_t::~schedule_gui_stats_t()
{
	delete current_stop_mark;
	delete last_schedule;
}

// shows/deletes highlighting of tiles
void schedule_gui_stats_t::highlight_schedule(bool marking)
{
	marking &= env_t::visualize_schedule;
	FOR(minivec_tpl<schedule_entry_t>, const& i, schedule->entries) {
		if (grund_t* const gr = welt->lookup(i.pos)) {
			for(  uint idx=0;  idx<gr->get_top();  idx++  ) {
				obj_t *obj = gr->obj_bei(idx);
				if(  marking  ) {
					if(  !obj->is_moving()  ) {
						obj->set_flag( obj_t::highlight );
					}
				}
				else {
					obj->clear_flag( obj_t::highlight );
				}
			}
			gr->set_flag( grund_t::dirty );
			// here on water
			if(  gr->is_water()  ||  gr->ist_natur()  ) {
				if(  marking  ) {
					gr->set_flag( grund_t::marked );
				}
				else {
					gr->clear_flag( grund_t::marked );
				}
			}

		}
	}
	// always remove
	if(  grund_t *old_gr = welt->lookup(current_stop_mark->get_pos())  ) {
		current_stop_mark->mark_image_dirty( current_stop_mark->get_image(), 0 );
		old_gr->obj_remove( current_stop_mark );
		old_gr->set_flag( grund_t::dirty );
		current_stop_mark->set_pos( koord3d::invalid );
	}
	// add if required
	if(  marking  &&  schedule->get_current_stop() < schedule->get_count() ) {
		current_stop_mark->set_pos( schedule->entries[schedule->get_current_stop()].pos );
		if(  grund_t *gr = welt->lookup(current_stop_mark->get_pos())  ) {
			gr->obj_add( current_stop_mark );
			current_stop_mark->set_flag( obj_t::dirty );
			gr->set_flag( grund_t::dirty );
		}
	}
	current_stop_mark->clear_flag( obj_t::highlight );
}

void schedule_gui_stats_t::update_schedule()
{
	// compare schedules
	bool ok = (last_schedule != NULL)  &&  last_schedule->entries.get_count() == schedule->entries.get_count();
	for(uint i=0; ok  &&  i<last_schedule->entries.get_count(); i++) {
		ok = last_schedule->entries[i] == schedule->entries[i];
	}
	if (ok) {
		if (!last_schedule->empty()) {
			entries[ last_schedule->get_current_stop() ]->set_active(false);
			entries[ schedule->get_current_stop() ]->set_active(true);
			last_schedule->set_current_stop( schedule->get_current_stop() );
		}
	}
	else {
		remove_all();
		entries.clear();
		buf.clear();
		buf.append(translator::translate(empty_message));
		if (schedule->empty()) {
			new_component<gui_textarea_t>(&buf);
		}
		else {
			for(uint i=0; i<schedule->entries.get_count(); i++) {
				entries.append( new_component<gui_schedule_entry_t>(player, schedule->entries[i], i));
				entries.back()->add_listener( this );
			}
			entries[ schedule->get_current_stop() ]->set_active(true);
		}
		if (last_schedule) {
			last_schedule->copy_from(schedule);
		}
		else {
			last_schedule = schedule->copy();
		}
		set_size(get_min_size());
	}
	highlight_schedule(true);
}

void schedule_gui_stats_t::draw(scr_coord offset)
{
	update_schedule();

	gui_aligned_container_t::draw(offset);
}

bool schedule_gui_stats_t::action_triggered(gui_action_creator_t *, value_t v)
{
	// has to be one of the entries
	call_listeners(v);
	return true;
}


cbuffer_t schedule_gui_stats_t::buf;

schedule_gui_t::schedule_gui_t(schedule_t* schedule_, player_t* player_, convoihandle_t cnv_) :
	gui_frame_t( translator::translate("Fahrplan"), NULL),
	line_selector(line_scrollitem_t::compare),
	departure_slot_group_selector(company_color_line_scroll_item_t::compare),
	lb_waitlevel(SYSCOL_TEXT_HIGHLIGHT, gui_label_t::right),
	lb_wait("1/"),
	lb_load("Full load"),
	lb_departure_slot_group("Departure slot group"),
	lb_max_speed("Maxspeed"),
	lb_tbgr_waiting_time("Additional goods routing waiting time"),
	stats(new schedule_gui_stats_t() ),
	scrolly(stats)
{
	scrolly.set_maximize( true );
	schedule = NULL;
	player   = NULL;
	schedule_filter[0] = 0;
	old_schedule_filter[0] = 0;
	if (schedule_) {
		init(schedule_, player_, cnv_);
	}
}

schedule_gui_t::~schedule_gui_t()
{
	if(  player  ) {
		update_tool( false );
		// hide schedule on minimap (may not current, but for safe)
		minimap_t::get_instance()->set_selected_cnv( convoihandle_t() );
	}
	delete schedule;
	delete stats;
}

void schedule_gui_t::init(schedule_t* schedule_, player_t* player, convoihandle_t cnv)
{
	// initialization
	this->old_schedule = schedule_;
	this->cnv = cnv;
	this->player = player;
	set_owner(player);

	// prepare editing
	old_schedule->start_editing();
	schedule = old_schedule->copy();
	if(  !cnv.is_bound()  ) {
		old_line = new_line = linehandle_t();
	}
	else {
		// set this schedule as current to show on minimap if possible
		minimap_t::get_instance()->set_selected_cnv( cnv );
		old_line = new_line = cnv->get_line();
	}
	old_line_count = 0;

	stats->player = player;
	stats->schedule = schedule;
	stats->update_schedule();
	stats->add_listener(this);

	set_table_layout(1,0);

	if(  cnv.is_bound()  ) {
		add_table(3,2);
		new_component<gui_label_t>("Filter:");
		name_filter_input.set_text(schedule_filter, lengthof(schedule_filter));
		name_filter_input.add_listener(this);
		add_component(&name_filter_input);
		new_component<gui_fill_t>();
		
		// things, only relevant to convois, like creating/selecting lines
		new_component<gui_label_t>("Serves Line:");
		bt_promote_to_line.init( button_t::roundbox, "promote to line");
		bt_promote_to_line.set_tooltip("Create a new line based on this schedule");
		bt_promote_to_line.add_listener(this);
		new_component<gui_fill_t>();
		add_component(&bt_promote_to_line);
		end_table();

		line_selector.clear_elements();

		init_line_selector();
		line_selector.add_listener(this);
		add_component(&line_selector);
	}

	// loading level and waiting time
	add_table(2,2);
	{
		add_component(&lb_load);

		numimp_load.set_width( 60 );
		numimp_load.set_value( schedule->get_current_entry().minimum_loading );
		numimp_load.set_limits( 0, 200 );
		numimp_load.set_increment_mode( gui_numberinput_t::PROGRESS );
		numimp_load.add_listener(this);
		add_component(&numimp_load);

		bt_wait_load.init(button_t::square_state, "month wait time");
		bt_wait_load.add_listener(this);
		add_component(&bt_wait_load);

		add_table(2, 1);
		add_component(&lb_wait);
		numimp_wait_load.set_width( 60 );
		numimp_wait_load.set_value( max(schedule->get_current_entry().waiting_time_shift, 1) );
		numimp_wait_load.set_limits( 1, 65535 );
		numimp_wait_load.set_increment_mode( gui_numberinput_t::POWER2 );
		numimp_wait_load.add_listener(this);
		add_component(&numimp_wait_load);
		end_table();
	}
	end_table();
	
	add_table(2,1);
	{
		bt_extract_settings.init(button_t::arrowdown, "Extract advanced settings");
		bt_extract_settings.set_tooltip("Show some more."); // TODO: fix me :(
		bt_extract_settings.add_listener(this);
		bt_extract_settings.pressed = false;
		add_component(&bt_extract_settings);
		
		new_component<gui_label_t>("Extract advanced settings");
	}
	end_table();
	
	// Components for advanced settings
	
	add_table(3,1);
	{
		bt_tmp_schedule.init(button_t::square_state, "Temporary schedule");
		bt_tmp_schedule.set_tooltip("This schedule does not affect the route cost calculation.");
		bt_tmp_schedule.add_listener(this);
		bt_tmp_schedule.pressed = schedule->is_temporary();
		add_component(&bt_tmp_schedule);
		
		bt_full_load_acceleration.init(button_t::square_state, "Full Load Acceleration");
		bt_full_load_acceleration.set_tooltip("Always use full load acceleration regardless of loadings.");
		bt_full_load_acceleration.add_listener(this);
		bt_full_load_acceleration.pressed = schedule->is_full_load_acceleration();
		add_component(&bt_full_load_acceleration);

		bt_full_load_time.init(button_t::square_state, "Full Get on/off Time");
		bt_full_load_time.set_tooltip("Always use maximum boarding and alighting time, regardless of boardings and alightings.");
		bt_full_load_time.add_listener(this);
		bt_full_load_time.pressed = schedule->is_full_load_time();
		add_component(&bt_full_load_time);
	}
	end_table();
	
	// load and unload settings
	add_table(3,1);
	{
		bt_no_load.init(button_t::square_state, "No Load");
		bt_no_load.set_tooltip("The convoy does not load goods and passengers at this stop.");
		bt_no_load.add_listener(this);
		bt_no_load.disable();
		add_component(&bt_no_load);
		bt_no_unload.init(button_t::square_state, "No Unload");
		bt_no_unload.set_tooltip("The convoy does not unload goods and passengers at this stop.");
		bt_no_unload.add_listener(this);
		bt_no_unload.disable();
		add_component(&bt_no_unload);
		bt_unload_all.init(button_t::square_state, "Unload All");
		bt_unload_all.set_tooltip("All passengers once get off at this stop.");
		bt_unload_all.add_listener(this);
		bt_unload_all.disable();
		add_component(&bt_unload_all);

	}
	end_table();

	add_table(1,1);
	{
		bt_transfer_interval.init(button_t::square_state, "transfer interval");
		bt_transfer_interval.set_tooltip("Passengers who boarded outside the section get off at this stop.");
		bt_transfer_interval.add_listener(this);
		bt_transfer_interval.disable();
		add_component(&bt_transfer_interval);
	}
	end_table();
	
	// max speed setting
	add_table(2,1);
	{
		add_component(&lb_max_speed);
		numimp_max_speed.set_width( 60 );
		numimp_max_speed.set_value( schedule->get_max_speed() );
		numimp_max_speed.set_limits( 0, 65535 );
		numimp_max_speed.set_increment_mode(1);
		numimp_max_speed.add_listener(this);
		add_component(&numimp_max_speed);
	}
	end_table();

	// Additional waiting time on goods routing, when TBGR is enabled
	add_table(2,1);
	{
		add_component(&lb_tbgr_waiting_time);
		numimp_tbgr_waiting_time.set_width( 60 );
		numimp_tbgr_waiting_time.set_value( schedule->get_additional_base_waiting_time() );
		numimp_tbgr_waiting_time.set_limits( 0, 999999 );
		numimp_tbgr_waiting_time.set_increment_mode(1);
		numimp_tbgr_waiting_time.add_listener(this);
		add_component(&numimp_tbgr_waiting_time);
	}
	end_table();
	
	// coupling related buttons
	add_table(2,1);
	{
		bt_wait_for_child.init(button_t::square_state, "Wait for coupling");
		bt_wait_for_child.set_tooltip("A convoy waits for other convoy to couple.");
		bt_wait_for_child.add_listener(this);
		bt_wait_for_child.disable();
		add_component(&bt_wait_for_child);
		
		bt_find_parent.init(button_t::square_state, "Try coupling");
		bt_find_parent.set_tooltip("A convoy tries to find a waiting convoy to couple with.");
		bt_find_parent.add_listener(this);
		bt_find_parent.disable();
		add_component(&bt_find_parent);
	}	
	end_table();

	// reverse setting
	add_table(2,1);
	{
		bt_reverse_convoy.init(button_t::square_state, "reverse convoy");
		bt_reverse_convoy.set_tooltip("Reverses the direction of convoy.");
		bt_reverse_convoy.add_listener(this);
		bt_reverse_convoy.disable();
		add_component(&bt_reverse_convoy);

		bt_reverse_coupling.init(button_t::square_state, "reverse convoy coupling");
		bt_reverse_coupling.set_tooltip("Reverses the parents-children order of the coupled convoys.");
		bt_reverse_coupling.add_listener(this);
		bt_reverse_coupling.disable();
		add_component(&bt_reverse_coupling);
	}
	end_table();


	// for departure time settings
	add_table(3,3);
	{
		bt_wait_for_time.init(button_t::square_state, "Wait for time");
		bt_wait_for_time.set_tooltip("If this is set, convoys will wait until one of the specified times before departing, the specified times being fractions of a month.");
		bt_wait_for_time.add_listener(this);
		bt_wait_for_time.disable();
		add_component(&bt_wait_for_time);
		
		lb_spacing.set_align(gui_label_t::align_t::centered);
		sprintf(lb_spacing_str,"off");
		lb_spacing.set_text_pointer(lb_spacing_str);
		add_component(&lb_spacing);
		
		lb_spacing_shift.set_align(gui_label_t::align_t::centered);
		sprintf(lb_spacing_shift_str,"");
		lb_spacing_shift.set_text_pointer(lb_spacing_shift_str);
		add_component(&lb_spacing_shift);
		
		lb_title1.set_text("Spacing cnv/month, shift");
		add_component(&lb_title1);
		
		const uint16 spacing_divisor = world()->get_settings().get_spacing_shift_divisor();
		
		numimp_spacing.set_width( 60 );
		numimp_spacing.set_value( 5 );
		numimp_spacing.set_limits( 1, spacing_divisor );
		numimp_spacing.set_increment_mode(1);
		numimp_spacing.disable();
		numimp_spacing.add_listener(this);
		add_component(&numimp_spacing);
		
		numimp_spacing_shift.set_width( 90 );
		numimp_spacing_shift.set_value( 0 );
		numimp_spacing_shift.set_limits( 0, spacing_divisor );
		numimp_spacing_shift.set_increment_mode(1);
		numimp_spacing_shift.add_listener(this);
		numimp_spacing_shift.disable();
		add_component(&numimp_spacing_shift);
		
		lb_title2.set_text("Delay tolerance");
		add_component(&lb_title2);
		
		numimp_delay_tolerance.set_width( 90 );
		numimp_delay_tolerance.set_value( 0 );
		numimp_delay_tolerance.set_increment_mode(1);
		numimp_delay_tolerance.disable();
		numimp_delay_tolerance.add_listener(this);
		add_component(&numimp_delay_tolerance);
		
		new_component<gui_fill_t>();
	}
	end_table();
	
	bt_load_before_departure.init(button_t::square_automatic, "Load before departure");
	bt_load_before_departure.set_tooltip("Do not load cargos until the departure time comes.");
	bt_load_before_departure.add_listener(this);
	add_component(&bt_load_before_departure);
	
	bt_same_dep_time.init(button_t::square_automatic, "Use same departure time for all stops");
	bt_same_dep_time.set_tooltip("Use one spacing, shift and delay tolerance value for all stops in schedule.");
	bt_same_dep_time.add_listener(this);
	bt_same_dep_time.pressed = schedule->is_same_dep_time();
	add_component(&bt_same_dep_time);

	if(  !cnv.is_bound()  ) {
		lb_departure_slot_group.set_tooltip(translator::translate("Shares the departure time slot with the selected line here."));
		add_component(&lb_departure_slot_group);

		init_departure_slot_group_selector();
		departure_slot_group_selector.add_listener(this);
		add_component(&departure_slot_group_selector);
	}
	
	extract_advanced_settings(false);

	add_table(2,1);
	{
		bt_revert.init(button_t::roundbox, "Revert schedule");
		bt_revert.set_tooltip("Revert to original schedule");
		bt_revert.add_listener(this);
		add_component(&bt_revert);

		// return tickets
		if(  !env_t::hide_rail_return_ticket  ||  schedule->get_waytype()==road_wt  ||  schedule->get_waytype()==air_wt  ||  schedule->get_waytype()==water_wt  ) {
			//  hide the return ticket on rail stuff, where it causes much trouble
			bt_return.init(button_t::roundbox, "return ticket");
			bt_return.set_tooltip("Add stops for backward travel");
			bt_return.add_listener(this);
			add_component(&bt_return);
		}
		else {
			new_component<gui_fill_t>();
		}
	}
	end_table();

	// action button row
	add_table(3,1)->set_force_equal_columns(true);
	bt_add.init(button_t::roundbox_state | button_t::flexible, "Add Stop");
	bt_add.set_tooltip("Appends stops at the end of the schedule");
	bt_add.add_listener(this);
	bt_add.pressed = true;
	add_component(&bt_add);

	bt_insert.init(button_t::roundbox_state | button_t::flexible, "Ins Stop");
	bt_insert.set_tooltip("Insert stop before the current stop");
	bt_insert.add_listener(this);
	bt_insert.pressed = false;
	add_component(&bt_insert);

	bt_remove.init(button_t::roundbox_state | button_t::flexible, "Del Stop");
	bt_remove.set_tooltip("Delete the current stop");
	bt_remove.add_listener(this);
	bt_remove.pressed = false;
	add_component(&bt_remove);
	end_table();

	scrolly.set_show_scroll_x(true);
	scrolly.set_scroll_amount_y(LINESPACE+1);
	add_component(&scrolly);

	mode = adding;
	update_selection();

	set_resizemode(diagonal_resize);

	reset_min_windowsize();
	set_windowsize(get_min_windowsize());
}


void schedule_gui_t::update_tool(bool set)
{
	if(!set  ||  mode==removing  ||  mode==undefined_mode) {
		// reset tools, if still selected ...
		if(welt->get_tool(player->get_player_nr())==tool_t::general_tool[TOOL_SCHEDULE_ADD]) {
			if(tool_t::general_tool[TOOL_SCHEDULE_ADD]->get_default_param()==(const char *)schedule) {
				welt->set_tool( tool_t::general_tool[TOOL_QUERY], player );
			}
		}
		else if(welt->get_tool(player->get_player_nr())==tool_t::general_tool[TOOL_SCHEDULE_INS]) {
			if(tool_t::general_tool[TOOL_SCHEDULE_INS]->get_default_param()==(const char *)schedule) {
				welt->set_tool( tool_t::general_tool[TOOL_QUERY], player );
			}
		}
	}
	else {
		//  .. or set them again
		if(mode==adding) {
			tool_t::general_tool[TOOL_SCHEDULE_ADD]->set_default_param((const char *)schedule);
			welt->set_tool( tool_t::general_tool[TOOL_SCHEDULE_ADD], player );
		}
		else if(mode==inserting) {
			tool_t::general_tool[TOOL_SCHEDULE_INS]->set_default_param((const char *)schedule);
			welt->set_tool( tool_t::general_tool[TOOL_SCHEDULE_INS], player );
		}
	}
}


void schedule_gui_t::update_selection()
{
	// First, disable all.
	lb_wait.set_color( SYSCOL_BUTTON_TEXT_DISABLED );
	numimp_wait_load.disable();
	bt_wait_load.disable();
	lb_load.set_color( SYSCOL_BUTTON_TEXT_DISABLED );
	numimp_load.disable();
	numimp_load.set_value( 0 );
	bt_find_parent.disable();
	bt_wait_for_child.disable();
	bt_no_load.disable();
	bt_no_unload.disable();
	bt_unload_all.disable();
	bt_wait_for_time.disable();
	numimp_spacing.disable();
	numimp_spacing_shift.disable();
	numimp_delay_tolerance.disable();
	bt_load_before_departure.disable();
	bt_transfer_interval.disable();
	bt_reverse_convoy.disable();
	bt_reverse_coupling.disable();

	if(  !schedule->empty()  ) {
		schedule->set_current_stop( min(schedule->get_count()-1,schedule->get_current_stop()) );
		const uint8 current_stop = schedule->get_current_stop();
		if(  haltestelle_t::get_halt(schedule->entries[current_stop].pos, player).is_bound()  ) {
			
			const uint8 c = schedule->entries[current_stop].get_coupling_point();
			bt_find_parent.enable();
			bt_find_parent.pressed = c==2;
			bt_wait_for_child.enable();
			bt_wait_for_child.pressed = c==1;
			bt_no_load.enable();
			bt_no_load.pressed = schedule->entries[current_stop].is_no_load();
			bt_no_unload.enable();
			bt_no_unload.pressed = schedule->entries[current_stop].is_no_unload();
			bt_unload_all.enable();
			bt_unload_all.pressed = schedule->entries[current_stop].is_unload_all();
			bt_load_before_departure.pressed = schedule->entries[current_stop].is_load_before_departure();
			bt_transfer_interval.enable();
			bt_transfer_interval.pressed = schedule->entries[current_stop].is_transfer_interval();
			bt_reverse_convoy.enable();
			bt_reverse_convoy.pressed = schedule->entries[current_stop].is_reverse_convoy();
			bt_reverse_coupling.enable();
			bt_reverse_coupling.pressed = schedule->entries[current_stop].is_reverse_convoi_coupling();

			
			// wait_for_time releated things
			const bool wft = schedule->entries[current_stop].get_wait_for_time();
			bt_wait_for_time.enable();
			bt_wait_for_time.pressed = wft;
			if(  wft  ) {
				// enable departure time settings only
				//schedule->entries[current_stop].spacing cannot be zero.
				const uint16 divisor = world()->get_settings().get_spacing_shift_divisor();
				const uint16 month_ratio_second = 86400/divisor;
				uint32 second = (86400/schedule->entries[current_stop].spacing)/month_ratio_second*month_ratio_second;
				uint8 hour = second/3600;
				uint8 minute = (second - hour * 3600)/60;
				second = second % 60;
				sprintf(lb_spacing_str, "%d (%02d:%02d:%02d)", divisor/schedule->entries[current_stop].spacing,hour,minute,second);

				uint32 second_shift = schedule->entries[current_stop].spacing_shift * month_ratio_second;
				uint8 hour_shift = second_shift/3600;
				uint8 minute_shift = (second_shift - hour_shift * 3600)/60;
				second_shift = second_shift % 60;
				sprintf(lb_spacing_shift_str, "(%02d:%02d:%02d)", hour_shift,minute_shift,second_shift);
				numimp_spacing.enable();
				numimp_spacing_shift.enable();
				numimp_delay_tolerance.enable();
				bt_load_before_departure.enable();
			}
			else {
				// disable departure time settings and enable minimum loading
				lb_load.set_color( SYSCOL_TEXT );
				numimp_load.enable();
				if(  schedule->entries[current_stop].minimum_loading>0  ||  schedule->entries[current_stop].get_coupling_point()!=0  ) {
					bt_wait_load.enable();
					uint16 wait = schedule->entries[current_stop].waiting_time_shift;
					bt_wait_load.pressed = wait>0;
					if(  wait>0  ) {
						lb_wait.set_color( SYSCOL_TEXT );
						numimp_wait_load.enable();
						if(  schedule->entries[current_stop].minimum_loading  ==  200  ){
							bt_load_before_departure.enable();
						}
					}
				}
				sprintf(lb_spacing_str, "off");
				sprintf(lb_spacing_shift_str,"");
			}
			
			numimp_load.set_value( schedule->entries[current_stop].minimum_loading );
			numimp_wait_load.set_value( max(1, schedule->entries[current_stop].waiting_time_shift) );
			numimp_spacing.set_value( schedule->entries[current_stop].spacing );
			numimp_spacing_shift.set_value( schedule->entries[current_stop].spacing_shift );
			numimp_delay_tolerance.set_value( schedule->entries[current_stop].delay_tolerance );
		}
	}
}


/**
 * Mouse clicks are hereby reported to its GUI-Components
 */
bool schedule_gui_t::infowin_event(const event_t *ev)
{
	if( (ev)->ev_class == EVENT_CLICK  &&  !((ev)->ev_code==MOUSE_WHEELUP  ||  (ev)->ev_code==MOUSE_WHEELDOWN)  ) {
		// close combo box; we must do it ourselves, since the box does not receive outside events ...
		if(  !line_selector.getroffen(ev->cx, ev->cy-D_TITLEBAR_HEIGHT)  ) {
			line_selector.close_box();
		}
		if(  !departure_slot_group_selector.getroffen(ev->cx, ev->cy-D_TITLEBAR_HEIGHT)  ) {
			departure_slot_group_selector.close_box();
		}
	}
	else if(  ev->ev_class == INFOWIN  &&  ev->ev_code == WIN_CLOSE  &&  schedule!=NULL  ) {

		stats->highlight_schedule( false );

		update_tool( false );
		schedule->cleanup();
		schedule->finish_editing();
		// now apply the changes
		if(  cnv.is_bound()  ) {
			// do not send changes if the convoi is about to be deleted
			if(  cnv->get_state() != convoi_t::SELF_DESTRUCT  ) {
				// if a line is selected
				if(  new_line.is_bound()  ) {
					// if the selected line is different to the convoi's line, apply it
					if(  new_line!=cnv->get_line()  ) {
						char id[16];
						sprintf( id, "%i,%i", new_line.get_id(), schedule->get_current_stop() );
						cnv->call_convoi_tool( 'l', id );
					}
					else {
						cbuffer_t buf;
						schedule->sprintf_schedule( buf );
						cnv->call_convoi_tool( 'g', buf );
					}
				}
				else {
					cbuffer_t buf;
					schedule->sprintf_schedule( buf );
					cnv->call_convoi_tool( 'g', buf );
				}

				if(  cnv->in_depot()  ) {
					const grund_t *const ground = welt->lookup( cnv->get_home_depot() );
					if(  ground  ) {
						const depot_t *const depot = ground->get_depot();
						if(  depot  ) {
							depot_frame_t *const frame = dynamic_cast<depot_frame_t *>( win_get_magic( (ptrdiff_t)depot ) );
							if(  frame  ) {
								frame->update_data();
							}
						}
					}
				}
			}
		}
	}
	else if(  ev->ev_class == INFOWIN  &&  (ev->ev_code == WIN_TOP  ||  ev->ev_code == WIN_OPEN)  &&  schedule!=NULL  ) {
		// just to be sure, renew the tools ...
		update_tool( true );
	}

	return gui_frame_t::infowin_event(ev);
}


bool schedule_gui_t::action_triggered( gui_action_creator_t *comp, value_t p)
{
DBG_MESSAGE("schedule_gui_t::action_triggered()","comp=%p combo=%p",comp,&line_selector);

	if(comp == &bt_add) {
		mode = adding;
		bt_add.pressed = true;
		bt_insert.pressed = false;
		bt_remove.pressed = false;
		update_tool( true );
	}
	else if(comp == &bt_insert) {
		mode = inserting;
		bt_add.pressed = false;
		bt_insert.pressed = true;
		bt_remove.pressed = false;
		update_tool( true );
	}
	else if(comp == &bt_remove) {
		mode = removing;
		bt_add.pressed = false;
		bt_insert.pressed = false;
		bt_remove.pressed = true;
		update_tool( false );
	}
	else if(comp == &bt_find_parent) {
		if(!schedule->empty()) {
			if(  bt_find_parent.pressed  ) {
				schedule->entries[schedule->get_current_stop()].reset_coupling();
			} else {
				schedule->entries[schedule->get_current_stop()].set_try_coupling();
			}
			bt_wait_for_child.pressed = false;
			schedule->entries[schedule->get_current_stop()].set_reverse_convoi_coupling(false);
			update_selection();
		}
	}
	else if(comp == &bt_wait_for_child) {
		if(!schedule->empty()) {
			if(  bt_wait_for_child.pressed  ) {
				schedule->entries[schedule->get_current_stop()].reset_coupling();
			} else {
				schedule->entries[schedule->get_current_stop()].set_wait_for_coupling();
			}
			bt_find_parent.pressed = false;
			schedule->entries[schedule->get_current_stop()].set_reverse_convoi_coupling(false);
			update_selection();
		}
	}
	else if(comp == &bt_reverse_convoy) {
		if(!schedule->empty()) {
			schedule->entries[schedule->get_current_stop()].set_reverse_convoy(!bt_reverse_convoy.pressed);
			update_selection();
		}
	}
	else if(comp == &bt_reverse_coupling) {
		if(!schedule->empty()) {
			schedule->entries[schedule->get_current_stop()].set_reverse_convoi_coupling(!bt_reverse_coupling.pressed);
			if(  bt_wait_for_child.pressed  ) {
				schedule->entries[schedule->get_current_stop()].reset_coupling();
			} 
			if(  bt_find_parent.pressed  ) {
				schedule->entries[schedule->get_current_stop()].reset_coupling();
			} 
			update_selection();
		}
	}
	else if(comp == &numimp_load) {
		if (!schedule->empty()) {
			schedule->entries[schedule->get_current_stop()].minimum_loading = (uint8)p.i;
			update_selection();
		}
	}
	else if(comp == &bt_wait_load) {
		if (!schedule->empty()) {
			schedule->entries[schedule->get_current_stop()].waiting_time_shift = !bt_wait_load.pressed;
			update_selection();
		}
	}
	else if(comp == &numimp_wait_load) {
		if(!schedule->empty()) {
			schedule->entries[schedule->get_current_stop()].waiting_time_shift = (uint16)p.i;
			update_selection();
		}
	}
	else if(comp == &bt_return) {
		schedule->add_return_way();
	}
	else if(comp == &line_selector) {
		uint32 selection = p.i;
		if(  line_scrollitem_t *li = dynamic_cast<line_scrollitem_t*>(line_selector.get_element(selection))  ) {
			new_line = li->get_line();
			stats->highlight_schedule( false );
			schedule->copy_from( new_line->get_schedule() );
			schedule->start_editing();
		}
		else {
			// remove line
			new_line = linehandle_t();
			line_selector.set_selection( 0 );
		}
	}
	else if(comp == &bt_promote_to_line) {
		// update line schedule via tool!
		tool_t *tool = create_tool( TOOL_CHANGE_LINE | SIMPLE_TOOL );
		cbuffer_t buf;
		buf.printf( "c,0,%i,%ld,", (int)schedule->get_type(), (long)(intptr_t)old_schedule );
		schedule->sprintf_schedule( buf );
		tool->set_default_param(buf);
		welt->set_tool( tool, player );
		// since init always returns false, it is safe to delete immediately
		delete tool;
	}
	else if (comp == stats) {
		// click on one of the schedule entries
		const int line = p.i;

		if(  line >= 0 && line < schedule->get_count()  ) {
			schedule->set_current_stop( line );
			if(  mode == removing  ) {
				stats->highlight_schedule( false );
				schedule->remove();
			}
			update_selection();
		}
	}
	else if(comp == &bt_extract_settings) {
		extract_advanced_settings(!bt_tmp_schedule.is_visible());
		// reload window
		reset_min_windowsize();
	}
	else if(comp == &bt_no_load) {
		if (!schedule->empty()) {
			schedule->entries[schedule->get_current_stop()].set_no_load(!bt_no_load.pressed);
			update_selection();
		}
	}
	else if(comp == &bt_no_unload) {
		if (!schedule->empty()) {
			schedule->entries[schedule->get_current_stop()].set_no_unload(!bt_no_unload.pressed);
			update_selection();
		}
	}
	else if(comp == &bt_unload_all) {
		if (!schedule->empty()) {
			schedule->entries[schedule->get_current_stop()].set_unload_all(!bt_unload_all.pressed);
			update_selection();
		}
	}
	else if(comp == &bt_tmp_schedule) {
		schedule->set_temporary(!bt_tmp_schedule.pressed);
		bt_tmp_schedule.pressed = schedule->is_temporary();
	}
	else if(comp == &bt_wait_for_time) {
		if (!schedule->empty()) {
			schedule->entries[schedule->get_current_stop()].set_wait_for_time(!bt_wait_for_time.pressed);
			update_selection();
		}
	}
	else if(comp == &numimp_spacing) {
		if (!schedule->empty()) {
			if(  schedule->is_same_dep_time()  ) {
				schedule->set_spacing_for_all((uint16)p.i);
			} else {
				schedule->entries[schedule->get_current_stop()].spacing = (uint16)p.i;
			}
			update_selection();
		}
	}
	else if(comp == &numimp_spacing_shift) {
		if (!schedule->empty()) {
			if(  schedule->is_same_dep_time()  ) {
				schedule->set_spacing_shift_for_all((uint16)p.i);
			} else {
				schedule->entries[schedule->get_current_stop()].spacing_shift = (uint16)p.i;
			}
			update_selection();
		}
	}
	else if(comp == &numimp_delay_tolerance) {
		if (!schedule->empty()) {
			if(  schedule->is_same_dep_time()  ) {
				schedule->set_delay_tolerance_for_all((uint16)p.i);
			} else {
				schedule->entries[schedule->get_current_stop()].delay_tolerance = (uint16)p.i;
			}
			update_selection();
		}
	}
	else if(comp == &bt_load_before_departure) {
		if (!schedule->empty()) {
			schedule->entries[schedule->get_current_stop()].set_load_before_departure(bt_load_before_departure.pressed);
			update_selection();
		}
	}
	else if(comp == &bt_same_dep_time) {
		schedule->set_same_dep_time(!schedule->is_same_dep_time());
		bt_same_dep_time.pressed = schedule->is_same_dep_time();
	}
	else if(comp == &bt_full_load_acceleration) {
		schedule->set_full_load_acceleration(!schedule->is_full_load_acceleration());
		bt_full_load_acceleration.pressed = schedule->is_full_load_acceleration();
	}
	else if(comp == &numimp_max_speed) {
		schedule->set_max_speed((uint16)p.i);
	}
	else if(comp == &numimp_tbgr_waiting_time) {
		schedule->set_additional_base_waiting_time((uint32)p.i);
	}
	else if(comp == &bt_transfer_interval) {
		if (!schedule->empty()) {
			schedule->entries[schedule->get_current_stop()].set_transfer_interval(!bt_transfer_interval.pressed);
			update_selection();
		}
	}
	else if(comp == &bt_full_load_time) {
		schedule->set_full_load_time(!schedule->is_full_load_time());
		bt_full_load_time.pressed = schedule->is_full_load_time();
	}
	else if (comp == &bt_revert) {
		// revert changes and tell listener

		if ( *schedule_filter ){
			schedule_filter[0] = 0;
			init_line_selector();
			strcpy(old_schedule_filter,schedule_filter);
		}

		if (schedule) {
			stats->highlight_schedule(false);
			delete schedule;
			schedule = NULL;
		}
		schedule = old_schedule->copy();
		stats->schedule = schedule;
		init_departure_slot_group_selector();
		stats->update_schedule();
		update_selection();
	}
	else if(  comp == &name_filter_input  ) {
		if(  strcmp(old_schedule_filter,schedule_filter)  ) {
			init_line_selector();
			strcpy(old_schedule_filter,schedule_filter);
		}
	}
	else if(comp == &departure_slot_group_selector) {
		uint32 selection = p.i;
		if(  line_scrollitem_t *li = dynamic_cast<line_scrollitem_t*>(departure_slot_group_selector.get_element(selection))  ) {
			const sint64 id = li->get_line()->get_schedule()->get_departure_slot_group_id();
			schedule->set_departure_slot_group_id(id);
		}
		else {
			schedule->set_new_departure_slot_group_id();
		}
	}
	// recheck lines
	if(  cnv.is_bound()  ) {
		// unequal to line => remove from line ...
		if(  new_line.is_bound()  &&  !schedule->matches(welt,new_line->get_schedule())  ) {
			new_line = linehandle_t();
			line_selector.set_selection(0);
		}
		// only assign old line, when new_line is not equal
		if(  !new_line.is_bound()  &&  old_line.is_bound()  &&   schedule->matches(welt,old_line->get_schedule())  ) {
			new_line = old_line;
			init_line_selector();
		}
	}
	return true;
}


void schedule_gui_t::init_line_selector()
{
	line_selector.clear_elements();
	int selection = 0;
	vector_tpl<linehandle_t> lines;

	player->simlinemgmt.get_lines(schedule->get_type(), &lines);

	// keep assignment with identical schedules
	if(  new_line.is_bound()  &&  !schedule->matches( welt, new_line->get_schedule() )  ) {
		if(  old_line.is_bound()  &&  schedule->matches( welt, old_line->get_schedule() )  ) {
			new_line = old_line;
		}
		else {
			new_line = linehandle_t();
		}
	}
	int offset = 0;
	if(  !new_line.is_bound()  ) {
		selection = 0;
		offset = 1;
		line_selector.new_component<gui_scrolled_list_t::const_text_scrollitem_t>( translator::translate("<no line>"), SYSCOL_TEXT ) ;
	}

	FOR(  vector_tpl<linehandle_t>, const line,  lines  ) {
		if(  !*schedule_filter  ||  utf8caseutf8(line->get_name(), schedule_filter)  ) {
			line_selector.new_component<line_scrollitem_t>(line);
		}
		if(  !new_line.is_bound()  ) {
			if(  schedule->matches( welt, line->get_schedule() )  ) {
				selection = line_selector.count_elements()-1;
				new_line = line;
			}
		}
		else if(  new_line == line  ) {
			selection = line_selector.count_elements()-1;
		}
	}

	line_selector.set_selection( selection );
	line_scrollitem_t::sort_mode = line_scrollitem_t::SORT_BY_NAME;
	line_selector.sort( offset );
	old_line_count = player->simlinemgmt.get_line_count();
	last_schedule_count = schedule->get_count();
}


void schedule_gui_t::init_departure_slot_group_selector()
{
	departure_slot_group_selector.clear_elements();

	departure_slot_group_selector.new_component<gui_scrolled_list_t::const_text_scrollitem_t>( translator::translate("<new departure slot group>"), SYSCOL_TEXT ) ;

	// When there are other lines whose departure_slot_group_id is same as this schedule,
	// select the line which matches first.
	// Otherwise, select the line of this schedule.
	uint32 this_schedule_index = 0;
	uint32 selection = 0;

	for(uint8 player_id = 0; player_id < MAX_PLAYER_COUNT; player_id++) {
		const player_t* player = world()->get_player(player_id);
		if(  !player  ) {
			continue;
		}
		vector_tpl<linehandle_t> lines;
		player->simlinemgmt.get_lines(schedule->get_type(), &lines);
		FOR(  vector_tpl<linehandle_t>, const line,  lines  ) {
			if(  schedule->matches(world(), line->get_schedule())  ) {
				this_schedule_index = departure_slot_group_selector.count_elements();
			}
			else if(  line->get_schedule()->get_departure_slot_group_id()==schedule->get_departure_slot_group_id()  &&  selection==0  ) {
				selection = departure_slot_group_selector.count_elements();
			}
			departure_slot_group_selector.new_component<company_color_line_scroll_item_t>(line);
		}
	}

	if(  selection==0  ) {
		selection = this_schedule_index;
	}
	departure_slot_group_selector.set_selection( selection );
	// line_scrollitem_t::sort_mode = line_scrollitem_t::SORT_BY_NAME;
	// departure_slot_group_selector.sort( offset );
}

void schedule_gui_t::draw(scr_coord pos, scr_size size)
{
	if(  player->simlinemgmt.get_line_count()!=old_line_count  ||  last_schedule_count!=schedule->get_count()  ) {
		// lines added or deleted
		init_line_selector();
		last_schedule_count = schedule->get_count();
	}

	// after loading in network games, the schedule might still being updated
	if(  cnv.is_bound()  &&  cnv->get_state()==convoi_t::EDIT_SCHEDULE  &&  schedule->is_editing_finished()  ) {
		assert( convoi_t::EDIT_SCHEDULE==1 ); // convoi_t::EDIT_SCHEDULE is 1
		schedule->start_editing();
		cnv->call_convoi_tool( 's', "1" );
	}

	// always dirty, to cater for shortening of halt names and change of selections
	set_dirty();
	gui_frame_t::draw(pos,size);
}


/**
 * Set window size and adjust component sizes and/or positions accordingly
 */
void schedule_gui_t::set_windowsize(scr_size size)
{
	gui_frame_t::set_windowsize(size);
	// make scrolly take all of space
	scrolly.set_size( scr_size(scrolly.get_size().w, get_client_windowsize().h - scrolly.get_pos().y - D_MARGIN_BOTTOM));

}


void schedule_gui_t::map_rotate90( sint16 y_size)
{
	schedule->rotate90(y_size);
}


void schedule_gui_t::rdwr(loadsave_t *file)
{
	// this handles only schedules of bound convois
	// lines are handled by line_management_gui_t

	// window size
	scr_size size = get_windowsize();
	size.rdwr( file );

	// convoy data
	convoi_t::rdwr_convoihandle_t(file, cnv);

	// save edited schedule
	if(  file->is_loading()  ) {
		// dummy types
		schedule = new truck_schedule_t();
	}
	schedule->rdwr(file);

	if(  file->is_loading()  ) {
		if(  cnv.is_bound() ) {

			schedule_t *save_schedule = schedule->copy();

			init(cnv->get_schedule(), cnv->get_owner(), cnv);
			// init replaced schedule, restore
			schedule->copy_from(save_schedule);
			delete save_schedule;

			set_windowsize(size);

			// draw will init editing phase again, has to be synchronized
			cnv->get_schedule()->finish_editing();
			schedule->finish_editing();

			win_set_magic(this, (ptrdiff_t)cnv->get_schedule());
		}
		else {
			player = NULL; // prevent destructor from updating
			destroy_win( this );
			dbg->error( "schedule_gui_t::rdwr", "Could not restore schedule window for (%d)", cnv.get_id() );
		}
	}
}

void schedule_gui_t::extract_advanced_settings(bool yesno) {
	bt_extract_settings.set_typ(yesno ? button_t::arrowup : button_t::arrowdown);
	bt_tmp_schedule.set_visible(yesno);
	bt_full_load_acceleration.set_visible(yesno);
	bt_full_load_time.set_visible(yesno);
	bt_no_load.set_visible(yesno);
	bt_no_unload.set_visible(yesno);
	bt_unload_all.set_visible(yesno);
	bt_wait_for_time.set_visible(yesno);
	lb_spacing.set_visible(yesno);
	lb_spacing_shift.set_visible(yesno);
	lb_title1.set_visible(yesno);
	lb_title2.set_visible(yesno);
	lb_max_speed.set_visible(yesno);
	numimp_spacing.set_visible(yesno);
	numimp_spacing_shift.set_visible(yesno);
	numimp_delay_tolerance.set_visible(yesno);
	numimp_max_speed.set_visible(yesno);
	bt_same_dep_time.set_visible(yesno);
	bt_load_before_departure.set_visible(yesno);
	bt_transfer_interval.set_visible(yesno);
	lb_departure_slot_group.set_visible(yesno);
	departure_slot_group_selector.set_visible(yesno);
	
	const bool coupling_waytype = schedule->get_waytype()!=road_wt  &&  schedule->get_waytype()!=air_wt  &&  schedule->get_waytype()!=water_wt;
	bt_wait_for_child.set_visible(coupling_waytype  &&  yesno);
	bt_find_parent.set_visible(coupling_waytype  &&  yesno);
	bt_reverse_convoy.set_visible(coupling_waytype  &&  yesno);
	bt_reverse_coupling.set_visible(coupling_waytype  &&  yesno);

	const bool is_tbgr_enabled = world()->get_settings().get_goods_routing_policy() == goods_routing_policy_t::GRP_FIFO_ET;
	lb_tbgr_waiting_time.set_visible(is_tbgr_enabled  &&  yesno);
	numimp_tbgr_waiting_time.set_visible(is_tbgr_enabled  &&  yesno);
}
