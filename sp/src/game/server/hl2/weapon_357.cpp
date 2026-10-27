//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		357 - hand gun
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
#include "engine/IEngineSound.h"
#include "te_effect_dispatch.h"
#include "gamestats.h"
#include "eventqueue.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
#include <beam_shared.h>
#include <explode.h>
#include <hl2_player.h>
#include <grenade_ar2.h>
#include <props.h>

//-----------------------------------------------------------------------------
// CWeapon357
//-----------------------------------------------------------------------------

#ifdef MAPBASE
extern acttable_t *GetPistolActtable();
extern int GetPistolActtableCount();
#endif

#define REVOLVER_ALT_COST 20
#define REVOLVER_ALT_LASER "sprites/yellowlaser1.vmt"
#define REVOLVER_ALT_SWEEP 8.0
#define RATIO_APPROX 1.4375 // ratio between the radius of a circle that contains a square and the inscribed circle's 
#define PARTICLENUKE "explosion_huge_h"

class CWeapon357 : public CBaseHLCombatWeapon
{
	DECLARE_CLASS( CWeapon357, CBaseHLCombatWeapon );
public:

	CWeapon357( void );
	void	Precache(void);
	void	PrimaryAttack( void );
	void	SecondaryAttack( void );
	void	TestEntityForDet(CBaseEntity* hitEnt, bool& firstTraceNPC, CBaseEntity* foundExplosive);
	void	ExtraBoomAt(CBaseEntity* hitEnt);
	void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
	bool	Deploy() override;
	bool	Holster(CBaseCombatWeapon* pSwitchingTo);
	float	WeaponAutoAimScale()	{ return 0.6f; }

#ifdef MAPBASE
	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	virtual int	GetMinBurst() { return 1; }
	virtual int	GetMaxBurst() { return 1; }
	virtual float	GetMinRestTime( void ) { return 1.0f; }
	virtual float	GetMaxRestTime( void ) { return 2.5f; }

	virtual float GetFireRate( void ) { return 0.6f; }

	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone = VECTOR_CONE_15DEGREES;
		if (!GetOwner() || !GetOwner()->IsNPC())
			return cone;

		static Vector AllyCone = VECTOR_CONE_2DEGREES;
		static Vector NPCCone = VECTOR_CONE_5DEGREES;

		if( GetOwner()->MyNPCPointer()->IsPlayerAlly() )
		{
			// 357 allies should be cooler
			return AllyCone;
		}

		return NPCCone;
	}

	void	FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, Vector &vecShootOrigin, Vector &vecShootDir );
	void	Operator_ForceNPCFire( CBaseCombatCharacter  *pOperator, bool bSecondary );

	virtual acttable_t		*GetBackupActivityList() { return GetPistolActtable(); }
	virtual int				GetBackupActivityListCount() { return GetPistolActtableCount(); }

	// DR: store alt fire remaining cd here
	float m_flRemainingRecharge;
#endif

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
#ifdef MAPBASE
	DECLARE_ACTTABLE();
#endif
};

LINK_ENTITY_TO_CLASS( weapon_357, CWeapon357 );

PRECACHE_WEAPON_REGISTER( weapon_357 );

IMPLEMENT_SERVERCLASS_ST( CWeapon357, DT_Weapon357 )
END_SEND_TABLE()

BEGIN_DATADESC( CWeapon357 )
END_DATADESC()

#ifdef MAPBASE
acttable_t	CWeapon357::m_acttable[] =
{
#if EXPANDED_HL2_WEAPON_ACTIVITIES
	{ ACT_IDLE,						ACT_IDLE_REVOLVER,					true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_REVOLVER,				true },
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_REVOLVER,			true },
	{ ACT_RELOAD,					ACT_RELOAD_REVOLVER,					true },
	{ ACT_WALK_AIM,					ACT_WALK_AIM_REVOLVER,				true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_REVOLVER,				true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_REVOLVER,	true },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_REVOLVER_LOW,				false },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_REVOLVER_LOW,		false },
	{ ACT_COVER_LOW,				ACT_COVER_REVOLVER_LOW,				false },
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_REVOLVER_LOW,			false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_REVOLVER,			false },
	{ ACT_WALK,						ACT_WALK_REVOLVER,					true },
	{ ACT_RUN,						ACT_RUN_REVOLVER,					true },
