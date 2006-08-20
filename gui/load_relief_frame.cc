/*
 * load_relief_frame.cc
 *
 * Copyright (c) 1997 - 2001 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project and may not be used
 * in other projects without written permission of the author.
 */


#include "../simdebug.h"
#include "load_relief_frame.h"
#include "../simworld.h"
#include "../dataobj/einstellungen.h"


/**
 * Aktion, die nach Knopfdruck gestartet wird.
 * @author Hansj�rg Malthaner
 */
void load_relief_frame_t::action(const char *filename)
{
    sets->heightfield = filename;
    welt->load_heightfield(sets);
}


void load_relief_frame_t::del_action(const char *filename)
{
    remove(filename);
}


/**
 * Konstruktor.
 * @author Hj. Malthaner
 */
load_relief_frame_t::load_relief_frame_t(karte_t * welt, einstellungen_t * sets) : savegame_frame_t(".ppm")
{
    setze_name("Laden");

    this->sets = sets;
    this->welt = welt;
}


/**
 * Manche Fenster haben einen Hilfetext assoziiert.
 * @return den Dateinamen f�r die Hilfe, oder NULL
 * @author Hj. Malthaner
 */
const char * load_relief_frame_t::gib_hilfe_datei() const
{
    return "load_relief.txt";
}
