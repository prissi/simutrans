/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "../simdebug.h"

#include "pillar_desc.h"
#include "../network/checksum.h"


 /// All pillars hashed by name
static stringhashtable_tpl<const pillar_desc_t*> desc_table;


void pillar_desc_t::register_desc(pillar_desc_t* desc)
{
	// avoid duplicates with same name
	if (const pillar_desc_t* old_desc = desc_table.remove(desc->get_name())) {
		delete old_desc;
	}
	desc_table.put(desc->get_name(), desc);
}


const pillar_desc_t* pillar_desc_t::get_desc(const char* name)
{
	return  (name ? desc_table.get(name) : NULL);
}


image_id get_left_img(slope_t slope) const
{
	uint8 west_h = slope % 3;
	sint8 south_h = (slope / 3) % 3;
	uint8 index = (west_h != south_h) + (south_h > west_h);
	if (abs(south_h - west_h)==2  ||  slope==ALL_UP_SLOPE) {
		index += 3;
	}
	return get_child<image_list_t>(2)->get_image_id(index);
}


image_id get_right_img(slope_t slope) const;
{
	uint8 south_h = (slope / 3) % 3;
	sint8 east_h = (slope / 9) % 3;
	uint8 index = (west_h != south_h) + (south_h > east_h);
	if (abs(south_h - east_h) == 2 || slope == ALL_UP_SLOPE) {
		index += 3;
	}
	return get_child<image_list_t>(2)->get_image_id(index);
}