#else
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
#endif

	// 
	// Activities ported from weapon_alyxgun below
	// 

	// Readiness activities (not aiming)
#if EXPANDED_HL2_WEAPON_ACTIVITIES
	{ ACT_IDLE_RELAXED,				ACT_IDLE_PISTOL_RELAXED,		false },//never aims
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_PISTOL_STIMULATED,		false },
#else
	{ ACT_IDLE_RELAXED,				ACT_IDLE_PISTOL,				false },//never aims
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_STIMULATED,			false },
#endif
	{ ACT_IDLE_AGITATED,			ACT_IDLE_ANGRY_PISTOL,			false },//always aims
	{ ACT_IDLE_STEALTH,				ACT_IDLE_STEALTH_PISTOL,		false },

#if EXPANDED_HL2_WEAPON_ACTIVITIES
	{ ACT_WALK_RELAXED,				ACT_WALK_PISTOL_RELAXED,		false },//never aims
	{ ACT_WALK_STIMULATED,			ACT_WALK_PISTOL_STIMULATED,		false },
#else
	{ ACT_WALK_RELAXED,				ACT_WALK,						false },//never aims
	{ ACT_WALK_STIMULATED,			ACT_WALK_STIMULATED,			false },
#endif
	{ ACT_WALK_AGITATED,			ACT_WALK_AIM_PISTOL,			false },//always aims
	{ ACT_WALK_STEALTH,				ACT_WALK_STEALTH_PISTOL,		false },
	
#if EXPANDED_HL2_WEAPON_ACTIVITIES
	{ ACT_RUN_RELAXED,				ACT_RUN_PISTOL_RELAXED,			false },//never aims
	{ ACT_RUN_STIMULATED,			ACT_RUN_PISTOL_STIMULATED,		false },
#else
	{ ACT_RUN_RELAXED,				ACT_RUN,						false },//never aims
	{ ACT_RUN_STIMULATED,			ACT_RUN_STIMULATED,				false },
#endif
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

	// Crouch activities
	{ ACT_CROUCHIDLE_STIMULATED,	ACT_CROUCHIDLE_STIMULATED,		false },
	{ ACT_CROUCHIDLE_AIM_STIMULATED,ACT_RANGE_AIM_PISTOL_LOW,		false },//always aims
	{ ACT_CROUCHIDLE_AGITATED,		ACT_RANGE_AIM_PISTOL_LOW,		false },//always aims

	// Readiness translations
	{ ACT_READINESS_RELAXED_TO_STIMULATED, ACT_READINESS_PISTOL_RELAXED_TO_STIMULATED, false },
	{ ACT_READINESS_RELAXED_TO_STIMULATED_WALK, ACT_READINESS_PISTOL_RELAXED_TO_STIMULATED_WALK, false },
	{ ACT_READINESS_AGITATED_TO_STIMULATED, ACT_READINESS_PISTOL_AGITATED_TO_STIMULATED, false },
	{ ACT_READINESS_STIMULATED_TO_RELAXED, ACT_READINESS_PISTOL_STIMULATED_TO_RELAXED, false },

#if EXPANDED_HL2_COVER_ACTIVITIES
	{ ACT_RANGE_AIM_MED,			ACT_RANGE_AIM_REVOLVER_MED,			false },
	{ ACT_RANGE_ATTACK1_MED,		ACT_RANGE_ATTACK_REVOLVER_MED,		false },
#endif

#ifdef MAPBASE
	// HL2:DM activities (for third-person animations in SP)
#if EXPANDED_HL2DM_ACTIVITIES
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_REVOLVER,                    false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_REVOLVER,                    false },
	{ ACT_HL2MP_WALK,					ACT_HL2MP_WALK_REVOLVER,                    false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_REVOLVER,            false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_REVOLVER,            false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_REVOLVER,    false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK2,	ACT_HL2MP_GESTURE_RANGE_ATTACK2_REVOLVER,    false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_REVOLVER,        false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_REVOLVER,                    false },
#else
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_PISTOL,                    false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_PISTOL,                    false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_PISTOL,            false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_PISTOL,            false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_PISTOL,    false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_PISTOL,        false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_PISTOL,                    false },
#endif
#endif
};


IMPLEMENT_ACTTABLE( CWeapon357 );

// Allows Weapon_BackupActivity() to access the 357's activity table.
acttable_t *Get357Acttable()
{
	return CWeapon357::m_acttable;
}

