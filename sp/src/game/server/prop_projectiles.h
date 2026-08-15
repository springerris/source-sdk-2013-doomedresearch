#ifndef PROP_PROJECTILES
#define PROP_PROJECTILES
#ifdef _WIN32
#pragma once
#endif

#include "props_shared.h"
#include "baseanimating.h"
#include "physics_bone_follower.h"
#include "player_pickup.h"
#include "positionwatcher.h"


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

enum BALLTYPE {
	YLW,
	YLW_HUGE,
	CYAN

};

#define PROJECTILE_SPEED = 100

extern ConVar func_breakdmg_bullet;
extern ConVar func_breakdmg_club;
extern ConVar func_breakdmg_explosive;

#endif // PROP_PROJECTILES