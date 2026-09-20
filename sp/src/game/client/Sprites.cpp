#include "cbase.h"
#include "Sprites.h"
#include "engine/ivmodelinfo.h"

#include "engine/ivdebugoverlay.h"
#include "tier0/memdbgon.h"

#define PLASMASPARK "Weapon_StunStick.Activate"


BEGIN_DATADESC(CPlasmaSprite)
	DEFINE_ENTITYFUNC(BallTouch)
END_DATADESC()

LINK_ENTITY_TO_CLASS(env_plasma_sprite, CPlasmaSprite);

//ConVar* sk_npc_dmg_ar2 = cvar->FindVar("sk_npc_dmg_ar2 ");

void CPlasmaSprite::Precache() {
	PrecacheScriptSound(PLASMASPARK);
	BaseClass::Precache();
}

CPlasmaSprite* CPlasmaSprite::SpriteCreate(const char* pSpriteName, const Vector& origin, bool animate)
{
	CPlasmaSprite* pSprite = CREATE_ENTITY(CPlasmaSprite, "env_plasma_sprite");
	pSprite->SpriteInit(pSpriteName, origin);
	pSprite->SetSolid(SOLID_BBOX);
	pSprite->AddSolidFlags(FSOLID_NOT_SOLID | FSOLID_TRIGGER);
	pSprite->SetTouch(&CPlasmaSprite::BallTouch);
	UTIL_SetSize(pSprite, vec3_origin, vec3_origin);
	pSprite->SetMoveType(MOVETYPE_FLY);
	pSprite->SetCollisionGroup(COLLISION_GROUP_PROJECTILE);
	
	if (animate)
		pSprite->TurnOn();
	//pSprite->AnimateForTime(10, MAX_COORD_FLOAT);
	return pSprite;
}

void CPlasmaSprite::BallTouch(CBaseEntity* pOther)
{
	if (pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS | FSOLID_TRIGGER))
	{
		// Some NPCs are triggers that can take damage (like antlion grubs). We should hit them.
#ifdef MAPBASE
		// But some physics objects that are also triggers (like weapons) shouldn't go through this check.
		// 
		// Note: rpg_missile has the same code, except it properly accounts for weapons in a different way.
		// This was discovered after I implemented this and both work fine, but if this ever causes problems,
		// use rpg_missile's implementation:
		// 
		// if ( pOther->IsSolidFlagSet(FSOLID_TRIGGER|FSOLID_VOLUME_CONTENTS) && pOther->GetCollisionGroup() != COLLISION_GROUP_WEAPON )
		// 
		if (pOther->GetMoveType() == MOVETYPE_NONE && ((pOther->m_takedamage == DAMAGE_NO) || (pOther->m_takedamage == DAMAGE_EVENTS_ONLY)))
#else
		if ((pOther->m_takedamage == DAMAGE_NO) || (pOther->m_takedamage == DAMAGE_EVENTS_ONLY))
#endif
			return;
	}

	if (pOther->m_takedamage)
	{

		if (pOther == GetOwnerEntity()) { return; }
		if (pOther->GetOwnerEntity() == GetOwnerEntity()) { return; }
		SetOwnerEntity(pOther);
		SetSolid(SOLID_NONE);
		SetCollisionGroup(COLLISION_GROUP_DEBRIS);
		pOther->TakeDamage(CTakeDamageInfo(this, this->GetOwnerEntity(), 3, DMG_BURN));
		EmitSound(PLASMASPARK);

		trace_t tr;
		Vector vDirection = GetAbsVelocity();
		VectorNormalize(vDirection);
		UTIL_TraceLine(GetAbsOrigin() - vDirection, GetAbsOrigin() + vDirection * 20, MASK_OPAQUE, NULL, COLLISION_GROUP_NONE, &tr);
		UTIL_DecalTrace(&tr, "RedGlowFade");
		UTIL_ImpactTrace(&tr, DMG_ENERGYBEAM);
		DieIn(0);
		return;
		
	}
	EmitSound(PLASMASPARK);

	trace_t tr;
	Vector vDirection = GetAbsVelocity();
	VectorNormalize(vDirection);
	UTIL_TraceLine(GetAbsOrigin() - vDirection, GetAbsOrigin() + vDirection * 20, MASK_OPAQUE, NULL, COLLISION_GROUP_NONE, &tr);
	UTIL_DecalTrace(&tr, "RedGlowFade");
	UTIL_ImpactTrace(&tr, DMG_ENERGYBEAM);
	
	DieIn(0);
}

