/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DESCRIPTOR_BRIDGE_DESC_H
#define DESCRIPTOR_BRIDGE_DESC_H


#include "skin_desc.h"
#include "image_list.h"
#include "text_desc.h"
#include "../simtypes.h"
#include "../display/simimg.h"

#include "../dataobj/ribi.h"

class tool_t;
class checksum_t;


/*
 *  BEWARE: non-standard node structure!
 *  0   Foreground-images
 *  1   Background-images
 *  2   Cursor/Icon
 *  3   Foreground-images - snow
 *  4   Background-images - snow
 */
class pillar_desc_t : public obj_named_desc_t {
	friend class bridge_reader_t;

public:
	static void register_desc(pillar_desc_t* desc);
	static const pillar_desc_t* get_desc(const char* name);

	/*
	 * Numbering of all image pieces
	 */
	enum pillar_img_t { left_flat1, left_WS1, left_SW1, left_flat2, left_WS2, left_SW2, right_flat1, right_ES1, right_SE1, right_flat2, right_ES2, right_SE2 };

	image_id get_left_img(slopte_t slope) const;
	image_id get_right_img(slopte_t slope) const;

	void calc_checksum(checksum_t *chk) const;
};

#endif