int Get357ActtableCount()
{
	return ARRAYSIZE(CWeapon357::m_acttable);
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon357::CWeapon357( void )
{
	m_flRemainingRecharge = 0.0;
	m_bReloadsSingly	= false;
	m_bFiresUnderwater	= false;

#ifdef MAPBASE
	m_fMinRange1		= 24;
	m_fMaxRange1		= 1000;
	m_fMinRange2		= 24;
	m_fMaxRange2		= 200;
#endif
}

void CWeapon357::Precache(void)
{
	PrecacheParticleSystem(PARTICLENUKE);
	PrecacheModel(REVOLVER_ALT_LASER);
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeapon357::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	switch( pEvent->event )
	{
		case EVENT_WEAPON_RELOAD:
			{
				CEffectData data;

				// Emit six spent shells
				for ( int i = 0; i < 6; i++ )
				{
					data.m_vOrigin = pOwner->WorldSpaceCenter() + RandomVector( -4, 4 );
					data.m_vAngles = QAngle( 90, random->RandomInt( 0, 360 ), 0 );
					data.m_nEntIndex = entindex();

					DispatchEffect( "ShellEject", data );
				}

				break;
			}
#ifdef MAPBASE
		case EVENT_WEAPON_PISTOL_FIRE:
			{
				Vector vecShootOrigin, vecShootDir;
				vecShootOrigin = pOperator->Weapon_ShootPosition();

				CAI_BaseNPC *npc = pOperator->MyNPCPointer();
				ASSERT( npc != NULL );

				vecShootDir = npc->GetActualShootTrajectory( vecShootOrigin );

				FireNPCPrimaryAttack( pOperator, vecShootOrigin, vecShootDir );
			}
			break;
		default:
			BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
			break;
#endif
	}
}

bool CWeapon357::Deploy()
{
	bool deployed = BaseClass::Deploy();
	m_flNextSecondaryAttack = m_flNextSecondaryAttack + m_flRemainingRecharge;
	return deployed;

}

bool CWeapon357::Holster(CBaseCombatWeapon* pSwitchingTo)
{
	float charge  = m_flNextSecondaryAttack - gpGlobals->curtime;
	bool shouldStore = BaseClass::Holster(pSwitchingTo);
	if (shouldStore) {
		m_flRemainingRecharge = charge;
	}
	return shouldStore;
}

#ifdef MAPBASE
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeapon357::FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, Vector &vecShootOrigin, Vector &vecShootDir )
{
	CSoundEnt::InsertSound( SOUND_COMBAT|SOUND_CONTEXT_GUNFIRE, pOperator->GetAbsOrigin(), SOUNDENT_VOLUME_PISTOL, 0.2, pOperator, SOUNDENT_CHANNEL_WEAPON, pOperator->GetEnemy() );

	WeaponSound( SINGLE_NPC );
	pOperator->FireBullets( 1, vecShootOrigin, vecShootDir, VECTOR_CONE_PRECALCULATED, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 1 );
	pOperator->DoMuzzleFlash();
	m_iClip1 = m_iClip1 - 1;
}

//-----------------------------------------------------------------------------
// Purpose: Some things need this. (e.g. the new Force(X)Fire inputs or blindfire actbusy)
//-----------------------------------------------------------------------------
void CWeapon357::Operator_ForceNPCFire( CBaseCombatCharacter *pOperator, bool bSecondary )
{
	// Ensure we have enough rounds in the clip
	m_iClip1++;

	Vector vecShootOrigin, vecShootDir;
	QAngle	angShootDir;
	GetAttachment( LookupAttachment( "muzzle" ), vecShootOrigin, angShootDir );
	AngleVectors( angShootDir, &vecShootDir );
	FireNPCPrimaryAttack( pOperator, vecShootOrigin, vecShootDir );
}
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeapon357::PrimaryAttack( void )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if ( !pPlayer )
	{
		return;
	}

	if ( m_iClip1 <= 0 )
	{
		if ( !m_bFireOnEmpty )
		{
			Reload();
		}
		else
		{
			WeaponSound( EMPTY );
			m_flNextPrimaryAttack = 0.15;
		}

		return;
	}

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );

	WeaponSound( SINGLE );
	pPlayer->DoMuzzleFlash();

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	m_flNextSecondaryAttack = gpGlobals->curtime + GetFireRate();

	m_iClip1--;

	Vector vecSrc		= pPlayer->Weapon_ShootPosition();
	Vector vecAiming	= pPlayer->GetAutoaimVector( AUTOAIM_SCALE_DEFAULT );	

	pPlayer->FireBullets( 1, vecSrc, vecAiming, VECTOR_CONE_1DEGREES * 0.05, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 0 );

	pPlayer->SetMuzzleFlashTime( gpGlobals->curtime + 0.5 );

	//Disorient the player



	pPlayer->ViewPunch(QAngle(-3, 0, 0));

	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 600, 0.2, GetOwner() );

	if ( !m_iClip1 && pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 ); 
	}
}