BEGIN_DATADESC(CBulletSprite)
DEFINE_ENTITYFUNC(BallTouch)
END_DATADESC()

LINK_ENTITY_TO_CLASS(env_bullet_sprite, CBulletSprite);

void CBulletSprite::Precache() {
	PrecacheScriptSound(PLASMASPARK);
	BaseClass::Precache();
}

CBulletSprite* CBulletSprite::SpriteTrailCreate(const char* pSpriteName, const Vector& origin, bool animate)
{
	CBulletSprite* pSprite = CREATE_ENTITY(CBulletSprite, "env_bullet_sprite");
	pSprite->SpriteInit(pSpriteName, origin);
	pSprite->SetSolid(SOLID_BBOX);
	pSprite->AddSolidFlags(FSOLID_NOT_SOLID | FSOLID_TRIGGER);
	pSprite->SetTouch(&CBulletSprite::BallTouch);
	UTIL_SetSize(pSprite, vec3_origin, vec3_origin);
	pSprite->SetMoveType(MOVETYPE_FLY);
	pSprite->SetCollisionGroup(COLLISION_GROUP_PROJECTILE);

	if (animate)
		pSprite->TurnOn();
	//pSprite->AnimateForTime(10, MAX_COORD_FLOAT);
	return pSprite;
}

void CBulletSprite::BallTouch(CBaseEntity* pOther)
{
	if (pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS | FSOLID_TRIGGER))
	{
		// Some NPCs are triggers that can take damage (like antlion grubs). We should hit them.
#ifdef MAPBASE
		// But some physics objects that are also triggers (like weapons) shouldn't go through this check.
		// 
		// Note: rpg_missile has the same code, except it properly accounts for weapons in a different way.
		// This was discovered after I implemented this and both work fine, but if this ever causes problems,
		// use rpg_missile's implementation:
		// 
		// if ( pOther->IsSolidFlagSet(FSOLID_TRIGGER|FSOLID_VOLUME_CONTENTS) && pOther->GetCollisionGroup() != COLLISION_GROUP_WEAPON )
		// 
		if (pOther->GetMoveType() == MOVETYPE_NONE && ((pOther->m_takedamage == DAMAGE_NO) || (pOther->m_takedamage == DAMAGE_EVENTS_ONLY)))
#else
		if ((pOther->m_takedamage == DAMAGE_NO) || (pOther->m_takedamage == DAMAGE_EVENTS_ONLY))
#endif
			return;
	}

	if (pOther->m_takedamage)
	{

		if (pOther == GetOwnerEntity()) { return; }
		if (pOther->GetOwnerEntity() == GetOwnerEntity()) { return; }
		SetOwnerEntity(pOther);
		SetSolid(SOLID_NONE);
		SetCollisionGroup(COLLISION_GROUP_DEBRIS);
		pOther->TakeDamage(CTakeDamageInfo(this, this->GetOwnerEntity(), 3, DMG_BURN));
		EmitSound(PLASMASPARK);
		SetAbsVelocity(vec3_origin);
		trace_t tr;
		Vector vDirection = GetAbsVelocity();
		VectorNormalize(vDirection);
		UTIL_TraceLine(GetAbsOrigin() - vDirection, GetAbsOrigin() + vDirection * 20, MASK_OPAQUE, NULL, COLLISION_GROUP_NONE, &tr);
		UTIL_DecalTrace(&tr, "SmallScorch");
		//UTIL_ImpactTrace(&tr, DMG_ENERGYBEAM);
		SetAbsVelocity(vec3_origin);
		DieIn(0.25);
		return;

	}
	EmitSound(PLASMASPARK);
	if (pOther->IsWorld() || pOther->IsBSPModel()) {
		trace_t tr;
		Vector vDirection = GetAbsVelocity();
		VectorNormalize(vDirection);
		UTIL_TraceLine(GetAbsOrigin() - vDirection, GetAbsOrigin() + vDirection * 20, MASK_OPAQUE, NULL, COLLISION_GROUP_NONE, &tr);
		UTIL_DecalTrace(&tr, "SmallScorch");
		//UTIL_ImpactTrace(&tr, DMG_ENERGYBEAM);
		SetAbsVelocity(vec3_origin);
		DieIn(0.25);
	}
}
