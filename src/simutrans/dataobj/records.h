/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DATAOBJ_RECORDS_H
#define DATAOBJ_RECORDS_H


#include "koord3d.h"
#include "../convoihandle.h"

class message_t;
class player_t;
class loadsave_t;

/**
 * World record speed management.
 * Keeps track of the fastest vehicles in game.
 */
class records_t {

public:
	records_t(message_t *_msg) : msg(_msg) {}
	~records_t() {}

	/** Returns the current speed record for the given way type. */
	sint32 get_record_speed( waytype_t w ) const;

	/** Posts a message that a new speed record has been set. */
	void notify_record( convoihandle_t cnv, sint32 max_speed, koord3d k, uint32 current_month );

	/** Resets all speed records. */
	void clear_speed_records();

	void rdwr(loadsave_t* f);

private:
	// Destination for world record notifications.
	message_t *msg;

	/**
	 * Class representing a word speed record.
	 */
	class speed_record_t {
	public:
		convoihandle_t cnv;
		char      name[128];
		sint32    speed;
		koord3d   pos;
		sint8     player_nr;  // Owner
		uint32    year_month;

		speed_record_t() : cnv(), speed(0), pos(koord3d::invalid), player_nr(PLAYER_UNOWNED), year_month(0) { name[0] = 0; }

		void rdwr(loadsave_t* f);
	};

	/// World rail speed record
	speed_record_t max_rail_speed;
	/// World monorail speed record
	speed_record_t max_monorail_speed;
	/// World maglev speed record
	speed_record_t max_maglev_speed;
	/// World narrowgauge speed record
	speed_record_t max_narrowgauge_speed;
	/// World road speed record
	speed_record_t max_road_speed;
	/// World ship speed record
	speed_record_t max_ship_speed;
	/// World air speed record
	speed_record_t max_air_speed;
};

#endif
