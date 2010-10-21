/*
 * Copyright (c) 1997 - 2001 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

#ifndef gui_password_frame_h
#define gui_password_frame_t_h


#include "components/action_listener.h"
#include "gui_frame.h"
#include "gui_container.h"
#include "components/gui_textinput.h"
#include "components/gui_label.h"


class password_frame_t : public gui_frame_t, action_listener_t
{
private:
	char ibuf[256], player_name_str[256];

protected:
	spieler_t *sp;

	gui_textinput_t player_name;
	gui_hidden_textinput_t password;
	gui_label_t fnlabel, const_player_name;

public:
	password_frame_t( spieler_t *sp );

	/**
	 * This method is called if an action is triggered
	 * @author Hj. Malthaner
	 *
	 * Returns true, if action is done and no more
	 * components should be triggered.
	 * V.Meyer
	 */
	virtual bool action_triggered( gui_action_creator_t *komp, value_t extra);
};

#endif
