/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef SIMVERSION_H
#define SIMVERSION_H


#ifndef REVISION
// include external generated revision file
#include "revision.h"
#endif

#define SIM_BUILD_NIGHTLY           0
#define SIM_BUILD_RELEASE_CANDIDATE 1
#define SIM_BUILD_RELEASE           2

#define SIM_VERSION_MAJOR 122
#define SIM_VERSION_MINOR   0
#define SIM_VERSION_PATCH   1
#define SIM_VERSION_BUILD SIM_BUILD_NIGHTLY

// Beware: SAVEGAME minor is often ahead of version minor when there were patches.
// ==> These have no direct connection at all!
#define SIM_SAVE_MINOR      0
#define SIM_SERVER_MINOR    0
// NOTE: increment before next release to enable save/load of new features

#define OTRP_VERSION_MAJOR 41
#define OTRP_VERSION_MINOR 0
#define OTRP_VERSION_PATCH 0
// NOTE: increment OTRP_VERSION_MAJOR when the save data structure changes.

#define MAKEOBJ_VERSION "60.5"
// new factory locations and provisio

#ifndef QUOTEME
#	define QUOTEME_(x) #x
#	define QUOTEME(x)  QUOTEME_(x)
#endif

#if SIM_VERSION_PATCH != 0
#	define SIM_VERSION_PATCH_STRING "." QUOTEME(SIM_VERSION_PATCH)
#else
#	define SIM_VERSION_PATCH_STRING
#endif

#if   SIM_VERSION_BUILD == SIM_BUILD_NIGHTLY
#	define SIM_VERSION_BUILD_STRING " Nightly"
#elif SIM_VERSION_BUILD == SIM_BUILD_RELEASE_CANDIDATE
#	define SIM_VERSION_BUILD_STRING " Release Candidate"
#elif SIM_VERSION_BUILD == SIM_BUILD_RELEASE
#	define SIM_VERSION_BUILD_STRING
#else
#	error invalid SIM_VERSION_BUILD
#endif

#define VERSION_NUMBER QUOTEME(SIM_VERSION_MAJOR) "." QUOTEME(SIM_VERSION_MINOR) SIM_VERSION_PATCH_STRING SIM_VERSION_BUILD_STRING

#define VERSION_DATE __DATE__

#define SAVEGAME_PREFIX  "Simutrans "
#define XML_SAVEGAME_PREFIX  "<?xml version=\"1.0\"?>"

#define SAVEGAME_VER_NR        "0." QUOTEME(SIM_VERSION_MAJOR) "." QUOTEME(SIM_SAVE_MINOR) "." QUOTEME(OTRP_VERSION_MAJOR)
#define SERVER_SAVEGAME_VER_NR "0." QUOTEME(SIM_VERSION_MAJOR) "." QUOTEME(SIM_SERVER_MINOR) "." QUOTEME(OTRP_VERSION_MAJOR)

#define RES_VERSION_NUMBER  0, SIM_VERSION_MAJOR, SIM_VERSION_MINOR, SIM_VERSION_PATCH

#ifdef REVISION
#	define SIM_TITLE_REVISION_STRING " - r" QUOTEME(REVISION)
#else
#	define SIM_TITLE_REVISION_STRING
#endif

#if OTRP_VERSION_PATCH != 0
#	define OTRP_VERSION_MINOR_STRING "." QUOTEME(OTRP_VERSION_MINOR) "." QUOTEME(OTRP_VERSION_PATCH)
#elif OTRP_VERSION_MINOR != 0
#	define OTRP_VERSION_MINOR_STRING "." QUOTEME(OTRP_VERSION_MINOR)
#else
#	define OTRP_VERSION_MINOR_STRING
#endif
# define OTRP_STRING "Simutrans OTRP v" QUOTEME(OTRP_VERSION_MAJOR) OTRP_VERSION_MINOR_STRING

# define UNOFFICIAL_MESSAGE " Unofficial_" QUOTEME(UNOFFICIAL_REVISION)

# define KUTA_MESSAGE QUOTEME(KUTA_REVISION)

#	define SIM_TITLE OTRP_STRING


/*********************** Settings related to network games ********************/

/* Server to announce status to */
#define ANNOUNCE_SERVER "servers.simutrans.org:80"

/* Relative URL of the announce function on server */
#define ANNOUNCE_URL "/announce"

/* Relative URL of the list function on server */
#define ANNOUNCE_LIST_URL "/list?format=csv"

/* url for obtaining the external IP for easz servers */
#define QUERY_ADDR_IP "simutrans-forum.de:80"
#define QUERY_ADDR_IPv4_ONLY "ipv4.simutrans-forum.de:80"

/* Relative URL of the IP function on server */
#define QUERY_ADDR_URL "/get_IP.php"

#endif
