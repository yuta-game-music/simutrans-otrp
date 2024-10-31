/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DATAOBJ_SCHEDULE_H
#define DATAOBJ_SCHEDULE_H


#include "schedule_entry.h"

#include "../halthandle_t.h"

#include "../tpl/minivec_tpl.h"


class cbuffer_t;
class grund_t;
class player_t;
class karte_t;


/**
 * Class to hold schedule of vehicles in Simutrans.
 */
class schedule_t
{
	bool  editing_finished;
	uint8 current_stop;
	
	uint8 flags;
	
	// operational maximum speed of this schedule. 0 => no limit.
	uint16 max_speed;

	// the id of the departure slot group.
	// defined in sint64 since uint64 value cannot be handled with rdwr_longlong
	sint64 departure_slot_group_id;

	// The base waiting time of the schedule on the goods route search when TBGR is enabled.
	// The unit is OTRP divided time, same as the departure time slot offset.
	uint32 additional_base_waiting_time;

	static schedule_entry_t dummy_entry;

	/**
	 * Fix up current_stop value, which we may have made out of range
	 */
	void make_current_stop_valid() {
		uint8 count = entries.get_count();
		if(  count == 0  ) {
			current_stop = 0;
		}
		else if(  current_stop >= count  ) {
			current_stop = count-1;
		}
	}

protected:
	schedule_t() : editing_finished(false), current_stop(0), flags(0), max_speed(0), departure_slot_group_id(0), additional_base_waiting_time(0) {}

public:
	enum schedule_type {
		schedule             = 0,
		truck_schedule       = 1,
		train_schedule       = 2,
		ship_schedule        = 3,
		airplane_schedule    = 4,
		monorail_schedule    = 5,
		tram_schedule        = 6,
		maglev_schedule      = 7,
		narrowgauge_schedule = 8
	};
	
	enum {
		NONE                   = 0,
		TEMPORARY              = 1U << 0, // This schedule is not used for goods routing.
		SAME_DEP_TIME          = 1U << 1, // Use same departure time for all entries.
		FULL_LOAD_ACCELERATION = 1U << 2, // Always use full load acceleration.
		FULL_LOAD_TIME         = 1U << 3,
	};

	minivec_tpl<schedule_entry_t> entries;

	/**
	 * Returns error message if stops are not allowed
	 */
	virtual char const* get_error_msg() const = 0;

	/**
	 * Returns true if this schedule allows stop at the
	 * given tile.
	 */
	bool is_stop_allowed(const grund_t *gr) const;

	bool empty() const { return entries.empty(); }

	uint8 get_count() const { return entries.get_count(); }

	virtual schedule_type get_type() const = 0;

	virtual waytype_t get_waytype() const = 0;

	/**
	 * Get current stop of the schedule.
	 */
	uint8 get_current_stop() const { return current_stop; }

	/// returns the current stop, always a valid entry
	schedule_entry_t const& get_current_entry() const { return current_stop >= entries.get_count() ? dummy_entry : entries[current_stop]; }
	
	schedule_entry_t const& get_next_entry() const;

	/**
	 * Set the current stop of the schedule .
	 * If new value is bigger than stops available, the max stop will be used.
	 */
	void set_current_stop(uint8 new_current_stop) {
		current_stop = new_current_stop;
		make_current_stop_valid();
	}

	/// advance current_stop by one
	void advance() {
		if(  !entries.empty()  ) {
			current_stop = (current_stop+1)%entries.get_count();
		}
	}

	inline bool is_editing_finished() const { return editing_finished; }
	void finish_editing() { editing_finished = true; }
	void start_editing() { editing_finished = false; }
	
