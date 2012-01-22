#ifndef simversion_h
#define simversion_h

#define MAKEOBJ_VERSION "52"

#define VERSION_NUMBER "111.1"
#define WIDE_VERSION_NUMBER L"-111.1"

#define VERSION_DATE __DATE__

#define SAVEGAME_PREFIX  "Simutrans "
#define XML_SAVEGAME_PREFIX  "<?xml version=\"1.0\"?>"

#define SAVEGAME_VER_NR  "0.111.1"
#define SERVER_SAVEGAME_VER_NR  "0.111.1"

#define RES_VERSION_NUMBER  0, 111, 1, 0


/*********************** Settings related to network games ********************/

/* Server to announce status to */
#define ANNOUNCE_SERVER "servers.simutrans.org:80"

/* Relative URL of the announce function on server */
#define ANNOUNCE_URL "/announce"

/* Relative URL of the list function on server */
#define ANNOUNCE_LIST_URL "/list?format=csv"

/* Name of file to save server listing to temporarily while downloading list */
#define SERVER_LIST_FILE "serverlist.csv"

#endif
