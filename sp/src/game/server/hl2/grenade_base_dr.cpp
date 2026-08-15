//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
//#include "grenade_base_dr.h"
#include "soundent.h"
#include "decals.h"
#include "smoke_trail.h"
#include "hl2_shareddefs.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "particle_parse.h"
#include "particle_system.h"
#include "soundenvelope.h"
#include "ai_utils.h"
#include "te_effect_dispatch.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
/*

ConVar* g_debug_antlion_worker = cvar->FindVar("g_debug_antlion_worker");

float GetCurrentGravity(void);




ConVar* sk_antlion_worker_spit_grenade_dmg = cvar->FindVar("sk_antlion_worker_spit_grenade_dmg");
ConVar* sk_antlion_worker_spit_grenade_radius = cvar->FindVar("sk_antlion_worker_spit_grenade_radius");
ConVar* sk_antlion_worker_spit_grenade_poison_ratio = cvar->FindVar("sk_antlion_worker_spit_grenade_poison_ratio");


class CParticleSystem;

#define SPIT_GRAVITY 600

BEGIN_DATADESC(CBaseGrenadeDR)
DEFINE_FIELD(m_bPlaySound, FIELD_BOOLEAN),
END_DATADESC()

class CGrenadeSlime : public CBaseGrenadeDR
{
	DECLARE_CLASS(CBaseGrenadeDR, CBaseGrenadeDR);

public:
	//CGrenadeSlime(void);

	virtual void		Spawn(void) override;
	virtual void		Precache(void) override;
	virtual void		Event_Killed(const CTakeDamageInfo& info) override;

	virtual	unsigned int	PhysicsSolidMaskForEntity(void) const { return (BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_WATER); }

	void				Detonate(void) override;
	void				Think(void) override;
	void 				GrenadeSlimeTouch(CBaseEntity* pOther);

private:
	DECLARE_DATADESC();

	void	InitHissSound(void);

	CHandle< CParticleSystem >	m_hSpitEffect;
	CSoundPatch* m_pHissSound;
	bool			m_bPlaySound;
};

LINK_ENTITY_TO_CLASS(grenade_slime, CGrenadeSlime);

BEGIN_DATADESC(CGrenadeSlime)
// Function pointers
DEFINE_ENTITYFUNC(GrenadeSlimeTouch),

END_DATADESC()




//CGrenadeSlime::CGrenadeSlime(void) : m_bPlaySound(true), m_pHissSound(NULL)
//{
//}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeSlime::Spawn(void)
{
	Precache();
	SetSolid(SOLID_BBOX);
	SetMoveType(MOVETYPE_FLYGRAVITY);
	SetSolidFlags(FSOLID_NOT_STANDABLE);

	SetModel("models/spitball_large_wobbly.mdl");
	SetModelScale(5.0);
	UTIL_SetSize(this, vec3_origin, vec3_origin);

	SetUse(&CBaseGrenade::DetonateUse);
	SetTouch(&CGrenadeSlime::GrenadeSlimeTouch);
	SetNextThink(gpGlobals->curtime + 0.1f);

	m_flDamage = sk_antlion_worker_spit_grenade_dmg->GetFloat() / 4;
	m_DmgRadius = sk_antlion_worker_spit_grenade_radius->GetFloat() / 4;
	m_takedamage = DAMAGE_NO;
	m_iHealth = 1;

	SetGravity(UTIL_ScaleForGravity(SPIT_GRAVITY));
	SetFriction(0.8f);

	SetCollisionGroup(HL2COLLISION_GROUP_SPIT);

	AddEFlags(EFL_FORCE_CHECK_TRANSMIT);

	// We're self-illuminating, so we don't take or give shadows
	AddEffects(EF_NOSHADOW | EF_NORECEIVESHADOW);

	// Create the dust effect in place
	m_hSpitEffect = (CParticleSystem*)CreateEntityByName("info_particle_system");
	if (m_hSpitEffect != NULL)
	{
		// Setup our basic parameters
		m_hSpitEffect->KeyValue("start_active", "1");
		m_hSpitEffect->KeyValue("effect_name", "antlion_spit_trail");
		m_hSpitEffect->SetParent(this);
		m_hSpitEffect->SetLocalOrigin(vec3_origin);
		DispatchSpawn(m_hSpitEffect);
		if (gpGlobals->curtime > 0.5f)
			m_hSpitEffect->Activate();
	}
}


void CGrenadeSlime::Event_Killed(const CTakeDamageInfo& info)
{
	Detonate();
}

//-----------------------------------------------------------------------------
// Purpose: Handle spitting
//-----------------------------------------------------------------------------
void CGrenadeSlime::GrenadeSlimeTouch(CBaseEntity* pOther)
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

	// Don't hit other spit
	if (pOther->GetCollisionGroup() == HL2COLLISION_GROUP_SPIT)
		return;

	// We want to collide with water
	const trace_t* pTrace = &CBaseEntity::GetTouchTrace();

	// copy out some important things about this trace, because the first TakeDamage
	// call below may cause another trace that overwrites the one global pTrace points
	// at.
	bool bHitWater = ((pTrace->contents & CONTENTS_WATER) != 0);
	CBaseEntity* const pTraceEnt = pTrace->m_pEnt;
	const Vector tracePlaneNormal = pTrace->plane.normal;

	if (bHitWater)
	{
		// Splash!
		CEffectData data;
		data.m_fFlags = 0;
		data.m_vOrigin = pTrace->endpos;
		data.m_vNormal = Vector(0, 0, 1);
		data.m_flScale = 8.0f;

		DispatchEffect("watersplash", data);
	}
	else
	{
		// Make a splat decal
		trace_t* pNewTrace = const_cast<trace_t*>(pTrace);
		UTIL_DecalTrace(pNewTrace, "BeerSplash");
	}

	// Part normal damage, part poison damage
	float poisonratio = sk_antlion_worker_spit_grenade_poison_ratio->GetFloat();

	// Take direct damage if hit
	// NOTE: assume that pTrace is invalidated from this line forward!
	if (pTraceEnt)
	{
		pTraceEnt->TakeDamage(CTakeDamageInfo(this, GetThrower(), m_flDamage * (1.0f - poisonratio), DMG_ACID));
		pTraceEnt->TakeDamage(CTakeDamageInfo(this, GetThrower(), m_flDamage * poisonratio, DMG_POISON));
	}

	CSoundEnt::InsertSound(SOUND_DANGER, GetAbsOrigin(), m_DmgRadius * 2.0f, 0.5f, GetThrower());

	QAngle vecAngles;
	VectorAngles(tracePlaneNormal, vecAngles);

	if (pOther->IsPlayer() || bHitWater)
	{
		// Do a lighter-weight effect if we just hit a player
		DispatchParticleEffect("antlion_spit_player", GetAbsOrigin(), vecAngles);
	}
	else
	{
		DispatchParticleEffect("antlion_spit", GetAbsOrigin(), vecAngles);
	}

	Detonate();
}

void CGrenadeSlime::Detonate(void)
{
	m_takedamage = DAMAGE_NO;

	EmitSound("GrenadeSpit.Hit");

	// Stop our hissing sound
	if (m_pHissSound != NULL)
	{
		CSoundEnvelopeController::GetController().SoundDestroy(m_pHissSound);
		m_pHissSound = NULL;
	}

	if (m_hSpitEffect)
	{
		UTIL_Remove(m_hSpitEffect);
	}

	UTIL_Remove(this);
}

void CGrenadeSlime::InitHissSound(void)
{
	if (m_bPlaySound == false)
		return;

	CSoundEnvelopeController& controller = CSoundEnvelopeController::GetController();
	if (m_pHissSound == NULL)
	{
		CPASAttenuationFilter filter(this);
		m_pHissSound = controller.SoundCreate(filter, entindex(), "NPC_Antlion.PoisonBall");
		controller.Play(m_pHissSound, 1.0f, 100);
	}
}

void CGrenadeSlime::Think(void)
{
	InitHissSound();
	if (m_pHissSound == NULL)
		return;

	// Add a doppler effect to the balls as they travel
	CBaseEntity* pPlayer = AI_GetSinglePlayer();
	if (pPlayer != NULL)
	{
		Vector dir;
		VectorSubtract(pPlayer->GetAbsOrigin(), GetAbsOrigin(), dir);
		VectorNormalize(dir);

		float velReceiver = DotProduct(pPlayer->GetAbsVelocity(), dir);
		float velTransmitter = -DotProduct(GetAbsVelocity(), dir);

		// speed of sound == 13049in/s
		int iPitch = 100 * ((1 - velReceiver / 13049) / (1 + velTransmitter / 13049));

		// clamp pitch shifts
		if (iPitch > 250)
		{
			iPitch = 250;
		}
		if (iPitch < 50)
		{
			iPitch = 50;
		}

		// Set the pitch we've calculated
		CSoundEnvelopeController::GetController().SoundChangePitch(m_pHissSound, iPitch, 0.1f);
	}

	// Set us up to think again shortly
	SetNextThink(gpGlobals->curtime + 0.05f);
}

void CGrenadeSlime::Precache(void)
{
	// m_nSquidSpitSprite = PrecacheModel("sprites/greenglow1.vmt");// client side spittle.

	PrecacheModel("models/spitball_large_wobbly.mdl");

	PrecacheScriptSound("GrenadeSpit.Hit");

	PrecacheParticleSystem("antlion_spit_player");
	PrecacheParticleSystem("antlion_spit");
}

bool CBaseGrenadeDR::DRGetGrenadeVector(CBaseEntity* pEdict, const Vector& vecStartPos, const Vector& vecTarget, Vector* vecOut, float flSpeed, bool bcanFail)
{
	// Try the most direct route
	Vector vecToss = DRVecCheckThrowTolerance(pEdict, vecStartPos, vecTarget, flSpeed, (10.0f * 12.0f), bcanFail);

	// If this failed then try a little faster (flattens the arc)

	if (vecToss == vec3_origin)
	{
		vecToss = DRVecCheckThrowTolerance(pEdict, vecStartPos, vecTarget, flSpeed * 1.5f, (10.0f * 12.0f), bcanFail);
		if (vecToss == vec3_origin)
			return false;
	}


	// Save out the result
	if (vecOut)
	{
		*vecOut = vecToss;
	}

	return true;
}


Vector CBaseGrenadeDR::DRVecCheckThrowTolerance(CBaseEntity* pEdict, const Vector& vecSpot1, Vector vecSpot2, float flSpeed, float flTolerance, bool bcanFail)
{
	flSpeed = MAX(1.0f, flSpeed);

	float flGravity = GetCurrentGravity();

	Vector vecGrenadeVel = (vecSpot2 - vecSpot1);

	// throw at a constant time
	float time = vecGrenadeVel.Length() / flSpeed;
	vecGrenadeVel = vecGrenadeVel * (1.0 / time);

	// adjust upward toss to compensate for gravity loss
	vecGrenadeVel.z += flGravity * time * 0.5;

	Vector vecApex = vecSpot1 + (vecSpot2 - vecSpot1) * 0.5;
	vecApex.z += 0.5 * flGravity * (time * 0.5) * (time * 0.5);


	trace_t tr;
	UTIL_TraceLine(vecSpot1, vecApex, MASK_SOLID, pEdict, COLLISION_GROUP_NONE, &tr);
	if (tr.fraction != 1.0)
	{
		// fail!
		if (g_debug_antlion_worker->GetBool())
		{
			NDebugOverlay::Line(vecSpot1, vecApex, 255, 0, 0, true, 5.0);
		}

		return vec3_origin;
	}

	if (g_debug_antlion_worker->GetBool())
	{
		NDebugOverlay::Line(vecSpot1, vecApex, 0, 255, 0, true, 5.0);
	}

	UTIL_TraceLine(vecApex, vecSpot2, MASK_SOLID_BRUSHONLY, pEdict, COLLISION_GROUP_NONE, &tr);
	if (tr.fraction != 1.0)
	{
		bool bFail = true;
		if (!bcanFail) {
			bFail = false;
		}
		// Didn't make it all the way there, but check if we're within our tolerance range
		if (flTolerance > 0.0f)
		{
			float flNearness = (tr.endpos - vecSpot2).LengthSqr();
			if (flNearness < Square(flTolerance))
			{
				if (g_debug_antlion_worker->GetBool())
				{
					NDebugOverlay::Sphere(tr.endpos, vec3_angle, flTolerance, 0, 255, 0, 0, true, 5.0);
				}

				bFail = false;
			}
		}

		if (bFail)
		{
			if (g_debug_antlion_worker->GetBool())
			{
				NDebugOverlay::Line(vecApex, vecSpot2, 255, 0, 0, true, 5.0);
				NDebugOverlay::Sphere(tr.endpos, vec3_angle, flTolerance, 255, 0, 0, 0, true, 5.0);
			}
			return vec3_origin;
		}
	}

	if (g_debug_antlion_worker->GetBool())
	{
		NDebugOverlay::Line(vecApex, vecSpot2, 0, 255, 0, true, 5.0);
	}

	return vecGrenadeVel;
}

void CBaseGrenadeDR::DRLaunchGrenadeAtTarget(CBaseEntity* pEdict, const Vector& vecSpot1, Vector vecSpot2, float flSpeed, float flTolerance, DRGrenade_t grenadeT, bool bcanFail)
{
	// Try and spit at our target
	Vector	vecToss;
	if (DRGetGrenadeVector(pEdict, vecSpot1, vecSpot2, &vecToss, flSpeed, bcanFail))
	{

	}

	// Find what our vertical theta is to estimate the time we'll impact the ground
	Vector vecToTarget = (vecSpot2 - vecSpot1);
	VectorNormalize(vecToTarget);
	float flVelocity = VectorNormalize(vecToss);
	float flCosTheta = DotProduct(vecToTarget, vecToss);
	float flTime = (vecSpot1 - vecSpot2).Length2D() / (flVelocity * flCosTheta);

	// Emit a sound where this is going to hit so that targets get a chance to act correctly
	CSoundEnt::InsertSound(SOUND_DANGER, vecSpot2, (15 * 12), flTime, pEdict);
	CBaseGrenadeDR* pGrenade = NULL;
	switch (grenadeT) {
	case SLIME:
		pGrenade = (CGrenadeSlime*)CreateEntityByName("grenade_slime");
		break;
	}
	if (pGrenade) {
		pGrenade->SetAbsOrigin(vecSpot1);
		pGrenade->SetAbsAngles(vec3_angle);
		DispatchSpawn(pGrenade);
		if (CBaseCombatCharacter* cbccPtr = dynamic_cast<CBaseCombatCharacter*>(pEdict)) {
			pGrenade->SetThrower(cbccPtr);
		}
		pGrenade->SetOwnerEntity(pEdict);
		pGrenade->SetAbsVelocity(vecToss * flVelocity);

		// Tumble through the air
		pGrenade->SetLocalAngularVelocity(
			QAngle(random->RandomFloat(-250, -500),
				random->RandomFloat(-250, -500),
				random->RandomFloat(-250, -500)));
	}

}
*/