/*
 * Copyright (c) 2008 Markus Pristovsek
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

/* simple passenger AI (not using trains, not preoptimized network) */

#include "../simcity.h"
#include "../simhalt.h"
#include "../simtools.h"
#include "../simworld.h"

#include "../bauer/brueckenbauer.h"
#include "../bauer/hausbauer.h"
#include "../bauer/tunnelbauer.h"
#include "../bauer/vehikelbauer.h"
#include "../bauer/wegbauer.h"

#include "../dataobj/loadsave.h"

#include "../utils/simstring.h"

#include "../vehicle/simvehikel.h"

#include "ai_passenger.h"


ai_passenger_t::ai_passenger_t(karte_t *wl, uint8 nr) : ai_t( wl, nr )
{
	state = NR_INIT;

	road_vehicle = NULL;
	road_weg = NULL;

	construction_speed = 8000;
	next_contruction_steps = welt->gib_steps()+simrand(construction_speed);

	air_transport = true;
	ship_transport = false;

	start_stadt = end_stadt = NULL;
	ziel = NULL;
	end_ausflugsziel = NULL;
}




/* Activates/deactivates a player
 * @author prissi
 */
bool ai_passenger_t::set_active(bool new_state)
{
	// only activate, when there are buses available!
	if(  new_state  ) {
		new_state = NULL!=vehikelbauer_t::vehikel_search( road_wt, welt->get_timeline_year_month(), 50, 80, warenbauer_t::passagiere, false, false );
	}
	return spieler_t::set_active( new_state );
}





/* return the hub of a city (always the very first stop) or zero
 * @author prissi
 */
halthandle_t ai_passenger_t::get_our_hub( const stadt_t *s ) const
{
	slist_iterator_tpl <halthandle_t> iter( halt_list );
	while(iter.next()) {
		halthandle_t halt = iter.get_current();
		if(  halt->get_pax_enabled()  &&  (halt->get_station_type()&haltestelle_t::busstop)!=0  ) {
			koord h=halt->gib_basis_pos();
			if(h.x>=s->get_linksoben().x  &&  h.y>=s->get_linksoben().y  &&  h.x<=s->get_rechtsunten().x  &&  h.y<=s->get_rechtsunten().y  ) {
DBG_MESSAGE("ai_passenger_t::get_our_hub()","found %s at (%i,%i)",s->gib_name(),h.x,h.y);
				return iter.get_current();
			}
		}
	}
	return halthandle_t();
}



koord ai_passenger_t::find_area_for_hub( const koord lo, const koord ru, const koord basis ) const
{
	// no found found
	koord best_pos = koord::invalid;
	// "shortest" distance
	int dist = 999;

	for( sint16 x=lo.x;  x<=ru.x;  x++ ) {
		for( sint16 y=lo.y;  y<=ru.y;  y++ ) {
			const koord trypos(x,y);
			const grund_t * gr = welt->lookup_kartenboden(trypos);
			if(gr) {
				// flat, solid
				if(  gr->gib_typ()==grund_t::boden  &&  gr->gib_grund_hang()==hang_t::flach  ) {
					const ding_t* thing = gr->obj_bei(0);
					int test_dist = abs_distance( trypos, basis );
					if (thing == NULL || thing->gib_besitzer() == NULL || thing->gib_besitzer() == this) {
						if(gr->is_halt()  &&  check_owner( this, gr->gib_halt()->gib_besitzer())  &&  gr->hat_weg(road_wt)) {
							// ok, one halt belongs already to us ... (should not really happen!) but might be a public stop
							return trypos;
						} else if(test_dist<dist  &&  gr->hat_weg(road_wt)  &&  !gr->is_halt()  ) {
							ribi_t::ribi  ribi = gr->gib_weg_ribi_unmasked(road_wt);
							if(  ribi_t::ist_gerade(ribi)  ||  ribi_t::ist_einfach(ribi)  ) {
								best_pos = trypos;
								dist = test_dist;
							}
						} else if(test_dist+2<dist  &&  gr->ist_natur()  ) {
							// also ok for a stop, but second choice
							// so wie gave it a malus of 2
							best_pos = trypos;
							dist = test_dist+2;
						}
					}
				}
			}
		}
	}
DBG_MESSAGE("ai_passenger_t::find_area_for_hub()","suggest hub at (%i,%i)",best_pos.x,best_pos.y);
	return best_pos;
}



/* tries to built a hub near the koordinate
 * @author prissi
 */
koord ai_passenger_t::find_place_for_hub( const stadt_t *s ) const
{
	halthandle_t h = get_our_hub( s );
	if(h.is_bound()) {
		return h->gib_basis_pos();
	}
	return find_area_for_hub( s->get_linksoben(), s->get_rechtsunten(), s->gib_pos() );
}



koord ai_passenger_t::find_harbour_pos(karte_t* welt, const stadt_t *s )
{
	koord bestpos = koord::invalid, k;
	sint32 bestdist = 999999;

	// try to find an airport place as close to the city as possible
	for(  k.y=max(6,s->get_linksoben().y-10); k.y<=min(welt->gib_groesse_y()-3-6,s->get_rechtsunten().y+10); k.y++  ) {
		for(  k.x=max(6,s->get_linksoben().x-25); k.x<=min(welt->gib_groesse_x()-3-6,s->get_rechtsunten().x+10); k.x++  ) {
			sint32 testdist = abs_distance( k, s->gib_pos() );
			if(  testdist<bestdist  ) {
				if(  k.x+2<s->get_linksoben().x  ||  k.y+2<s->get_linksoben().y  ||  k.x>=s->get_rechtsunten().x  ||  k.y>=s->get_rechtsunten().y  ) {
					// malus for out of town
					testdist += 5;
				}
				if(  testdist<bestdist  ) {
					grund_t *gr = welt->lookup_kartenboden(k);
					hang_t::typ hang = gr->gib_grund_hang();
					if(gr->ist_natur()  &&  gr->gib_hoehe()==welt->gib_grundwasser()  &&  hang_t::ist_wegbar(hang)  &&  welt->ist_wasser(k-koord(hang),koord(hang)*4+koord(1,1))  ) {
						// can built busstop here?
						koord bushalt = k+koord(hang);
						gr = welt->lookup_kartenboden(bushalt);
						if(gr  &&  gr->ist_natur()) {
							bestpos = k;
							bestdist = testdist;
						}
					}
				}
			}
		}
	}
	return bestpos;
}



