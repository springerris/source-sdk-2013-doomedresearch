//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Pistol - hand gun
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "basehlcombatweapon.h"
#include "basecombatcharacter.h"
#include "ai_basenpc.h"
#include "player.h"
#include "gamerules.h"
#include "in_buttons.h"
#include "soundent.h"
#include "game.h"
#include "vstdlib/random.h"
#include "gamestats.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	PISTOL_FASTEST_REFIRE_TIME		0.1f
#define	PISTOL_FASTEST_DRY_REFIRE_TIME	0.1f
Vector	vecSideShots = Vector(0.02618, 0, 0);
#define	PISTOL_ACCURACY_SHOT_PENALTY_TIME		0.1f	// Applied amount of time each shot adds to the time we must recover from
#define	PISTOL_ACCURACY_MAXIMUM_PENALTY_TIME	0.9f	// Maximum penalty to deal out


//-----------------------------------------------------------------------------
// CWeaponExBlaster
//-----------------------------------------------------------------------------

class CWeaponExBlaster : public CBaseHLCombatWeapon
{
	DECLARE_DATADESC();

public:
	DECLARE_CLASS(CWeaponExBlaster, CBaseHLCombatWeapon);

	CWeaponExBlaster(void);

	DECLARE_SERVERCLASS();

	void	Precache(void);
	void	ItemPostFrame(void);
	void	ItemPreFrame(void);
	void	ItemBusyFrame(void);
	void	PrimaryAttack(void);
	void	AddViewKick(void);
	void	DryFire(void);
	void	Operator_HandleAnimEvent(animevent_t* pEvent, CBaseCombatCharacter* pOperator);
#ifdef MAPBASE
	void	FireNPCPrimaryAttack(CBaseCombatCharacter* pOperator, Vector& vecShootOrigin, Vector& vecShootDir);
	void	Operator_ForceNPCFire(CBaseCombatCharacter* pOperator, bool bSecondary);
#endif

	void	UpdatePenaltyTime(void);

	int		CapabilitiesGet(void) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	Activity	GetPrimaryAttackActivity(void);

	virtual bool Reload(void);

	virtual const Vector& GetBulletSpread(void)
	{
		// Handle NPCs first
		static Vector npcCone = VECTOR_CONE_5DEGREES;
		if (GetOwner() && GetOwner()->IsNPC())
			return npcCone;

		static Vector cone;

		// Old value
		cone = VECTOR_CONE_1DEGREES;


		return cone;
	}

	virtual int	GetMinBurst()
	{
		return 1;
	}

	virtual int	GetMaxBurst()
	{
		return 3;
	}

	virtual float GetFireRate(void)
	{
		return PISTOL_FASTEST_REFIRE_TIME;
	}

#ifdef MAPBASE
	// Pistols are their own backup activities
	virtual acttable_t* GetBackupActivityList() { return NULL; }
	virtual int				GetBackupActivityListCount() { return 0; }
#endif

	DECLARE_ACTTABLE();

private:
	float	m_flSoonestPrimaryAttack;
	float	m_flLastAttackTime;
	float	m_flAccuracyPenalty;
	int		m_nNumShotsFired;
};


IMPLEMENT_SERVERCLASS_ST(CWeaponExBlaster, DT_WeaponExBlaster)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(weapon_exblaster, CWeaponExBlaster);
PRECACHE_WEAPON_REGISTER(weapon_exblaster);

BEGIN_DATADESC(CWeaponExBlaster)

DEFINE_FIELD(m_flSoonestPrimaryAttack, FIELD_TIME),
DEFINE_FIELD(m_flLastAttackTime, FIELD_TIME),
DEFINE_FIELD(m_flAccuracyPenalty, FIELD_FLOAT), //NOTENOTE: This is NOT tracking game time
DEFINE_FIELD(m_nNumShotsFired, FIELD_INTEGER),

END_DATADESC()

