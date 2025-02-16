/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef OBJ_PILLAR_H
#define OBJ_PILLAR_H


#include "simobj.h"
#include "../descriptor/pillar_desc.h"
#include "../tpl/minivec_tpl.h"

class loadsave_t;
class karte_t;

/**
 * Bridge piece (visible)
 */
class pillar_t : public obj_t
{
private:
	const pillar_desc_t *desc;

	minivec_tpl<image_id> left;
	minivec_tpl<image_id> right;

protected:
	void rdwr(loadsave_t *file) OVERRIDE;

public:
	pillar_t(loadsave_t *file);
	pillar_t(koord3d pos, player_t *player, const bridge_desc_t *desc, int hoehe);

	const char* get_name() const OVERRIDE { return "Pillar"; }
	typ get_typ() const OVERRIDE { return obj_t::pillar; }

	const bridge_desc_t* get_desc() const { return desc; }

	image_id get_image() const OVERRIDE { return asymmetric ? IMG_EMPTY : image; }

	// asymmetric pillars are placed at the southern/eastern boundary of the tile
	// thus the images have to be displayed after vehicles
	image_id get_front_image() const OVERRIDE { return asymmetric ? image : IMG_EMPTY;}

	// needs to check for hiding asymmetric pillars
	void calc_image() OVERRIDE;

	/**
	 * Einen Beschreibungsstring fuer das Objekt, der z.B. in einem
	 * Beobachtungsfenster angezeigt wird.
	 */
	void show_info() OVERRIDE;

	void rotate90() OVERRIDE;
};

#endif