bool ai_passenger_t::create_water_transport_vehikel(const stadt_t* start_stadt, const koord target_pos)
{
	const vehikel_besch_t *v_besch = vehikelbauer_t::vehikel_search(water_wt, welt->get_timeline_year_month(), 10, 40, warenbauer_t::passagiere, false, true );
	if(v_besch==NULL  ) {
		// no ship there
		return false;
	}

	stadt_t *end_stadt = NULL;
	grund_t *ziel = welt->lookup_kartenboden(target_pos);
	gebaeude_t *gb = ziel->find<gebaeude_t>();
	if(gb  &&  gb->ist_rathaus()) {
		end_stadt = gb->get_stadt();
	}
	else if(!ziel->is_halt()  ||  !ziel->ist_wasser()) {
		// not townhall, not factory => we will not built this line for attractions!
		return false;
	}

	halthandle_t start_hub = get_our_hub( start_stadt );
	halthandle_t start_connect_hub = halthandle_t();
	koord start_harbour = koord::invalid;
	if(start_hub.is_bound()) {
		if(  (start_hub->get_station_type()&haltestelle_t::dock)==0  ) {
			start_connect_hub = start_hub;
			start_hub = halthandle_t();
			// is there already one harbour next to this one?
			slist_iterator_tpl<warenziel_t>iter(start_connect_hub->gib_warenziele_passenger());
			while(iter.next()) {
				if(iter.get_current().gib_zielhalt()->get_station_type()&haltestelle_t::dock) {
					start_hub = iter.get_current().gib_zielhalt();
					break;
				}
			}
		}
		else {
			start_harbour = start_hub->gib_basis_pos();
		}
	}
	// find an dock place
	if(!start_hub.is_bound()) {
		start_harbour = find_harbour_pos( welt, start_stadt );
	}
	if(start_harbour==koord::invalid) {
		// sorry, no suitable place
		return false;
	}
	// same for end
	halthandle_t end_hub = end_stadt ? get_our_hub( end_stadt ) : ziel->gib_halt();
	halthandle_t end_connect_hub = halthandle_t();
	koord end_harbour = koord::invalid;
	if(end_hub.is_bound()) {
		if(  (end_hub->get_station_type()&haltestelle_t::dock)==0  ) {
			end_connect_hub = end_hub;
			end_hub = halthandle_t();
			// is there already one dock next to this town?
			slist_iterator_tpl<warenziel_t>iter(end_connect_hub->gib_warenziele_passenger());
			while(iter.next()) {
				if(iter.get_current().gib_zielhalt()->get_station_type()&haltestelle_t::dock) {
					end_hub = iter.get_current().gib_zielhalt();
					break;
				}
			}
		}
		else {
			end_harbour = end_hub->gib_basis_pos();
		}
	}
	if(!end_hub.is_bound()  &&  end_stadt) {
		// find an dock place
		end_harbour = find_harbour_pos( welt, end_stadt );
	}
	if(end_harbour==koord::invalid) {
		// sorry, no suitable place
		return false;
	}

	const koord start_dx(welt->lookup_kartenboden(start_harbour)->gib_grund_hang());
	const koord end_dx(welt->lookup_kartenboden(end_harbour)->gib_grund_hang());

	// now we must check, if these two seas are connected
	{
		// we use the free own vehikel_besch_t
		vehikel_besch_t remover_besch( water_wt, 500, vehikel_besch_t::diesel );
		vehikel_t* test_driver = vehikelbauer_t::baue( koord3d(start_harbour-start_dx,welt->gib_grundwasser()), this, NULL, &remover_besch );
		route_t verbindung;
		bool connected = verbindung.calc_route(welt, koord3d(start_harbour-start_dx,welt->gib_grundwasser()), koord3d(end_harbour-end_dx,welt->gib_grundwasser()), test_driver, 0);
		delete test_driver;
		if(!connected) {
			return false;
		}
	}

	// built the harbour if neccessary
	if(!start_hub.is_bound()) {
		koord bushalt = start_harbour+start_dx;
		const koord town_road = find_place_for_hub( start_stadt );
		// first: built street to harbour
		if(town_road!=bushalt) {
			wegbauer_t bauigel(welt, this);
			// no bridges => otherwise first tile might be bridge start ...
			bauigel.route_fuer( wegbauer_t::strasse, wegbauer_t::weg_search( road_wt, 25, welt->get_timeline_year_month(), weg_t::type_flat ), tunnelbauer_t::find_tunnel(road_wt,road_vehicle->gib_geschw(),welt->get_timeline_year_month()), NULL );
			bauigel.set_keep_existing_faster_ways(true);
			bauigel.set_keep_city_roads(true);
			bauigel.set_maximum(10000);
			bauigel.calc_route( welt->lookup_kartenboden(bushalt)->gib_pos(), welt->lookup_kartenboden(town_road)->gib_pos() );
			if(bauigel.max_n <= 1) {
				return false;
			}
			bauigel.baue();
		}
	}
	if(!end_hub.is_bound()) {
		koord bushalt = end_harbour+end_dx;
		const koord town_road = find_place_for_hub( end_stadt );
		// first: built street to harbour
		if(town_road!=bushalt) {
			wegbauer_t bauigel(welt, this);
			// no bridges => otherwise first tile might be bridge start ...
			bauigel.route_fuer( wegbauer_t::strasse, wegbauer_t::weg_search( road_wt, 25, welt->get_timeline_year_month(), weg_t::type_flat ), tunnelbauer_t::find_tunnel(road_wt,road_vehicle->gib_geschw(),welt->get_timeline_year_month()), NULL );
			bauigel.set_keep_existing_faster_ways(true);
			bauigel.set_keep_city_roads(true);
			bauigel.set_maximum(10000);
			bauigel.calc_route( welt->lookup_kartenboden(bushalt)->gib_pos(), welt->lookup_kartenboden(town_road)->gib_pos() );
			if(bauigel.max_n <= 1) {
				return false;
			}
			bauigel.baue();
		}
	}
	// now built the stops ... (since the roads were ok!)
	if(!start_hub.is_bound()) {
		/* first we must built the bus stop, since this will be the default stop for all our buses
		 * we want to keep the name of a dock, thus wil will create it beforehand
		 */
		koord bushalt = start_harbour+start_dx;
		const haus_besch_t* busstop_besch = hausbauer_t::gib_random_station(haus_besch_t::generic_stop, road_wt, welt->get_timeline_year_month(), haltestelle_t::PAX );
		// now built the bus stop
		if(!call_general_tool( WKZ_STATION, bushalt, busstop_besch->gib_name() )) {
			return false;
		}
		// and change name to dock ...
		halthandle_t halt = welt->lookup(bushalt)->gib_halt();
		char *name = halt->create_name(bushalt, "Dock");
		halt->setze_name( name );
		free(name);
		// finally built the dock
		const haus_besch_t* dock_besch = hausbauer_t::gib_random_station(haus_besch_t::hafen, water_wt, welt->get_timeline_year_month(), 0);
		welt->lookup_kartenboden(start_harbour)->obj_loesche_alle(this);
		call_general_tool( WKZ_STATION, start_harbour, dock_besch->gib_name() );
		start_hub = welt->lookup(start_harbour)->gib_halt();
		// eventually we must built a hub in the next town
		start_connect_hub = get_our_hub( start_stadt );
		if(!start_connect_hub.is_bound()) {
			koord sch = find_place_for_hub( start_stadt );
			call_general_tool( WKZ_STATION, sch, busstop_besch->gib_name() );
			start_connect_hub = get_our_hub( start_stadt );
			assert( start_connect_hub.is_bound() );
		}
	}
	if(!end_hub.is_bound()) {
		/* agian we must built the bus stop first, since this will be the default stop for all our buses
		 * we want to keep the name of a dock, thus wil will create it beforehand
		 */
		koord bushalt = end_harbour+end_dx;
		const haus_besch_t* busstop_besch = hausbauer_t::gib_random_station(haus_besch_t::generic_stop, road_wt, welt->get_timeline_year_month(), haltestelle_t::PAX );
		// now built the bus stop
		if(!call_general_tool( WKZ_STATION, bushalt, busstop_besch->gib_name() )) {
			return false;
		}
		// and change name to dock ...
		halthandle_t halt = welt->lookup(bushalt)->gib_halt();
		char *name = halt->create_name(bushalt, "Dock");
		halt->setze_name( name );
		free(name);
		// finally built the dock
		const haus_besch_t* dock_besch = hausbauer_t::gib_random_station(haus_besch_t::hafen, water_wt, welt->get_timeline_year_month(), 0 );
		welt->lookup_kartenboden(end_harbour)->obj_loesche_alle(this);
		call_general_tool( WKZ_STATION, end_harbour, dock_besch->gib_name() );
		end_hub = welt->lookup(end_harbour)->gib_halt();
		// eventually we must built a hub in the next town
		end_connect_hub = get_our_hub( end_stadt );
		if(!end_connect_hub.is_bound()) {
			koord ech = find_place_for_hub( end_stadt );
			call_general_tool( WKZ_STATION, ech, busstop_besch->gib_name() );
			end_connect_hub = get_our_hub( end_stadt );
			assert( end_connect_hub.is_bound() );
		}
	}

	// now we have harbour => find start position for ships
	koord pos1 = start_harbour-start_dx;
	koord start_pos = pos1;
	for(  int y = pos1.y-welt->gib_einstellungen()->gib_station_coverage();  y<=pos1.y+welt->gib_einstellungen()->gib_station_coverage();  y++  ) {
		for(  int x = pos1.x-welt->gib_einstellungen()->gib_station_coverage();  x<=pos1.x+welt->gib_einstellungen()->gib_station_coverage();  x++  ) {
			koord p(x,y);
			const planquadrat_t *plan = welt->lookup(p);
			if(plan) {
				grund_t *gr = plan->gib_kartenboden();
				if(  gr->ist_wasser()  &&  !gr->gib_halt().is_bound()  ) {
					if(plan->get_haltlist_count()>=1  &&  plan->get_haltlist()[0]==start_hub  &&  abs_distance(start_pos,end_harbour)>abs_distance(p,end_harbour)) {
						start_pos = p;
					}
				}
			}
		}
	}
	// now we have harbour => find start position for ships
	pos1 = end_harbour-end_dx;
	koord end_pos = pos1;
	for(  int y = pos1.y-welt->gib_einstellungen()->gib_station_coverage();  y<=pos1.y+welt->gib_einstellungen()->gib_station_coverage();  y++  ) {
		for(  int x = pos1.x-welt->gib_einstellungen()->gib_station_coverage();  x<=pos1.x+welt->gib_einstellungen()->gib_station_coverage();  x++  ) {
			koord p(x,y);
			const planquadrat_t *plan = welt->lookup(p);
			if(plan) {
				grund_t *gr = plan->gib_kartenboden();
				if(  gr->ist_wasser()  &&  !gr->gib_halt().is_bound()  ) {
					if(plan->get_haltlist_count()>=1  &&  plan->get_haltlist()[0]==end_hub  &&  abs_distance(end_pos,start_harbour)>abs_distance(p,start_harbour)) {
						end_pos = p;
					}
				}
			}
		}
	}

	// since 86.01 we use lines for vehicles ...
	fahrplan_t *fpl=new schifffahrplan_t();
	fpl->append( welt->lookup_kartenboden(start_pos), 0, 0 );
	fpl->append( welt->lookup_kartenboden(end_pos), 90, 0 );
	fpl->aktuell = 1;
	linehandle_t line=simlinemgmt.create_line(simline_t::shipline,fpl);
	delete fpl;

	// now create one plane
	vehikel_t* v = vehikelbauer_t::baue( koord3d(start_pos,welt->gib_grundwasser()), this, NULL, v_besch);
	convoi_t* cnv = new convoi_t(this);
	cnv->setze_name(v->gib_besch()->gib_name());
	cnv->add_vehikel( v );
	welt->sync_add( cnv );
	cnv->set_line(line);
	cnv->start();

	// eventually build a shuttle bus ...
	if(start_connect_hub.is_bound()  &&  start_connect_hub!=start_hub) {
		koord stops[2] = { start_harbour+start_dx, start_connect_hub->gib_basis_pos() };
		create_bus_transport_vehikel( stops[1], 1, stops, 2, false );
	}

	// eventually build a airport shuttle bus ...
	if(end_connect_hub.is_bound()  &&  end_connect_hub!=end_hub) {
		koord stops[2] = { end_harbour+end_dx, end_connect_hub->gib_basis_pos() };
		create_bus_transport_vehikel( stops[1], 1, stops, 2, false );
	}

	return true;
}