	uint8 get_flags() const { return flags; }
	void set_flags(uint8 f) { flags = f; }
	bool is_temporary() const { return (flags&TEMPORARY)>0; }
	void set_temporary(bool y) { y ? flags |= TEMPORARY : flags &= ~TEMPORARY; }
	bool is_same_dep_time() const { return (flags&SAME_DEP_TIME)>0; }
	void set_same_dep_time(bool y) { y ? flags |= SAME_DEP_TIME : flags &= ~SAME_DEP_TIME; }
	bool is_full_load_acceleration() const { return (flags&FULL_LOAD_ACCELERATION)>0; }
	void set_full_load_acceleration(bool y) { y ? flags |= FULL_LOAD_ACCELERATION : flags &= ~FULL_LOAD_ACCELERATION; }
	uint16 get_max_speed() const { return max_speed; }
	void set_max_speed(uint16 v) { max_speed = v; }
	sint64 get_departure_slot_group_id() const { return departure_slot_group_id; }
	void set_departure_slot_group_id(sint64 id) { departure_slot_group_id = id; }
	void set_new_departure_slot_group_id();

	bool is_full_load_time() const { return (flags&FULL_LOAD_TIME)>0; }
	void set_full_load_time(bool y) { y ? flags |= FULL_LOAD_TIME : flags &= ~FULL_LOAD_TIME; }
	uint32 get_additional_base_waiting_time() const { return additional_base_waiting_time; }
	void set_additional_base_waiting_time(uint32 w) { additional_base_waiting_time = w; }
	
	void set_spacing_for_all(uint16);
	void set_spacing_shift_for_all(uint16);
	void set_delay_tolerance_for_all(uint16);

	virtual ~schedule_t() {}

	/**
	 * returns a halthandle for the next halt in the schedule (or unbound)
	 */
	halthandle_t get_next_halt( player_t *player, halthandle_t halt ) const;

	/**
	 * returns a halthandle for the previous halt in the schedule (or unbound)
	 */
	halthandle_t get_prev_halt( player_t *player ) const;

	/**
	 * Inserts a coordinate at current_stop into the schedule.
	 */
	bool insert(const grund_t* gr, uint8 minimum_loading = 0, uint16 waiting_time_shift = 0, uint16 stop_flags = 0);

	/**
	 * Appends a coordinate to the schedule.
	 */
	bool append(const grund_t* gr, uint8 minimum_loading = 0, uint16 waiting_time_shift = 0, uint16 stop_flags = 0);

	/**
	 * Cleanup a schedule, removes double entries.
	 */
	void cleanup();

	/**
	 * Remove current_stop entry from the schedule.
	 */
	bool remove();

	void rdwr(loadsave_t *file);

	void rotate90( sint16 y_size );

	/**
	 * if the passed in schedule matches "this", then return true
	 */
	bool matches(karte_t *welt, const schedule_t *schedule);

	/**
	 * Compare this schedule with another, ignoring order and exact positions and waypoints.
	 */
	bool similar( const schedule_t *schedule, const player_t *player );

	/**
	 * Calculates a return way for this schedule.
	 * Will add elements 1 to end in reverse order to schedule.
	 */
	void add_return_way();

	virtual schedule_t* copy() = 0;//{ return new schedule_t(this); }

	// copy all entries from schedule src to this and adjusts current_stop
	void copy_from(const schedule_t *src);

	// fills the given buffer with a schedule
	void sprintf_schedule( cbuffer_t &buf ) const;

	// converts this string into a schedule
	bool sscanf_schedule( const char * );

	/**
	 * Append description of entry to buf.
	 * If @p max_chars > 0 then append short version, without loading level and position.
	 */
	static void gimme_stop_name(cbuffer_t& buf, karte_t* welt, player_t const* player_, schedule_entry_t const& entry, int max_chars);
	
	/*
	 * Get the index of the corresponding entry of this schedule to Nth entry of the other schedule.
	 * Removes the effect of depot entries.
	 * Returns -1 if it does not exist.
	 */
	sint16 get_corresponding_entry_index(const schedule_t* other, uint8 n) const;
	
	// get current_stop excluding depot entries
	uint8 get_current_stop_exluding_depot() const;

	static void get_schedule_flag_text(cbuffer_t& buf, schedule_t* schedule);

	static sint64 issue_new_departure_slot_group_id();

	// Returns the median journey time ticks between the given index halt and the previous halt (or waypoint).
	// Returns Euclid distance / max_speed if no journey time record is available.
	uint32 get_median_journey_time(uint8 index, uint32 max_speed_kmh) const;
};


/**
 * Schedules with stops on tracks.
 */