void CWeapon357::SecondaryAttack(void)
{
	// Only the player fires this way so we can cast
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());

	if (!pPlayer)
	{
		return;
	}

	if (m_iClip1 <= 0)
	{
		if (!m_bFireOnEmpty)
		{
			Reload();
		}
		else
		{
			WeaponSound(EMPTY);
			m_flNextPrimaryAttack = 0.15;
			m_flNextSecondaryAttack = 0.15;
		}

		return;
	}

	if (CHL2_Player* plr = dynamic_cast<CHL2_Player*>(pPlayer)) {
		float power = plr->SuitPower_GetCurrentPercentage();
		if (power < REVOLVER_ALT_COST) {
			WeaponSound(EMPTY);
	
			m_flNextSecondaryAttack = gpGlobals->curtime + 0.15;
			return;
		}
		else
		{
			plr->SuitPower_Drain(REVOLVER_ALT_COST);
		}
	}

	m_iSecondaryAttacks++;
	Vector vecSrc = pPlayer->Weapon_ShootPosition();
	Vector eyeR;
	Vector eyeF;
	pPlayer->EyeVectors(&eyeF, &eyeR, NULL);
	Vector vecAiming = pPlayer->GetAutoaimVector(AUTOAIM_SCALE_DEFAULT);
	trace_t laserTrace;
	trace_t laserTrace2;
	CBaseEntity* endEnt = CreateEntityByName("info_teleport_destination");
	bool wasNPC = false;
	Vector wasAt = vec3_origin;
	UTIL_TraceHull(vecSrc + vecAiming * 32, vecSrc + vecAiming * MAX_TRACE_LENGTH, Vector(-1,-1,-1) * REVOLVER_ALT_SWEEP, Vector(1, 1, 1) * REVOLVER_ALT_SWEEP, (MASK_SHOT | CONTENTS_GRATE), NULL, COLLISION_GROUP_NONE, &laserTrace);
	UTIL_TraceLine(vecSrc + vecAiming * 32, vecSrc + vecAiming * MAX_TRACE_LENGTH, (MASK_SHOT | CONTENTS_GRATE), NULL, COLLISION_GROUP_NONE, &laserTrace2);

	if (laserTrace.DidHitNonWorldEntity()) {
		CBaseEntity* traceHitEnt = laserTrace.m_pEnt;
		wasNPC = traceHitEnt->IsNPC();
		wasAt = traceHitEnt->GetAbsOrigin();
	}
	endEnt->SetAbsOrigin(laserTrace.endpos);
	
	CBaseAnimating* pBeamEnt = pPlayer->GetViewModel();
	int iAttachment = pBeamEnt->LookupAttachment("muzzle");

	Vector vOrigin;
	QAngle vAngle;

	pBeamEnt->GetAttachment(iAttachment, vOrigin, vAngle);
	vOrigin.z = vOrigin.z + 64;
	CBeam* pBeam = CBeam::BeamCreate(REVOLVER_ALT_LASER, 3);
	//CSprite* pSprite = CSprite::SpriteCreate(REVOLVER_ALT_LASER, vOrigin, true);
	//pSprite->SetScale(1);
	pBeam->SetAbsOrigin(vecSrc + eyeR * 2 + eyeF * 16);
	pBeam->EntsInit(pBeam, endEnt);

	pBeam->SetColor(255, 255, 255);
	pBeam->SetScrollRate(25);
	pBeam->SetBrightness(128);
	pBeam->LiveForTime(1.5);
	pBeam->SetFadeLength(1.0);
	
	pBeam->FadeAwayIn(2.0);
	variant_t variant;
	g_EventQueue.AddEvent(endEnt, "Kill", variant, 2.05, pPlayer, pPlayer);
	pPlayer->ViewPunch(QAngle(-20, random->RandomFloat(-1, 1), 0));
	QAngle angles = pPlayer->GetLocalAngles();
	angles.x = angles.x - 2;
	pPlayer->SnapEyeAngles(angles);

	
	//FireBulletsInfo_t fBI = FireBulletsInfo_t(1, vecSrc, vecAiming, vec3_origin, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, false);
	//fBI.SetDamage(fBI.GetDamage() + 10);
	//pPlayer->FireBullets(fBI);

	if (wasNPC) {
		CAI_BaseNPC* bNPCP = dynamic_cast<CAI_BaseNPC*>(laserTrace.m_pEnt);
		CTakeDamageInfo info(this, this->GetOwnerEntity(), 50, DMG_ALWAYSGIB & DMG_BURN);
		bNPCP->Ignite(5);
		bNPCP->TakeDamage(info);
	}
	WeaponSound(SPECIAL1);
	// DR: alright here we will VERY CAREFULLY check that the entity is AN NPC and is DEAD or DYING or doesn't exist ANYMORE
	bool firstTraceNPC = false;
	CBaseEntity* foundExplosive = NULL;
	if (laserTrace.DidHitNonWorldEntity()) {
		

		CBaseEntity* hitEnt = laserTrace.m_pEnt;
		
		if (hitEnt) 
			TestEntityForDet(hitEnt, firstTraceNPC, foundExplosive);
			if (!foundExplosive) {
				// try to explode attached grenade if we hit near it
				foundExplosive = gEntList.FindEntityByClassnameNearest("npc_grenade_frag", laserTrace.endpos, REVOLVER_ALT_SWEEP*RATIO_APPROX);
				if (foundExplosive) {
					if (CBaseGrenade* cBGp = dynamic_cast<CBaseGrenade*>(foundExplosive)) {
						foundExplosive = hitEnt;
						ExtraBoomAt(hitEnt);
						cBGp->Detonate();
					}
				}
			}

		// here we assume the entity got NULLed on the same frame
		else if (wasNPC) {
			ExplosionCreate(hitEnt->GetAbsOrigin() + Vector(0, 0, 16), QAngle(90, 0, 0), GetOwner(), 80.0f, 250,
				SF_ENVEXPLOSION_NODLIGHTS | SF_ENVEXPLOSION_NOSOUND | SF_ENVEXPLOSION_NOFIREBALLSMOKE | SF_ENVEXPLOSION_NOCLAMPMAX, 1300.0f);
		}
	}
	else {
		if (!foundExplosive) {
			// try to explode grenade that could not be reached by the fatass bounding box
			foundExplosive = gEntList.FindEntityByClassnameNearest("npc_grenade_frag", laserTrace.endpos, REVOLVER_ALT_SWEEP * RATIO_APPROX);
#ifdef DEBUG
			debugoverlay->AddBoxOverlay(laserTrace.endpos, Vector(1, 1, 1)* REVOLVER_ALT_SWEEP* RATIO_APPROX / -2, Vector(1, 1, 1)* REVOLVER_ALT_SWEEP* RATIO_APPROX / 2, QAngle(0, 0, 0), 200, 200, 20, 255, 10);
#endif // DEBUG

			
			if (foundExplosive) {
				if (CBaseGrenade* cBGp = dynamic_cast<CBaseGrenade*>(foundExplosive)) {
					foundExplosive = laserTrace.m_pEnt;
					ExtraBoomAt(laserTrace.m_pEnt);
					cBGp->Detonate();
				}
			}
			else {
				foundExplosive = gEntList.FindEntityByClassnameNearest("grenade_ar2", laserTrace.endpos, REVOLVER_ALT_SWEEP * RATIO_APPROX);
				if (foundExplosive) {
					if (CGrenadeAR2* cBGp = dynamic_cast<CGrenadeAR2*>(foundExplosive)) {
						foundExplosive = laserTrace.m_pEnt;
						ExtraBoomAt(laserTrace.m_pEnt);
						cBGp->Detonate();
					}
				}
			}
		}
	}

	if (laserTrace2.DidHitNonWorldEntity()) {
		CBaseEntity* hitEnt = laserTrace2.m_pEnt;

		if (hitEnt)
			TestEntityForDet(hitEnt, firstTraceNPC, foundExplosive);
	}

	m_iSecondaryAttacks++;
	gamestats->Event_WeaponFired(pPlayer, true, GetClassname());
	SendWeaponAnim(ACT_VM_PRIMARYATTACK);
	pPlayer->SetAnimation(PLAYER_ATTACK1);
	m_iClip1--;
	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	m_flNextSecondaryAttack = gpGlobals->curtime + GetFireRate() * 3;
	m_flRemainingRecharge = gpGlobals->curtime - m_flNextSecondaryAttack;
	/*
	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired(pPlayer, true, GetClassname());

	WeaponSound(SINGLE);
	pPlayer->DoMuzzleFlash();

	SendWeaponAnim(ACT_VM_PRIMARYATTACK);
	pPlayer->SetAnimation(PLAYER_ATTACK1);

	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	m_flNextSecondaryAttack = gpGlobals->curtime + GetFireRate()/3;

	m_iClip1--;

	Vector vecSrc = pPlayer->Weapon_ShootPosition();
	Vector vecAiming = pPlayer->GetAutoaimVector(AUTOAIM_SCALE_DEFAULT);
	pPlayer->FireBullets(1, vecSrc, vecAiming, VECTOR_CONE_1DEGREES, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 0);

	pPlayer->SetMuzzleFlashTime(gpGlobals->curtime + 0.5);

	//Disorient the player
	QAngle angles = pPlayer->GetLocalAngles();

	angles.x += random->RandomInt(-1, 1);
	angles.y += random->RandomInt(-1, 1);
	angles.z = 0;

	

	pPlayer->ViewPunch(QAngle(-5, random->RandomFloat(-1, 1), 0));
	*/

	CSoundEnt::InsertSound(SOUND_COMBAT, GetAbsOrigin(), 600, 0.2, GetOwner());

	if (!m_iClip1 && pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
	}

}