halthandle_t ai_passenger_t::build_airport(const stadt_t* city, koord pos, int rotation)
{
	// not too close to border?
	if(pos.x<6  ||  pos.y<6  ||  pos.x+3+6>=welt->gib_groesse_x()  ||  pos.y+3+6>=welt->gib_groesse_y()  ) {
		return halthandle_t();
	}
	// ok, not prematurely doomed
	bool needs_hub=false;
	// can we built airports at all?
	const weg_besch_t *taxi_besch = wegbauer_t::weg_search( air_wt, 25, welt->get_timeline_year_month(), weg_t::type_flat );
	const weg_besch_t *runway_besch = wegbauer_t::weg_search( air_wt, 250, welt->get_timeline_year_month(), weg_t::type_elevated );
	if(taxi_besch==NULL  ||  runway_besch==NULL) {
		return halthandle_t();
	}
	// first, check if at least one tile is within city limits
	const koord lo = city->get_linksoben();
	const koord ru = city->get_rechtsunten();
	koord size(3-1,3-1);
	// make sure pos is within city limits!
	if(pos.x<lo.x) {
		if(pos.x+size.x<lo.x) {
			// not within limits!
			needs_hub = true;
		}
		pos.x += size.x;
		size.x = -size.x;
	}
	if(pos.y<lo.y) {
		if(pos.y+size.y<lo.y) {
			// not within limits!
			needs_hub = true;
		}
		pos.y += size.y;
		size.y = -size.y;
	}
	if(pos.x>ru.x) {
		// not within limits!
		needs_hub = true;
	}
	if(pos.y>ru.y) {
		// not within limits!
		needs_hub = true;
	}
	// find coord to connect in the town
	halthandle_t hub = get_our_hub( city );
	const koord town_road = hub.is_bound() ? hub->gib_basis_pos() : find_place_for_hub( city );
	if(  town_road==koord::invalid) {
		return halthandle_t();
	}
	// ok, now we could built it => flatten the land
	sint16 h = max( welt->gib_grundwasser()+Z_TILE_STEP, welt->lookup_kartenboden(pos)->gib_hoehe() );
	const koord dx( size.x/2, size.y/2 );
	for(  sint16 i=0;  i!=size.y+dx.y;  i+=dx.y  ) {
		for( sint16 j=0;  j!=size.x+dx.x;  j+=dx.x  ) {
			if(!welt->ebne_planquadrat(this,pos+koord(j,i),h)) {
				return halthandle_t();
			}
		}
	}
	// now taxiways
	wegbauer_t bauigel(welt, this);
	// 3x3 layout, first we make the taxiway cross
	koord center=pos+dx;
	bauigel.route_fuer( wegbauer_t::luft, taxi_besch, NULL, NULL );
	bauigel.calc_straight_route( welt->lookup_kartenboden(center+koord::nord)->gib_pos(), welt->lookup_kartenboden(center+koord::sued)->gib_pos() );
	assert(bauigel.max_n > 1);
	bauigel.baue();
	bauigel.route_fuer( wegbauer_t::luft, taxi_besch, NULL, NULL );
	bauigel.calc_straight_route( welt->lookup_kartenboden(center+koord::west)->gib_pos(), welt->lookup_kartenboden(center+koord::ost)->gib_pos() );
	assert(bauigel.max_n > 1);
	bauigel.baue();
	// now try to connect one of the corners with a road
	koord bushalt = koord::invalid, runwaystart, runwayend;
	koord trypos[4] = { koord(0,0), koord(size.x,0), koord(0,size.y), koord(size.x,size.y) };
	sint32 lenght=9999;
	rotation=-1;

	bauigel.route_fuer( wegbauer_t::strasse, wegbauer_t::weg_search( road_wt, 25, welt->get_timeline_year_month(), weg_t::type_flat ), tunnelbauer_t::find_tunnel(road_wt,road_vehicle->gib_geschw(),welt->get_timeline_year_month()), brueckenbauer_t::find_bridge(road_wt,road_vehicle->gib_geschw(),welt->get_timeline_year_month()) );
	bauigel.set_keep_existing_faster_ways(true);
	bauigel.set_keep_city_roads(true);
	bauigel.set_maximum(10000);

	// find the closest one to town ...
	for(  int i=0;  i<4;  i++  ) {
		bushalt = pos+trypos[i];
		bauigel.calc_route(welt->lookup_kartenboden(bushalt)->gib_pos(),welt->lookup_kartenboden(town_road)->gib_pos());
		// no road => try next
		if(  bauigel.max_n>=1  &&   bauigel.max_n<lenght  ) {
			rotation = i;
			lenght = bauigel.max_n;
		}
	}

	if(rotation==-1) {
		// if we every get here that means no connection road => remove airport
		welt->lookup_kartenboden(center+koord::nord)->remove_everything_from_way( this, air_wt, ribi_t::keine );
		welt->lookup_kartenboden(center+koord::sued)->remove_everything_from_way( this, air_wt, ribi_t::keine );
		welt->lookup_kartenboden(center+koord::west)->remove_everything_from_way( this, air_wt, ribi_t::keine );
		welt->lookup_kartenboden(center+koord::ost)->remove_everything_from_way( this, air_wt, ribi_t::keine );
		welt->lookup_kartenboden(center)->remove_everything_from_way( this, air_wt, ribi_t::alle );
		return halthandle_t();
	}

	bushalt = pos+trypos[rotation];
	bauigel.calc_route(welt->lookup_kartenboden(bushalt)->gib_pos(),welt->lookup_kartenboden(town_road)->gib_pos());
	bauigel.baue();
	// now the busstop (our next hub ... )
	const haus_besch_t* busstop_besch = hausbauer_t::gib_random_station(haus_besch_t::generic_stop, road_wt, welt->get_timeline_year_month(), haltestelle_t::PAX );
	// get an airport name (even though the hub is the bus stop ... )
	// now built the bus stop
	if(!call_general_tool( WKZ_STATION, bushalt, busstop_besch->gib_name() )) {
		welt->lookup_kartenboden(center+koord::nord)->remove_everything_from_way( this, air_wt, ribi_t::keine );
		welt->lookup_kartenboden(center+koord::sued)->remove_everything_from_way( this, air_wt, ribi_t::keine );
		welt->lookup_kartenboden(center+koord::west)->remove_everything_from_way( this, air_wt, ribi_t::keine );
		welt->lookup_kartenboden(center+koord::ost)->remove_everything_from_way( this, air_wt, ribi_t::keine );
		welt->lookup_kartenboden(center)->remove_everything_from_way( this, air_wt, ribi_t::alle );
		return halthandle_t();
	}
	// and change name to airport ...
	halthandle_t halt = welt->lookup(bushalt)->gib_halt();
	char *name = halt->create_name( bushalt, "Airport" );
	halt->setze_name( name );
	free(name);
	// built also runway now ...
	bauigel.route_fuer( wegbauer_t::luft, runway_besch, NULL, NULL );
	bauigel.calc_straight_route( welt->lookup_kartenboden(pos+trypos[rotation==0?3:0])->gib_pos(), welt->lookup_kartenboden(pos+trypos[1+(rotation&1)])->gib_pos() );
	assert(bauigel.max_n > 1);
	bauigel.baue();
	// now the airstops (only on single tiles, this will always work
	const haus_besch_t* airstop_besch = hausbauer_t::gib_random_station(haus_besch_t::generic_stop, air_wt, welt->get_timeline_year_month(), 0 );
	for(  int i=0;  i<4;  i++  ) {
		if(  abs_distance(center+koord::nsow[i],bushalt)==1  &&  ribi_t::ist_einfach( welt->lookup_kartenboden(center+koord::nsow[i])->gib_weg_ribi_unmasked(air_wt) )  ) {
			call_general_tool( WKZ_STATION, center+koord::nsow[i], airstop_besch->gib_name() );
		}
	}
	// and now the one far away ...
	for(  int i=0;  i<4;  i++  ) {
		if(  abs_distance(center+koord::nsow[i],bushalt)>1  &&  ribi_t::ist_einfach( welt->lookup_kartenboden(center+koord::nsow[i])->gib_weg_ribi_unmasked(air_wt) )  ) {
			call_general_tool( WKZ_STATION, center+koord::nsow[i], airstop_besch->gib_name() );
		}
	}
	// sucess
	return halt;
}