acttable_t	CWeaponExBlaster::m_acttable[] =
{
	{ ACT_IDLE,						ACT_IDLE_PISTOL,				true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_PISTOL,			true },
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_PISTOL,		true },
	{ ACT_RELOAD,					ACT_RELOAD_PISTOL,				true },
	{ ACT_WALK_AIM,					ACT_WALK_AIM_PISTOL,			true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_PISTOL,				true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_PISTOL,true },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_PISTOL_LOW,			false },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_PISTOL_LOW,	false },
	{ ACT_COVER_LOW,				ACT_COVER_PISTOL_LOW,			false },
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_PISTOL_LOW,		false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_PISTOL,		false },
	{ ACT_WALK,						ACT_WALK_PISTOL,				false },
	{ ACT_RUN,						ACT_RUN_PISTOL,					false },

#ifdef MAPBASE
	// 
	// Activities ported from weapon_alyxgun below
	// 

#if EXPANDED_HL2_WEAPON_ACTIVITIES
	// Readiness activities (not aiming)
	{ ACT_IDLE_RELAXED,				ACT_IDLE_PISTOL_RELAXED,		false },//never aims
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_PISTOL_STIMULATED,		false },
	{ ACT_IDLE_AGITATED,			ACT_IDLE_ANGRY_PISTOL,			false },//always aims
	{ ACT_IDLE_STEALTH,				ACT_IDLE_STEALTH_PISTOL,		false },

	{ ACT_WALK_RELAXED,				ACT_WALK_PISTOL_RELAXED,		false },//never aims
	{ ACT_WALK_STIMULATED,			ACT_WALK_PISTOL_STIMULATED,		false },
	{ ACT_WALK_AGITATED,			ACT_WALK_AIM_PISTOL,			false },//always aims
	{ ACT_WALK_STEALTH,				ACT_WALK_STEALTH_PISTOL,		false },

	{ ACT_RUN_RELAXED,				ACT_RUN_PISTOL_RELAXED,			false },//never aims
	{ ACT_RUN_STIMULATED,			ACT_RUN_PISTOL_STIMULATED,		false },
	{ ACT_RUN_AGITATED,				ACT_RUN_AIM_PISTOL,				false },//always aims
	{ ACT_RUN_STEALTH,				ACT_RUN_STEALTH_PISTOL,			false },

	// Readiness activities (aiming)
	{ ACT_IDLE_AIM_RELAXED,			ACT_IDLE_PISTOL_RELAXED,		false },//never aims	
	{ ACT_IDLE_AIM_STIMULATED,		ACT_IDLE_AIM_PISTOL_STIMULATED,	false },
	{ ACT_IDLE_AIM_AGITATED,		ACT_IDLE_ANGRY_PISTOL,			false },//always aims
	{ ACT_IDLE_AIM_STEALTH,			ACT_IDLE_STEALTH_PISTOL,		false },

	{ ACT_WALK_AIM_RELAXED,			ACT_WALK_PISTOL_RELAXED,		false },//never aims
	{ ACT_WALK_AIM_STIMULATED,		ACT_WALK_AIM_PISTOL,			false },
	{ ACT_WALK_AIM_AGITATED,		ACT_WALK_AIM_PISTOL,			false },//always aims
	{ ACT_WALK_AIM_STEALTH,			ACT_WALK_AIM_STEALTH_PISTOL,	false },//always aims

	{ ACT_RUN_AIM_RELAXED,			ACT_RUN_PISTOL_RELAXED,			false },//never aims
	{ ACT_RUN_AIM_STIMULATED,		ACT_RUN_AIM_PISTOL,				false },
	{ ACT_RUN_AIM_AGITATED,			ACT_RUN_AIM_PISTOL,				false },//always aims
	{ ACT_RUN_AIM_STEALTH,			ACT_RUN_AIM_STEALTH_PISTOL,		false },//always aims
	//End readiness activities