void CWeapon357::TestEntityForDet(CBaseEntity* hitEnt, bool& firstTraceNPC, CBaseEntity* foundExplosive)
{
	{
		if (hitEnt->GetFlags() & FL_NPC) {
			firstTraceNPC = true;
			CAI_BaseNPC* bNPCP = dynamic_cast<CAI_BaseNPC*>(hitEnt);
			if (bNPCP) {
				if (!bNPCP->IsAlive()) {
					ExplosionCreate(hitEnt->GetAbsOrigin() + Vector(0, 0, 16), QAngle(90, 0, 0), GetOwner(), 80.0f, 250,
						SF_ENVEXPLOSION_NODLIGHTS | SF_ENVEXPLOSION_NOSOUND | SF_ENVEXPLOSION_NOFIREBALLSMOKE | SF_ENVEXPLOSION_NOCLAMPMAX, 1300.0f);
				}
			}
		}
		else 
		if (!foundExplosive) {
			if (CBaseGrenade* cBGp = dynamic_cast<CBaseGrenade*>(hitEnt)) {
				foundExplosive = hitEnt;
				ExtraBoomAt(hitEnt);
				cBGp->Detonate();
			}
			else if (CBreakableProp* cBPp = dynamic_cast<CBreakableProp*>(hitEnt)) {

				float rad = cBPp->GetExplosiveRadius();
				float dmg = cBPp->GetExplosiveDamage();
	#ifdef DEBUG
				DevMsg("REVOLVER FOUND BREAKABLE PROP WITH EXPLOSIVE RADIUS %f AND DMG %f\n", rad, dmg);
				if (rad > 0.0 && dmg > 0.0 && (cBPp->m_takedamage == DAMAGE_YES)) {
					foundExplosive = hitEnt;
					ExtraBoomAt(hitEnt);
				}
	#endif // DEBUG

			}
		}
	}
}

void CWeapon357::ExtraBoomAt(CBaseEntity* hitEnt)
{
	ExplosionCreate(hitEnt->GetAbsOrigin(), QAngle(90, 0, 0), GetOwner(), 150.0f, 450,
		SF_ENVEXPLOSION_NODLIGHTS | SF_ENVEXPLOSION_NOSOUND | SF_ENVEXPLOSION_NOFIREBALLSMOKE | SF_ENVEXPLOSION_NOCLAMPMAX, 1300.0f);
	DispatchParticleEffect(PARTICLENUKE, hitEnt->GetAbsOrigin(), QAngle(90, 0, 0));
}