static koord find_airport_pos(karte_t* welt, const stadt_t *s )
{
	koord bestpos = koord::invalid, k;
	sint32 bestdist = 999999;

	// try to find an airport place as close to the city as possible
	for(  k.y=max(6,s->get_linksoben().y-10); k.y<=min(welt->gib_groesse_y()-3-6,s->get_rechtsunten().y+10); k.y++  ) {
		for(  k.x=max(6,s->get_linksoben().x-25); k.x<=min(welt->gib_groesse_x()-3-6,s->get_rechtsunten().x+10); k.x++  ) {
			sint32 testdist = abs_distance( k, s->gib_pos() );
			if(  testdist<bestdist  ) {
				if(  k.x+2<s->get_linksoben().x  ||  k.y+2<s->get_linksoben().y  ||  k.x>=s->get_rechtsunten().x  ||  k.y>=s->get_rechtsunten().y  ) {
					// malus for out of town
					testdist += 5;
				}
				if(  testdist<bestdist  &&  welt->ist_platz_frei( k, 3, 3, NULL, ALL_CLIMATES )  ) {
					bestpos = k;
					bestdist = testdist;
				}
			}
		}
	}
	return bestpos;
}



/* builts airports and planes
 * @author prissi
 */
bool ai_passenger_t::create_air_transport_vehikel(const stadt_t *start_stadt, const stadt_t *end_stadt)
{
	const vehikel_besch_t *v_besch = vehikelbauer_t::vehikel_search(air_wt, welt->get_timeline_year_month(), 10, 900, warenbauer_t::passagiere, false, true );
	if(v_besch==NULL) {
		// no aircraft there
		return false;
	}

	halthandle_t start_hub = get_our_hub( start_stadt );
	halthandle_t start_connect_hub = halthandle_t();
	koord start_airport;
	if(start_hub.is_bound()) {
		if(  (start_hub->get_station_type()&haltestelle_t::airstop)==0  ) {
			start_connect_hub = start_hub;
			start_hub = halthandle_t();
			// is there already one airport next to this town?
			slist_iterator_tpl<warenziel_t>iter(start_connect_hub->gib_warenziele_passenger());
			while(iter.next()) {
				if(iter.get_current().gib_zielhalt()->get_station_type()&haltestelle_t::airstop) {
					start_hub = iter.get_current().gib_zielhalt();
					break;
				}
			}
		}
		else {
			start_airport = start_hub->gib_basis_pos();
		}
	}
	// find an airport place
	if(!start_hub.is_bound()) {
		start_airport = find_airport_pos( welt, start_stadt );
		if(start_airport==koord::invalid) {
			// sorry, no suitable place
			return false;
		}
	}
	// same for end
	halthandle_t end_hub = get_our_hub( end_stadt );
	halthandle_t end_connect_hub = halthandle_t();
	koord end_airport;
	if(end_hub.is_bound()) {
		if(  (end_hub->get_station_type()&haltestelle_t::airstop)==0  ) {
			end_connect_hub = end_hub;
			end_hub = halthandle_t();
			// is there already one airport next to this town?
			slist_iterator_tpl<warenziel_t>iter(end_connect_hub->gib_warenziele_passenger());
			while(iter.next()) {
				if(iter.get_current().gib_zielhalt()->get_station_type()&haltestelle_t::airstop) {
					end_hub = iter.get_current().gib_zielhalt();
					break;
				}
			}
		}
		else {
			end_airport = end_hub->gib_basis_pos();
		}
	}
	if(!end_hub.is_bound()) {
		// find an airport place
		end_airport = find_airport_pos( welt, end_stadt );
		if(end_airport==koord::invalid) {
			// sorry, no suitable place
			return false;
		}
	}
	// eventually construct them
	if(start_airport!=koord::invalid  &&  end_airport!=koord::invalid) {
		// built the airport if neccessary
		if(!start_hub.is_bound()) {
			start_hub = build_airport(start_stadt, start_airport, true);
			if(!start_hub.is_bound()) {
				return false;
			}
			start_connect_hub = get_our_hub( start_stadt );
			if(!start_connect_hub.is_bound()) {
				koord sch = find_place_for_hub( start_stadt );
				// probably we must construct a city hub, since the airport is outside of the city limits
				const haus_besch_t* busstop_besch = hausbauer_t::gib_random_station(haus_besch_t::generic_stop, road_wt, welt->get_timeline_year_month(), haltestelle_t::PAX );
				call_general_tool( WKZ_STATION, sch, busstop_besch->gib_name() );
				start_connect_hub = get_our_hub( start_stadt );
				assert( start_connect_hub.is_bound() );
			}
		}
		if(!end_hub.is_bound()) {
			end_hub = build_airport(end_stadt, end_airport, true);
			if(!end_hub.is_bound()) {
				if(start_hub->gib_warenziele_passenger()->count()==0) {
					// remove airport busstop
					welt->lookup_kartenboden(start_hub->gib_basis_pos())->remove_everything_from_way( this, road_wt, ribi_t::keine );
					koord center = start_hub->gib_basis_pos() + koord( welt->lookup_kartenboden(start_hub->gib_basis_pos())->gib_weg_ribi_unmasked( air_wt ) );
					// now the remaining taxi-/runways
					for( sint16 y=center.y-1;  y<=center.y+1;  y++  ) {
						for( sint16 x=center.x-1;  x<=center.x+1;  x++  ) {
							welt->lookup_kartenboden(koord(x,y))->remove_everything_from_way( this, air_wt, ribi_t::keine );
						}
					}
				}
				return false;
			}
			end_connect_hub = get_our_hub( end_stadt );
			if(!end_connect_hub.is_bound()) {
				koord ech = find_place_for_hub( end_stadt );
				// probably we must construct a city hub, since the airport is outside of the city limits
				const haus_besch_t* busstop_besch = hausbauer_t::gib_random_station(haus_besch_t::generic_stop, road_wt, welt->get_timeline_year_month(), haltestelle_t::PAX );
				call_general_tool( WKZ_STATION, ech, busstop_besch->gib_name() );
				end_connect_hub = get_our_hub( end_stadt );
				assert( end_connect_hub.is_bound() );
			}
		}
	}
	// now we have aiports (albeit first tile is bus stop)
	const grund_t *start = start_hub->find_matching_position(air_wt);
	const grund_t *end = end_hub->find_matching_position(air_wt);

	// since 86.01 we use lines for vehicles ...
	fahrplan_t *fpl=new airfahrplan_t();
	fpl->append( start, 0, 0 );
	fpl->append( end, 90, 0 );
	fpl->aktuell = 1;
	linehandle_t line=simlinemgmt.create_line(simline_t::airline,fpl);
	delete fpl;

	// now create one plane
	vehikel_t* v = vehikelbauer_t::baue( start->gib_pos(), this, NULL, v_besch);
	convoi_t* cnv = new convoi_t(this);
	cnv->setze_name(v->gib_besch()->gib_name());
	cnv->add_vehikel( v );
	welt->sync_add( cnv );
	cnv->set_line(line);
	cnv->start();

	// eventually build a airport shuttle bus ...
	if(start_connect_hub.is_bound()  &&  start_connect_hub!=start_hub) {
		koord stops[2] = { start_hub->gib_basis_pos(), start_connect_hub->gib_basis_pos() };
		create_bus_transport_vehikel( stops[1], 1, stops, 2, false );
	}

	// eventually build a airport shuttle bus ...
	if(end_connect_hub.is_bound()  &&  end_connect_hub!=end_hub) {
		koord stops[2] = { end_hub->gib_basis_pos(), end_connect_hub->gib_basis_pos() };
		create_bus_transport_vehikel( stops[1], 1, stops, 2, false );
	}

	return true;
}