#else
	// Readiness activities (not aiming)
	{ ACT_IDLE_RELAXED,				ACT_IDLE_PISTOL,				false },//never aims
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_STIMULATED,			false },
	{ ACT_IDLE_AGITATED,			ACT_IDLE_ANGRY_PISTOL,			false },//always aims
	{ ACT_IDLE_STEALTH,				ACT_IDLE_STEALTH_PISTOL,		false },

	{ ACT_WALK_RELAXED,				ACT_WALK,						false },//never aims
	{ ACT_WALK_STIMULATED,			ACT_WALK_STIMULATED,			false },
	{ ACT_WALK_AGITATED,			ACT_WALK_AIM_PISTOL,			false },//always aims
	{ ACT_WALK_STEALTH,				ACT_WALK_STEALTH_PISTOL,		false },

	{ ACT_RUN_RELAXED,				ACT_RUN,						false },//never aims
	{ ACT_RUN_STIMULATED,			ACT_RUN_STIMULATED,				false },
	{ ACT_RUN_AGITATED,				ACT_RUN_AIM_PISTOL,				false },//always aims
	{ ACT_RUN_STEALTH,				ACT_RUN_STEALTH_PISTOL,			false },

	// Readiness activities (aiming)
	{ ACT_IDLE_AIM_RELAXED,			ACT_IDLE_PISTOL,				false },//never aims	
	{ ACT_IDLE_AIM_STIMULATED,		ACT_IDLE_ANGRY_PISTOL,			false },
	{ ACT_IDLE_AIM_AGITATED,		ACT_IDLE_ANGRY_PISTOL,			false },//always aims
	{ ACT_IDLE_AIM_STEALTH,			ACT_IDLE_STEALTH_PISTOL,		false },

	{ ACT_WALK_AIM_RELAXED,			ACT_WALK,						false },//never aims
	{ ACT_WALK_AIM_STIMULATED,		ACT_WALK_AIM_PISTOL,			false },
	{ ACT_WALK_AIM_AGITATED,		ACT_WALK_AIM_PISTOL,			false },//always aims
	{ ACT_WALK_AIM_STEALTH,			ACT_WALK_AIM_STEALTH_PISTOL,	false },//always aims

	{ ACT_RUN_AIM_RELAXED,			ACT_RUN,						false },//never aims
	{ ACT_RUN_AIM_STIMULATED,		ACT_RUN_AIM_PISTOL,				false },
	{ ACT_RUN_AIM_AGITATED,			ACT_RUN_AIM_PISTOL,				false },//always aims
	{ ACT_RUN_AIM_STEALTH,			ACT_RUN_AIM_STEALTH_PISTOL,		false },//always aims
	//End readiness activities
#endif

	// Crouch activities
	{ ACT_CROUCHIDLE_STIMULATED,	ACT_CROUCHIDLE_STIMULATED,		false },
	{ ACT_CROUCHIDLE_AIM_STIMULATED,ACT_RANGE_AIM_PISTOL_LOW,		false },//always aims
	{ ACT_CROUCHIDLE_AGITATED,		ACT_RANGE_AIM_PISTOL_LOW,		false },//always aims

	// Readiness translations
	{ ACT_READINESS_RELAXED_TO_STIMULATED, ACT_READINESS_PISTOL_RELAXED_TO_STIMULATED, false },
	{ ACT_READINESS_RELAXED_TO_STIMULATED_WALK, ACT_READINESS_PISTOL_RELAXED_TO_STIMULATED_WALK, false },
	{ ACT_READINESS_AGITATED_TO_STIMULATED, ACT_READINESS_PISTOL_AGITATED_TO_STIMULATED, false },
	{ ACT_READINESS_STIMULATED_TO_RELAXED, ACT_READINESS_PISTOL_STIMULATED_TO_RELAXED, false },
#endif

#if EXPANDED_HL2_WEAPON_ACTIVITIES
	{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_PISTOL,			true },
	{ ACT_WALK_CROUCH_AIM,			ACT_WALK_CROUCH_AIM_PISTOL,		true },
	{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_PISTOL,			true },
	{ ACT_RUN_CROUCH_AIM,			ACT_RUN_CROUCH_AIM_PISTOL,		true },
#endif

#if EXPANDED_HL2_COVER_ACTIVITIES
	{ ACT_RANGE_AIM_MED,			ACT_RANGE_AIM_PISTOL_MED,			false },
	{ ACT_RANGE_ATTACK1_MED,		ACT_RANGE_ATTACK_PISTOL_MED,		false },

	{ ACT_COVER_WALL_R,			ACT_COVER_WALL_R_PISTOL,		false },
	{ ACT_COVER_WALL_L,			ACT_COVER_WALL_L_PISTOL,		false },
	{ ACT_COVER_WALL_LOW_R,		ACT_COVER_WALL_LOW_R_PISTOL,	false },
	{ ACT_COVER_WALL_LOW_L,		ACT_COVER_WALL_LOW_L_PISTOL,	false },