class train_schedule_t : public schedule_t
{
public:
	train_schedule_t() {}
	schedule_t* copy() OVERRIDE { schedule_t *s = new train_schedule_t(); s->copy_from(this); return s; }
	const char *get_error_msg() const OVERRIDE { return "Zughalt muss auf\nSchiene liegen!\n"; }

	schedule_type get_type() const OVERRIDE { return train_schedule; }

	waytype_t get_waytype() const OVERRIDE { return track_wt; }
};

/**
 * Schedules with stops on tram tracks.
 */
class tram_schedule_t : public train_schedule_t
{
public:
	tram_schedule_t() {}
	schedule_t* copy() OVERRIDE { schedule_t *s = new tram_schedule_t(); s->copy_from(this); return s; }

	schedule_type get_type() const OVERRIDE { return tram_schedule; }

	waytype_t get_waytype() const OVERRIDE { return tram_wt; }
};


/**
 * Schedules with stops on roads.
 */
class truck_schedule_t : public schedule_t
{
public:
	truck_schedule_t() {}
	schedule_t* copy() OVERRIDE { schedule_t *s = new truck_schedule_t(); s->copy_from(this); return s; }
	const char *get_error_msg() const OVERRIDE { return "Autohalt muss auf\nStrasse liegen!\n"; }

	schedule_type get_type() const OVERRIDE { return truck_schedule; }

	waytype_t get_waytype() const OVERRIDE { return road_wt; }
};


/**
 * Schedules with stops on water.
 */
class ship_schedule_t : public schedule_t
{
public:
	ship_schedule_t() {}
	schedule_t* copy() OVERRIDE { schedule_t *s = new ship_schedule_t(); s->copy_from(this); return s; }
	const char *get_error_msg() const OVERRIDE { return "Schiffhalt muss im\nWasser liegen!\n"; }

	schedule_type get_type() const OVERRIDE { return ship_schedule; }

	waytype_t get_waytype() const OVERRIDE { return water_wt; }
};


/**
 * Schedules for airplanes.
 */
class airplane_schedule_t : public schedule_t
{
public:
	airplane_schedule_t() {}
	schedule_t* copy() OVERRIDE { schedule_t *s = new airplane_schedule_t(); s->copy_from(this); return s; }
	const char *get_error_msg() const OVERRIDE { return "Flugzeughalt muss auf\nRunway liegen!\n"; }

	schedule_type get_type() const OVERRIDE { return airplane_schedule; }

	waytype_t get_waytype() const OVERRIDE { return air_wt; }
};

/**
 * Schedules with stops on mono-rails.
 */
class monorail_schedule_t : public schedule_t
{
public:
	monorail_schedule_t() {}
	schedule_t* copy() OVERRIDE { schedule_t *s = new monorail_schedule_t(); s->copy_from(this); return s; }
	const char *get_error_msg() const OVERRIDE { return "Monorailhalt muss auf\nMonorail liegen!\n"; }

	schedule_type get_type() const OVERRIDE { return monorail_schedule; }

	waytype_t get_waytype() const OVERRIDE { return monorail_wt; }
};

/**
 * Schedules with stops on maglev tracks.
 */
class maglev_schedule_t : public schedule_t
{
public:
	maglev_schedule_t() {}
	schedule_t* copy() OVERRIDE { schedule_t *s = new maglev_schedule_t(); s->copy_from(this); return s; }
	const char *get_error_msg() const OVERRIDE { return "Maglevhalt muss auf\nMaglevschiene liegen!\n"; }

	schedule_type get_type() const OVERRIDE { return maglev_schedule; }

	waytype_t get_waytype() const OVERRIDE { return maglev_wt; }
};

/**
 * Schedules with stops on narrowgauge tracks.
 */
class narrowgauge_schedule_t : public schedule_t
{
public:
	narrowgauge_schedule_t() {}
	schedule_t* copy() OVERRIDE { schedule_t *s = new narrowgauge_schedule_t(); s->copy_from(this); return s; }
	const char *get_error_msg() const OVERRIDE { return "On narrowgauge track only!\n"; }

	schedule_type get_type() const OVERRIDE { return narrowgauge_schedule; }

	waytype_t get_waytype() const OVERRIDE { return narrowgauge_wt; }
};


#endif