/* creates a more general road transport
 * @author prissi
 */
void ai_passenger_t::create_bus_transport_vehikel(koord startpos2d,int anz_vehikel,koord *stops,int anzahl,bool do_wait)
{
DBG_MESSAGE("ai_passenger_t::create_bus_transport_vehikel()","bus at (%i,%i)",startpos2d.x,startpos2d.y);
	// now start all vehicle one field before, so they load immediately
	koord3d startpos = welt->lookup(startpos2d)->gib_kartenboden()->gib_pos();

	// since 86.01 we use lines for road vehicles ...
	fahrplan_t *fpl=new autofahrplan_t();
	// do not start at current stop => wont work ...
	for(int j=0;  j<anzahl;  j++) {
		fpl->append(welt->lookup(stops[j])->gib_kartenboden(), j == 0 || !do_wait ? 0 : 10);
	}
	fpl->aktuell = (stops[0]==startpos2d);
	linehandle_t line=simlinemgmt.create_line(simline_t::truckline,fpl);
	delete fpl;

	// now create all vehicles as convois
	for(int i=0;  i<anz_vehikel;  i++) {
		vehikel_t* v = vehikelbauer_t::baue(startpos, this, NULL, road_vehicle);
		convoi_t* cnv = new convoi_t(this);
		// V.Meyer: give the new convoi name from first vehicle
		cnv->setze_name(v->gib_besch()->gib_name());
		cnv->add_vehikel( v );

		welt->sync_add( cnv );
		cnv->set_line(line);
		cnv->start();
	}
}


// now we follow all adjacent streets recursively and mark them
// if they below to this stop, then we continue
void
ai_passenger_t::walk_city( linehandle_t &line, grund_t *&start, const int limit )
{
	//maximum number of stops reached?
	if(line->get_fahrplan()->maxi()>=limit)  {
		return;
	}

	ribi_t::ribi ribi = start->gib_weg_ribi(road_wt);

	for(int r=0; r<4; r++) {

		// a way in our direction?
		if(  (ribi & ribi_t::nsow[r])==0  ) {
			continue;
		}

		// ok, if connected, not marked, and not owner by somebody else
		grund_t *to;
		if(start->get_neighbour(to, road_wt, koord::nsow[r] )  &&  !welt->ist_markiert(to)  &&  check_owner(this, to->obj_bei(0)->gib_besitzer())) {

			// ok, here is a valid street tile
			welt->markiere(to);

			// can built a station here
			if(  ribi_t::ist_gerade(to->gib_weg_ribi(road_wt))  ) {

				// find out how many tiles we have covered already
				int covered_tiles=0;
				int house_tiles=0;
				for(  sint16 y=to->gib_pos().y-welt->gib_einstellungen()->gib_station_coverage();  y<=to->gib_pos().y+welt->gib_einstellungen()->gib_station_coverage()+1;  y++  ) {
					for(  sint16 x=to->gib_pos().x-welt->gib_einstellungen()->gib_station_coverage();  x<=to->gib_pos().x+welt->gib_einstellungen()->gib_station_coverage()+1;  x++  ) {
						const planquadrat_t *pl = welt->lookup(koord(x,y));
						// check, if we have a passenger stop already here
						if(pl  &&  pl->get_haltlist_count()>0) {
							const halthandle_t *hl=pl->get_haltlist();
							for( uint8 own=0;  own<pl->get_haltlist_count();  own++  ) {
								if(  hl[own]->is_enabled(warenbauer_t::passagiere)  ) {
									if(  hl[own]->gib_besitzer()==this  ) {
										covered_tiles ++;
										break;
									}
								}
							}
						}
						// check for houses
						if(pl  &&  pl->gib_kartenboden()->gib_typ()==grund_t::fundament) {
							house_tiles++;
						}
					}
				}
				// now decide, if we build here
				// just using the ration of covered tiles versus house tiles
				const int max_tiles = (welt->gib_einstellungen()->gib_station_coverage()*2+1);
				if(  covered_tiles<(max_tiles*max_tiles)/3  &&  house_tiles>=3  ) {
					// ok, lets do it
					const haus_besch_t* bs = hausbauer_t::gib_random_station(haus_besch_t::generic_stop, road_wt, welt->get_timeline_year_month(), haltestelle_t::PAX);
					if(  call_general_tool( WKZ_STATION, to->gib_pos().gib_2d(), bs->gib_name() )  ) {
						//add to line
						line->get_fahrplan()->append(to,0); // no need to register it yet; done automatically, when convois will be assinged
					}
				}
				// start road, but no houses anywhere => stop searching
				if(house_tiles==0) {
					return;
				}
			}
			// now do recursion
			walk_city( line, to, limit );
		}
	}
}



/* tries to cover a city with bus stops that does not overlap much and cover as much as possible
 * returns the line created, if sucessful
 */