#endif

#ifdef MAPBASE
	// HL2:DM activities (for third-person animations in SP)
	{ ACT_HL2MP_IDLE,                    ACT_HL2MP_IDLE_PISTOL,                    false },
	{ ACT_HL2MP_RUN,                    ACT_HL2MP_RUN_PISTOL,                    false },
	{ ACT_HL2MP_IDLE_CROUCH,            ACT_HL2MP_IDLE_CROUCH_PISTOL,            false },
	{ ACT_HL2MP_WALK_CROUCH,            ACT_HL2MP_WALK_CROUCH_PISTOL,            false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,    ACT_HL2MP_GESTURE_RANGE_ATTACK_PISTOL,    false },
	{ ACT_HL2MP_GESTURE_RELOAD,            ACT_HL2MP_GESTURE_RELOAD_PISTOL,        false },
	{ ACT_HL2MP_JUMP,                    ACT_HL2MP_JUMP_PISTOL,                    false },
#if EXPANDED_HL2DM_ACTIVITIES
	{ ACT_HL2MP_WALK,					ACT_HL2MP_WALK_PISTOL,						false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK2,	ACT_HL2MP_GESTURE_RANGE_ATTACK2_PISTOL,		false },
#endif
#endif
};


IMPLEMENT_ACTTABLE(CWeaponExBlaster);

#ifdef MAPBASE
// Allows Weapon_BackupActivity() to access the pistol's activity table.
acttable_t* GetExBlasterActtable()
{
	return CWeaponExBlaster::m_acttable;
}