void ai_passenger_t::cover_city_with_bus_route(koord start_pos, int number_of_stops)
{
	// nothing in lists
	welt->unmarkiere_alle();

	// and init all stuff for recursion
	grund_t *start = welt->lookup_kartenboden(start_pos);
	linehandle_t line = simlinemgmt.create_line( simline_t::truckline, new autofahrplan_t() );
	line->get_fahrplan()->append(start,0);

	// now create a line
	walk_city( line, start, number_of_stops );

	road_vehicle = vehikelbauer_t::vehikel_search( road_wt, welt->get_timeline_year_month(), 1, 50, warenbauer_t::passagiere, false, false );
	if( line->get_fahrplan()->maxi()>1  ) {
		// success: add a bus to the line
		vehikel_t* v = vehikelbauer_t::baue(start->gib_pos(), this, NULL, road_vehicle);
		convoi_t* cnv = new convoi_t(this);

		cnv->setze_name(v->gib_besch()->gib_name());
		cnv->add_vehikel( v );

		welt->sync_add( cnv );
		cnv->set_line(line);
		cnv->start();
	}
	else {
		simlinemgmt.delete_line( line );
	}
}


// BUS AI
void ai_passenger_t::step()
{
	// needed for schedule of stops ...
	spieler_t::step();

	if(!automat) {
		// I am off ...
		return;
	}

	// one route per month ...
	if(  welt->gib_steps() < next_contruction_steps  ) {
		return;
	}

	switch(state) {
		case NR_INIT:
		{
			// time to update hq?
			built_update_headquarter();

			// assume fail
			state = CHECK_CONVOI;

			/* if we have this little money, we do nothing
			 * The second condition may happen due to extensive replacement operations;
			 * in such a case it is save enough to expand anyway.
			 */
			if(!(konto>0  ||  finance_history_month[0][COST_ASSETS]+konto>welt->gib_einstellungen()->gib_starting_money())  ) {
				return;
			}

			const weighted_vector_tpl<stadt_t*>& staedte = welt->gib_staedte();
			int anzahl = staedte.get_count();
			int offset = (anzahl>1) ? simrand(anzahl-1) : 0;
			// start with previous target
			const stadt_t* last_start_stadt=start_stadt;
			start_stadt = end_stadt;
			end_stadt = NULL;
			end_ausflugsziel = NULL;
			ziel = NULL;
			platz2 = koord::invalid;
			// if no previous town => find one
			if(start_stadt==NULL) {
				// larger start town preferred
				start_stadt = staedte.at_weight( simrand(staedte.get_sum_weight()) );
				offset = staedte.index_of(start_stadt);
			}
DBG_MESSAGE("ai_passenger_t::do_passenger_ki()","using city %s for start",start_stadt->gib_name());
			const halthandle_t start_halt = get_our_hub(start_stadt);
			// find starting place

if(!start_halt.is_bound()) {
	DBG_MESSAGE("ai_passenger_t::do_passenger_ki()","new_hub");
}
			platz1 = start_halt.is_bound() ? start_halt->gib_basis_pos() : find_place_for_hub( start_stadt );
			if(platz1==koord::invalid) {
				return;
			}
DBG_MESSAGE("ai_passenger_t::do_passenger_ki()","using place (%i,%i) for start",platz1.x,platz1.y);

			if(anzahl==1  ||  simrand(3)==0) {
DBG_MESSAGE("ai_passenger_t::do_passenger_ki()","searching attraction");
				// 25 % of all connections are tourist attractions
				const weighted_vector_tpl<gebaeude_t*> &ausflugsziele = welt->gib_ausflugsziele();
				// this way, we are sure, our factory is connected to this town ...
				const weighted_vector_tpl<fabrik_t *> &fabriken = start_stadt->gib_arbeiterziele();
				unsigned	last_dist = 0xFFFFFFFF;
				bool ausflug=simrand(2)!=0;	// holidays first ...
				int ziel_count=ausflug?ausflugsziele.get_count():fabriken.get_count();
				for( int i=0;  i<ziel_count;  i++  ) {
					unsigned	dist;
					koord pos, size;
					if(ausflug) {
						const gebaeude_t* a = ausflugsziele[i];
						if (a->gib_post_level() <= 25) {
							// not a good object to go to ...
							continue;
						}
						pos  = a->gib_pos().gib_2d();
						size = a->gib_tile()->gib_besch()->gib_groesse(a->gib_tile()->gib_layout());
					}
					else {
						const fabrik_t* f = fabriken[i];
						if (f->gib_besch()->gib_pax_level() <= 10) {
							// not a good object to go to ... we want more action ...
							continue;
						}
						pos  = f->gib_pos().gib_2d();
						size = f->gib_besch()->gib_haus()->gib_groesse(f->get_rotate());
					}
					const stadt_t *next_town = welt->suche_naechste_stadt(pos);
					if(next_town==NULL  ||  start_stadt==next_town) {
						// this is either a town already served (so we do not create a new hub)
						// or a lonely point somewhere
						// in any case we do not want to serve this location already
						const koord cov = koord(welt->gib_einstellungen()->gib_station_coverage(),welt->gib_einstellungen()->gib_station_coverage());
						koord test_platz=find_area_for_hub(pos-cov,pos+size+cov,pos);
						if(!is_my_halt(test_platz)) {
							// not served
							dist = abs_distance(platz1,test_platz);
							if(dist+simrand(50)<last_dist  &&   dist>3) {
								// but closer than the others
								if(ausflug) {
									end_ausflugsziel = ausflugsziele[i];
								}
								else {
									ziel = fabriken[i];
								}
								last_dist = dist;
								platz2 = test_platz;
							}
						}
					}
				}
				// test for success
				if(platz2!=koord::invalid) {
					// found something
					state = NR_SAMMLE_ROUTEN;
DBG_MESSAGE("ai_passenger_t::do_passenger_ki()","decision: %s wants to built network between %s and %s",gib_name(),start_stadt->gib_name(),ausflug?end_ausflugsziel->gib_tile()->gib_besch()->gib_name():ziel->gib_name());
				}
			}
			else {
DBG_MESSAGE("ai_passenger_t::do_passenger_ki()","searching town");
				int last_dist = 9999999;
				// find a good route
				for( int i=0;  i<anzahl;  i++  ) {
					const int nr = (i+offset)%anzahl;
					const stadt_t* cur = staedte[nr];
					if(cur!=last_start_stadt  &&  cur!=start_stadt) {
						halthandle_t end_halt = get_our_hub(cur);
						int dist = abs_distance(platz1,cur->gib_pos());
						if(  end_halt.is_bound()  &&  is_connected(platz1,end_halt->gib_basis_pos(),warenbauer_t::passagiere) ) {
							// already connected
							continue;
						}
						// check if more close
						if(  dist<last_dist  ) {
							end_stadt = cur;
							last_dist = dist;
						}
					}
				}
				// ok, found two cities
				if(start_stadt!=NULL  &&  end_stadt!=NULL) {
					state = NR_SAMMLE_ROUTEN;
DBG_MESSAGE("ai_passenger_t::do_passenger_ki()","%s wants to built network between %s and %s",gib_name(),start_stadt->gib_name(),end_stadt->gib_name());
				}
			}
		}
		break;

		// so far only busses
		case NR_SAMMLE_ROUTEN:
		{
			//
			koord end_hub_pos = koord::invalid;
DBG_MESSAGE("ai_passenger_t::do_passenger_ki()","Find hub");
			// also for target (if not tourist attraction!)
			if(end_stadt!=NULL) {
DBG_MESSAGE("ai_passenger_t::do_passenger_ki()","try to built connect to city %p", end_stadt );
				// target is town
				halthandle_t h = get_our_hub(end_stadt);
				if(h.is_bound()) {
					end_hub_pos = h->gib_basis_pos();
				}
				else {
					end_hub_pos = find_place_for_hub( end_stadt );
				}
			}
			else {
				// already found place
				end_hub_pos = platz2;
			}
			// was successful?
			if(end_hub_pos!=koord::invalid) {
				// ok, now we check the vehicle
				platz2 = end_hub_pos;
DBG_MESSAGE("ai_passenger_t::do_passenger_ki()","hub found -> NR_BAUE_ROUTE1");
				state = NR_BAUE_ROUTE1;
			}
			else {
				// no success
DBG_MESSAGE("ai_passenger_t::do_passenger_ki()","no suitable hub found");
				end_stadt = NULL;
				state = CHECK_CONVOI;
			}
		}
		break;

		// get a suitable vehicle
		case NR_BAUE_ROUTE1:
		// wait for construction semaphore
		{
			// we want the fastest we can get!
			road_vehicle = vehikelbauer_t::vehikel_search( road_wt, welt->get_timeline_year_month(), 50, 80, warenbauer_t::passagiere, false, false );
			if(road_vehicle!=NULL) {
				// find the best => AI will never survive
//				road_weg = wegbauer_t::weg_search( road_wt, road_vehicle->gib_geschw(), welt->get_timeline_year_month(),weg_t::type_flat );
				// find the really cheapest road
				road_weg = wegbauer_t::weg_search( road_wt, 10, welt->get_timeline_year_month(), weg_t::type_flat );
				state = NR_BAUE_STRASSEN_ROUTE;
DBG_MESSAGE("ai_passenger_t::do_passenger_ki()","using %s on %s",road_vehicle->gib_name(),road_weg->gib_name());
			}
			else {
				// no success
				end_stadt = NULL;
				state = CHECK_CONVOI;
			}
		}
		break;

		// built a simple road (no bridges, no tunnels)
		case NR_BAUE_STRASSEN_ROUTE:
		{
			state = NR_BAUE_WATER_ROUTE;	// assume failure
			const haus_besch_t* bs = hausbauer_t::gib_random_station(haus_besch_t::generic_stop, road_wt, welt->get_timeline_year_month(), haltestelle_t::PAX);
			if(bs  &&  create_simple_road_transport(platz1, koord(1,1),platz2,koord(1,1),road_weg)  ) {
				// since the road my have led to a crossing at the indended stop position ...
				bool ok = true;
				if(!is_my_halt(platz1)) {
					if(  !call_general_tool( WKZ_STATION, platz1, bs->gib_name() )  ) {
						platz1 = find_area_for_hub( platz1-koord(2,2), platz1+koord(2,2), platz1 );
						ok = call_general_tool( WKZ_STATION, platz1, bs->gib_name() );
					}
				}
				if(  ok  ) {
					if(!is_my_halt(platz2)) {
						if(  !call_general_tool( WKZ_STATION, platz2, bs->gib_name() )  ) {
							platz2 = find_area_for_hub( platz2-koord(2,2), platz2+koord(2,2), platz2 );
							ok = call_general_tool( WKZ_STATION, platz2, bs->gib_name() );
						}
					}
				}
				// still continue?
				if(ok) {
					koord list[2]={ platz1, platz2 };
					// wait only, if target is not a hub but an attraction/factory
					create_bus_transport_vehikel(platz1,1,list,2,end_stadt==NULL);
					state = NR_SUCCESS;
					// tell the player
					cbuffer_t buf(1024);
					if(end_ausflugsziel!=NULL) {
						platz1 = end_ausflugsziel->gib_pos().gib_2d();
						buf.printf(translator::translate("%s now\noffers bus services\nbetween %s\nand attraction\n%s\nat (%i,%i).\n"), gib_name(), start_stadt->gib_name(), make_single_line_string(translator::translate(end_ausflugsziel->gib_tile()->gib_besch()->gib_name()),2), platz1.x, platz1.y );
						end_stadt = start_stadt;
					}
					else if(ziel!=NULL) {
						platz1 = ziel->gib_pos().gib_2d();
						buf.printf( translator::translate("%s now\noffers bus services\nbetween %s\nand factory\n%s\nat (%i,%i).\n"), gib_name(), start_stadt->gib_name(), make_single_line_string(translator::translate(ziel->gib_name()),2), platz1.x, platz1.y );
						end_stadt = start_stadt;
					}
					else {
						buf.printf( translator::translate("Travellers now\nuse %s's\nbusses between\n%s \nand %s.\n"), gib_name(), start_stadt->gib_name(), end_stadt->gib_name() );
						// add two intown routes
						cover_city_with_bus_route(platz1, 6);
						cover_city_with_bus_route(platz2, 6);
					}
					welt->get_message()->add_message((const char *)buf,platz1,message_t::ai,PLAYER_FLAG|player_nr,road_vehicle->gib_basis_bild());
				}
			}
		}
		break;

		case NR_BAUE_WATER_ROUTE:
			if(  end_ausflugsziel == NULL  &&  ship_transport  &&
					create_water_transport_vehikel(start_stadt, end_stadt ? end_stadt->gib_pos() : ziel->gib_pos().gib_2d())) {
				// add two intown routes
				cover_city_with_bus_route( get_our_hub(start_stadt)->gib_basis_pos(), 6);
				if(end_stadt!=NULL) {
					cover_city_with_bus_route( get_our_hub(end_stadt)->gib_basis_pos(), 6);
				}
				else {
					// start again with same town
					end_stadt = start_stadt;
				}
				cbuffer_t buf(1024);
				buf.printf( translator::translate("Ferry service by\n%s\nnow between\n%s \nand %s.\n"), gib_name(), start_stadt->gib_name(), end_stadt->gib_name() );
				welt->get_message()->add_message((const char *)buf,end_stadt->gib_pos(),message_t::ai,PLAYER_FLAG|player_nr,road_vehicle->gib_basis_bild());
				state = NR_SUCCESS;
			}
			else {
				if(  end_ausflugsziel==NULL  &&  ziel==NULL  ) {
					state = NR_BAUE_AIRPORT_ROUTE;
				}
				else {
					state = NR_BAUE_CLEAN_UP;
				}
			}
		break;

		// despite its name: try airplane
		case NR_BAUE_AIRPORT_ROUTE:
			// try airline (if we are wealthy enough) ...
			if(  !air_transport  ||  finance_history_month[1][COST_CASH]<welt->gib_einstellungen()->gib_starting_money()  ||  !create_air_transport_vehikel( start_stadt, end_stadt )) {
				state = NR_BAUE_CLEAN_UP;
			}
			else {
				// add two intown routes
				cover_city_with_bus_route( get_our_hub(start_stadt)->gib_basis_pos(), 6);
				cover_city_with_bus_route( get_our_hub(end_stadt)->gib_basis_pos(), 6);
				cbuffer_t buf(1024);
				buf.printf( translator::translate("Airline service by\n%s\nnow between\n%s \nand %s.\n"), gib_name(), start_stadt->gib_name(), end_stadt->gib_name() );
				welt->get_message()->add_message((const char *)buf,end_stadt->gib_pos(),message_t::ai,PLAYER_FLAG|player_nr,road_vehicle->gib_basis_bild());
				state = NR_SUCCESS;
			}
		break;

		// remove marker etc.
		case NR_BAUE_CLEAN_UP:
			state = CHECK_CONVOI;
			end_stadt = NULL; // otherwise it may always try to built the same route!
		break;

		// successful construction
		case NR_SUCCESS:
		{
			state = CHECK_CONVOI;
			next_contruction_steps = welt->gib_steps() + simrand( construction_speed/16 );
		}
		break;


		// add vehicles to crowded lines
		case CHECK_CONVOI:
		{
			// next time: do something different
			state = NR_INIT;
			next_contruction_steps = welt->gib_steps() + simrand( construction_speed ) + 25;

			vector_tpl<linehandle_t> lines(0);
			simlinemgmt.get_lines( simline_t::line, &lines);
			const uint32 offset = simrand(lines.get_count());
			for (uint32 i = 0;  i<lines.get_count();  i++  ) {
				linehandle_t line = lines[(i+offset)%lines.get_count()];
				if(line->get_linetype()!=simline_t::airline  &&  line->get_linetype()!=simline_t::truckline) {
					continue;
				}

				// remove empty lines
				if(line->count_convoys()==0) {
					simlinemgmt.delete_line(line);
					break;
				}

				// avoid empty schedule ?!?
				assert(line->get_fahrplan()->maxi()>0);

				// made loss with this line
				if(line->get_finance_history(0,LINE_PROFIT)<0) {
					// try to update the vehicles
					if(welt->use_timeline()  &&  line->count_convoys()>1) {
						// do not update unimportant lines with single vehicles
						slist_tpl <convoihandle_t> obsolete;
						uint32 capacity = 0;
						for(  uint i=0;  i<line->count_convoys();  i++  ) {
							convoihandle_t cnv = line->get_convoy(i);
							if(cnv->has_obsolete_vehicles()) {
								obsolete.append(cnv);
								capacity += cnv->gib_vehikel(0)->gib_besch()->gib_zuladung();
							}
						}
						if(capacity>0) {
							// now try to finde new vehicle
							const vehikel_besch_t *v_besch = vehikelbauer_t::vehikel_search( line->get_convoy(0)->gib_vehikel(0)->gib_waytype(), welt->get_current_month(), 50, welt->get_average_speed(line->get_convoy(0)->gib_vehikel(0)->gib_waytype()), warenbauer_t::passagiere, false, false );
							if(  !v_besch->is_retired(welt->get_current_month())  &&  v_besch!=line->get_convoy(0)->gib_vehikel(0)->gib_besch()) {
								// there is a newer one ...
								for(  uint32 new_capacity=0;  capacity>new_capacity;  new_capacity+=v_besch->gib_zuladung()) {
									vehikel_t* v = vehikelbauer_t::baue( line->get_fahrplan()->eintrag[0].pos, this, NULL, v_besch  );
									convoi_t* new_cnv = new convoi_t(this);
									new_cnv->setze_name( v->gib_besch()->gib_name() );
									new_cnv->add_vehikel( v );
									welt->sync_add( new_cnv );
									new_cnv->set_line(line);
									new_cnv->start();
								}
								// delete all old convois
								while(!obsolete.empty()) {
									obsolete.remove_first()->self_destruct();
								}
								return;
							}
						}
					}
				}
				// next: check for stucked convois ...

				sint64	free_cap = line->get_finance_history(0,LINE_CAPACITY);
				sint64	used_cap = line->get_finance_history(0,LINE_TRANSPORTED_GOODS);

				if(free_cap+used_cap==0) {
					continue;
				}

				sint32 ratio = (sint32)((free_cap*100l)/(free_cap+used_cap));

				// next: check for overflowing lines, i.e. running with 3/4 of the capacity
				if(  ratio<10  ) {
					// else add the first convoi again
					vehikel_t* v = vehikelbauer_t::baue( line->get_fahrplan()->eintrag[0].pos, this, NULL, line->get_convoy(0)->gib_vehikel(0)->gib_besch()  );
					convoi_t* new_cnv = new convoi_t(this);
					new_cnv->setze_name( v->gib_besch()->gib_name() );
					new_cnv->add_vehikel( v );
					welt->sync_add( new_cnv );
					new_cnv->set_line( line );
					// on waiting line, wait at alternating stations for load balancing
					if(  line->get_fahrplan()->eintrag[1].ladegrad==90  &&  line->get_linetype()!=simline_t::truckline  &&  (line->count_convoys()&1)==0  ) {
						new_cnv->gib_fahrplan()->eintrag[0].ladegrad = 90;
						new_cnv->gib_fahrplan()->eintrag[1].ladegrad = 0;
					}
					new_cnv->start();
					return;
				}

				// next: check for too many cars, i.e. running with too many cars
				if(  ratio>40  &&  line->count_convoys()>1) {
					// remove one convoi
					line->get_convoy(0)->self_destruct();
					return;
				}
			}
		}
		break;

		default:
			DBG_MESSAGE("ai_passenger_t::do_passenger_ki()",	"Illegal state %d!", state );
			end_stadt = NULL;
			state = NR_INIT;
	}
}