int GetExBlasterActtableCount()
{
	return ARRAYSIZE(CWeaponExBlaster::m_acttable);
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponExBlaster::CWeaponExBlaster(void)
{
	m_flSoonestPrimaryAttack = gpGlobals->curtime;
	m_flAccuracyPenalty = 0.0f;

	m_fMinRange1 = 24;
	m_fMaxRange1 = 1500;
	m_fMinRange2 = 24;
	m_fMaxRange2 = 200;

	m_bFiresUnderwater = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponExBlaster::Precache(void)
{
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeaponExBlaster::Operator_HandleAnimEvent(animevent_t* pEvent, CBaseCombatCharacter* pOperator)
{
	switch (pEvent->event)
	{
	case EVENT_WEAPON_PISTOL_FIRE:
	{
		Vector vecShootOrigin, vecShootDir;
		vecShootOrigin = pOperator->Weapon_ShootPosition();

		CAI_BaseNPC* npc = pOperator->MyNPCPointer();
		ASSERT(npc != NULL);

		vecShootDir = npc->GetActualShootTrajectory(vecShootOrigin);

#ifdef MAPBASE
		FireNPCPrimaryAttack(pOperator, vecShootOrigin, vecShootDir);
#else
		CSoundEnt::InsertSound(SOUND_COMBAT | SOUND_CONTEXT_GUNFIRE, pOperator->GetAbsOrigin(), SOUNDENT_VOLUME_PISTOL, 0.2, pOperator, SOUNDENT_CHANNEL_WEAPON, pOperator->GetEnemy());

		WeaponSound(SINGLE_NPC);
		pOperator->FireBullets(1, vecShootOrigin, vecShootDir, VECTOR_CONE_PRECALCULATED, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 2);
		pOperator->DoMuzzleFlash();
		m_iClip1 = m_iClip1 - 1;
#endif
	}
	break;
	default:
		BaseClass::Operator_HandleAnimEvent(pEvent, pOperator);
		break;
	}
}

#ifdef MAPBASE
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponExBlaster::FireNPCPrimaryAttack(CBaseCombatCharacter* pOperator, Vector& vecShootOrigin, Vector& vecShootDir)
{
	CSoundEnt::InsertSound(SOUND_COMBAT | SOUND_CONTEXT_GUNFIRE, pOperator->GetAbsOrigin(), SOUNDENT_VOLUME_PISTOL, 0.2, pOperator, SOUNDENT_CHANNEL_WEAPON, pOperator->GetEnemy());

	WeaponSound(SINGLE_NPC);
	pOperator->FireBullets(1, vecShootOrigin, vecShootDir, VECTOR_CONE_PRECALCULATED, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 2);
	pOperator->DoMuzzleFlash();
	m_iClip1 = m_iClip1 - 1;
}

//-----------------------------------------------------------------------------
// Purpose: Some things need this. (e.g. the new Force(X)Fire inputs or blindfire actbusy)
//-----------------------------------------------------------------------------
void CWeaponExBlaster::Operator_ForceNPCFire(CBaseCombatCharacter* pOperator, bool bSecondary)
{
	// Ensure we have enough rounds in the clip
	m_iClip1++;

	Vector vecShootOrigin, vecShootDir;
	QAngle	angShootDir;
	GetAttachment(LookupAttachment("muzzle"), vecShootOrigin, angShootDir);
	AngleVectors(angShootDir, &vecShootDir);
	FireNPCPrimaryAttack(pOperator, vecShootOrigin, vecShootDir);
}
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponExBlaster::DryFire(void)
{
	WeaponSound(EMPTY);
	SendWeaponAnim(ACT_VM_DRYFIRE);

	m_flSoonestPrimaryAttack = gpGlobals->curtime + PISTOL_FASTEST_DRY_REFIRE_TIME;
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponExBlaster::PrimaryAttack(void)
{


	if ((gpGlobals->curtime - m_flLastAttackTime) > 0.5f)
	{
		m_nNumShotsFired = 0;
	}
	else
	{
		m_nNumShotsFired++;
	}


	m_flLastAttackTime = gpGlobals->curtime;
	m_flSoonestPrimaryAttack = gpGlobals->curtime + PISTOL_FASTEST_REFIRE_TIME;
	CSoundEnt::InsertSound(SOUND_COMBAT, GetAbsOrigin(), SOUNDENT_VOLUME_PISTOL, 0.2, GetOwner());

	CBasePlayer* pOwner = ToBasePlayer(GetOwner());
	Vector	vecSrc = pOwner->Weapon_ShootPosition();
	//Vector	vecAiming = pOwner->GetAutoaimVector(AUTOAIM_SCALE_DEFAULT);
	//Vector	vecAiming = pOwner->EyeVectors
	Vector vecAiming;
	Vector eyeUp;
	Vector eyeRight;
	pOwner->EyeVectors(&vecAiming, &eyeRight, &eyeUp);
#ifdef DEBUG
	DevMsg("Eye fwdVector (%f %f %f)\n", vecAiming.x, vecAiming.y, vecAiming.z);
#endif // DEBUG

	Vector vecAimingL;
	Vector vecAimingR;
	QAngle aimAngles;
	QAngle aimAngleL;
	QAngle aimAngleR;
	VectorAngles(vecAiming.Normalized(), aimAngles);
	DevMsg("Eye fwdVectorAngles (%f %f %f)\n", aimAngles.x, aimAngles.y, aimAngles.z);
	//VectorAngles(vecAiming, eyeU,p);
	aimAngleL = aimAngles;
	aimAngleR = aimAngles;
	//aimAngleL.y = aimAngleL.y + 3;
	//aimAngleR.y = aimAngleR.y - 3;
	DevMsg("Eye fwdVectorAnglesL (%f %f %f)\n", aimAngleL.x, aimAngleL.y, aimAngleL.z);
	DevMsg("Eye fwdVectorAnglesR (%f %f %f)\n", aimAngleR.x, aimAngleR.y, aimAngleR.z);
	//AngleVectors(aimAngleL, &vecAimingL);
	//AngleVectors(aimAngleR, &vecAimingR);
	vecAimingL = vecAiming - eyeRight * VECTOR_CONE_5DEGREES.z;
	vecAimingR = vecAiming + eyeRight * VECTOR_CONE_5DEGREES.z;
	DevMsg("Eye fwdVectorL (%f %f %f)\n", vecAimingL.x, vecAimingL.y, vecAimingL.z);
	DevMsg("Eye fwdVectorR (%f %f %f)\n", vecAimingR.x, vecAimingR.y, vecAimingR.z);
	VectorAngles(vecAimingL.Normalized(), aimAngles);
	DevMsg("Eye fwdVectorAnglesLAgain (%f %f %f)\n", aimAngleL.x, aimAngleL.y, aimAngleL.z);

	VectorAngles(vecAimingR.Normalized(), aimAngles);
	DevMsg("Eye fwdVectorAnglesRAgain (%f %f %f)\n", aimAngleR.x, aimAngleR.y, aimAngleR.z);
	if (pOwner)
	{
		// Each time the player fires the pistol, reset the view punch. This prevents
		// the aim from 'drifting off' when the player fires very quickly. This may
		// not be the ideal way to achieve this, but it's cheap and it works, which is
		// great for a feature we're evaluating. (sjb)
		pOwner->ViewPunchReset();
	}

	BaseClass::PrimaryAttack();
	vecSrc = pOwner->Weapon_ShootPosition();
	pOwner->FireBullets(1, vecSrc, vecAimingL, Vector(0.001, 0.001, 0.001), MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 0, -1, -1, 0, NULL, true, true);
	pOwner->FireBullets(1, vecSrc, vecAimingR, Vector(0.001, 0.001, 0.001), MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 0, -1, -1, 0, NULL, true, true);

	// Add an accuracy penalty which can move past our maximum penalty time if we're really spastic
	m_flAccuracyPenalty += PISTOL_ACCURACY_SHOT_PENALTY_TIME;


	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired(pOwner, true, GetClassname());
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponExBlaster::UpdatePenaltyTime(void)
{
	CBasePlayer* pOwner = ToBasePlayer(GetOwner());

	if (pOwner == NULL)
		return;

	// Check our penalty time decay
	if (((pOwner->m_nButtons & IN_ATTACK) == false) && (m_flSoonestPrimaryAttack < gpGlobals->curtime))
	{
		m_flAccuracyPenalty -= gpGlobals->frametime;
		m_flAccuracyPenalty = clamp(m_flAccuracyPenalty, 0.0f, PISTOL_ACCURACY_MAXIMUM_PENALTY_TIME);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponExBlaster::ItemPreFrame(void)
{
	UpdatePenaltyTime();

	BaseClass::ItemPreFrame();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponExBlaster::ItemBusyFrame(void)
{
	UpdatePenaltyTime();

	BaseClass::ItemBusyFrame();
}

//-----------------------------------------------------------------------------
// Purpose: Allows firing as fast as button is pressed
//-----------------------------------------------------------------------------
void CWeaponExBlaster::ItemPostFrame(void)
{
	BaseClass::ItemPostFrame();

	if (m_bInReload)
		return;

	CBasePlayer* pOwner = ToBasePlayer(GetOwner());

	if (pOwner == NULL)
		return;

	//Allow a refire as fast as the player can click
	if (((pOwner->m_nButtons & IN_ATTACK) == false) && (m_flSoonestPrimaryAttack < gpGlobals->curtime))
	{
		m_flNextPrimaryAttack = gpGlobals->curtime - 0.1f;
	}
	else if ((pOwner->m_nButtons & IN_ATTACK) && (m_flNextPrimaryAttack < gpGlobals->curtime) && (m_iClip1 <= 0))
	{
		DryFire();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
Activity CWeaponExBlaster::GetPrimaryAttackActivity(void)
{
	if (m_nNumShotsFired < 1)
		return ACT_VM_PRIMARYATTACK;

	if (m_nNumShotsFired < 2)
		return ACT_VM_RECOIL1;

	if (m_nNumShotsFired < 3)
		return ACT_VM_RECOIL2;

	return ACT_VM_RECOIL3;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CWeaponExBlaster::Reload(void)
{
	bool fRet = DefaultReload(GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD);
	if (fRet)
	{
		WeaponSound(RELOAD);
		m_flAccuracyPenalty = 0.0f;
	}
	return fRet;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponExBlaster::AddViewKick(void)
{
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());

	if (pPlayer == NULL)
		return;

	QAngle	viewPunch;

#ifdef MAPBASE
	viewPunch.x = random->RandomFloat(0.25f, 0.5f);
	//viewPunch.x = weapon_pistol_upwards_viewkick.GetBool() ? random->RandomFloat(-0.5f, -0.25f) : random->RandomFloat(0.25f, 0.5f);
#else
	
#endif
	viewPunch.y = random->RandomFloat(-.6f, .6f);
	viewPunch.z = 0.0f;

	//Add it to the view punch
	pPlayer->ViewPunch(viewPunch);
}