void ai_passenger_t::rdwr(loadsave_t *file)
{
	xml_tag_t t( file, "ai_passenger_t" );

	// first: do all the administration
	spieler_t::rdwr(file);

	// then check, if we have to do something or the game is too old ...
	if(file->get_version()<101000) {
		// ignore saving, reinit on loading
		if(  file->is_loading()  ) {
			next_contruction_steps = welt->gib_steps()+simrand(construction_speed);
		}
		return;
	}

	// now save current state ...
	file->rdwr_enum(state, "");
	file->rdwr_long( construction_speed, "" );
	file->rdwr_bool( air_transport, "" );
	file->rdwr_bool( ship_transport, "" );
	platz1.rdwr( file );
	platz2.rdwr( file );

	if(file->is_saving()) {
		// save current pointers
		sint32 delta_steps = next_contruction_steps-welt->gib_steps();
		file->rdwr_long(delta_steps, " ");
		koord k = start_stadt ? start_stadt->gib_pos() : koord::invalid;
		k.rdwr(file);
		k = end_stadt ? end_stadt->gib_pos() : koord::invalid;
		k.rdwr(file);
		koord3d k3d = end_ausflugsziel ? end_ausflugsziel->gib_pos() : koord3d::invalid;
		k3d.rdwr(file);
		k3d = ziel ? ziel->gib_pos() : koord3d::invalid;
		k3d.rdwr(file);
	}
	else {
		// since steps in loaded game == 0
		file->rdwr_long(next_contruction_steps, " ");
		next_contruction_steps += welt->gib_steps();
		// reinit current pointers
		koord k;
		k.rdwr(file);
		start_stadt = welt->suche_naechste_stadt(k);
		k.rdwr(file);
		end_stadt = welt->suche_naechste_stadt(k);
		koord3d k3d;
		k3d.rdwr(file);
		end_ausflugsziel = welt->lookup(k3d) ? welt->lookup(k3d)->find<gebaeude_t>() : NULL;
		k3d.rdwr(file);
		ziel = fabrik_t::gib_fab( welt, k3d.gib_2d() );
	}
}



/**
 * Dealing with stucked  or lost vehicles:
 * - delete lost ones
 * - ignore stucked ones
 * @author prissi
 * @date 30-Dec-2008
 */
void ai_passenger_t::bescheid_vehikel_problem(convoihandle_t cnv,const koord3d ziel)
{
	switch(cnv->get_state()) {

		case convoi_t::NO_ROUTE:
DBG_MESSAGE("ai_passenger_t::bescheid_vehikel_problem","Vehicle %s can't find a route to (%i,%i)!", cnv->gib_name(),ziel.x,ziel.y);
			if(this==welt->get_active_player()) {
				char buf[256];
				sprintf(buf,translator::translate("Vehicle %s can't find a route!"), cnv->gib_name());
				welt->get_message()->add_message(buf, cnv->gib_pos().gib_2d(),message_t::convoi,PLAYER_FLAG|player_nr,cnv->gib_vehikel(0)->gib_basis_bild());
			}
			else {
				cnv->self_destruct();
			}
			break;

		case convoi_t::WAITING_FOR_CLEARANCE_ONE_MONTH:
		case convoi_t::CAN_START_ONE_MONTH:
DBG_MESSAGE("ai_passenger_t::bescheid_vehikel_problem","Vehicle %s stucked!", cnv->gib_name(),ziel.x,ziel.y);
			if(this==welt->get_active_player()) {
				char buf[256];
				sprintf(buf,translator::translate("Vehicle %s is stucked!"), cnv->gib_name());
				welt->get_message()->add_message(buf, cnv->gib_pos().gib_2d(),message_t::convoi,PLAYER_FLAG|player_nr,cnv->gib_vehikel(0)->gib_basis_bild());
			}
			break;

		default:
DBG_MESSAGE("ai_passenger_t::bescheid_vehikel_problem","Vehicle %s, state %i!", cnv->gib_name(), cnv->get_state());
	}
}
