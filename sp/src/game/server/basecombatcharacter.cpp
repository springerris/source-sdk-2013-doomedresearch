//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Base combat character with no AI
//
//=============================================================================//


#include "cbase.h"
#include "hl2_player.h"
#include "baseentity.h"
#include "basecombatcharacter.h"
#include "basecombatweapon.h"
#include "animation.h"
#include "gib.h"
#include "entitylist.h"
#include "gamerules.h"
#include "ai_basenpc.h"
#include "ai_squadslot.h"
#include "ammodef.h"
#include "ndebugoverlay.h"
#include "player.h"
#include "physics.h"
#include "engine/IEngineSound.h"
#include "tier1/strtools.h"
#include "sendproxy.h"
#include "EntityFlame.h"
#include "CRagdollMagnet.h"
#include "IEffects.h"
#include "iservervehicle.h"
#include "igamesystem.h"
#include "icommandline.h"
#include "globals.h"
#include "physics_prop_ragdoll.h"
#include "physics_impact_damage.h"
#include "saverestore_utlvector.h"
#include "eventqueue.h"
#include "world.h"
#include "globalstate.h"
#include "items.h"
#include "movevars_shared.h"
#include "RagdollBoogie.h"
#include "rumble_shared.h"
#include "saverestoretypes.h"
#include "nav_mesh.h"

#ifdef NEXT_BOT
#include "NextBot/NextBotManager.h"
#endif

#ifdef HL2_DLL
#include "weapon_physcannon.h"
#include "hl2_gamerules.h"
#endif

#ifdef PORTAL
	#include "portal_util_shared.h"
	#include "prop_portal_shared.h"
	#include "portal_shareddefs.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef HL2_DLL
extern int	g_interactionBarnacleVictimReleased;
#endif //HL2_DLL
#include <in_buttons.h>
#include <npc_antlion.h>

#include <npc_headcrab.h>

extern ConVar weapon_showproficiency;

ConVar ai_show_hull_attacks( "ai_show_hull_attacks", "0" );
ConVar ai_force_serverside_ragdoll( "ai_force_serverside_ragdoll", "0" );

ConVar nb_last_area_update_tolerance( "nb_last_area_update_tolerance", "4.0", FCVAR_CHEAT, "Distance a character needs to travel in order to invalidate cached area" ); // 4.0 tested as sweet spot (for wanderers, at least). More resulted in little benefit, less quickly diminished benefit [7/31/2008 tom]

#ifdef MAPBASE
// ShouldUseVisibilityCache() is used as an actual function now
ConVar ai_use_visibility_cache( "ai_use_visibility_cache", "1" );
#else
#ifndef _RETAIL
ConVar ai_use_visibility_cache( "ai_use_visibility_cache", "1" );
#define ShouldUseVisibilityCache() ai_use_visibility_cache.GetBool()
#else
#define ShouldUseVisibilityCache() true
#endif
#endif



BEGIN_DATADESC( CBaseCombatCharacter )

#ifdef INVASION_DLL
	DEFINE_FIELD( m_iPowerups, FIELD_INTEGER ),
	DEFINE_ARRAY( m_flPowerupAttemptTimes, FIELD_TIME, MAX_POWERUPS ),
	DEFINE_ARRAY( m_flPowerupEndTimes, FIELD_TIME, MAX_POWERUPS ),
	DEFINE_FIELD( m_flFractionalBoost, FIELD_FLOAT ),
#endif

	DEFINE_FIELD( m_flNextAttack, FIELD_TIME ),
	DEFINE_FIELD( m_eHull, FIELD_INTEGER ),
	DEFINE_FIELD( m_bloodColor, FIELD_INTEGER ),
	DEFINE_FIELD( m_iDamageCount, FIELD_INTEGER ),
	
	DEFINE_FIELD( m_flFieldOfView, FIELD_FLOAT ),
	DEFINE_FIELD( m_HackedGunPos, FIELD_VECTOR ),
	DEFINE_KEYFIELD( m_RelationshipString, FIELD_STRING, "Relationship" ),

	DEFINE_FIELD( m_LastHitGroup, FIELD_INTEGER ),
	DEFINE_FIELD( m_flDamageAccumulator, FIELD_FLOAT ),
	DEFINE_INPUT( m_impactEnergyScale, FIELD_FLOAT, "physdamagescale" ),
	DEFINE_FIELD( m_CurrentWeaponProficiency, FIELD_INTEGER),
#ifdef MAPBASE
	DEFINE_INPUT( m_ProficiencyOverride, FIELD_INTEGER, "SetProficiencyOverride"),
#endif

	DEFINE_UTLVECTOR(m_Relationship,	FIELD_EMBEDDED),
	DEFINE_UTLVECTOR(m_StatusEffectList, FIELD_EMBEDDED),
	DEFINE_FIELD(rootState, FIELD_INTEGER),

	DEFINE_AUTO_ARRAY( m_iAmmo, FIELD_INTEGER ),
	DEFINE_AUTO_ARRAY( m_hMyWeapons, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hActiveWeapon, FIELD_EHANDLE ),
#ifdef MAPBASE
	DEFINE_INPUT( m_bForceServerRagdoll, FIELD_BOOLEAN, "SetForceServerRagdoll" ),
#else
	DEFINE_FIELD( m_bForceServerRagdoll, FIELD_BOOLEAN ),
#endif
	DEFINE_FIELD( m_bPreventWeaponPickup, FIELD_BOOLEAN ),

#ifndef MAPBASE // See CBaseEntity::InputKilledNPC()
	DEFINE_INPUTFUNC( FIELD_VOID, "KilledNPC", InputKilledNPC ),
#endif

#ifdef MAPBASE
	// DR ADDITIONS
	DEFINE_FIELD(m_flDamageTracker, FIELD_FLOAT),
	DEFINE_FIELD(m_flDamageTrackerLimit, FIELD_FLOAT),
	DEFINE_FIELD(m_CurrentWeaponProficiency, FIELD_INTEGER),
	DEFINE_INPUTFUNC(FIELD_STRING, "AddStatusEffect", InputAddStatusEffect),
	DEFINE_INPUTFUNC(FIELD_INTEGER, "CheckHavingStatusEffect", InputCheckHavingStatusEffect),
	
	DEFINE_THINKFUNC(StatusThink),
	DEFINE_INPUTFUNC(FIELD_FLOAT, "GetDamageTracker", InputGetDamageTracker),
	DEFINE_INPUTFUNC(FIELD_VOID, "ResetDamageTracker", InputResetDamageTracker),
	DEFINE_INPUTFUNC(FIELD_INTEGER, "SetBloodColor", InputSetBloodColor),
	//
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetBloodColor", InputSetBloodColor ),

	DEFINE_INPUTFUNC( FIELD_STRING, "SetRelationship", InputSetRelationship ),

	DEFINE_INPUTFUNC( FIELD_VOID, "HolsterWeapon", InputHolsterWeapon ),
	DEFINE_INPUTFUNC( FIELD_VOID, "HolsterAndDestroyWeapon", InputHolsterAndDestroyWeapon ),
	DEFINE_INPUTFUNC( FIELD_STRING, "UnholsterWeapon", InputUnholsterWeapon ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SwitchToWeapon", InputSwitchToWeapon ),

	DEFINE_INPUTFUNC( FIELD_STRING, "GiveWeapon", InputGiveWeapon ),
	DEFINE_INPUTFUNC( FIELD_STRING, "DropWeapon", InputDropWeapon ),
	DEFINE_INPUTFUNC( FIELD_EHANDLE, "PickupWeaponInstant", InputPickupWeaponInstant ),
	DEFINE_OUTPUT( m_OnWeaponEquip, "OnWeaponEquip" ),
	DEFINE_OUTPUT( m_OnWeaponDrop, "OnWeaponDrop" ),

	DEFINE_OUTPUT( m_OnKilledEnemy, "OnKilledEnemy" ),
	DEFINE_OUTPUT( m_OnKilledPlayer, "OnKilledPlayer" ),
	DEFINE_OUTPUT( m_OnHealthChanged, "OnHealthChanged" ),
#endif

END_DATADESC()

#ifdef MAPBASE_VSCRIPT
ScriptHook_t	CBaseCombatCharacter::g_Hook_RelationshipType;
ScriptHook_t	CBaseCombatCharacter::g_Hook_RelationshipPriority;

BEGIN_ENT_SCRIPTDESC(CBaseCombatCharacter, CBaseFlex, "The base class shared by players and NPCs.")

DEFINE_SCRIPTFUNC_NAMED(ScriptGetActiveWeapon, "GetActiveWeapon", "Get the character's active weapon entity.")
DEFINE_SCRIPTFUNC(WeaponCount, "Get the number of weapons a character possesses.")
DEFINE_SCRIPTFUNC_NAMED(ScriptGetWeapon, "GetWeapon", "Get a specific weapon in the character's inventory.")
DEFINE_SCRIPTFUNC_NAMED(ScriptGetWeaponByType, "FindWeapon", "Find a specific weapon in the character's inventory by its classname.")
DEFINE_SCRIPTFUNC_NAMED(ScriptGetAllWeapons, "GetAllWeapons", "Get the character's weapon inventory.")
DEFINE_SCRIPTFUNC_NAMED(ScriptGetCurrentWeaponProficiency, "GetCurrentWeaponProficiency", "Get the character's current proficiency (accuracy) with their current weapon.")

DEFINE_SCRIPTFUNC_NAMED(Weapon_ShootPosition, "ShootPosition", "Get the character's shoot position.")
DEFINE_SCRIPTFUNC_NAMED(Weapon_DropAll, "DropAllWeapons", "Make the character drop all of its weapons.")
DEFINE_SCRIPTFUNC_NAMED(ScriptEquipWeapon, "EquipWeapon", "Make the character equip the specified weapon entity. If they don't already own the weapon, they will acquire it instantly.")
DEFINE_SCRIPTFUNC_NAMED(ScriptDropWeapon, "DropWeapon", "Make the character drop the specified weapon entity if they own it.")

DEFINE_SCRIPTFUNC_NAMED(ScriptGiveAmmo, "GiveAmmo", "Gives the specified amount of the specified ammo type. The third parameter is whether or not to suppress the ammo pickup sound. Returns the amount of ammo actually given, which is 0 if the player's ammo for this type is already full.")
DEFINE_SCRIPTFUNC_NAMED(ScriptRemoveAmmo, "RemoveAmmo", "Removes the specified amount of the specified ammo type.")
DEFINE_SCRIPTFUNC_NAMED(ScriptGetAmmoCount, "GetAmmoCount", "Get the ammo count of the specified ammo type.")
DEFINE_SCRIPTFUNC_NAMED(ScriptSetAmmoCount, "SetAmmoCount", "Set the ammo count of the specified ammo type.")

DEFINE_SCRIPTFUNC(DoMuzzleFlash, "Does a muzzle flash.")
DEFINE_SCRIPTFUNC_NAMED(ScriptGetAttackSpread, "GetAttackSpread", "Get the attack spread.")
DEFINE_SCRIPTFUNC_NAMED(ScriptGetSpreadBias, "GetSpreadBias", "Get the spread bias.")

DEFINE_SCRIPTFUNC_NAMED(ScriptRelationType, "GetRelationship", "Get a character's relationship to a specific entity.")
DEFINE_SCRIPTFUNC_NAMED(ScriptRelationPriority, "GetRelationPriority", "Get a character's relationship priority for a specific entity.")
DEFINE_SCRIPTFUNC_NAMED(ScriptSetRelationship, "SetRelationship", "Set a character's relationship with a specific entity.")
DEFINE_SCRIPTFUNC_NAMED(ScriptSetClassRelationship, "SetClassRelationship", "Set a character's relationship with a specific Classify() class.")

DEFINE_SCRIPTFUNC_NAMED(ScriptGetVehicleEntity, "GetVehicleEntity", "Get the entity for a character's current vehicle if they're in one.")

DEFINE_SCRIPTFUNC_NAMED(ScriptInViewCone, "InViewCone", "Check if the specified position is in the character's viewcone.")
DEFINE_SCRIPTFUNC_NAMED(ScriptEntInViewCone, "EntInViewCone", "Check if the specified entity is in the character's viewcone.")

DEFINE_SCRIPTFUNC_NAMED(ScriptInAimCone, "InAimCone", "Check if the specified position is in the character's aim cone.")
DEFINE_SCRIPTFUNC_NAMED(ScriptEntInViewCone, "EntInAimCone", "Check if the specified entity is in the character's aim cone.")

DEFINE_SCRIPTFUNC_NAMED(ScriptBodyAngles, "BodyAngles", "Get the body's angles.")
DEFINE_SCRIPTFUNC(BodyDirection2D, "Get the body's 2D direction.")
DEFINE_SCRIPTFUNC(BodyDirection3D, "Get the body's 3D direction.")
DEFINE_SCRIPTFUNC(HeadDirection2D, "Get the head's 2D direction.")
DEFINE_SCRIPTFUNC(HeadDirection3D, "Get the head's 3D direction.")
DEFINE_SCRIPTFUNC(EyeDirection2D, "Get the eyes' 2D direction.")
DEFINE_SCRIPTFUNC(EyeDirection3D, "Get the eyes' 3D direction.")

DEFINE_SCRIPTFUNC(LastHitGroup, "Get the last hitgroup.")

DEFINE_SCRIPTFUNC(CheckHavingStatusEffect, "Check if this entity has status effect with given ID")
DEFINE_SCRIPTFUNC(GetDamageMul, "Get this entity's damage multiplier");
DEFINE_SCRIPTFUNC(GetDamageAdd, "Get this entity's damage addition");

#ifdef GLOWS_ENABLE
	DEFINE_SCRIPTFUNC( AddGlowEffect, "" )
	DEFINE_SCRIPTFUNC( RemoveGlowEffect, "" )
	DEFINE_SCRIPTFUNC( IsGlowEffectActive, "" )
	DEFINE_SCRIPTFUNC( SetGlowColor, "" )
#endif

	// 
	// Hooks
	// 
	BEGIN_SCRIPTHOOK( CBaseCombatCharacter::g_Hook_RelationshipType, "RelationshipType", FIELD_INTEGER, "Called when a character's relationship to another entity is requested. Returning a disposition will make the game use that disposition instead of the default relationship. (note: 'default' in this case includes overrides from ai_relationship/SetRelationship)" )
		DEFINE_SCRIPTHOOK_PARAM( "entity", FIELD_HSCRIPT )
		DEFINE_SCRIPTHOOK_PARAM( "def", FIELD_INTEGER )
	END_SCRIPTHOOK()

	BEGIN_SCRIPTHOOK( CBaseCombatCharacter::g_Hook_RelationshipPriority, "RelationshipPriority", FIELD_INTEGER, "Called when a character's relationship priority for another entity is requested. Returning a number will make the game use that priority instead of the default priority. (note: 'default' in this case includes overrides from ai_relationship/SetRelationship)" )
		DEFINE_SCRIPTHOOK_PARAM( "entity", FIELD_HSCRIPT )
		DEFINE_SCRIPTHOOK_PARAM( "def", FIELD_INTEGER )
	END_SCRIPTHOOK()

END_SCRIPTDESC();
#endif


BEGIN_SIMPLE_DATADESC( Relationship_t )
	DEFINE_FIELD( entity,			FIELD_EHANDLE ),
	DEFINE_FIELD( classType,		FIELD_INTEGER ),
	DEFINE_FIELD( disposition,	FIELD_INTEGER ),
	DEFINE_FIELD( priority,	FIELD_INTEGER ),
END_DATADESC();

BEGIN_SIMPLE_DATADESC( AnimSpeedPrio_t )
	DEFINE_FIELD(speed, FIELD_FLOAT),
	DEFINE_FIELD(duration, FIELD_INTEGER),
	DEFINE_FIELD(priority, FIELD_INTEGER),
END_DATADESC();





//-----------------------------------------------------------------------------
// Init static variables
//-----------------------------------------------------------------------------
int					CBaseCombatCharacter::m_lastInteraction   = 0;
Relationship_t**	CBaseCombatCharacter::m_DefaultRelationship	= NULL;

//-----------------------------------------
// Implementation of status effects goes here.
//-----------------------------------------

constexpr auto ARMOUR_EXCHANGE_RATE = 6;
constexpr auto SUIT_DEFAULT_REGEN_PER_TICK = 12.5/66;

void CBaseCombatCharacter::SetPlaybackRate(float rate)
{
	CBaseAnimating::SetPlaybackRate(rate);
}

CStatusEffect::CStatusEffect(int duration, CHandle<CBaseCombatCharacter>  receiver, CHandle<CBaseCombatCharacter>  inflictor, Status_t type) {
	// adding a little randomization to passed global time so entities dont visually tick all at once 
	this->rndOffset = random->RandomInt(30, 1377);
	this->LocalCounter = LocalCounter + this->rndOffset;
	// only list debuffs here? maybe the opposite in the future? depending on of which there is more
	// maybe list debuffs last and do a greater than/less comparison on the enum
	if (receiver.Get()->IsPlayer()) {
		this->receiverIsPlayer = true;
	}

	switch (type)
	{
	case ST_NONE:
		this->isBuff = false;
		break;
	case ST_BLEED:
		this->isBuff = false;
		break;
	case ST_POISON:
		this->isBuff = false;
		break;
	case ST_SLIMED:
		this->isBuff = false;
		break;
	default:
		this->isBuff = true;
		break;
	}
	this->activated = false;
	this->speedMod = 1.0;
	// if receiver isn't player try casting receiver to CAI_BaseNPC
	if (!receiverIsPlayer) {
		CAI_BaseNPC* npcPtr = dynamic_cast<CAI_BaseNPC*>(receiver.Get());
		// if cast is successful extract speed modifier
		if (npcPtr) {
			this->speedMod = npcPtr->m_flSpeedModifier;
		}
	}
	this->receiver = receiver;
	this->efDuration = duration;
	this->statusType = type;
	this->receiverIsPlayer = false;

	//precaching block X
	// no, scale set block.
	switch (type)
	{

	case ST_ELITE_REGENERATIVE:
		sprScale = 0.25;
		
		break;
	case ST_POISON:
		sprScale = 0.15;
		//sprName = "particle/smokestack.vmt";
		break;
	case ST_SLIMED:
		mdlScale = 5.0;
		//mdlName = "models/spitball_large.mdl";
		break;

	}

	// keep these null until needed.
	m_pSprite = nullptr;
	m_pModel = nullptr;
	
}

CStatusEffect::CStatusEffect() {
	// adding a little randomization to passed global time so entities dont visually tick all at once 
	this->rndOffset = random->RandomInt(30, 1377);
	this->activated = false;
	//this->receiver.Term(); // ????
	this->efDuration = 0;
	this->statusType = ST_NONE;

}



CStatusEffect::CStatusEffect(CHandle<CBaseCombatCharacter>  receiver, Status_t type) {
	// adding a little randomization to passed global time so entities dont visually tick all at once 
	this->rndOffset = random->RandomInt(30, 1377);
	this->activated = false;
	this->efDuration = 1;
	this->receiver = receiver;
	this->statusType = type;


	}

// why do I have to define this?..
CStatusEffect::CStatusEffect(const CStatusEffect &old) {
	//*this = old; keeping this to look at what the fuck was I thinking
	this->isBuff = old.isBuff;
	this->activated = old.activated;
	this->efDuration = old.efDuration;
	this->receiver = old.receiver;
	this->statusType = old.statusType;
	this->statusName = old.statusName;
	this->receiverIsPlayer = old.receiverIsPlayer;
	this->m_pModel = old.m_pModel;
	this->m_pSprite = old.m_pSprite;
	this->inflictor = old.inflictor;
}

 void CStatusEffect::onActivate() {
	if (!this->activated) {
		CBaseCombatCharacter* recPtr = receiver.Get();
		this->activated = true;
		char* sprName = nullptr;
		char* mdlName = nullptr;
#if DEBUG
		if (inflictor.IsValid()) {
			DevMsg("Entity %s activating status effectID %d duration left %d inflicted by %s\n", (recPtr)->GetClassname(), this->getType(), this->efDuration, this->inflictor.Get()->GetClassname());
		}
		else {
			DevMsg("Entity %s activating status effectID %d duration left %d inflicted by %s\n", (recPtr)->GetClassname(), this->getType(), this->efDuration, "null/nobody");
		}
#endif
		if (recPtr) {
			switch (this->getType())
			{
			case ST_ARMOUROVERCHARGE:

				receiver->EmitSound("Status.Overcharge");
				break;

			case ST_ELITE_REGENERATIVE:
				sprName = STATUS_SPRITES[1];
				// become G R E E N.
				recPtr->SetRenderColorB(recPtr->GetRenderColor().b / 4);
				recPtr->SetRenderColorR(recPtr->GetRenderColor().r / 4);
				if (recPtr->GetMaxHealth() < 100) {
					recPtr->SetMaxHealth(recPtr->GetMaxHealth() * 2);
					recPtr->SetHealth(recPtr->GetMaxHealth());
				}
				else {
					recPtr->SetMaxHealth(recPtr->GetMaxHealth() * 1.5);
					recPtr->SetHealth(recPtr->GetMaxHealth());

				}

				recPtr->SetDamageTrackerLimit(50.0);
				efDuration = INT_MAX;
				break;

			case ST_POISON:
				sprName = STATUS_SPRITES[0];
				break;
			case ST_SLIMED:
				mdlScale = 5.0;
				mdlName = STATUS_MODELS[0];
				receiver->EmitSound("Status.Slimed");
				recPtr->rootCharacter();
				break;
			case ST_CROSSBOW_TEMP_IMMUNITY:
			{
				ConVar* sk_plr_dmg_crossbow = cvar->FindVar("sk_plr_dmg_crossbow");
				CTakeDamageInfo info(recPtr, recPtr, sk_plr_dmg_crossbow->GetInt() * 10, DMG_BLAST);
				recPtr->TakeDamage(info);
				recPtr->IgniteLifetime(5);
			}
				break;
			case ST_HOLOGRAM:
				{
				receiver->dmgMul*=2;
				receiver->AddSpawnFlags(SF_NPC_NO_WEAPON_DROP);
				receiver->EmitSound("sfx.hologramsummon");
				receiver->AddEFlags(EF_NOSHADOW);
				receiver->AddEFlags(EF_NORECEIVESHADOW);
				receiver->m_bDisableFlashlight = true;
				color32 rgba = receiver->GetRenderColor();
				receiver->SetRenderColor(rgba.r / 10, rgba.g * 2, rgba.b * 2, 120);
				receiver->SetRenderMode(kRenderTransColor);
				receiver->SetBloodColor(BLOOD_COLOR_MECH);
				receiver->m_nRenderFX = 15;
				efDuration = INT_MAX;
				}
				break;
			default:
				break;
			}

			if (sprName) {
				Vector absOrigin = recPtr->GetAbsOrigin();
				// put it slightly above origin
				absOrigin.z = absOrigin.z + 32 * (getScaleFromSize(recPtr->GetHullType())/2);
				m_pSprite = CSprite::SpriteCreate(sprName, absOrigin , FALSE);
				m_pSprite->SetTransparency(kRenderWorldGlow, 100, 225, 65, 225, kRenderFxNoDissipation);
				// if we can find a suitable attachment, put it on it
				int attachment = 0;
				if (attachment == 0) attachment = recPtr->LookupAttachment("eyes");
				if (attachment == 0) attachment = recPtr->LookupAttachment("head");
				if (attachment != 0)
				{
					m_pSprite->SetParent(recPtr, attachment);
					// do not maintain offset
					m_pSprite->SetLocalOrigin(vec3_origin);
					m_pSprite->SetLocalAngles(vec3_angle);
				}
				else m_pSprite->SetParent(recPtr);
				m_pSprite->SetBrightness(180);
				m_pSprite->SetGlowProxySize(24*sprScale);
				m_pSprite->SetScale(sprScale * getScaleFromSize(recPtr->GetHullType()));
				m_pSprite->TurnOn();
			}

			if (mdlName) {
				QAngle rndYaw = QAngle(0, random->RandomFloat(0.0f, 360.0f), 0);
				m_pModel = (CBaseAnimating*)CreateEntityByName("prop_dynamic");;
				if (m_pModel.IsValid())
				{
					m_pModel->SetModel((mdlName));
					m_pModel->SetModelScale(mdlScale * getScaleFromSize(recPtr->GetHullType()));
					m_pModel->SetAbsOrigin(recPtr->GetAbsOrigin());
					m_pModel->SetAbsAngles(recPtr->GetAbsAngles());
					m_pModel->SetParent(recPtr);
					m_pModel->SetOwnerEntity(recPtr);
					m_pModel->Spawn();
					m_pModel->SetMoveType(MOVETYPE_NONE);
					m_pModel->SetGroundEntity(NULL);
				}
	
				
			}
		} else Warning("Receiver for statusEf %d|%d is null. Investigate.\n", this->getType(), this->efDuration);
	}

	else {
		return;
	}
};
 ConVar dr_elites_seed("dr_elites_seed", "0");
 string_t CStatusEffect::getStatusName() {
	 return this->statusName;
 }

 const Status_t CStatusEffect::getType() const{
	 return this->statusType;
 }

 bool CStatusEffectLess::Less(const CStatusEffect& src1, const CStatusEffect& src2, void* pCtx) {
	 return (src1.getType() < src2.getType());
 }

void CStatusEffect::doTick(int GlobalTime) {
	// if receiver dead prepare for clearing
	if (!receiver.Get()->IsAlive()) {
		efDuration = 0;
		return;
	}

	// when damaging don't forget all damage gets divided by 2 for some reason
	//LocalCounter = LocalCounter + this->rndOffset;
	this->efDuration--;
	this->LocalCounter++;
#if DEBUG
	//Log("Entity %s receiving status effectID %d duration left %d\n",(receiver.Get())->GetClassname(), this->getType(), this->efDuration);
#endif

	switch (this->getType())
	{
		case ST_ARMOUROVERCHARGE:
		{

			if (LocalCounter%(6/STATUS_TICK_MUL) == 0)
			{
				if (this->receiverIsPlayer) 
				{
#if DEBUG
					Log("receiver is player. doing armour exchange.\n");
#endif
					CHL2_Player* plr = dynamic_cast<CHL2_Player*>(receiver.Get());
#if DEBUG
					Log("Current suit power to be exchanged: %.6f - %d\n", plr->SuitPower_GetCurrentPercentage(), ARMOUR_EXCHANGE_RATE);
#endif
					if ((plr->SuitPower_GetCurrentPercentage()-ARMOUR_EXCHANGE_RATE+SUIT_DEFAULT_REGEN_PER_TICK*5) > 0) 
					{
						float addValue = ARMOUR_EXCHANGE_RATE;
						int armorval = plr->ArmorValue();
						if (armorval > 100) {
#if DEBUG
							Log("armor to be exchanged is higher than 100 %d.\n", armorval);
#endif
							// overcharging CAN go beyond 100, but approaches 200 without reaching it.
							addValue = 0;
							// run the function ARMOUR_EXCHANGE_RATE-1 times
							for (int i = 0; i < ARMOUR_EXCHANGE_RATE; i++) {
#if DEBUG
								Log("summing %0.4f.\n", addValue);
#endif
								addValue = addValue+(100.0f * (100.0f / -(armorval+i)) + 100)-(100.0f * (100.0f / -(armorval+i-1)) + 100);
							}
						}
#if DEBUG
						Log("summed %0.4f\n", addValue);
						Log("armor to be set is %0.4f\n", (armorval + addValue));
#endif
						plr->SetArmorValue(armorval + addValue);
						plr->SuitPower_Drain(ARMOUR_EXCHANGE_RATE + SUIT_DEFAULT_REGEN_PER_TICK * 5);
					}
					else {
						this->efDuration = 0;
					}
				}
			}
		}
			break;

		case ST_ELITE_REGENERATIVE:
		{
			// regenerate 6 accumulated damage per half a second
			if (LocalCounter % (DEFAULT_TPS/2 / STATUS_TICK_MUL) == 0) 
			{
				int healAmount = receiver->GetHealth() + REGENERATIVE_REGEN_TICK;
				DevMsg("Regenerative Elite class|entityindex: %s|%d attempting deferred healing %d\n", receiver->GetClassname(), receiver->entindex(),healAmount);
				if (receiver->AddToDamageTracker(-REGENERATIVE_REGEN_TICK)) {
					DevMsg("Regenerative Elite class|entityindex: %s|%d doing deferred healing %d\n",receiver->GetClassname(), receiver->entindex(), healAmount);
					receiver->SetHealth(healAmount);
				}

			}

			// regenerate 5% max HP per 10 seconds :)
			
			if (LocalCounter % (DEFAULT_TPS*5 / STATUS_TICK_MUL) == 0) 
			{
				if (receiver->GetHealth() < receiver->GetMaxHealth()) 
				{	

					receiver->SetHealth(receiver->GetHealth() + receiver->GetMaxHealth() * (5.0f / 100));
				}

			}
		}
		break;
		case ST_BLEED:
		{
			// do 4% of max HP per second.
			
			if (LocalCounter%(DEFAULT_TPS/STATUS_TICK_MUL) == 0) {
				Vector up;
				receiver.Get()->GetVectors(NULL, NULL, &up);
				UTIL_BloodSpray(receiver.Get()->GetAbsOrigin() + Vector(0, 0, 16), up, BLOOD_COLOR_RED, 8 * getScaleFromSize(receiver.Get()->GetHullType()), FX_BLOODSPRAY_ALL);
#if DEBUG
				Log("doing bleed.\n");
#endif

				CBaseCombatCharacter* cbccptr = receiver.Get();
				cbccptr->EmitSound("Status.Bleed");
				CTakeDamageInfo info(cbccptr, cbccptr,cbccptr->GetMaxHealth()*(4.0f/100), DMG_SLASH);
				cbccptr->TakeDamage(info);
				
			};
		}
		break;
		case ST_POISON:
		{
			// do 10% of current HP per 2 seconds.
			if (LocalCounter % (66*2/STATUS_TICK_MUL) == 0) {
#if DEBUG
				Log("doing poison.\n");
#endif
				CBaseCombatCharacter* cbccptr = receiver.Get();
				cbccptr->EmitSound("Status.Poison");
				CTakeDamageInfo info(cbccptr, cbccptr, cbccptr->GetHealth() * (10.0f / 100), DMG_ACID);
				cbccptr->TakeDamage(info);

			};
		}
		break;
		default:
			break;
	}
	
};


void CStatusEffect::onDeactivate() {
	CBaseCombatCharacter* recPtr = receiver.Get();
	switch (this->getType())
	{
	case ST_ARMOUROVERCHARGE:
#if DEBUG
		Log("Entity %s activating status effectID %d duration left %d\n", (recPtr)->GetClassname(), this->getType(), this->efDuration);
#endif
		
		break;
	case ST_SLIMED:
		receiver->EmitSound("Status.Unslimed");
		recPtr->unrootCharacter();

		break;
	default:
		break;
	}
	if (m_pSprite) {
		UTIL_Remove(this->m_pSprite);
	}
	if (m_pModel.IsValid() && m_pModel.Get()) {
		(UTIL_Remove(this->m_pModel));
	}
	return;
}

// get approximate scale from node Hull size for effect sprite/model
float CStatusEffect::getScaleFromSize(Hull_t hull)
{
	float scale = 1.0;
	switch (hull) 
	{
	case HULL_HUMAN:
		scale = 1.0;
		break;
	case HULL_SMALL_CENTERED:
		scale = 1.0;
		break;
	case HULL_WIDE_HUMAN:
		scale = 1.5;
		break;
	case HULL_TINY:
		scale = 0.75;
		break;
	case HULL_MEDIUM:
		scale = 1.25;
		break;
	case HULL_LARGE:
		scale = 2.5;
		break;
	case HULL_LARGE_CENTERED:
		scale = 3;
		break;
	
	case HULL_MEDIUM_TALL:
		scale = 2.25;
		break;
	
	}
	return scale;
}

// root/unroot player (disable movement and sprint)
void CBaseCombatCharacter::rootCharacter()
{
	// doing this stupid shit because almost nobody actually inherits CAI_BaseNPC methods and that's the first class to have m_flSpeedModifier
	CAI_BaseNPC* npcPtr = dynamic_cast<CAI_BaseNPC*>(this);
	// if cast is successful moify speed
	if (npcPtr) {
		npcPtr->m_flSpeedModifier = npcPtr->m_flSpeedModifier * 0.01;
		//npcPtr->SetGravity(npcPtr->GetGravity() + 5000);
	}
	/*
	if (ptr->IsPlayer()) {
		if (CBasePlayer* plr = dynamic_cast<CBasePlayer*>(ptr)) {
			int nMask = 0;
			nMask |= IN_SPEED;
			nMask |= IN_JUMP;
			nMask |= IN_FORWARD;
			nMask |= IN_BACK;
			nMask |= IN_MOVELEFT;
			nMask |= IN_MOVERIGHT;
			//plr->SetLaggedMovementValue(0.01f);
			plr->DisableButtons(nMask);
		}
	}
	else {
		CAI_BaseNPC* npcPtr = dynamic_cast<CAI_BaseNPC*>(ptr);
		// if cast is successful moify speed
		if (npcPtr) {
			npcPtr->m_flSpeedModifier = npcPtr->m_flSpeedModifier * 0.01;
			npcPtr->SetGravity(npcPtr->GetGravity()+5000);
			// Disable extra movement capabilities for NPCs
			const char* classname = npcPtr->GetClassname();
			if (V_strcmp(classname, "npc_antlion") == 0) {
				if (CNPC_Antlion* newPtr = dynamic_cast<CNPC_Antlion*>(npcPtr)) {
					newPtr->m_bDisableJump = true;
				}
			} else			
			// since AntlionGuard is a little BITCH that wasn't exposed in the headers I'll have to expose them myself later.
			if (V_strcmp(classname, "npc_antlionguard") == 0) {
				if (CNPC_AntlionGuard* newPtr = dynamic_cast<CNPC_AntlionGuard*>(npcPtr)) {
					newPtr->ClearCondition(COND_ANTLIONGUARD_CAN_CHARGE);
				}
			}
			// since Hunter is a little BITCH that wasn't exposed in the headers I'll have to expose them myself later.
			
			if (V_strcmp(classname, "npc_hunter") == 0) {
				if (CNPC_Hunter* newPtr = dynamic_cast<CNPC_Hunter*>(npcPtr)) {
					newPtr->m_flNextChargeTime += (FLT_MAX/100);
					newPtr->SetIdealActivity(ACT_HUNTER_CHARGE_STOP);
				}
			}
			if (V_stristr(classname, "npc_headcrab")) {
				if (CBaseHeadcrab* newPtr = dynamic_cast<CBaseHeadcrab*>(npcPtr)) {
					newPtr->m_bMidJump = true;

				}
			}

		}
	}
	*/
}

void CBaseCombatCharacter::unrootCharacter()
{

	// doing this stupid shit because almost nobody actually inherits CAI_BaseNPC methods and that's the first class to have m_flSpeedModifier
	CAI_BaseNPC* npcPtr = dynamic_cast<CAI_BaseNPC*>(this);
	// if cast is successful unmodify speed
	if (npcPtr)
	{
		npcPtr->m_flSpeedModifier = npcPtr->m_flSpeedModifier * 100;
		//npcPtr->SetGravity(npcPtr->GetGravity() - 5000);
	}
	//if (ptr->IsPlayer()) {
	//	if (CBasePlayer* plr = dynamic_cast<CBasePlayer*>(ptr)) {
	//		int nMask = 0;
	//		nMask |= IN_SPEED;
	//		nMask |= IN_JUMP;
	//		nMask |= IN_FORWARD;
	//		nMask |= IN_BACK;
	//		nMask |= IN_MOVELEFT;
	//		nMask |= IN_MOVERIGHT;
	//		//plr->SetLaggedMovementValue(1.0f);
	//		plr->EnableButtons(nMask);
	//	}
	//} 
	//else {


	//		// Revert extra movement capabilities for NPCs
	//		const char* classname = npcPtr->GetClassname();
	//		if (V_strcmp(classname, "npc_antlion") == 0) {
	//			if (CNPC_Antlion* newPtr = dynamic_cast<CNPC_Antlion*>(npcPtr)) {
	//				newPtr->m_bDisableJump = false;
	//			}
	//		}
	//		else
	//		// since AntlionGuard is a little BITCH that wasn't exposed in the headers I'll have to expose them myself later.
	//			
	//		if (V_strcmp(classname, "npc_antlionguard") == 0) {
	//				if (CNPC_AntlionGuard* newPtr = dynamic_cast<CNPC_AntlionGuard*>(npcPtr)) {
	//					newPtr->SetCondition(COND_ANTLIONGUARD_CAN_CHARGE);
	//				}
	//			}
	//		else
	//		
	//		// since Hunter is a little BITCH that wasn't exposed in the headers I'll have to expose them myself later.
	//		if (V_strcmp(classname, "npc_hunter") == 0) {
	//			if (CNPC_Hunter* newPtr = dynamic_cast<CNPC_Hunter*>(npcPtr)) {
	//				newPtr->m_flNextChargeTime -= (FLT_MAX / 100);
	//				
	//			}

	//		if (V_stristr(classname, "npc_headcrab")) {
	//			if (CBaseHeadcrab* newPtr = dynamic_cast<CBaseHeadcrab*>(npcPtr)) {
	//				newPtr->m_bMidJump = false;

	//			}
	//		}

	//	}
	//}

}


void CBaseCombatCharacter::addRootState(int in) {
	int transition = 0;

	int oldRootState = this->rootState;
	if (oldRootState < 1) transition |= 1;

	int newRootState = oldRootState + in;
	if (newRootState < 1) transition |= 2;

	this->rootState = newRootState;
	switch (transition) {
	case 1:
		this->rootCharacter();
		break;
	case 2:
		this->unrootCharacter();
		break;
	}
}

/*
class CStatusEffect
{
public:
	DECLARE_CLASS_NOBASE(CStatusEffect); // this class doesn't inherit from anything, what do I put as the base class?..
	// 05.02.2026 above was previously typed when this dumbass didn't know DECLARE_CLASS isnt the only DECLARE for CLASSes
	DECLARE_DATADESC();
	CHandle<CBaseCombatCharacter> receiver;
	int efDuration;
	bool isBuff;
	bool activated;
	bool receiverIsPlayer;
	int rndOffset;
	float mdlScale;
	float sprScale;
	// store sprite as pointer idk Valve did it that way
	CSprite* m_pSprite;
	CHandle<CBaseAnimating> m_pModel;
	CStatusEffect(int duration, CHandle<CBaseCombatCharacter>  receiver, Status_t type);
	CStatusEffect(CHandle<CBaseCombatCharacter>  receiver, Status_t type);
	CStatusEffect();
	CStatusEffect(const CStatusEffect& old);
	virtual void doTick(int GlobalTime);
	virtual void onActivate();
	virtual void onDeactivate();
	static float getScaleFromSize(Hull_t hull);
	static void rootPlayer(CBasePlayer* plr);
	static void unrootPlayer(CBasePlayer* plr);
	string_t getStatusName();
	Status_t getType();
	float speedMod;
protected:
	Status_t statusType;
	string_t statusName;
};*/
BEGIN_DATADESC_NO_BASE(CStatusEffect)
	DEFINE_FIELD(statusType, FIELD_INTEGER),
	DEFINE_FIELD(rndOffset, FIELD_INTEGER),
	DEFINE_FIELD(efDuration, FIELD_INTEGER),
	DEFINE_FIELD(statusName, FIELD_STRING),
	DEFINE_FIELD(activated, FIELD_BOOLEAN),
	DEFINE_FIELD(receiver, FIELD_EHANDLE),
	DEFINE_FIELD(inflictor, FIELD_EHANDLE),
	DEFINE_FIELD(m_pSprite, FIELD_CLASSPTR),
	DEFINE_FIELD(m_pModel, FIELD_EHANDLE),
	DEFINE_FIELD(speedMod, FIELD_FLOAT),
END_DATADESC()



Status_t StatusStringToStatusEnum(char* str) {
	if (V_strcmp(str, "ST_ELITE_REGENERATIVE") == 0) {
		return ST_ELITE_REGENERATIVE;
	}
	if (V_strcmp(str, "ST_ELITE_BLAZING") == 0) {
		return ST_ELITE_BLAZING;
	}
	if (V_strcmp(str,"ST_ARMOUROVERCHARGE") == 0) {
		return ST_ARMOUROVERCHARGE;
	}
	if (V_strcmp(str, "ST_BLEED") == 0) {
		return ST_BLEED;
	}
	if (V_strcmp(str, "ST_POISON") == 0) {
		return ST_POISON;
	}
	if (V_strcmp(str, "ST_SLIMED") == 0) {
		return ST_SLIMED;
	}
	for (int i = 0; i < Status_t_size; i++) {
		if (V_strcmp(str, Status_t_mapped[i]) == 0) {
			return static_cast<Status_t>(i);
		}
	}

	return ST_NONE;
}
	


// I can't make inheritance work with a container of sublcasses in C++. A container of references/pointers would work, but I do not want to go through the pain of serializing those.
/*
class CStatusEffectArmourOvercharge : public CStatusEffect {

public:
	bool isBuff = true;
	void doTick(int GlobalTime) override {
		CStatusEffect::doTick(GlobalTime);
		if (GlobalTime % 5 == 0) {
			if (receiver->IsPlayer()) {
				CHL2_Player* plr = dynamic_cast<CHL2_Player*>(receiver.Get());
				plr->SetArmorValue(plr->ArmorValue() + ARMOUR_EXCHANGE_RATE);
				plr->SuitPower_Drain(ARMOUR_EXCHANGE_RATE);
			}
		}
	};
	void onActivate() override {
		CStatusEffect::onActivate();
		receiver->EmitSound("Status.Overcharge");
		CStatusEffect::onActivate();
	};
	CStatusEffectArmourOvercharge(int duration, CHandle<CBaseCombatCharacter>  receiver, Status_t type) : CStatusEffect(duration, receiver, ST_ARMOUROVERCHARGE) {};
	CStatusEffectArmourOvercharge(CHandle<CBaseCombatCharacter>  receiver, Status_t type) : CStatusEffect(receiver, ST_ARMOUROVERCHARGE) {};
	CStatusEffectArmourOvercharge() : CStatusEffect() {};

protected:
	string_t statusName = MAKE_STRING("ST_ARMOUROVERCHARGE");
};

class CStatusEffectBleed : public CStatusEffect {
public:
	bool isBuff = false;
	CStatusEffectBleed(int duration, CHandle<CBaseCombatCharacter>  receiver, Status_t type) : CStatusEffect(duration, receiver, ST_BLEED) {};
	CStatusEffectBleed(CHandle<CBaseCombatCharacter>  receiver, Status_t type) : CStatusEffect(receiver, ST_BLEED) {};
	CStatusEffectBleed() : CStatusEffect() {};
	void doTick(int GlobalTime) override {
		CStatusEffect::doTick(GlobalTime);
		// do 3% of max HP per second.
		if (GlobalTime % 60 == 0) {
			CBaseCombatCharacter* cbccptr = receiver.Get();
			CTakeDamageInfo info(cbccptr, cbccptr, (3.0f / cbccptr->GetMaxHealth()), DMG_SLASH);
			cbccptr->TakeDamage(info);
			cbccptr->EmitSound("Status.Bleed");
		};
	};

protected:
	string_t statusName = MAKE_STRING("ST_BLEED");
};
*/


void CBaseCombatCharacter::StatusThink(void) {
	if (!this->m_StatusEffectList.IsEmpty()) {

		FOR_EACH_VEC(this->m_StatusEffectList, i) 
		{
			if (this->m_StatusEffectList[i].efDuration < 1) 
			{
				this->m_StatusEffectList[i].onDeactivate();
				this->m_StatusEffectList.Remove(i);
			}
		}
	}
	if (!this->m_StatusEffectList.IsEmpty()) 
	{
		int time = gpGlobals->tickcount;
		FOR_EACH_VEC(this->m_StatusEffectList, i)
		{
			CStatusEffect* ptrEf = &(this->m_StatusEffectList[i]);
			Log("pointer to statusEf: %p\n", ptrEf);
			if (ptrEf)
			{
				
				ptrEf->doTick(time);
			}
			else {
				Warning("Why the hell is the pointer of statusEf %d at %p NULL?..\n", (this->m_StatusEffectList[i]).getType(), &(this->m_StatusEffectList[i]));
			}

		}

	}
	// 1 think per tick is waaaay too much probably.
	SetNextThink(gpGlobals->curtime + STATUS_TICK_MUL*(this->tickinterval), "StatusContext");
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CCleanupDefaultRelationShips : public CAutoGameSystem
{
public:
	CCleanupDefaultRelationShips( char const *name ) : CAutoGameSystem( name )
	{
	}

	virtual void Shutdown()
	{
		if ( !CBaseCombatCharacter::m_DefaultRelationship )
			return;

		for ( int i=0; i<NUM_AI_CLASSES; ++i )
		{
			delete[] CBaseCombatCharacter::m_DefaultRelationship[ i ];
		}

		delete[] CBaseCombatCharacter::m_DefaultRelationship;
		CBaseCombatCharacter::m_DefaultRelationship = NULL;
	}
};

static CCleanupDefaultRelationShips g_CleanupDefaultRelationships( "CCleanupDefaultRelationShips" );

void *SendProxy_SendBaseCombatCharacterLocalDataTable( const SendProp *pProp, const void *pStruct, const void *pVarData, CSendProxyRecipients *pRecipients, int objectID )
{
	// Only send to local player if this is a player
	pRecipients->ClearAllRecipients();
	
	CBaseCombatCharacter *pBCC = ( CBaseCombatCharacter * )pStruct;
	if ( pBCC != NULL)
	{
		if ( pBCC->IsPlayer() )
		{
			pRecipients->SetOnly( pBCC->entindex() - 1 );
		}
		else
		{
			// If it's a vehicle, send to "driver" (e.g., operator of tf2 manned guns)
			IServerVehicle *pVehicle = pBCC->GetServerVehicle();
			if ( pVehicle != NULL )
			{
				CBaseCombatCharacter *pDriver = pVehicle->GetPassenger();
				if ( pDriver != NULL )
				{
					pRecipients->SetOnly( pDriver->entindex() - 1 );
				}
			}
		}
	}
	return ( void * )pVarData;
}
REGISTER_SEND_PROXY_NON_MODIFIED_POINTER( SendProxy_SendBaseCombatCharacterLocalDataTable );

// Only send active weapon index to local player
BEGIN_SEND_TABLE_NOBASE( CBaseCombatCharacter, DT_BCCLocalPlayerExclusive )
	SendPropTime( SENDINFO( m_flNextAttack ) ),
END_SEND_TABLE();

//-----------------------------------------------------------------------------
// This table encodes the CBaseCombatCharacter
//-----------------------------------------------------------------------------
IMPLEMENT_SERVERCLASS_ST(CBaseCombatCharacter, DT_BaseCombatCharacter)
#ifdef GLOWS_ENABLE
	SendPropBool( SENDINFO( m_bGlowEnabled ) ),
	SendPropVector( SENDINFO( m_GlowColor ), 8, 0, 0, 1 ),
	SendPropFloat( SENDINFO( m_GlowAlpha ) ),
#endif // GLOWS_ENABLE
	// Data that only gets sent to the local player.
	SendPropDataTable( "bcc_localdata", 0, &REFERENCE_SEND_TABLE(DT_BCCLocalPlayerExclusive), SendProxy_SendBaseCombatCharacterLocalDataTable ),

	SendPropEHandle( SENDINFO( m_hActiveWeapon ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_hMyWeapons), SendPropEHandle( SENDINFO_ARRAY(m_hMyWeapons) ) ),

#ifdef INVASION_DLL
	SendPropInt( SENDINFO(m_iPowerups), MAX_POWERUPS, SPROP_UNSIGNED ), 
#endif

END_SEND_TABLE()


//-----------------------------------------------------------------------------
// Interactions
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::InitInteractionSystem()
{
	// interaction ids continue to go up with every map load, otherwise you get
	// collisions if a future map has a different set of NPCs from a current map
}


//-----------------------------------------------------------------------------
// Purpose: Return an interaction ID (so we have no collisions)
//-----------------------------------------------------------------------------
int	CBaseCombatCharacter::GetInteractionID(void)
{
	m_lastInteraction++;
	return (m_lastInteraction);
}

#ifdef MAPBASE
//-----------------------------------------------------------------------------
// Purpose: New method of adding interactions which allows their name to be available (currently used for VScript)
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::AddInteractionWithString( int &interaction, const char *szName )
{
	interaction = GetInteractionID();

	if (g_pScriptVM)
	{
		ScriptRegisterConstantNamed( g_pScriptVM, interaction, szName, "An interaction which could be used with HandleInteraction or DispatchInteraction. NOTE: These are usually only initialized by certain types of NPCs when an instance of one spawns in the level for the first time!!! (the fact you're seeing this one means there was an NPC in the level which initialized it)" );
	}
}
#endif

// ============================================================================
bool CBaseCombatCharacter::HasHumanGibs( void )
{
#if defined( HL2_DLL )
	Class_T myClass = Classify();
	if ( myClass == CLASS_CITIZEN_PASSIVE   ||
		 myClass == CLASS_CITIZEN_REBEL		||
		 myClass == CLASS_COMBINE			||
		 myClass == CLASS_CONSCRIPT			||
		 myClass == CLASS_METROPOLICE		||
		 myClass == CLASS_PLAYER )	
		 return true;

#elif defined( HL1_DLL )
	Class_T myClass = Classify();
	if (	myClass == CLASS_HUMAN_MILITARY		||
			myClass == CLASS_PLAYER_ALLY		||
			myClass == CLASS_HUMAN_PASSIVE		||
			myClass == CLASS_PLAYER )
	{
		return true;
	}

#elif defined( CSPORT_DLL )
	Class_T myClass = Classify();
	if (	 myClass == CLASS_PLAYER )	
	{
		return true;
	}

#endif

	return false;
}


bool CBaseCombatCharacter::HasAlienGibs( void )
{
#if defined( HL2_DLL )
	Class_T myClass = Classify();
	if ( myClass == CLASS_BARNACLE		 || 
		 myClass == CLASS_STALKER		 ||
		 myClass == CLASS_ZOMBIE		 ||
		 myClass == CLASS_VORTIGAUNT	 ||
		 myClass == CLASS_HEADCRAB )
	{
		 return true;
	}

#elif defined( HL1_DLL )
	Class_T myClass = Classify();
	if ( myClass == CLASS_ALIEN_MILITARY ||
		 myClass == CLASS_ALIEN_MONSTER	||
		 myClass == CLASS_INSECT  ||
		 myClass == CLASS_ALIEN_PREDATOR  ||
		 myClass == CLASS_ALIEN_PREY )
	{
		return true;
	}
#endif

	return false;
}

void CBaseCombatCharacter::InputGetDamageTracker(inputdata_t& inputdata)
{
	inputdata.value.SetFloat(this->GetDamageTracker());
}

bool CBaseCombatCharacter::AddToDamageTracker(float dmg)
{
	if (m_flDamageTrackerLimit < 0) {
		m_flDamageTracker = m_flDamageTracker + dmg;
		return true;
	}
	else
	{
		if (m_flDamageTracker + dmg < 0) {
			m_flDamageTracker = 0;
			return false;
		}
		if (m_flDamageTracker + dmg > m_flDamageTrackerLimit) {
			m_flDamageTracker = m_flDamageTrackerLimit;
			return false;
		}
		m_flDamageTracker = m_flDamageTracker + dmg;
		return true;
	}
}

void CBaseCombatCharacter::SetDamageTrackerLimit(float dmg)
{
	m_flDamageTrackerLimit = dmg;
}

void CBaseCombatCharacter::InputResetDamageTracker(inputdata_t& inputdata)
{
	ResetDamageTracker();
}


void CBaseCombatCharacter::CorpseFade( void )
{
	StopAnimation();
	SetAbsVelocity( vec3_origin );
	SetMoveType( MOVETYPE_NONE );
	SetLocalAngularVelocity( vec3_angle );
	m_flAnimTime = gpGlobals->curtime;
	IncrementInterpolationFrame();
	SUB_StartFadeOut();
}

//-----------------------------------------------------------------------------
// Visibility caching
//-----------------------------------------------------------------------------

struct VisibilityCacheEntry_t
{
	CBaseEntity *pEntity1;
	CBaseEntity *pEntity2;
	EHANDLE		pBlocker;
	float		time;
};

class CVisibilityCacheEntryLess
{
public:
	CVisibilityCacheEntryLess( int ) {}
	bool operator!() const { return false; }
	bool operator()( const VisibilityCacheEntry_t &lhs, const VisibilityCacheEntry_t &rhs ) const
	{
		return ( memcmp( &lhs, &rhs, offsetof( VisibilityCacheEntry_t, pBlocker ) ) < 0 );
	}
};

static CUtlRBTree<VisibilityCacheEntry_t, unsigned short, CVisibilityCacheEntryLess> g_VisibilityCache;
const float VIS_CACHE_ENTRY_LIFE = ( !IsXbox() ) ? .090 : .500;

bool CBaseCombatCharacter::FVisible( CBaseEntity *pEntity, int traceMask, CBaseEntity **ppBlocker )
{
	VPROF( "CBaseCombatCharacter::FVisible" );

#ifdef MAPBASE
	if ( traceMask != MASK_BLOCKLOS || !ShouldUseVisibilityCache( pEntity ) || pEntity == this || !ai_use_visibility_cache.GetBool()
		 )
#else
	if ( traceMask != MASK_BLOCKLOS || !ShouldUseVisibilityCache() || pEntity == this
#if defined(HL2_DLL)
		 || Classify() == CLASS_BULLSEYE || pEntity->Classify() == CLASS_BULLSEYE 
#endif
		 )
#endif
	{
		return BaseClass::FVisible( pEntity, traceMask, ppBlocker );
	}

	VisibilityCacheEntry_t cacheEntry;

	if ( this < pEntity )
	{
		cacheEntry.pEntity1 = this;
		cacheEntry.pEntity2 = pEntity;
	}
	else
	{
		cacheEntry.pEntity1 = pEntity;
		cacheEntry.pEntity2 = this;
	}

	int iCache = g_VisibilityCache.Find( cacheEntry );

	if ( iCache != g_VisibilityCache.InvalidIndex() )
	{
		if ( gpGlobals->curtime - g_VisibilityCache[iCache].time < VIS_CACHE_ENTRY_LIFE )
		{
			bool bCachedResult = !g_VisibilityCache[iCache].pBlocker.IsValid();
			if ( bCachedResult )
			{
				if ( ppBlocker )
				{
					*ppBlocker = g_VisibilityCache[iCache].pBlocker;
					if ( !*ppBlocker )
					{
						*ppBlocker = GetWorldEntity();
					}
				}
			}
			else
			{
				if ( ppBlocker )
				{
					*ppBlocker = NULL;
				}
			}
			return bCachedResult;
		}
	}
	else
	{
		if ( g_VisibilityCache.Count() != g_VisibilityCache.InvalidIndex() )
		{
			iCache = g_VisibilityCache.Insert( cacheEntry );
		}
		else
		{
			return BaseClass::FVisible( pEntity, traceMask, ppBlocker );
		}
	}

	CBaseEntity *pBlocker = NULL;
	if ( ppBlocker == NULL )
	{
		ppBlocker = &pBlocker;
	}

	bool bResult = BaseClass::FVisible( pEntity, traceMask, ppBlocker );

	if ( !bResult )
	{
		g_VisibilityCache[iCache].pBlocker = *ppBlocker;
	}
	else
	{
		g_VisibilityCache[iCache].pBlocker = NULL;
	}

	g_VisibilityCache[iCache].time = gpGlobals->curtime;

	return bResult;
}

void CBaseCombatCharacter::ResetVisibilityCache( CBaseCombatCharacter *pBCC )
{
	VPROF( "CBaseCombatCharacter::ResetVisibilityCache" );
	if ( !pBCC )
	{
		g_VisibilityCache.RemoveAll();
		return;
	}

	int i = g_VisibilityCache.FirstInorder();
	CUtlVector<unsigned short> removals;
	while ( i != g_VisibilityCache.InvalidIndex() )
	{
		if ( g_VisibilityCache[i].pEntity1 == pBCC || g_VisibilityCache[i].pEntity2 == pBCC )
		{
			removals.AddToTail( i );
		}
		i = g_VisibilityCache.NextInorder( i );
	}

	for ( i = 0; i < removals.Count(); i++ )
	{
		g_VisibilityCache.RemoveAt( removals[i] );
	}
}

#ifdef MAPBASE
bool CBaseCombatCharacter::ShouldUseVisibilityCache( CBaseEntity *pEntity )
{
#ifdef HL2_DLL
	return Classify() != CLASS_BULLSEYE && pEntity->Classify() != CLASS_BULLSEYE;
#else
	return true;
#endif
}
#endif

#ifdef PORTAL
bool CBaseCombatCharacter::FVisibleThroughPortal( const CProp_Portal *pPortal, CBaseEntity *pEntity, int traceMask, CBaseEntity **ppBlocker )
{
	VPROF( "CBaseCombatCharacter::FVisible" );

	if ( pEntity->GetFlags() & FL_NOTARGET )
		return false;

#if HL1_DLL
	// FIXME: only block LOS through opaque water
	// don't look through water
	if ((m_nWaterLevel != 3 && pEntity->m_nWaterLevel == 3) 
		|| (m_nWaterLevel == 3 && pEntity->m_nWaterLevel == 0))
		return false;
#endif

	Vector vecLookerOrigin = EyePosition();//look through the caller's 'eyes'
	Vector vecTargetOrigin = pEntity->EyePosition();

	// Use the custom LOS trace filter
	CTraceFilterLOS traceFilter( this, COLLISION_GROUP_NONE, pEntity );

	Vector vecTranslatedTargetOrigin;
	UTIL_Portal_PointTransform( pPortal->m_hLinkedPortal->MatrixThisToLinked(), vecTargetOrigin, vecTranslatedTargetOrigin );
	Ray_t ray;
	ray.Init( vecLookerOrigin, vecTranslatedTargetOrigin );

	trace_t tr;

	// If we're doing an opaque search, include NPCs.
	if ( traceMask == MASK_BLOCKLOS )
	{
		traceMask = MASK_BLOCKLOS_AND_NPCS;
	}

	UTIL_Portal_TraceRay_Bullets( pPortal, ray, traceMask, &traceFilter, &tr );

	if (tr.fraction != 1.0 || tr.startsolid )
	{
		// If we hit the entity we're looking for, it's visible
		if ( tr.m_pEnt == pEntity )
			return true;

		// Got line of sight on the vehicle the player is driving!
		if ( pEntity && pEntity->IsPlayer() )
		{
			CBasePlayer *pPlayer = assert_cast<CBasePlayer*>( pEntity );
			if ( tr.m_pEnt == pPlayer->GetVehicleEntity() )
				return true;
		}

		if (ppBlocker)
		{
			*ppBlocker = tr.m_pEnt;
		}

		return false;// Line of sight is not established
	}

	return true;// line of sight is valid.
}
#endif

//-----------------------------------------------------------------------------

//=========================================================
// FInViewCone - returns true is the passed ent is in
// the caller's forward view cone. The dot product is performed
// in 2d, making the view cone infinitely tall. 
//=========================================================
bool CBaseCombatCharacter::FInViewCone( CBaseEntity *pEntity )
{
	return FInViewCone( pEntity->WorldSpaceCenter() );
}

//=========================================================
// FInViewCone - returns true is the passed Vector is in
// the caller's forward view cone. The dot product is performed
// in 2d, making the view cone infinitely tall. 
//=========================================================
bool CBaseCombatCharacter::FInViewCone( const Vector &vecSpot )
{
	Vector los = ( vecSpot - EyePosition() );

	// do this in 2D
	los.z = 0;
	VectorNormalize( los );

	Vector facingDir = EyeDirection2D( );

	float flDot = DotProduct( los, facingDir );

	if ( flDot > m_flFieldOfView )
		return true;

	return false;
}

#ifdef PORTAL
//=========================================================
// FInViewCone - returns true is the passed ent is in
// the caller's forward view cone. The dot product is performed
// in 2d, making the view cone infinitely tall. 
//=========================================================
CProp_Portal* CBaseCombatCharacter::FInViewConeThroughPortal( CBaseEntity *pEntity )
{
	return FInViewConeThroughPortal( pEntity->WorldSpaceCenter() );
}

//=========================================================
// FInViewCone - returns true is the passed Vector is in
// the caller's forward view cone. The dot product is performed
// in 2d, making the view cone infinitely tall. 
//=========================================================
CProp_Portal* CBaseCombatCharacter::FInViewConeThroughPortal( const Vector &vecSpot )
{
	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	if( iPortalCount == 0 )
		return NULL;

	const Vector ptEyePosition = EyePosition();

	float fDistToBeat = 1e20; //arbitrarily high number
	CProp_Portal *pBestPortal = NULL;

	CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();

	// Check through both portals
	for ( int iPortal = 0; iPortal < iPortalCount; ++iPortal )
	{
		CProp_Portal *pPortal = pPortals[iPortal];

		// Check if this portal is active, linked, and in the view cone
		if( pPortal->IsActivedAndLinked() && FInViewCone( pPortal ) )
		{
			// The facing direction is the eye to the portal to set up a proper FOV through the relatively small portal hole
			Vector facingDir = pPortal->GetAbsOrigin() - ptEyePosition;

			// If the portal isn't facing the eye, bail
			if ( facingDir.Dot( pPortal->m_plane_Origin.normal ) > 0.0f )
				continue;

			// If the point is behind the linked portal, bail
			if ( ( vecSpot - pPortal->m_hLinkedPortal->GetAbsOrigin() ).Dot( pPortal->m_hLinkedPortal->m_plane_Origin.normal ) < 0.0f )
				continue;

			// Remove height from the equation
			facingDir.z = 0.0f;
			float fPortalDist = VectorNormalize( facingDir );

			// Translate the target spot across the portal
			Vector vTranslatedVecSpot;
			UTIL_Portal_PointTransform( pPortal->m_hLinkedPortal->MatrixThisToLinked(), vecSpot, vTranslatedVecSpot );

			// do this in 2D
			Vector los = ( vTranslatedVecSpot - ptEyePosition );
			los.z = 0.0f;
			float fSpotDist = VectorNormalize( los );

			if( fSpotDist > fDistToBeat )
				continue; //no point in going further, we already have a better portal

			// If the target point is closer than the portal (banana juice), bail
			// HACK: Extra 32 is a fix for the player who's origin can be on one side of a portal while his center mirrored across is closer than the portal.
			if ( fPortalDist > fSpotDist + 32.0f )
				continue;

			// Get the worst case FOV from the portal's corners
			float fFOVThroughPortal = 1.0f;

			for ( int i = 0; i < 4; ++i )
			{
				//Vector vPortalCorner = pPortal->GetAbsOrigin() + vPortalRight * PORTAL_HALF_WIDTH * ( ( i / 2 == 0 ) ? ( 1.0f ) : ( -1.0f ) ) + 
				//												 vPortalUp * PORTAL_HALF_HEIGHT * ( ( i % 2 == 0 ) ? ( 1.0f ) : ( -1.0f ) );

				Vector vEyeToCorner = pPortal->m_vPortalCorners[i] - ptEyePosition;
				vEyeToCorner.z = 0.0f;
				VectorNormalize( vEyeToCorner );

				float flCornerDot = DotProduct( vEyeToCorner, facingDir );

				if ( flCornerDot < fFOVThroughPortal )
					fFOVThroughPortal = flCornerDot;
			}

			float flDot = DotProduct( los, facingDir );

			// Use the tougher FOV of either the standard FOV or FOV clipped to the portal hole
			if ( flDot > MAX( fFOVThroughPortal, m_flFieldOfView ) )
			{
				float fActualDist = ptEyePosition.DistToSqr( vTranslatedVecSpot );
				if( fActualDist < fDistToBeat )
				{
					fDistToBeat = fActualDist;
					pBestPortal = pPortal;
				}
			}
		}
	}

	return pBestPortal;
}
#endif


//=========================================================
// FInAimCone - returns true is the passed ent is in
// the caller's forward aim cone. The dot product is performed
// in 2d, making the aim cone infinitely tall. 
//=========================================================
bool CBaseCombatCharacter::FInAimCone( CBaseEntity *pEntity )
{
	return FInAimCone( pEntity->BodyTarget( EyePosition() ) );
}


//=========================================================
// FInAimCone - returns true is the passed Vector is in
// the caller's forward aim cone. The dot product is performed
// in 2d, making the view cone infinitely tall. By default, the
// callers aim cone is assumed to be very narrow
//=========================================================
bool CBaseCombatCharacter::FInAimCone( const Vector &vecSpot )
{
	Vector los = ( vecSpot - GetAbsOrigin() );

	// do this in 2D
	los.z = 0;
	VectorNormalize( los );

	Vector facingDir = BodyDirection2D( );

	float flDot = DotProduct( los, facingDir );

	if ( flDot > 0.994 )//!!!BUGBUG - magic number same as FacingIdeal(), what is this?
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose:  This is a generic function (to be implemented by sub-classes) to
//			 handle specific interactions between different types of characters
//			 (For example the barnacle grabbing an NPC)
// Input  :  The type of interaction, extra info pointer, and who started it
// Output :	 true  - if sub-class has a response for the interaction
//			 false - if sub-class has no response
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::HandleInteraction( int interactionType, void *data, CBaseCombatCharacter* sourceEnt )
{
#ifdef HL2_DLL
	if ( interactionType == g_interactionBarnacleVictimReleased )
	{
		// For now, throw away the NPC and leave the ragdoll.
		UTIL_Remove( this );
		return true;
	}
#endif // HL2_DLL
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor : Initialize some fields
//-----------------------------------------------------------------------------
CBaseCombatCharacter::CBaseCombatCharacter( void )
{
#ifdef _DEBUG
	// necessary since in debug, we initialize vectors to NAN for debugging
	m_HackedGunPos.Init();
#endif
	this->tickinterval = DEFAULT_TICK_INTERVAL;
	// gameinterface.cpp line 861
	// override if tick rate specified in command line
	if (CommandLine()->CheckParm("-tickrate"))
	{
		float tickrate = CommandLine()->ParmValue("-tickrate", 0);
		if (tickrate > 10)
			tickinterval = 1.0f / tickrate;
	}

	m_StatusEffectList;
	// Zero the damage accumulator.
	m_flDamageAccumulator = 0.0f;
	m_flDamageTracker = 0.0;
	// negative means no limit
	m_flDamageTrackerLimit = -1.0f;

	// Init weapon and Ammo data
	m_hActiveWeapon			= NULL;

	// reset all ammo values to 0
	RemoveAllAmmo();
	
	// not alive yet
	m_aliveTimer.Invalidate();
	m_hasBeenInjured = 0;

	for( int t=0; t<MAX_DAMAGE_TEAMS; ++t )
	{
		m_damageHistory[t].team = TEAM_INVALID;
	}

	// not standing on a nav area yet
	m_lastNavArea = NULL;
	m_registeredNavTeam = TEAM_INVALID;

	for (int i = 0; i < MAX_WEAPONS; i++)
	{
		m_hMyWeapons.Set( i, NULL );
	}

	// Default so that spawned entities have this set
	m_impactEnergyScale = 1.0f;

	m_bForceServerRagdoll = ai_force_serverside_ragdoll.GetBool();

	dmgMul=1.0;
	dmgAdd=0.0;

#ifdef GLOWS_ENABLE
	m_bGlowEnabled.Set( false );
	m_GlowColor.GetForModify().Init( 0.76f, 0.76f, 0.76f );
	m_GlowAlpha.Set(1.0f);
#endif // GLOWS_ENABLE
}

//------------------------------------------------------------------------------
// Purpose : Destructor
// Input   :
// Output  :
//------------------------------------------------------------------------------
CBaseCombatCharacter::~CBaseCombatCharacter( void )
{
	// ~~Clear the status effect list pointers. Hopefully it's not a memory leak! :D~~
	// You dumbass how are you gonna serialize pointers into storage if they'll point to garbage next session
	//                                                                             
	/*
	if (this->m_StatusEffectList.Count() > 0) {
		for (int i = m_StatusEffectList.Count()-1; i >= -1; i--) {
			delete m_StatusEffectList[i];
		}
	}
	*/
	//this->m_StatusEffectList.PurgeAndDeleteElements();
	
 	ResetVisibilityCache( this );
	ClearLastKnownArea();
}

void CBaseCombatCharacter::SetStatusThink() {
	RegisterThinkContext("StatusContext");
	SetContextThink(&CBaseCombatCharacter::StatusThink, gpGlobals->curtime, "StatusContext");
}

//-----------------------------------------------------------------------------
// Purpose: Put the combat character into the environment
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::Spawn( void )
{
	BaseClass::Spawn();
	
	SetBlocksLOS( false );
	m_aliveTimer.Start();
	m_hasBeenInjured = 0;

	for( int t=0; t<MAX_DAMAGE_TEAMS; ++t )
	{
		m_damageHistory[t].team = TEAM_INVALID;
	}

	// not standing on a nav area yet
	ClearLastKnownArea();

	// Will need to manually add this to some NPC families Spawn() since not all call CBaseCombatCharacter::Spawn()
	SetStatusThink();

	

}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::Precache()
{
	BaseClass::Precache();

	PrecacheScriptSound( "BaseCombatCharacter.CorpseGib" );
	PrecacheScriptSound( "BaseCombatCharacter.StopWeaponSounds" );
	PrecacheScriptSound( "BaseCombatCharacter.AmmoPickup" );
	

	
	// also shit the status sounds here cuz why not
	PrecacheScriptSound("Status.Overcharge");
	PrecacheScriptSound("Status.Bleed");
	PrecacheScriptSound("Status.Poison");
	PrecacheScriptSound("Status.Unslimed");
	PrecacheScriptSound("Status.Slimed");

	PrecacheScriptSound("sfx.hologramdie");



	for ( int i = m_Relationship.Count() - 1; i >= 0 ; i--) 
	{
		if ( !m_Relationship[i].entity && m_Relationship[i].classType == CLASS_NONE ) 
		{
			DevMsg( 2, "Removing relationship for lost entity\n" );
			m_Relationship.FastRemove( i );
		}
	}

	for (int i = 0; i <	sizeof(STATUS_SPRITES)/sizeof(char*); i++) {
		PrecacheModel(STATUS_SPRITES[i]);
	}

	for (int i = 0; i < sizeof(STATUS_MODELS)/ sizeof(char*); i++) {
		PrecacheModel(STATUS_MODELS[i]);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseCombatCharacter::Restore( IRestore &restore )
{
	int status = BaseClass::Restore(restore);
	if ( !status )
		return 0;

	if ( gpGlobals->eLoadType == MapLoad_Transition )
	{
		DevMsg( 2, "%s (%s) removing class relationships due to level transition\n", STRING( GetEntityName() ), GetClassname() );

		for ( int i = m_Relationship.Count() - 1; i >= 0; --i )
		{
			if ( !m_Relationship[i].entity && m_Relationship[i].classType != CLASS_NONE ) 
			{
				m_Relationship.FastRemove( i );
			}
		}
	}
	return status;
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::UpdateOnRemove( void )
{
	int i;
	// Make sure any weapons I didn't drop get removed.
	for (i=0;i<MAX_WEAPONS;i++) 
	{
		if (m_hMyWeapons[i]) 
		{
			UTIL_Remove( m_hMyWeapons[i] );
		}
	}

	// tell owner ( if any ) that we're dead.This is mostly for NPCMaker functionality.
	CBaseEntity *pOwner = GetOwnerEntity();
	if ( pOwner )
	{
		pOwner->DeathNotice( this );
		SetOwnerEntity( NULL );
	}

#ifdef GLOWS_ENABLE
	RemoveGlowEffect();
#endif // GLOWS_ENABLE

	// Chain at end to mimic destructor unwind order
	BaseClass::UpdateOnRemove();
}


//=========================================================
// CorpseGib - create some gore and get rid of a character's
// model.
//=========================================================
bool CBaseCombatCharacter::CorpseGib( const CTakeDamageInfo &info )
{
	trace_t		tr;
	bool		gibbed = false;

	EmitSound( "BaseCombatCharacter.CorpseGib" );

	// only humans throw skulls !!!UNDONE - eventually NPCs will have their own sets of gibs
	if ( HasHumanGibs() )
	{
		CGib::SpawnHeadGib( this );
		CGib::SpawnRandomGibs( this, 4, GIB_HUMAN );	// throw some human gibs.
		gibbed = true;
	}
	else if ( HasAlienGibs() )
	{
		CGib::SpawnRandomGibs( this, 4, GIB_ALIEN );	// Throw alien gibs
		gibbed = true;
	}

	return gibbed;
}

//=========================================================
// GetDeathActivity - determines the best type of death
// anim to play.
//=========================================================
Activity CBaseCombatCharacter::GetDeathActivity ( void )
{
	Activity	deathActivity;
	bool		fTriedDirection;
	float		flDot;
	trace_t		tr;
	Vector		vecSrc;

	if (IsPlayer())
	{
		// die in an interesting way
		switch( random->RandomInt(0,7) )
		{
		case 0:	return ACT_DIESIMPLE;
		case 1: return ACT_DIEBACKWARD;
		case 2: return ACT_DIEFORWARD;
		case 3: return ACT_DIEVIOLENT;
		case 4: return ACT_DIE_HEADSHOT;
		case 5: return ACT_DIE_CHESTSHOT;
		case 6: return ACT_DIE_GUTSHOT;
		case 7: return ACT_DIE_BACKSHOT;
		}
	}

	vecSrc = WorldSpaceCenter();

	fTriedDirection = false;
	deathActivity = ACT_DIESIMPLE;// in case we can't find any special deaths to do.

	Vector forward;
	AngleVectors( GetLocalAngles(), &forward );
	flDot = -DotProduct( forward, g_vecAttackDir );

	switch ( m_LastHitGroup )
	{
		// try to pick a region-specific death.
	case HITGROUP_HEAD:
		deathActivity = ACT_DIE_HEADSHOT;
		break;

	case HITGROUP_STOMACH:
		deathActivity = ACT_DIE_GUTSHOT;
		break;

	case HITGROUP_GENERIC:
		// try to pick a death based on attack direction
		fTriedDirection = true;

		if ( flDot > 0.3 )
		{
			deathActivity = ACT_DIEFORWARD;
		}
		else if ( flDot <= -0.3 )
		{
			deathActivity = ACT_DIEBACKWARD;
		}
		break;

	default:
		// try to pick a death based on attack direction
		fTriedDirection = true;

		if ( flDot > 0.3 )
		{
			deathActivity = ACT_DIEFORWARD;
		}
		else if ( flDot <= -0.3 )
		{
			deathActivity = ACT_DIEBACKWARD;
		}
		break;
	}


	// can we perform the prescribed death?
	if ( SelectWeightedSequence ( deathActivity ) == ACTIVITY_NOT_AVAILABLE )
	{
		// no! did we fail to perform a directional death? 
		if ( fTriedDirection )
		{
			// if yes, we're out of options. Go simple.
			deathActivity = ACT_DIESIMPLE;
		}
		else
		{
			// cannot perform the ideal region-specific death, so try a direction.
			if ( flDot > 0.3 )
			{
				deathActivity = ACT_DIEFORWARD;
			}
			else if ( flDot <= -0.3 )
			{
				deathActivity = ACT_DIEBACKWARD;
			}
		}
	}

	if ( SelectWeightedSequence ( deathActivity ) == ACTIVITY_NOT_AVAILABLE )
	{
		// if we're still invalid, simple is our only option.
		deathActivity = ACT_DIESIMPLE;

		if ( SelectWeightedSequence ( deathActivity ) == ACTIVITY_NOT_AVAILABLE )
		{
			Msg( "ERROR! %s missing ACT_DIESIMPLE\n", STRING(GetModelName()) );
		}
	}

	if ( deathActivity == ACT_DIEFORWARD )
	{
			// make sure there's room to fall forward
			UTIL_TraceHull ( vecSrc, vecSrc + forward * 64, Vector(-16,-16,-18), 
				Vector(16,16,18), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

			if ( tr.fraction != 1.0 )
			{
				deathActivity = ACT_DIESIMPLE;
			}
	}

	if ( deathActivity == ACT_DIEBACKWARD )
	{
			// make sure there's room to fall backward
			UTIL_TraceHull ( vecSrc, vecSrc - forward * 64, Vector(-16,-16,-18), 
				Vector(16,16,18), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

			if ( tr.fraction != 1.0 )
			{
				deathActivity = ACT_DIESIMPLE;
			}
	}

	return deathActivity;
}


// UNDONE: Should these operate on a list of weapon/items
Activity CBaseCombatCharacter::Weapon_TranslateActivity( Activity baseAct, bool *pRequired )
{
	Activity translated = baseAct;

	if ( m_hActiveWeapon )
	{
		translated = m_hActiveWeapon->ActivityOverride( baseAct, pRequired );
	}
	else if (pRequired)
	{
		*pRequired = false;
	}

	return translated;
}

//-----------------------------------------------------------------------------
// Purpose: NPCs should override this function to translate activities
//			such as ACT_WALK, etc.
// Input  :
// Output :
//-----------------------------------------------------------------------------
Activity CBaseCombatCharacter::NPC_TranslateActivity( Activity baseAct )
{
	return baseAct;
}


void CBaseCombatCharacter::Weapon_SetActivity( Activity newActivity, float duration )
{
	if ( m_hActiveWeapon )
	{
		m_hActiveWeapon->SetActivity( newActivity, duration );
	}
}

void CBaseCombatCharacter::Weapon_FrameUpdate( void )
{
	if ( m_hActiveWeapon )
	{
		m_hActiveWeapon->Operator_FrameUpdate( this );
	}
}


//------------------------------------------------------------------------------
// Purpose :	expects a length to trace, amount 
//				of damage to do, and damage type. Returns a pointer to
//				the damaged entity in case the NPC wishes to do
//				other stuff to the victim (punchangle, etc)
//
//				Used for many contact-range melee attacks. Bites, claws, etc.
// Input   :
// Output  :
//------------------------------------------------------------------------------
CBaseEntity *CBaseCombatCharacter::CheckTraceHullAttack( float flDist, const Vector &mins, const Vector &maxs, int iDamage, int iDmgType, float forceScale, bool bDamageAnyNPC )
{
	// If only a length is given assume we want to trace in our facing direction
	Vector forward;
	AngleVectors( GetAbsAngles(), &forward );
	Vector vStart = GetAbsOrigin();

	// The ideal place to start the trace is in the center of the attacker's bounding box.
	// however, we need to make sure there's enough clearance. Some of the smaller monsters aren't 
	// as big as the hull we try to trace with. (SJB)
	float flVerticalOffset = WorldAlignSize().z * 0.5;

	if( flVerticalOffset < maxs.z )
	{
		// There isn't enough room to trace this hull, it's going to drag the ground.
		// so make the vertical offset just enough to clear the ground.
		flVerticalOffset = maxs.z + 1.0;
	}

	vStart.z += flVerticalOffset;
	Vector vEnd = vStart + (forward * flDist );
	return CheckTraceHullAttack( vStart, vEnd, mins, maxs, iDamage, iDmgType, forceScale, bDamageAnyNPC );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pHandleEntity - 
//			contentsMask - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CTraceFilterMelee::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	if ( !StandardFilterRules( pHandleEntity, contentsMask ) )
		return false;

	if ( !PassServerEntityFilter( pHandleEntity, m_pPassEnt ) )
		return false;

	// Don't test if the game code tells us we should ignore this collision...
	CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
	
	if ( pEntity )
	{
		if ( !pEntity->ShouldCollide( m_collisionGroup, contentsMask ) )
			return false;
		
		if ( !g_pGameRules->ShouldCollide( m_collisionGroup, pEntity->GetCollisionGroup() ) )
			return false;

		if ( pEntity->m_takedamage == DAMAGE_NO )
			return false;

#ifdef MAPBASE // Moved from CheckTraceHullAttack()
		if( m_pPassEnt && !pEntity->CanBeHitByMeleeAttack( const_cast<CBaseEntity*>(EntityFromEntityHandle( m_pPassEnt ) ) ) )
			return false;
#endif

		// FIXME: Do not translate this to the driver because the driver only accepts damage from the vehicle
		// Translate the vehicle into its driver for damage
		/*
		if ( pEntity->GetServerVehicle() != NULL )
		{
			CBaseEntity *pDriver = pEntity->GetServerVehicle()->GetPassenger();

			if ( pDriver != NULL )
			{
				pEntity = pDriver;
			}
		}
		*/

		Vector	attackDir = pEntity->WorldSpaceCenter() - m_dmgInfo->GetAttacker()->WorldSpaceCenter();
		VectorNormalize( attackDir );

		CTakeDamageInfo info = (*m_dmgInfo);				
		CalculateMeleeDamageForce( &info, attackDir, info.GetAttacker()->WorldSpaceCenter(), m_flForceScale );

		CBaseCombatCharacter *pBCC = info.GetAttacker()->MyCombatCharacterPointer();
		CBaseCombatCharacter *pVictimBCC = pEntity->MyCombatCharacterPointer();

		// Only do these comparisons between NPCs
		if ( pBCC && pVictimBCC )
		{
			// Can only damage other NPCs that we hate
#ifdef MAPBASE
			if ( m_bDamageAnyNPC || pBCC->IRelationType( pEntity ) <= D_FR )
#else
			if ( m_bDamageAnyNPC || pBCC->IRelationType( pEntity ) == D_HT )
#endif
			{
				if ( info.GetDamage() )
				{
					pEntity->TakeDamage( info );
				}
				
				// Put a combat sound in
				CSoundEnt::InsertSound( SOUND_COMBAT, info.GetDamagePosition(), 200, 0.2f, info.GetAttacker() );

				m_pHit = pEntity;
				return true;
			}
		}
		else
		{
#ifdef MAPBASE
			// Do not override an existing hit entity
			if (!m_pHit)
#endif
			m_pHit = pEntity;

			// Make sure if the player is holding this, he drops it
			Pickup_ForcePlayerToDropThisObject( pEntity );

			// Otherwise just damage passive objects in our way
			if ( info.GetDamage() )
			{
				pEntity->TakeDamage( info );
			}
		}
	}

	return false;
}

//------------------------------------------------------------------------------
// Purpose :	start and end trace position, amount 
//				of damage to do, and damage type. Returns a pointer to
//				the damaged entity in case the NPC wishes to do
//				other stuff to the victim (punchangle, etc)
//
//				Used for many contact-range melee attacks. Bites, claws, etc.
// Input   :
// Output  :
//------------------------------------------------------------------------------
CBaseEntity *CBaseCombatCharacter::CheckTraceHullAttack( const Vector &vStart, const Vector &vEnd, const Vector &mins, const Vector &maxs, int iDamage, int iDmgType, float flForceScale, bool bDamageAnyNPC )
{
	// Handy debuging tool to visualize HullAttack trace
	if ( ai_show_hull_attacks.GetBool() )
	{
		float length	 = (vEnd - vStart ).Length();
		Vector direction = (vEnd - vStart );
		VectorNormalize( direction );
		Vector hullMaxs = maxs;
		hullMaxs.x = length + hullMaxs.x;
		NDebugOverlay::BoxDirection(vStart, mins, hullMaxs, direction, 100,255,255,20,1.0);
		NDebugOverlay::BoxDirection(vStart, mins, maxs, direction, 255,0,0,20,1.0);
	}

#if 1

	CTakeDamageInfo	dmgInfo( this, this, iDamage, iDmgType );
	
	// COLLISION_GROUP_PROJECTILE does some handy filtering that's very appropriate for this type of attack, as well. (sjb) 7/25/2007
	CTraceFilterMelee traceFilter( this, COLLISION_GROUP_PROJECTILE, &dmgInfo, flForceScale, bDamageAnyNPC );

	Ray_t ray;
	ray.Init( vStart, vEnd, mins, maxs );

	trace_t tr;
	enginetrace->TraceRay( ray, MASK_SHOT_HULL, &traceFilter, &tr );

	CBaseEntity *pEntity = traceFilter.m_pHit;
	
	if ( pEntity == NULL )
	{
		// See if perhaps I'm trying to claw/bash someone who is standing on my head.
		Vector vecTopCenter;
		Vector vecEnd;
		Vector vecMins, vecMaxs;

		// Do a tracehull from the top center of my bounding box.
		vecTopCenter = GetAbsOrigin();
		CollisionProp()->WorldSpaceAABB( &vecMins, &vecMaxs );
		vecTopCenter.z = vecMaxs.z + 1.0f;
		vecEnd = vecTopCenter;
		vecEnd.z += 2.0f;
		
		ray.Init( vecTopCenter, vEnd, mins, maxs );
		enginetrace->TraceRay( ray, MASK_SHOT_HULL, &traceFilter, &tr );

		pEntity = traceFilter.m_pHit;
	}

#ifndef MAPBASE // Moved to CTraceFilterMelee
	if( pEntity && !pEntity->CanBeHitByMeleeAttack(this) )
	{
		// If we touched something, but it shouldn't be hit, return nothing.
		pEntity = NULL;
	}
#endif

	return pEntity;

#else

	trace_t tr;
	UTIL_TraceHull( vStart, vEnd, mins, maxs, MASK_SHOT_HULL, this, COLLISION_GROUP_NONE, &tr );

	CBaseEntity *pEntity = tr.m_pEnt;

	if ( !pEntity )
	{
		// See if perhaps I'm trying to claw/bash someone who is standing on my head.
		Vector vecTopCenter;
		Vector vecEnd;
		Vector vecMins, vecMaxs;

		// Do a tracehull from the top center of my bounding box.
		vecTopCenter = GetAbsOrigin();
		CollisionProp()->WorldSpaceAABB( &vecMins, &vecMaxs );
		vecTopCenter.z = vecMaxs.z + 1.0f;
		vecEnd = vecTopCenter;
		vecEnd.z += 2.0f;
		UTIL_TraceHull( vecTopCenter, vecEnd, mins, maxs, MASK_SHOT_HULL, this, COLLISION_GROUP_NONE, &tr );
		pEntity = tr.m_pEnt;
	}

	if ( !pEntity || !pEntity->m_takedamage || !pEntity->IsAlive() )
		return NULL;

	// Translate the vehicle into its driver for damage
	if ( pEntity->GetServerVehicle() != NULL )
	{
		CBaseEntity *pDriver = pEntity->GetServerVehicle()->GetPassenger();

		if ( pDriver != NULL )
		{
			pEntity = pDriver;
			//FIXME: Hook for damage scale in car here
		}
	}

	// Must hate the hit entity
	if ( IRelationType( pEntity ) == D_HT )
	{
		if ( iDamage > 0 )
		{
			CTakeDamageInfo info( this, this, iDamage, iDmgType );
			CalculateMeleeDamageForce( &info, (vEnd - vStart), vStart, forceScale );
			pEntity->TakeDamage( info );
		}
	}
	return pEntity;

#endif

}


bool  CBaseCombatCharacter::Event_Gibbed( const CTakeDamageInfo &info )
{
	bool fade = false;

	if ( HasHumanGibs() )
	{
		ConVarRef violence_hgibs( "violence_hgibs" );
		if ( violence_hgibs.IsValid() && violence_hgibs.GetInt() == 0 )
		{
			fade = true;
		}
	}
	else if ( HasAlienGibs() )
	{
		ConVarRef violence_agibs( "violence_agibs" );
		if ( violence_agibs.IsValid() && violence_agibs.GetInt() == 0 )
		{
			fade = true;
		}
	}

	m_takedamage	= DAMAGE_NO;
	AddSolidFlags( FSOLID_NOT_SOLID );
	m_lifeState		= LIFE_DEAD;

	if ( fade )
	{
		CorpseFade();
		return false;
	}
	else
	{
		AddEffects( EF_NODRAW ); // make the model invisible.
		return CorpseGib( info );
	}
}


Vector CBaseCombatCharacter::CalcDamageForceVector( const CTakeDamageInfo &info )
{
	// Already have a damage force in the data, use that.
	bool bNoPhysicsForceDamage = g_pGameRules->Damage_NoPhysicsForce( info.GetDamageType() );
	if ( info.GetDamageForce() != vec3_origin || bNoPhysicsForceDamage )
	{
		if( info.GetDamageType() & DMG_BLAST )
		{
			// Fudge blast forces a little bit, so that each
			// victim gets a slightly different trajectory. 
			// This simulates features that usually vary from
			// person-to-person variables such as bodyweight,
			// which are all indentical for characters using the same model.
			float scale = random->RandomFloat( 0.85, 1.15 );
			Vector force = info.GetDamageForce();
			force.x *= scale;
			force.y *= scale;
			// Try to always exaggerate the upward force because we've got pretty harsh gravity
			force.z *= (force.z > 0) ? 1.15 : scale;
			return force;
		}

		return info.GetDamageForce();
	}

	CBaseEntity *pForce = info.GetInflictor();
	if ( !pForce )
	{
		pForce = info.GetAttacker();
	}

	if ( pForce )
	{
		// Calculate an impulse large enough to push a 75kg man 4 in/sec per point of damage
		float forceScale = info.GetDamage() * 75 * 4;

		Vector forceVector;
		// If the damage is a blast, point the force vector higher than usual, this gives 
		// the ragdolls a bodacious "really got blowed up" look.
		if( info.GetDamageType() & DMG_BLAST )
		{
			// exaggerate the force from explosions a little (37.5%)
			forceVector = (GetLocalOrigin() + Vector(0, 0, WorldAlignSize().z) ) - pForce->GetLocalOrigin();
			VectorNormalize(forceVector);
			forceVector *= 1.375f;
		}
		else
		{
			// taking damage from self?  Take a little random force, but still try to collapse on the spot.
			if ( this == pForce )
			{
				forceVector.x = random->RandomFloat( -1.0f, 1.0f );
				forceVector.y = random->RandomFloat( -1.0f, 1.0f );
				forceVector.z = 0.0;
				forceScale = random->RandomFloat( 1000.0f, 2000.0f );
			}
			else
			{
				// UNDONE: Collision forces are baked in to CTakeDamageInfo now
				// UNDONE: Is this MOVETYPE_VPHYSICS code still necessary?
				if ( pForce->GetMoveType() == MOVETYPE_VPHYSICS )
				{
					// killed by a physics object
					IPhysicsObject *pPhysics = VPhysicsGetObject();
					if ( !pPhysics )
					{
						pPhysics = pForce->VPhysicsGetObject();
					}
					pPhysics->GetVelocity( &forceVector, NULL );
					forceScale = pPhysics->GetMass();
				}
				else
				{
					forceVector = GetLocalOrigin() - pForce->GetLocalOrigin();
					VectorNormalize(forceVector);
				}
			}
		}
		return forceVector * forceScale;
	}
	return vec3_origin;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::FixupBurningServerRagdoll( CBaseEntity *pRagdoll )
{
	if ( !IsOnFire() )
		return;

	// Move the fire effects entity to the ragdoll
	CEntityFlame *pFireChild = dynamic_cast<CEntityFlame *>( GetEffectEntity() );
	if ( pFireChild )
	{
		SetEffectEntity( NULL );
		pRagdoll->AddFlag( FL_ONFIRE );
		pFireChild->SetAbsOrigin( pRagdoll->GetAbsOrigin() );
		pFireChild->AttachToEntity( pRagdoll );
		pFireChild->AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
 		pRagdoll->SetEffectEntity( pFireChild );

		color32 color = GetRenderColor();
		pRagdoll->SetRenderColor( color.r, color.g, color.b );
	}
}

inline bool CBaseCombatCharacter::ShouldFadeServerRagdolls() const
{
#ifdef MAPBASE
	return IsNPC() ? HasSpawnFlags( SF_NPC_FADE_CORPSE ) : true;
#else
	return true;
#endif
}

bool CBaseCombatCharacter::BecomeRagdollBoogie( CBaseEntity *pKiller, const Vector &forceVector, float duration, int flags )
{
	Assert( CanBecomeRagdoll() );

	CTakeDamageInfo info( pKiller, pKiller, 1.0f, DMG_GENERIC );

	info.SetDamageForce( forceVector );

	CBaseEntity *pRagdoll = CreateServerRagdoll( this, 0, info, COLLISION_GROUP_INTERACTIVE_DEBRIS, ShouldFadeServerRagdolls() );

	pRagdoll->SetCollisionBounds( CollisionProp()->OBBMins(), CollisionProp()->OBBMaxs() );

	CRagdollBoogie::Create( pRagdoll, 200, gpGlobals->curtime, duration, flags );

	CTakeDamageInfo ragdollInfo( pKiller, pKiller, 10000.0, DMG_GENERIC | DMG_REMOVENORAGDOLL );
	ragdollInfo.SetDamagePosition(WorldSpaceCenter());
	ragdollInfo.SetDamageForce( Vector( 0, 0, 1) );
	TakeDamage( ragdollInfo );

	return true;
}

#ifdef MAPBASE
CBaseEntity *CBaseCombatCharacter::BecomeRagdollBoogie( CBaseEntity *pKiller, const Vector &forceVector, float duration, int flags, const Vector *vecColor )
{
	Assert( CanBecomeRagdoll() );

	CTakeDamageInfo info( pKiller, pKiller, 1.0f, DMG_GENERIC );

	info.SetDamageForce( forceVector );

	CBaseEntity *pRagdoll = CreateServerRagdoll( this, 0, info, COLLISION_GROUP_INTERACTIVE_DEBRIS, ShouldFadeServerRagdolls() );

	pRagdoll->SetCollisionBounds( CollisionProp()->OBBMins(), CollisionProp()->OBBMaxs() );

	CBaseEntity *pBoogie = CRagdollBoogie::Create( pRagdoll, 200, gpGlobals->curtime, duration, flags, vecColor );

	CTakeDamageInfo ragdollInfo( pKiller, pKiller, 10000.0, DMG_GENERIC | DMG_REMOVENORAGDOLL );
	ragdollInfo.SetDamagePosition( WorldSpaceCenter() );
	ragdollInfo.SetDamageForce( Vector( 0, 0, 1 ) );
	TakeDamage( ragdollInfo );

	return pBoogie;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::BecomeRagdoll( const CTakeDamageInfo &info, const Vector &forceVector )
{
	if ( (info.GetDamageType() & DMG_VEHICLE) && !g_pGameRules->IsMultiplayer() )
	{
		CTakeDamageInfo info2 = info;
		info2.SetDamageForce( forceVector );
		Vector pos = info2.GetDamagePosition();
		float flAbsMinsZ = GetAbsOrigin().z + WorldAlignMins().z;
		if ( (pos.z - flAbsMinsZ) < 24 )
		{
			// HACKHACK: Make sure the vehicle impact is at least 2ft off the ground
			pos.z = flAbsMinsZ + 24;
			info2.SetDamagePosition( pos );
		}

// UNDONE: Put in a real sound cue here, don't do this bogus hack anymore
#if 0
		Vector soundOrigin = info.GetDamagePosition();
		CPASAttenuationFilter filter( soundOrigin );

		EmitSound_t ep;
		ep.m_nChannel = CHAN_STATIC;
		ep.m_pSoundName = "NPC_MetroPolice.HitByVehicle";
		ep.m_flVolume = 1.0f;
		ep.m_SoundLevel = SNDLVL_NORM;
		ep.m_pOrigin = &soundOrigin;

		EmitSound( filter, SOUND_FROM_WORLD, ep );
#endif
		// in single player create ragdolls on the server when the player hits someone
		// with their vehicle - for more dramatic death/collisions
		CBaseEntity *pRagdoll = CreateServerRagdoll( this, m_nForceBone, info2, COLLISION_GROUP_INTERACTIVE_DEBRIS, ShouldFadeServerRagdolls() );
		FixupBurningServerRagdoll( pRagdoll );
		RemoveDeferred();
		return true;
	}

	//Fix up the force applied to server side ragdolls. This fixes magnets not affecting them.
	CTakeDamageInfo newinfo = info;
	newinfo.SetDamageForce( forceVector );

#ifdef HL2_EPISODIC
	// Burning corpses are server-side in episodic, if we're in darkness mode
	if ( IsOnFire() && HL2GameRules()->IsAlyxInDarknessMode() )
	{
		CBaseEntity *pRagdoll = CreateServerRagdoll( this, m_nForceBone, newinfo, COLLISION_GROUP_DEBRIS, ShouldFadeServerRagdolls() );
		FixupBurningServerRagdoll( pRagdoll );
		RemoveDeferred();
		return true;
	}
#endif

#ifdef HL2_DLL	

	bool bMegaPhyscannonActive = false;
#if !defined( HL2MP )
	bMegaPhyscannonActive = HL2GameRules()->MegaPhyscannonActive();
#endif // !HL2MP

	// Mega physgun requires everything to be a server-side ragdoll
	if ( m_bForceServerRagdoll == true || ( ( bMegaPhyscannonActive == true ) && !IsPlayer() && Classify() != CLASS_PLAYER_ALLY_VITAL && Classify() != CLASS_PLAYER_ALLY ) )
	{
		if ( CanBecomeServerRagdoll() == false )
			return false;

		//FIXME: This is fairly leafy to be here, but time is short!
		CBaseEntity *pRagdoll = CreateServerRagdoll( this, m_nForceBone, newinfo, COLLISION_GROUP_INTERACTIVE_DEBRIS, ShouldFadeServerRagdolls() );
		FixupBurningServerRagdoll( pRagdoll );
		PhysSetEntityGameFlags( pRagdoll, FVPHYSICS_NO_SELF_COLLISIONS );
		RemoveDeferred();

		return true;
	}

	if( hl2_episodic.GetBool() && Classify() == CLASS_PLAYER_ALLY_VITAL )
	{
		CreateServerRagdoll( this, m_nForceBone, newinfo, COLLISION_GROUP_INTERACTIVE_DEBRIS, ShouldFadeServerRagdolls() );
		RemoveDeferred();
		return true;
	}
#endif //HL2_DLL

	return BecomeRagdollOnClient( forceVector );
}


/*
============
Killed
============
*/
void CBaseCombatCharacter::Event_Killed( const CTakeDamageInfo &info )
{
	extern ConVar npc_vphysics;

	// Advance life state to dying
	m_lifeState = LIFE_DYING;

	// Calculate death force
	Vector forceVector = CalcDamageForceVector( info );

	// See if there's a ragdoll magnet that should influence our force.
	CRagdollMagnet *pMagnet = CRagdollMagnet::FindBestMagnet( this );
	if( pMagnet )
	{
#ifdef MAPBASE
		if (pMagnet->BoneTarget() && pMagnet->BoneTarget()[0] != '\0')
		{
			int iBone = -1;
			forceVector += pMagnet->GetForceVector( this, &iBone );
			if (iBone != -1)
				m_nForceBone = GetPhysicsBone(iBone);
		}
		else
		{
			forceVector += pMagnet->GetForceVector( this );
		}

		pMagnet->m_OnUsed.Set(forceVector, this, pMagnet);
#else
		forceVector += pMagnet->GetForceVector( this );
#endif
	}

	CBaseCombatWeapon *pDroppedWeapon = m_hActiveWeapon.Get();
	// DR: DO NOT DROP ANYTHING OR CREATE ANYTHING IF I AM A HOLOGRAM!!
	
	bool amIHologram = CheckHavingStatusEffect(ST_HOLOGRAM);
#ifdef DEBUG
	DevMsg("AM I HOLOGRAM? %s %d\n", this->GetClassname(), amIHologram);
#endif // DEBUG

	
	// Drop any weapon that I own
		if (VPhysicsGetObject())
		{
			Vector weaponForce = forceVector * VPhysicsGetObject()->GetInvMass();
			Weapon_Drop(m_hActiveWeapon, NULL, &weaponForce);
		}
		else
		{
			Weapon_Drop(m_hActiveWeapon);
		}

	
	// if flagged to drop a health kit

	if (HasSpawnFlags(SF_NPC_DROP_HEALTHKIT) && !amIHologram)
	{
		CBaseEntity *pItem = CBaseEntity::Create( "item_healthvial", GetAbsOrigin(), GetAbsAngles() );
		if (pItem)
		{
#ifdef MAPBASE
			if (MyNPCPointer())
				MyNPCPointer()->m_OnItemDrop.Set( pItem, pItem, this );
#endif
		}
	}
	// clear the deceased's sound channels.(may have been firing or reloading when killed)
	EmitSound( "BaseCombatCharacter.StopWeaponSounds" );

	// Tell my killer that he got me!
	if( info.GetAttacker() )
	{
		info.GetAttacker()->Event_KilledOther(this, info);
		g_EventQueue.AddEvent( info.GetAttacker(), "KilledNPC", 0.3, this, this );
	}
	SendOnKilledGameEvent( info );

	// Ragdoll unless we've gibbed
	if (amIHologram) {
		DevMsg("AM I HOLOGRAM? I'M NOT, JUST DISAPPEAR. %s %d\n", this->GetClassname(), amIHologram);
		this->EmitSound("sfx.hologramdie");
		UTIL_Remove(this);
	}
	else {
		if (ShouldGib(info) == false)
		{
			bool bRagdollCreated = false;
			if ((info.GetDamageType() & DMG_DISSOLVE) && CanBecomeRagdoll())
			{
				int nDissolveType = ENTITY_DISSOLVE_NORMAL;
				if (info.GetDamageType() & DMG_SHOCK)
				{
					nDissolveType = ENTITY_DISSOLVE_ELECTRICAL;
				}

				bRagdollCreated = Dissolve(NULL, gpGlobals->curtime, false, nDissolveType);

				// Also dissolve any weapons we dropped
				if (pDroppedWeapon)
				{
					pDroppedWeapon->Dissolve(NULL, gpGlobals->curtime, false, nDissolveType);
				}
			}
#ifdef HL2_DLL
#ifdef MAPBASE
			else if (PlayerHasMegaPhysCannon() && GlobalEntity_GetCounter("super_phys_gun") != 1)
#else
			else if (PlayerHasMegaPhysCannon())
#endif
			{
				if (pDroppedWeapon)
				{
					pDroppedWeapon->Dissolve(NULL, gpGlobals->curtime, false, ENTITY_DISSOLVE_NORMAL);
				}
			}
#endif

			if (!bRagdollCreated && (info.GetDamageType() & DMG_REMOVENORAGDOLL) == 0)
			{
				BecomeRagdoll(info, forceVector);
			}
		}
	}
	
	// no longer standing on a nav area
	ClearLastKnownArea();

#if 0
	// L4D specific hack for zombie commentary mode
	if( GetOwnerEntity() != NULL )
	{
		GetOwnerEntity()->DeathNotice( this );
	}
#endif
	
#ifdef NEXT_BOT
	// inform bots
	TheNextBots().OnKilled( this, info );
#endif

#ifdef GLOWS_ENABLE
	RemoveGlowEffect();
#endif // GLOWS_ENABLE
}

void CBaseCombatCharacter::Event_Dying( const CTakeDamageInfo &info )
{
}

void CBaseCombatCharacter::Event_Dying()
{
	CTakeDamageInfo info;
	Event_Dying( info );
}


// ===========================================================================
//  > Weapons
// ===========================================================================
bool CBaseCombatCharacter::Weapon_Detach( CBaseCombatWeapon *pWeapon )
{
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		if ( pWeapon == m_hMyWeapons[i] )
		{
			pWeapon->Detach();
			if ( pWeapon->HolsterOnDetach() )
			{
				pWeapon->Holster();
			}
			m_hMyWeapons.Set( i, NULL );
			pWeapon->SetOwner( NULL );

			if ( pWeapon == m_hActiveWeapon )
				ClearActiveWeapon();
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// For weapon strip
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::ThrowDirForWeaponStrip( CBaseCombatWeapon *pWeapon, const Vector &vecForward, Vector *pVecThrowDir )
{
	// HACK! Always throw the physcannon directly in front of the player
	// This is necessary for the physgun upgrade scene.
	if ( FClassnameIs( pWeapon, "weapon_physcannon" ) )
	{
		if( hl2_episodic.GetBool() )
		{
			// It has been discovered that it's possible to throw the physcannon out of the world this way.
			// So try to find a direction to throw the physcannon that's legal.
			Vector vecOrigin = EyePosition();
			Vector vecRight;

			CrossProduct( vecForward, Vector( 0, 0, 1), vecRight );

			Vector vecTest[ 4 ];
			vecTest[0] = vecForward;
			vecTest[1] = -vecForward;
			vecTest[2] = vecRight;
			vecTest[3] = -vecRight;

			trace_t tr;
			int i;
			for( i = 0 ; i < 4 ; i++ )
			{
				UTIL_TraceLine( vecOrigin, vecOrigin + vecTest[ i ] * 48.0f, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );

				if ( !tr.startsolid && tr.fraction == 1.0f )
				{
					*pVecThrowDir = vecTest[ i ];
					return;
				}
			}
		}

		// Well, fall through to what we did before we tried to make this a bit more robust.
		*pVecThrowDir = vecForward;
	}
	else
	{
		// Nowhere in particular; just drop it.
		VMatrix zRot;
		MatrixBuildRotateZ( zRot, random->RandomFloat( -60.0f, 60.0f ) );

		Vector vecThrow;
		Vector3DMultiply( zRot, vecForward, *pVecThrowDir );

		pVecThrowDir->z = random->RandomFloat( -0.5f, 0.5f );
		VectorNormalize( *pVecThrowDir );
	}
}


//-----------------------------------------------------------------------------
// For weapon strip
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::DropWeaponForWeaponStrip( CBaseCombatWeapon *pWeapon, 
	const Vector &vecForward, const QAngle &vecAngles, float flDiameter )
{
	Vector vecOrigin;
	CollisionProp()->RandomPointInBounds( Vector( 0.5f, 0.5f, 0.5f ), Vector( 0.5f, 0.5f, 1.0f ), &vecOrigin );

	// Nowhere in particular; just drop it.
	Vector vecThrow;
	ThrowDirForWeaponStrip( pWeapon, vecForward, &vecThrow );

	Vector vecOffsetOrigin;
	VectorMA( vecOrigin, flDiameter, vecThrow, vecOffsetOrigin );

	trace_t	tr;
	UTIL_TraceLine( vecOrigin, vecOffsetOrigin, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );
		
	if ( tr.startsolid || tr.allsolid || ( tr.fraction < 1.0f && tr.m_pEnt != pWeapon ) )
	{
		//FIXME: Throw towards a known safe spot?
		vecThrow.Negate();
		VectorMA( vecOrigin, flDiameter, vecThrow, vecOffsetOrigin );
	}

	vecThrow *= random->RandomFloat( 400.0f, 600.0f );

	pWeapon->SetAbsOrigin( vecOrigin );
	pWeapon->SetAbsAngles( vecAngles );
	pWeapon->Drop( vecThrow );
	pWeapon->SetRemoveable( false );
	Weapon_Detach( pWeapon );
}



//-----------------------------------------------------------------------------
// For weapon strip
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::Weapon_DropAll( bool bDisallowWeaponPickup )
{
	if ( GetFlags() & FL_NPC )
	{
		for (int i=0; i<MAX_WEAPONS; ++i) 
		{
			CBaseCombatWeapon *pWeapon = m_hMyWeapons[i];
			if (!pWeapon)
				continue;

 			Weapon_Drop( pWeapon );
		}
		return;
	}

	QAngle gunAngles;
	VectorAngles( BodyDirection2D(), gunAngles );

	Vector vecForward;
	AngleVectors( gunAngles, &vecForward, NULL, NULL );

	float flDiameter = sqrt( CollisionProp()->OBBSize().x * CollisionProp()->OBBSize().x +
		CollisionProp()->OBBSize().y * CollisionProp()->OBBSize().y );

	CBaseCombatWeapon *pActiveWeapon = GetActiveWeapon();
	for (int i=0; i<MAX_WEAPONS; ++i) 
	{
		CBaseCombatWeapon *pWeapon = m_hMyWeapons[i];
		if (!pWeapon)
			continue;

		// Have to drop this after we've dropped everything else, so autoswitch doesn't happen
		if ( pWeapon == pActiveWeapon )
			continue;

		DropWeaponForWeaponStrip( pWeapon, vecForward, gunAngles, flDiameter );

		// HACK: This hack is required to allow weapons to be disintegrated
		// in the citadel weapon-strip scene
		// Make them not pick-uppable again. This also has the effect of allowing weapons
		// to collide with triggers. 
		if ( bDisallowWeaponPickup )
		{
			pWeapon->RemoveSolidFlags( FSOLID_TRIGGER );
			
			IPhysicsObject *pObj = pWeapon->VPhysicsGetObject();
			
			if ( pObj != NULL )
			{	
				pObj->SetGameFlags( FVPHYSICS_NO_PLAYER_PICKUP );
			}
		}
	}

	// Drop the active weapon normally...
	if ( pActiveWeapon )
	{
		// Nowhere in particular; just drop it.
		Vector vecThrow;
		ThrowDirForWeaponStrip( pActiveWeapon, vecForward, &vecThrow );

		// Throw a little more vigorously; it starts closer to the player
		vecThrow *= random->RandomFloat( 800.0f, 1000.0f );

		Weapon_Drop( pActiveWeapon, NULL, &vecThrow );
		pActiveWeapon->SetRemoveable( false );

		// HACK: This hack is required to allow weapons to be disintegrated
		// in the citadel weapon-strip scene
		// Make them not pick-uppable again. This also has the effect of allowing weapons
		// to collide with triggers. 
		if ( bDisallowWeaponPickup )
		{
			pActiveWeapon->RemoveSolidFlags( FSOLID_TRIGGER );
		}
	}
}

	
//-----------------------------------------------------------------------------
// Purpose: Drop the active weapon, optionally throwing it at the given target position.
// Input  : pWeapon - Weapon to drop/throw.
//			pvecTarget - Position to throw it at, NULL for none.
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::Weapon_Drop( CBaseCombatWeapon *pWeapon, const Vector *pvecTarget /* = NULL */, const Vector *pVelocity /* = NULL */ )
{
	if ( !pWeapon )
		return;

	// If I'm an NPC, fill the weapon with ammo before I drop it.
#ifdef MAPBASE
	if ( GetFlags() & FL_NPC && !pWeapon->HasSpawnFlags(SF_WEAPON_PRESERVE_AMMO) )
#else
	if ( GetFlags() & FL_NPC )
#endif
	{
		if ( pWeapon->UsesClipsForAmmo1() )
		{
			pWeapon->m_iClip1 = pWeapon->GetDefaultClip1();

			if( FClassnameIs( pWeapon, "weapon_smg1" ) )
			{
				// Drop enough ammo to kill 2 of me.
				// Figure out how much damage one piece of this type of ammo does to this type of enemy.
				float flAmmoDamage = g_pGameRules->GetAmmoDamage( UTIL_PlayerByIndex(1), this, pWeapon->GetPrimaryAmmoType() );
				pWeapon->m_iClip1 = (GetMaxHealth() / flAmmoDamage) * 2;
			}
		}
		if ( pWeapon->UsesClipsForAmmo2() )
		{
			pWeapon->m_iClip2 = pWeapon->GetDefaultClip2();
		}

		if ( IsXbox() )
		{
			pWeapon->AddEffects( EF_ITEM_BLINK );
		}
	}

	if ( IsPlayer() )
	{
		Vector vThrowPos = Weapon_ShootPosition() - Vector(0,0,12);

		if( UTIL_PointContents(vThrowPos) & CONTENTS_SOLID )
		{
			Msg("Weapon spawning in solid!\n");
		}

		pWeapon->SetAbsOrigin( vThrowPos );

		QAngle gunAngles;
		VectorAngles( BodyDirection2D(), gunAngles );
		pWeapon->SetAbsAngles( gunAngles );
	}
	else
	{
		int iBIndex = -1;
		int iWeaponBoneIndex = -1;

		CStudioHdr *hdr = pWeapon->GetModelPtr();
		// If I have a hand, set the weapon position to my hand bone position.
		if ( hdr && hdr->numbones() > 0 )
		{
			// Assume bone zero is the root
			for ( iWeaponBoneIndex = 0; iWeaponBoneIndex < hdr->numbones(); ++iWeaponBoneIndex )
			{
				iBIndex = LookupBone( hdr->pBone( iWeaponBoneIndex )->pszName() );
				// Found one!
				if ( iBIndex != -1 )
				{
					break;
				}
			}

			if ( iBIndex == -1 )
			{
				iBIndex = LookupBone( "ValveBiped.Weapon_bone" );
			}
		}
		else
		{
			iBIndex = LookupBone( "ValveBiped.Weapon_bone" );
		}

		if ( iBIndex != -1)  
		{
			Vector origin;
			QAngle angles;
			matrix3x4_t transform;

			// Get the transform for the weapon bonetoworldspace in the NPC
			GetBoneTransform( iBIndex, transform );

			// find offset of root bone from origin in local space
			// Make sure we're detached from hierarchy before doing this!!!
			pWeapon->StopFollowingEntity();
			pWeapon->SetAbsOrigin( Vector( 0, 0, 0 ) );
			pWeapon->SetAbsAngles( QAngle( 0, 0, 0 ) );
			pWeapon->InvalidateBoneCache();
			matrix3x4_t rootLocal;
			pWeapon->GetBoneTransform( iWeaponBoneIndex, rootLocal );

			// invert it
			matrix3x4_t rootInvLocal;
			MatrixInvert( rootLocal, rootInvLocal );

			matrix3x4_t weaponMatrix;
			ConcatTransforms( transform, rootInvLocal, weaponMatrix );
			MatrixAngles( weaponMatrix, angles, origin );
			
			pWeapon->Teleport( &origin, &angles, NULL );
		}
		// Otherwise just set in front of me.
		else 
		{
			Vector vFacingDir = BodyDirection2D();
			vFacingDir = vFacingDir * 10.0; 
			pWeapon->SetAbsOrigin( Weapon_ShootPosition() + vFacingDir );
		}
	}

	Vector vecThrow;
	if (pvecTarget)
	{
		// I've been told to throw it somewhere specific.
		vecThrow = VecCheckToss( this, pWeapon->GetAbsOrigin(), *pvecTarget, 0.2, 1.0, false );
	}
	else
	{
		if ( pVelocity )
		{
			vecThrow = *pVelocity;
			float flLen = vecThrow.Length();
			if (flLen > 400)
			{
				VectorNormalize(vecThrow);
				vecThrow *= 400;
			}
		}
		else
		{
			// Nowhere in particular; just drop it.
			float throwForce = ( IsPlayer() ) ? 400.0f : random->RandomInt( 64, 128 );
			vecThrow = BodyDirection3D() * throwForce;
		}
	}

	pWeapon->Drop( vecThrow );
	Weapon_Detach( pWeapon );

#ifdef MAPBASE
	m_OnWeaponDrop.FireOutput(pWeapon, this);
#endif

	if ( HasSpawnFlags( SF_NPC_NO_WEAPON_DROP ) )
	{
		// Don't drop weapons when the super physgun is happening.
		UTIL_Remove( pWeapon );
	}

}


//-----------------------------------------------------------------------------
// Lighting origin
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::SetLightingOriginRelative( CBaseEntity *pLightingOrigin )
{
	BaseClass::SetLightingOriginRelative( pLightingOrigin );
	if ( GetActiveWeapon() )
	{
		GetActiveWeapon()->SetLightingOriginRelative( pLightingOrigin );
	}
}


#ifdef MAPBASE
//-----------------------------------------------------------------------------
// Purpose:	Gives character new weapon and equips it
// Input  : New weapon
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::Weapon_Equip( CBaseCombatWeapon *pWeapon )
{
	Weapon_HandleEquip(pWeapon);

	// Players don't automatically holster their current weapon
	if ( IsPlayer() == false )
	{
		if ( m_hActiveWeapon )
		{
			m_hActiveWeapon->Holster();
			// FIXME: isn't this handeled by the weapon?
			m_hActiveWeapon->AddEffects( EF_NODRAW );
		}
		SetActiveWeapon( pWeapon );
		m_hActiveWeapon->RemoveEffects( EF_NODRAW );

	}

	WeaponProficiency_t proficiency;
	proficiency = CalcWeaponProficiency( pWeapon );
	
	if( weapon_showproficiency.GetBool() != 0 )
	{
		Msg("%s equipped with %s, proficiency is %s\n", GetClassname(), pWeapon->GetClassname(), GetWeaponProficiencyName( proficiency ) );
	}

	SetCurrentWeaponProficiency( proficiency );
}

//-----------------------------------------------------------------------------
// Purpose:	Puts a new weapon in the inventory
// Input  : New weapon
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::Weapon_EquipHolstered( CBaseCombatWeapon *pWeapon )
{
	Weapon_HandleEquip(pWeapon);
	pWeapon->AddEffects( EF_NODRAW );
}

//-----------------------------------------------------------------------------
// Purpose:	Adds new weapon to the character
// Input  : New weapon
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::Weapon_HandleEquip( CBaseCombatWeapon *pWeapon )
{
	// Add the weapon to my weapon inventory
	if (IsPlayer())
	{
		// This code drops existing weapons that are in the same bucket and bucket position.
		// This doesn't really harm anything since that situation would've broken the HUD anyway.
		// 
		// It goes through every single index in case there's a NULL pointer in between weapons.
		int iFirstNullIndex = -1;
		for (int i=0;i<MAX_WEAPONS;i++) 
		{
			if (!m_hMyWeapons[i])
			{
				if (iFirstNullIndex == -1)
					iFirstNullIndex = i;
			}
			else
			{
				if (pWeapon->GetSlot() == m_hMyWeapons[i]->GetSlot() &&
					pWeapon->GetPosition() == m_hMyWeapons[i]->GetPosition())
				{
					// Replace our existing weapon in this slot
					Weapon_Drop(m_hMyWeapons[i]);
					{
						// We found a slot, we don't care about the first null index anymore
						iFirstNullIndex = -1;

						m_hMyWeapons.Set( i, pWeapon );
						break;
					}
				}
			}
		}

		if (iFirstNullIndex != -1)
			m_hMyWeapons.Set( iFirstNullIndex, pWeapon );
	}
	else
	{
		for (int i=0;i<MAX_WEAPONS;i++) 
		{
			if (!m_hMyWeapons[i]) 
			{
				m_hMyWeapons.Set( i, pWeapon );
				break;
			}
		}
	}

	// Weapon is now on my team
	pWeapon->ChangeTeam( GetTeamNumber() );

	bool bPreserveAmmo = pWeapon->HasSpawnFlags(SF_WEAPON_PRESERVE_AMMO);
	if (!bPreserveAmmo)
	{
		// ----------------------
		//  Give Primary Ammo
		// ----------------------
		// If gun doesn't use clips, just give ammo
		if (pWeapon->GetMaxClip1() == -1)
		{
#ifdef HL2_DLL
			if( FStrEq(STRING(gpGlobals->mapname), "d3_c17_09") && FClassnameIs(pWeapon, "weapon_rpg") && pWeapon->NameMatches("player_spawn_items") )
			{
				// !!!HACK - Don't give any ammo with the spawn equipment RPG in d3_c17_09. This is a chapter
				// start and the map is way to easy if you start with 3 RPG rounds. It's fine if a player conserves
				// them and uses them here, but it's not OK to start with enough ammo to bypass the snipers completely.
				GiveAmmo( 0, pWeapon->m_iPrimaryAmmoType); 
			}
			else
#endif // HL2_DLL
				GiveAmmo(pWeapon->GetDefaultClip1(), pWeapon->m_iPrimaryAmmoType); 
		}
		// If default ammo given is greater than clip
		// size, fill clips and give extra ammo
		else if ( pWeapon->GetDefaultClip1() >  pWeapon->GetMaxClip1() )
		{
			pWeapon->m_iClip1 = pWeapon->GetMaxClip1();
			GiveAmmo( (pWeapon->GetDefaultClip1() - pWeapon->GetMaxClip1()), pWeapon->m_iPrimaryAmmoType); 
		}

		// ----------------------
		//  Give Secondary Ammo
		// ----------------------
		// If gun doesn't use clips, just give ammo
		if (pWeapon->GetMaxClip2() == -1)
		{
			GiveAmmo(pWeapon->GetDefaultClip2(), pWeapon->m_iSecondaryAmmoType); 
		}
		// If default ammo given is greater than clip
		// size, fill clips and give extra ammo
		else if ( pWeapon->GetDefaultClip2() > pWeapon->GetMaxClip2() )
		{
			pWeapon->m_iClip2 = pWeapon->GetMaxClip2();
			GiveAmmo( (pWeapon->GetDefaultClip2() - pWeapon->GetMaxClip2()), pWeapon->m_iSecondaryAmmoType); 
		}
	}
	else //if (IsPlayer())
	{
		if (pWeapon->UsesClipsForAmmo1())
		{
			if (pWeapon->m_iClip1 > pWeapon->GetMaxClip1())
			{
				// Handle excess ammo
				GiveAmmo( pWeapon->m_iClip1 - pWeapon->GetMaxClip1(), pWeapon->m_iPrimaryAmmoType );
				pWeapon->m_iClip1 = pWeapon->GetMaxClip1();
			}
		}
		else if (pWeapon->m_iClip1 > 0)
		{
			// Just because the weapon can't use clips doesn't mean
			// the mapper can't override their clip value for ammo.
			GiveAmmo(pWeapon->m_iClip1, pWeapon->m_iPrimaryAmmoType);
			pWeapon->m_iClip1 = WEAPON_NOCLIP;
		}

		if (pWeapon->UsesClipsForAmmo2())
		{
			if (pWeapon->m_iClip2 > pWeapon->GetMaxClip2())
			{
				// Handle excess ammo
				GiveAmmo(pWeapon->m_iClip2 - pWeapon->GetMaxClip2(), pWeapon->m_iSecondaryAmmoType);
				pWeapon->m_iClip2 = pWeapon->GetMaxClip2();
			}
		}
		else if (pWeapon->m_iClip2 > 0)
		{
			// Just because the weapon can't use clips doesn't mean
			// the mapper can't override their clip value for ammo.
			GiveAmmo(pWeapon->m_iClip2, pWeapon->m_iSecondaryAmmoType);
			pWeapon->m_iClip2 = WEAPON_NOCLIP;
		}
	}

	pWeapon->Equip( this );
	
	// Gotta do this *after* Equip because it may whack maxRange
	if ( IsPlayer() == false )
	{
		// If SF_NPC_LONG_RANGE spawn flags is set let weapon work from any distance
		if ( HasSpawnFlags(SF_NPC_LONG_RANGE) )
		{
			pWeapon->m_fMaxRange1 = 999999999;
			pWeapon->m_fMaxRange2 = 999999999;
		}
	}
	else if (bPreserveAmmo)
	{
		// The clip doesn't update on the client unless we do this.
		// This is the only way I've figured out how to update without doing something worse.
		// TODO: Remove this hack, we've finally fixed it
		/*
		variant_t clip1;
		clip1.SetInt(pWeapon->m_iClip1);
		variant_t clip2;
		clip2.SetInt(pWeapon->m_iClip2);

		pWeapon->m_iClip1 = pWeapon->m_iClip1 - 1;
		pWeapon->m_iClip2 = pWeapon->m_iClip2 - 1;

		g_EventQueue.AddEvent(pWeapon, "SetAmmo1", clip1, 0.0001f, this, this, 0);
		g_EventQueue.AddEvent(pWeapon, "SetAmmo2", clip2, 0.0001f, this, this, 0);
		*/
	}

	// Pass the lighting origin over to the weapon if we have one
	pWeapon->SetLightingOriginRelative( GetLightingOriginRelative() );

	//if (m_aliveTimer.IsLessThen(0.01f))
		m_OnWeaponEquip.FireOutput(pWeapon, this);
}
#else
//-----------------------------------------------------------------------------
// Purpose:	Add new weapon to the character
// Input  : New weapon
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::Weapon_Equip( CBaseCombatWeapon *pWeapon )
{
	// Add the weapon to my weapon inventory
	for (int i=0;i<MAX_WEAPONS;i++) 
	{
		if (!m_hMyWeapons[i]) 
		{
			m_hMyWeapons.Set( i, pWeapon );
			break;
		}
	}

	// Weapon is now on my team
	pWeapon->ChangeTeam( GetTeamNumber() );

	// ----------------------
	//  Give Primary Ammo
	// ----------------------
	// If gun doesn't use clips, just give ammo
	if (pWeapon->GetMaxClip1() == -1)
	{
#ifdef HL2_DLL
		if( FStrEq(STRING(gpGlobals->mapname), "d3_c17_09") && FClassnameIs(pWeapon, "weapon_rpg") && pWeapon->NameMatches("player_spawn_items") )
		{
			// !!!HACK - Don't give any ammo with the spawn equipment RPG in d3_c17_09. This is a chapter
			// start and the map is way to easy if you start with 3 RPG rounds. It's fine if a player conserves
			// them and uses them here, but it's not OK to start with enough ammo to bypass the snipers completely.
			GiveAmmo( 0, pWeapon->m_iPrimaryAmmoType); 
		}
		else
#endif // HL2_DLL
		GiveAmmo(pWeapon->GetDefaultClip1(), pWeapon->m_iPrimaryAmmoType); 
	}
	// If default ammo given is greater than clip
	// size, fill clips and give extra ammo
	else if (pWeapon->GetDefaultClip1() >  pWeapon->GetMaxClip1() )
	{
		pWeapon->m_iClip1 = pWeapon->GetMaxClip1();
		GiveAmmo( (pWeapon->GetDefaultClip1() - pWeapon->GetMaxClip1()), pWeapon->m_iPrimaryAmmoType); 
	}

	// ----------------------
	//  Give Secondary Ammo
	// ----------------------
	// If gun doesn't use clips, just give ammo
	if (pWeapon->GetMaxClip2() == -1)
	{
		GiveAmmo(pWeapon->GetDefaultClip2(), pWeapon->m_iSecondaryAmmoType); 
	}
	// If default ammo given is greater than clip
	// size, fill clips and give extra ammo
	else if ( pWeapon->GetDefaultClip2() > pWeapon->GetMaxClip2() )
	{
		pWeapon->m_iClip2 = pWeapon->GetMaxClip2();
		GiveAmmo( (pWeapon->GetDefaultClip2() - pWeapon->GetMaxClip2()), pWeapon->m_iSecondaryAmmoType); 
	}

	pWeapon->Equip( this );

	// Players don't automatically holster their current weapon
	if ( IsPlayer() == false )
	{
		if ( m_hActiveWeapon )
		{
			m_hActiveWeapon->Holster();
			// FIXME: isn't this handeled by the weapon?
			m_hActiveWeapon->AddEffects( EF_NODRAW );
		}
		SetActiveWeapon( pWeapon );
		m_hActiveWeapon->RemoveEffects( EF_NODRAW );

	}
	
	// Gotta do this *after* Equip because it may whack maxRange
	if ( IsPlayer() == false )
	{
		// If SF_NPC_LONG_RANGE spawn flags is set let weapon work from any distance
		if ( HasSpawnFlags(SF_NPC_LONG_RANGE) )
		{
			m_hActiveWeapon->m_fMaxRange1 = 999999999;
			m_hActiveWeapon->m_fMaxRange2 = 999999999;
		}
	}

	WeaponProficiency_t proficiency;
	proficiency = CalcWeaponProficiency( pWeapon );
	
	if( weapon_showproficiency.GetBool() != 0 )
	{
		Msg("%s equipped with %s, proficiency is %s\n", GetClassname(), pWeapon->GetClassname(), GetWeaponProficiencyName( proficiency ) );
	}

	SetCurrentWeaponProficiency( proficiency );

	// Pass the lighting origin over to the weapon if we have one
	pWeapon->SetLightingOriginRelative( GetLightingOriginRelative() );
}
#endif

//-----------------------------------------------------------------------------
// Purpose:	Leaves weapon, giving only ammo to the character
// Input  : Weapon
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::Weapon_EquipAmmoOnly( CBaseCombatWeapon *pWeapon )
{
	// Check for duplicates
	for (int i=0;i<MAX_WEAPONS;i++) 
	{
		if ( m_hMyWeapons[i].Get() && FClassnameIs(m_hMyWeapons[i], pWeapon->GetClassname()) )
		{
			// Just give the ammo from the clip
			int	primaryGiven	= (pWeapon->UsesClipsForAmmo1()) ? pWeapon->m_iClip1 : pWeapon->GetPrimaryAmmoCount();
			int secondaryGiven	= (pWeapon->UsesClipsForAmmo2()) ? pWeapon->m_iClip2 : pWeapon->GetSecondaryAmmoCount();

			int takenPrimary   = GiveAmmo( primaryGiven, pWeapon->m_iPrimaryAmmoType); 
			int takenSecondary = GiveAmmo( secondaryGiven, pWeapon->m_iSecondaryAmmoType); 
			
			if( pWeapon->UsesClipsForAmmo1() )
			{
				pWeapon->m_iClip1 -= takenPrimary;
			}
			else
			{
				pWeapon->SetPrimaryAmmoCount( pWeapon->GetPrimaryAmmoCount() - takenPrimary );
			}

			if( pWeapon->UsesClipsForAmmo2() )
			{
				pWeapon->m_iClip2 -= takenSecondary;
			}
			else
			{
				pWeapon->SetSecondaryAmmoCount( pWeapon->GetSecondaryAmmoCount() - takenSecondary );
			}
			
			//Only succeed if we've taken ammo from the weapon
			if ( takenPrimary > 0 || takenSecondary > 0 )
				return true;
			
			return false;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether the weapon passed in would occupy a slot already occupied by the carrier
// Input  : *pWeapon - weapon to test for
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::Weapon_SlotOccupied( CBaseCombatWeapon *pWeapon )
{
	if ( pWeapon == NULL )
		return false;

	//Check to see if there's a resident weapon already in this slot
	if ( Weapon_GetSlot( pWeapon->GetSlot() ) == NULL )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the weapon (if any) in the requested slot
// Input  : slot - which slot to poll
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CBaseCombatCharacter::Weapon_GetSlot( int slot ) const
{
	int	targetSlot = slot;

	// Check for that slot being occupied already
	for ( int i=0; i < MAX_WEAPONS; i++ )
	{
		if ( m_hMyWeapons[i].Get() != NULL )
		{
			// If the slots match, it's already occupied
			if ( m_hMyWeapons[i]->GetSlot() == targetSlot )
				return m_hMyWeapons[i];
		}
	}
	
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Get a pointer to a weapon this character has that uses the specified ammo
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CBaseCombatCharacter::Weapon_GetWpnForAmmo( int iAmmoIndex )
{
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		CBaseCombatWeapon *weapon = GetWeapon( i );
		if ( !weapon )
			continue;

		if ( weapon->GetPrimaryAmmoType() == iAmmoIndex )
			return weapon;
		if ( weapon->GetSecondaryAmmoType() == iAmmoIndex )
			return weapon;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Can this character operate this weapon?
// Input  : A weapon
// Output :	true or false
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::Weapon_CanUse( CBaseCombatWeapon *pWeapon )
{
	acttable_t *pTable		= pWeapon->ActivityList();
	int			actCount	= pWeapon->ActivityListCount();

	if( actCount < 1 )
	{
		// If the weapon has no activity table, it definitely cannot be used.
		return false;
	}

	for ( int i = 0; i < actCount; i++, pTable++ )
	{
		if ( pTable->required )
		{
			// The NPC might translate the weapon activity into another activity
			Activity translatedActivity = NPC_TranslateActivity( (Activity)(pTable->weaponAct) );

			if ( SelectWeightedSequence(translatedActivity) == ACTIVITY_NOT_AVAILABLE )
			{
#ifdef MAPBASE
				// Do we have a backup?
				translatedActivity = Weapon_BackupActivity((Activity)(pTable->baseAct), true, pWeapon);
				if (SelectWeightedSequence(translatedActivity) != ACTIVITY_NOT_AVAILABLE)
					return true;
#endif
				return false;
			}
		}
	}

	return true;
}

#ifdef MAPBASE

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static Activity Weapon_BackupActivityFromList( CBaseCombatCharacter *pBCC, acttable_t *pTable, int actCount, Activity activity, bool weaponTranslationWasRequired, CBaseCombatWeapon *pWeapon )
{
	int i = 0;
	for ( ; i < actCount; i++, pTable++ )
	{
		if ( activity == pTable->baseAct )
		{
			// Don't pick backup activities we don't actually have an animation for.
			if (!pBCC->GetModelPtr()->HaveSequenceForActivity(pTable->weaponAct))
				break;

			return (Activity)pTable->weaponAct;
		}
	}

	// We didn't succeed in finding an activity. See if we can recurse
	acttable_t *pBackupTable = CBaseCombatWeapon::GetDefaultBackupActivityList( pTable - i, actCount );
	if (pBackupTable)
	{
		return Weapon_BackupActivityFromList( pBCC, pBackupTable, actCount, activity, weaponTranslationWasRequired, pWeapon );
	}

	return activity;
}

//-----------------------------------------------------------------------------
// Purpose:	Uses an activity from a different weapon when the activity we were originally looking for does not exist on this character.
//			This gives NPCs and players the ability to use weapons they are otherwise unable to use.
//-----------------------------------------------------------------------------
Activity CBaseCombatCharacter::Weapon_BackupActivity( Activity activity, bool weaponTranslationWasRequired, CBaseCombatWeapon *pSpecificWeapon )
{
	CBaseCombatWeapon *pWeapon = pSpecificWeapon ? pSpecificWeapon : GetActiveWeapon();
	if (!pWeapon)
		return activity;

	// Make sure the weapon allows this activity to have a backup.
	if (!pWeapon->SupportsBackupActivity(activity))
		return activity;

	// UNDONE: Sometimes, a NPC is supposed to use the default activity. Return that if the weapon translation was "not required" and we have an original activity.
	/*
	if (!weaponTranslationWasRequired && GetModelPtr()->HaveSequenceForActivity(activity) && !IsPlayer())
	{
		return activity;
	}
	*/

	acttable_t *pTable = pWeapon->GetBackupActivityList();
	int actCount = pWeapon->GetBackupActivityListCount();
	if (!pTable)
	{
		// Look for a default list
		actCount = pWeapon->ActivityListCount();
		pTable = CBaseCombatWeapon::GetDefaultBackupActivityList( pWeapon->ActivityList(), actCount );
	}

	if (pTable && GetModelPtr())
	{
		return Weapon_BackupActivityFromList( this, pTable, actCount, activity, weaponTranslationWasRequired, pWeapon );
	}

	return activity;
}
#endif

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CBaseCombatCharacter::Weapon_Create( const char *pWeaponName )
{
	CBaseCombatWeapon *pWeapon = static_cast<CBaseCombatWeapon *>( Create( pWeaponName, GetLocalOrigin(), GetLocalAngles(), this ) );

	return pWeapon;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::Weapon_HandleAnimEvent( animevent_t *pEvent )
{
	// UNDONE: Some check to make sure that pEvent->pSource is a weapon I'm holding?
	if ( m_hActiveWeapon )
	{
		// UNDONE: Pass to pEvent->pSource instead?
		m_hActiveWeapon->Operator_HandleAnimEvent( pEvent, this );
	}
}

void CBaseCombatCharacter::RemoveAllWeapons()
{
	ClearActiveWeapon();
	for (int i = 0; i < MAX_WEAPONS; i++)
	{
		if ( m_hMyWeapons[i] )
		{
			m_hMyWeapons[i]->Delete( );
			m_hMyWeapons.Set( i, NULL );
		}
	}
}


// take health
int CBaseCombatCharacter::TakeHealth (float flHealth, int bitsDamageType)
{
	if (!m_takedamage)
		return 0;

#ifdef MAPBASE
	float flRatio = clamp( (float)m_iHealth / (float)m_iMaxHealth, 0.f, 1.f );
	m_OnHealthChanged.Set(flRatio, NULL, this);
#endif
	
	return BaseClass::TakeHealth(flHealth, bitsDamageType);
}


/*
============
OnTakeDamage

The damage is coming from inflictor, but get mad at attacker
This should be the only function that ever reduces health.
bitsDamageType indicates the type of damage sustained, ie: DMG_SHOCK

Time-based damage: only occurs while the NPC is within the trigger_hurt.
When a NPC is poisoned via an arrow etc it takes all the poison damage at once.



GLOBALS ASSUMED SET:  g_iSkillLevel
============
*/
int CBaseCombatCharacter::OnTakeDamage( const CTakeDamageInfo &info )
{
	int retVal = 0;

	if (!m_takedamage)
		return 0;

	m_iDamageCount++;

	if ( info.GetDamageType() & DMG_SHOCK )
	{
		g_pEffects->Sparks( info.GetDamagePosition(), 2, 2 );
		UTIL_Smoke( info.GetDamagePosition(), random->RandomInt( 10, 15 ), 10 );
	}

	// track damage history
	if ( info.GetAttacker() )
	{
		int attackerTeam = info.GetAttacker()->GetTeamNumber();

		m_hasBeenInjured |= ( 1 << attackerTeam );

		for( int i=0; i<MAX_DAMAGE_TEAMS; ++i )
		{
			if ( m_damageHistory[i].team == attackerTeam )
			{
				// restart the injury timer
				m_damageHistory[i].interval.Start();
				break;
			}

			if ( m_damageHistory[i].team == TEAM_INVALID )
			{
				// team not registered yet
				m_damageHistory[i].team = attackerTeam;
				m_damageHistory[i].interval.Start();
				break;
			}
		}
	}

	switch( m_lifeState )
	{
	case LIFE_ALIVE:
		retVal = OnTakeDamage_Alive( info );
#ifdef MAPBASE
		if (retVal)
		{
			float flRatio = clamp( (float)m_iHealth / (float)m_iMaxHealth, 0.f, 1.f );
			m_OnHealthChanged.Set(flRatio, NULL, this);
		}
#endif
		if ( m_iHealth <= 0 )
		{
#ifdef MAPBASE_VSCRIPT
			// False = Cheat death
			if (ScriptDeathHook( const_cast<CTakeDamageInfo*>(&info) ) == false)
				return retVal;
#endif

			IPhysicsObject *pPhysics = VPhysicsGetObject();
			if ( pPhysics )
			{
				pPhysics->EnableCollisions( false );
			}
			
			bool bGibbed = false;

			Event_Killed( info );

			// Only classes that specifically request it are gibbed
			if ( ShouldGib( info ) )
			{
				bGibbed = Event_Gibbed( info );
			}
			
			if ( bGibbed == false )
			{
				Event_Dying( info );
			}
		}
		return retVal;
		break;

	case LIFE_DYING:
#ifdef MAPBASE
		retVal = OnTakeDamage_Dying( info );
		if (retVal)
		{
			float flRatio = clamp( (float)m_iHealth / (float)m_iMaxHealth, 0.f, 1.f );
			m_OnHealthChanged.Set(flRatio, NULL, this);
		}
		return retVal;
#else
		return OnTakeDamage_Dying( info );
#endif
	
	default:
	case LIFE_DEAD:
		retVal = OnTakeDamage_Dead( info );
#ifdef MAPBASE
		if (retVal)
		{
			float flRatio = clamp( (float)m_iHealth / (float)m_iMaxHealth, 0.f, 1.f );
			m_OnHealthChanged.Set(flRatio, NULL, this);
		}
#endif
		if ( m_iHealth <= 0 && g_pGameRules->Damage_ShouldGibCorpse( info.GetDamageType() ) && ShouldGib( info ) )
		{
			// DR: do not gib holograms
			if (!this->CheckHavingStatusEffect(ST_HOLOGRAM)) {
				Event_Gibbed(info);
			}
			retVal = 0;
		}
		return retVal;
	}
}

// Note: since I am lazy the damage values simply get divided and subtracted with those values, and then ran through the OnTakeDamage function. Don't think about overflowing :)
int	CBaseCombatCharacter::OnTakeDamagePure(const CTakeDamageInfo& info) {
	CTakeDamageInfo newInfo = info;
	newInfo.SetDamage((info.GetDamage()-dmgAdd)/dmgMul);
	return CBaseCombatCharacter::OnTakeDamage(newInfo);
}

int CBaseCombatCharacter::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	// grab the vector of the incoming attack. ( pretend that the inflictor is a little lower than it really is, so the body will tend to fly upward a bit).
	Vector vecDir = vec3_origin;
	if (info.GetInflictor())
	{
		vecDir = info.GetInflictor()->WorldSpaceCenter() - Vector ( 0, 0, 10 ) - WorldSpaceCenter();
		VectorNormalize(vecDir);
	}
	g_vecAttackDir = vecDir;

	//!!!LATER - make armor consideration here!
	// do the damage
	if ( m_takedamage != DAMAGE_EVENTS_ONLY )
	{
		// DR: added this var so it's easier do modify damage
		float dmg = info.GetDamage()*dmgMul+dmgAdd;
		// damage is tracked after modification
		AddToDamageTracker(dmg);

		// Separate the fractional amount of damage from the whole
		float flFractionalDamage = dmg - floor(dmg);
		float flIntegerDamage = dmg - flFractionalDamage;

		// Add fractional damage to the accumulator
		m_flDamageAccumulator += flFractionalDamage;

		// If the accumulator is holding a full point of damage, move that point
		// of damage into the damage we're about to inflict.
		if( m_flDamageAccumulator >= 1.0 )
		{
			flIntegerDamage += 1.0;
			m_flDamageAccumulator -= 1.0;
		}

		if ( flIntegerDamage <= 0 )
			return 0;

		m_iHealth -= flIntegerDamage;
	}

	return 1;
}


int CBaseCombatCharacter::OnTakeDamage_Dying( const CTakeDamageInfo &info )
{
	return 1;
}

int CBaseCombatCharacter::OnTakeDamage_Dead( const CTakeDamageInfo &info )
{
	// do the damage
	if ( m_takedamage != DAMAGE_EVENTS_ONLY )
	{
		m_iHealth -= info.GetDamage();
	}

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Sets vBodyDir to the body direction (2D) of the combat character.  
//			Used as NPC's and players extract facing direction differently
// Input  :
// Output :
//-----------------------------------------------------------------------------
QAngle CBaseCombatCharacter::BodyAngles()
{
	return GetAbsAngles();
}


Vector CBaseCombatCharacter::BodyDirection2D( void )
{
	Vector vBodyDir = BodyDirection3D( );
	vBodyDir.z = 0;
	vBodyDir.AsVector2D().NormalizeInPlace();
	return vBodyDir;
}


Vector CBaseCombatCharacter::BodyDirection3D( void )
{
	QAngle angles = BodyAngles();

	// FIXME: cache this
	Vector vBodyDir;
	AngleVectors( angles, &vBodyDir );
	return vBodyDir;
}


void CBaseCombatCharacter::SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways )
{
	// Skip this work if we're already marked for transmission.
	if ( pInfo->m_pTransmitEdict->Get( entindex() ) )
		return;

	BaseClass::SetTransmit( pInfo, bAlways );

	bool bLocalPlayer = ( pInfo->m_pClientEnt == edict() );

	if ( bLocalPlayer )
	{
		for ( int i=0; i < MAX_WEAPONS; i++ )
		{
			CBaseCombatWeapon *pWeapon = m_hMyWeapons[i];
			if ( !pWeapon )
				continue;

			// The local player is sent all of his weapons.
			pWeapon->SetTransmit( pInfo, bAlways );
		}
	}
	else
	{
		// The check for EF_NODRAW is useless because the weapon will be networked anyway. In CBaseCombatWeapon::
		// UpdateTransmitState all weapons with owners will transmit to clients in the PVS.
		if ( m_hActiveWeapon && !m_hActiveWeapon->IsEffectActive( EF_NODRAW ) )
			m_hActiveWeapon->SetTransmit( pInfo, bAlways );
	}
}



//-----------------------------------------------------------------------------
// Purpose: Add or Change a class relationship for this entity
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::AddClassRelationship ( Class_T class_type, Disposition_t disposition, int priority )
{
	// First check to see if a relationship has already been declared for this class
	// If so, update it with the new relationship
	for (int i=m_Relationship.Count()-1;i >= 0;i--) 
	{
		if (m_Relationship[i].classType == class_type) 
		{
			m_Relationship[i].disposition = disposition;
			if ( priority != DEF_RELATIONSHIP_PRIORITY )
				m_Relationship[i].priority	  = priority;
			return;
		}
	}

	int index = m_Relationship.AddToTail();
	// Add the new class relationship to our relationship table
	m_Relationship[index].classType		= class_type;
	m_Relationship[index].entity		= NULL;
	m_Relationship[index].disposition	= disposition;
	m_Relationship[index].priority		= ( priority != DEF_RELATIONSHIP_PRIORITY ) ? priority : 0;
}

#ifdef MAPBASE
//-----------------------------------------------------------------------------
// Purpose: Removes a class relationship from our list
// Input  : *class_type - Class with whom the relationship should be ended
// Output : True is relation was removed, false if it was not found
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::RemoveClassRelationship( Class_T class_type )
{
	// Find the relationship in our list, if it exists
	for ( int i = m_Relationship.Count()-1; i >= 0; i-- ) 
	{
		if ( m_Relationship[i].classType == class_type )
		{
			// Done, remove it
			m_Relationship.Remove( i );
			return true;
		}
	}

	return false;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Add or Change a entity relationship for this entity
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::AddEntityRelationship ( CBaseEntity* pEntity, Disposition_t disposition, int priority )
{
	// First check to see if a relationship has already been declared for this entity
	// If so, update it with the new relationship
	for (int i=m_Relationship.Count()-1;i >= 0;i--) 
	{
		if (m_Relationship[i].entity == pEntity) 
		{
			m_Relationship[i].disposition	= disposition;
			if ( priority != DEF_RELATIONSHIP_PRIORITY )
				m_Relationship[i].priority	= priority;
			return;
		}
	}

	int index = m_Relationship.AddToTail();
	// Add the new class relationship to our relationship table
	m_Relationship[index].classType		= CLASS_NONE;
	m_Relationship[index].entity		= pEntity;
	m_Relationship[index].disposition	= disposition;
	m_Relationship[index].priority		= ( priority != DEF_RELATIONSHIP_PRIORITY ) ? priority : 0;
}

//-----------------------------------------------------------------------------
// Purpose: Removes an entity relationship from our list
// Input  : *pEntity - Entity with whom the relationship should be ended
// Output : True is entity was removed, false if it was not found
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::RemoveEntityRelationship( CBaseEntity *pEntity )
{
	// Find the entity in our list, if it exists
	for ( int i = m_Relationship.Count()-1; i >= 0; i-- ) 
	{
		if ( m_Relationship[i].entity == pEntity )
		{
			// Done, remove it
			m_Relationship.Remove( i );
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Allocates default relationships
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::AllocateDefaultRelationships( )
{
	if (!m_DefaultRelationship)
	{
		m_DefaultRelationship = new Relationship_t*[NUM_AI_CLASSES];

		for (int i=0; i<NUM_AI_CLASSES; ++i)
		{
			// Be default all relationships are neutral of priority zero
			m_DefaultRelationship[i] = new Relationship_t[NUM_AI_CLASSES];
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Return an interaction ID (so we have no collisions)
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::SetDefaultRelationship(Class_T nClass, Class_T nClassTarget, Disposition_t nDisposition, int nPriority)
{
	if (m_DefaultRelationship)
	{
		m_DefaultRelationship[nClass][nClassTarget].disposition	= nDisposition;
		m_DefaultRelationship[nClass][nClassTarget].priority	= nPriority;
	}
}

#ifdef MAPBASE
//-----------------------------------------------------------------------------
// Purpose: Determine whether or not default relationships are loaded
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::DefaultRelationshipsLoaded()
{
	return m_DefaultRelationship != NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Fetch the default (ignore ai_relationship changes) relationship
// Input  :
// Output :
//-----------------------------------------------------------------------------
Disposition_t CBaseCombatCharacter::GetDefaultRelationshipDisposition( Class_T nClassSource, Class_T nClassTarget )
{
	Assert( m_DefaultRelationship != NULL );

	return m_DefaultRelationship[nClassSource][nClassTarget].disposition;
}

//-----------------------------------------------------------------------------
// Purpose: Fetch the default (ignore ai_relationship changes) priority
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CBaseCombatCharacter::GetDefaultRelationshipPriority( Class_T nClassSource, Class_T nClassTarget )
{
	Assert( m_DefaultRelationship != NULL );

	return m_DefaultRelationship[nClassSource][nClassTarget].priority;
}

//-----------------------------------------------------------------------------
// Purpose: Fetch the default (ignore ai_relationship changes) priority
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CBaseCombatCharacter::GetDefaultRelationshipPriority( Class_T nClassTarget )
{
	Assert( m_DefaultRelationship != NULL );

	return m_DefaultRelationship[Classify()][nClassTarget].priority;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Fetch the default (ignore ai_relationship changes) relationship
// Input  :
// Output :
//-----------------------------------------------------------------------------
Disposition_t CBaseCombatCharacter::GetDefaultRelationshipDisposition( Class_T nClassTarget )
{
	Assert( m_DefaultRelationship != NULL );

	return m_DefaultRelationship[Classify()][nClassTarget].disposition;
}


//-----------------------------------------------------------------------------
// Purpose: describes the relationship between two types of NPC.
// Input  :
// Output :
//-----------------------------------------------------------------------------
Relationship_t *CBaseCombatCharacter::FindEntityRelationship( CBaseEntity *pTarget )
{
	if ( !pTarget )
	{
		static Relationship_t dummy; 
		return &dummy;
	}

	// First check for specific relationship with this edict
	int i;
	for (i=0;i<m_Relationship.Count();i++) 
	{
		if (pTarget == (CBaseEntity *)m_Relationship[i].entity) 
		{
			return &m_Relationship[i];
		}
	}

	if (pTarget->Classify() != CLASS_NONE)
	{
		// Then check for relationship with this edict's class
		for (i=0;i<m_Relationship.Count();i++) 
		{
			if (pTarget->Classify() == m_Relationship[i].classType) 
			{
				return &m_Relationship[i];
			}
		}
	}
	AllocateDefaultRelationships();
	// If none found return the default
	return &m_DefaultRelationship[ Classify() ][ pTarget->Classify() ];
}

Disposition_t CBaseCombatCharacter::IRelationType ( CBaseEntity *pTarget )
{
	if ( pTarget )
	{
#ifdef MAPBASE_VSCRIPT
		if (m_ScriptScope.IsInitialized() && g_Hook_RelationshipType.CanRunInScope( m_ScriptScope ))
		{
			// entity, default
			ScriptVariant_t functionReturn;
			ScriptVariant_t args[] = { ScriptVariant_t( pTarget->GetScriptInstance() ), FindEntityRelationship( pTarget )->disposition };
			if (g_Hook_RelationshipType.Call( m_ScriptScope, &functionReturn, args ) && (functionReturn.m_type == FIELD_INTEGER && functionReturn.m_int != D_ER))
			{
				// Use the disposition returned by the script
				return (Disposition_t)functionReturn.m_int;
			}
		}
#endif

		return FindEntityRelationship( pTarget )->disposition;
	}

	return D_NU;
}

//-----------------------------------------------------------------------------
// Purpose: describes the relationship between two types of NPC.
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CBaseCombatCharacter::IRelationPriority( CBaseEntity *pTarget )
{
	if ( pTarget )
	{
#ifdef MAPBASE_VSCRIPT
		if (m_ScriptScope.IsInitialized() && g_Hook_RelationshipPriority.CanRunInScope( m_ScriptScope ))
		{
			// entity, default
			ScriptVariant_t functionReturn;
			ScriptVariant_t args[] = { ScriptVariant_t( pTarget->GetScriptInstance() ), FindEntityRelationship( pTarget )->priority };
			if (g_Hook_RelationshipPriority.Call( m_ScriptScope, &functionReturn, args ) && functionReturn.m_type == FIELD_INTEGER)
			{
				// Use the priority returned by the script
				return functionReturn.m_int;
			}
		}
#endif

		return FindEntityRelationship( pTarget )->priority;
	}

	return 0;
}

#ifdef MAPBASE
//-----------------------------------------------------------------------------
// Purpose: Ported from CAI_BaseNPC so players can use it
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::AddRelationship( const char *pszRelationship, CBaseEntity *pActivator )
{
	// Parse the keyvalue data
	char parseString[1000];
	Q_strncpy(parseString, pszRelationship, sizeof(parseString));

	// Look for an entity string
	char *entityString = strtok(parseString," ");
	while (entityString)
	{
		// Get the disposition
		char *dispositionString = strtok(NULL," ");
		Disposition_t disposition = D_NU;
		if ( dispositionString )
		{
			if (!stricmp(dispositionString,"D_HT"))
			{
				disposition = D_HT;
			}
			else if (!stricmp(dispositionString,"D_FR"))
			{
				disposition = D_FR;
			}
			else if (!stricmp(dispositionString,"D_LI"))
			{
				disposition = D_LI;
			}
			else if (!stricmp(dispositionString,"D_NU"))
			{
				disposition = D_NU;
			}
			else
			{
				disposition = D_NU;
				Warning( "***ERROR***\nBad relationship type (%s) to unknown entity (%s)!\n", dispositionString,entityString );
				Assert( 0 );
				return;
			}
		}
		else
		{
			Warning("Can't parse relationship info (%s) - Expecting 'name [D_HT, D_FR, D_LI, D_NU] [1-99]'\n", pszRelationship );
			Assert(0);
			return;
		}

		// Get the priority
		char *priorityString	= strtok(NULL," ");
		int	priority = ( priorityString ) ? atoi(priorityString) : DEF_RELATIONSHIP_PRIORITY;

		bool bFoundEntity = false;

		// Try to get pointer to an entity of this name
		CBaseEntity *entity = gEntList.FindEntityByName( NULL, entityString, this, pActivator );
		while( entity )
		{
			// make sure you catch all entities of this name.
			bFoundEntity = true;
			AddEntityRelationship(entity, disposition, priority );
			entity = gEntList.FindEntityByName( entity, entityString, this, pActivator );
		}

		if( !bFoundEntity )
		{
			// Need special condition for player as we can only have one
			if (!stricmp("player", entityString) || !stricmp("!player", entityString))
			{
				AddClassRelationship( CLASS_PLAYER, disposition, priority );
			}
			// Otherwise try to create one too see if a valid classname and get class type
			else
			{
				// HACKHACK:
				CBaseEntity *pEntity = CanCreateEntityClass( entityString ) ? CreateEntityByName( entityString ) : NULL;
				if (pEntity)
				{
					AddClassRelationship( pEntity->Classify(), disposition, priority );
					UTIL_RemoveImmediate(pEntity);
				}
				else
				{
					// NEW: Classify class relationships
					if (!Q_strnicmp(entityString, "CLASS_", 5))
					{
						// Go through all of the classes and find which one this is
						Class_T resultClass = CLASS_NONE;
						for (int i = 0; i < NUM_AI_CLASSES; i++)
						{
							if (FStrEq(g_pGameRules->AIClassText(i), entityString))
							{
								resultClass = (Class_T)i;
							}
						}
						
						if (resultClass != CLASS_NONE)
						{
							AddClassRelationship( resultClass, disposition, priority );
							bFoundEntity = true;
						}
					}
					
					if (!bFoundEntity)
						DevWarning( "Couldn't set relationship to unknown entity or class (%s)!\n", entityString );
				}
			}
		}
		// Check for another entity in the list
		entityString		= strtok(NULL," ");
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::InputSetRelationship( inputdata_t &inputdata )
{
	AddRelationship( inputdata.value.String(), inputdata.pActivator );
}
#endif

//-----------------------------------------------------------------------------
// DR: status effect method implementations go here
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::InputAddStatusEffect(inputdata_t& inputdata) {
	// by the way since we're already getting technical and I'm the only developer for this you can also add effects by their ID (enum value)
	/*char* inputStr = (char*)inputdata.value.String();
	char* token1;
	char* token2;
	char* rest = inputStr;
	// token 1 - status name, token 2 - duration.
	token1 = V_strtok_s(inputStr, token1, &rest);
	token2 = V_strtok_s(inputStr, token1, &rest);
	for (i = 0; i < strlen(token2); i++);
	*/
	// "I didn't feel like using strtok today." - some guy at Valve in ai_playerally.cpp
	// same honestly.
	Log("Entity %s trying to InputAddStatusEffect with parameters %s\n", this->GetClassname(), inputdata.value.String());
	if (inputdata.value.StringID() != NULL_STRING) {
		bool firstValNum = true;
		Log("Entity %s trying to InputAddStatusEffect with parameters %s\n", this->GetClassname(), inputdata.value.String());
		CUtlStringList vecStrings;
		V_SplitString(inputdata.value.String(),"|",vecStrings);
		if (vecStrings.Count() > 1) {

			Log("Entity %s trying to AddStatusEffect with '%s' and '%s'\n",this->GetClassname(), vecStrings[0], vecStrings[1]);

			if (V_strlen(vecStrings[0]) < 1) return;
			if (V_strlen(vecStrings[1]) < 1) return;
			//string_t str = MAKE_STRING(vecStrings[1]);
			for (int i = 0; i < V_strlen(vecStrings[0]); i++) {
				if (!V_isdigit(vecStrings[0][i])) {
					firstValNum = false;
					break;
				}
			}
			for (int i = 0; i < V_strlen(vecStrings[1]); i++) {
				if (!V_isdigit(vecStrings[1][i])) {
					Warning("Entity %s trying to AddStatusEffect with '%s' and '%s' with duration having non-digit\n", this->GetClassname(), vecStrings[0], vecStrings[1]);
					return;
				}
			}
			if (firstValNum) { this->AddStatusEffect(static_cast<Status_t>(V_atoi(vecStrings[0])), V_atoi(vecStrings[1]), dynamic_cast<CBaseCombatCharacter*>(inputdata.pCaller)); }
			else {
				this->AddStatusEffect(StatusStringToStatusEnum(vecStrings[0]), V_atoi(vecStrings[1]),dynamic_cast<CBaseCombatCharacter*>(inputdata.pCaller));
			}
		}
	}

}

// since I/O has 1 variable to communicate we set the parameter override to 0 if effect no found, the effect ID otherwise.
void CBaseCombatCharacter::InputCheckHavingStatusEffect(inputdata_t& inputdata)
{
	if (CheckHavingStatusEffect(inputdata.value.Int())) return;
	DevMsg("no match, return 0\n");
	inputdata.value.SetInt(0);
}

void CBaseCombatCharacter::AddStatusEffect(Status_t ef, int dur, CHandle<CBaseCombatCharacter> inflictor) {

	// dont do anything if bullseye (avoid stupid bullshit)
	if (this->ClassMatches("npc_bullseye")) return;


	// stack duration instead of adding a copy
	// TODO: actually make different types of stacking
	if (!this->m_StatusEffectList.IsEmpty()) {
		FOR_EACH_VEC(this->m_StatusEffectList, i) {
			if (this->m_StatusEffectList[i].getType() == ef) {
				this->m_StatusEffectList[i].efDuration = this->m_StatusEffectList[i].efDuration + dur;
				return;
			}
		}
	}
	CStatusEffect efObj(dur, this, inflictor, ef);
	efObj.onActivate();
	this->m_StatusEffectList.Insert(efObj);
	return;
}

bool CBaseCombatCharacter::CheckHavingStatusEffect(int ef)
{
	// TODO: BINARY SEARCH
	FOR_EACH_VEC(this->m_StatusEffectList, i) {
		DevMsg("Comparing effect %d and %d\n", this->m_StatusEffectList[i].getType(), ef);
		if (this->m_StatusEffectList[i].getType() == ef) {
			DevMsg("Comparing effect %d and %d - they match\n", this->m_StatusEffectList[i].getType(), ef);
			return true;
		}
	}
	return false;
}

//_____________________________________________________________________________
//-----------------------------------------------------------------------------
// Purpose: Get shoot position of BCC at current position/orientation
// Input  :
// Output :
//-----------------------------------------------------------------------------
Vector CBaseCombatCharacter::Weapon_ShootPosition( )
{
	Vector forward, right, up;

	AngleVectors( GetAbsAngles(), &forward, &right, &up );

	Vector vecSrc = GetAbsOrigin() 
					+ forward * m_HackedGunPos.y 
					+ right * m_HackedGunPos.x 
					+ up * m_HackedGunPos.z;

	return vecSrc;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CBaseEntity *CBaseCombatCharacter::FindHealthItem( const Vector &vecPosition, const Vector &range )
{
	CBaseEntity *list[1024];
	int count = UTIL_EntitiesInBox( list, 1024, vecPosition - range, vecPosition + range, 0 );

	for ( int i = 0; i < count; i++ )
	{
		CItem *pItem = dynamic_cast<CItem *>(list[ i ]);

		if( pItem )
		{
#ifdef MAPBASE
			if (pItem->HasSpawnFlags(SF_ITEM_NO_NPC_PICKUP))
				continue;
#endif

			// Healthkits and healthvials
			if( pItem->ClassMatches( "item_health*" ) && FVisible( pItem ) )
			{
				return pItem;
			}
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Compares the weapon's center with this character's current origin, so it
// will not give reliable results for weapons that are visible to the NPC
// but are upstairs/downstairs, etc.
//
// A weapon is said to be on the ground if it is no more than 12 inches above
// or below the caller's feet.
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::Weapon_IsOnGround( CBaseCombatWeapon *pWeapon )
{
	if( pWeapon->IsConstrained() )
	{
		// Constrained to a rack.
		return false;
	}

	if( fabs(pWeapon->WorldSpaceCenter().z - GetAbsOrigin().z) >= 12.0f )
	{
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &range - 
// Output : CBaseEntity
//-----------------------------------------------------------------------------
CBaseEntity *CBaseCombatCharacter::Weapon_FindUsable( const Vector &range )
{
	bool bConservative = false;

#ifdef MAPBASE
	if (HasContext("weapon_conservative:1"))
		bConservative = true;
#ifdef HL2_DLL
	else if (hl2_episodic.GetBool() && !GetActiveWeapon())
	{
		// Unarmed citizens are conservative in their weapon finding...in Episode One
		if (Classify() != CLASS_PLAYER_ALLY_VITAL && Q_strncmp(STRING(gpGlobals->mapname), "ep1_", 4) == 0)
			bConservative = true;
	}
#endif
#else
#ifdef HL2_DLL
	if( hl2_episodic.GetBool() && !GetActiveWeapon() )
	{
		// Unarmed citizens are conservative in their weapon finding
		if ( Classify() != CLASS_PLAYER_ALLY_VITAL )
		{
			bConservative = true;
		}
	}
#endif
#endif

	CBaseCombatWeapon *weaponList[64];
	CBaseCombatWeapon *pBestWeapon = NULL;

	Vector mins = GetAbsOrigin() - range;
	Vector maxs = GetAbsOrigin() + range;
	int listCount = CBaseCombatWeapon::GetAvailableWeaponsInBox( weaponList, ARRAYSIZE(weaponList), mins, maxs );

	float fBestDist = 1e6;

	for ( int i = 0; i < listCount; i++ )
	{
		// Make sure not moving (ie flying through the air)
		Vector velocity;

		CBaseCombatWeapon *pWeapon = weaponList[i];
		Assert(pWeapon);
		pWeapon->GetVelocity( &velocity, NULL );

		if ( pWeapon->CanBePickedUpByNPCs() == false )
			continue;

#ifdef MAPBASE
		if ( pWeapon->HasSpawnFlags(SF_WEAPON_NO_NPC_PICKUP) )
			continue;
#endif

		if ( velocity.LengthSqr() > 1 || !Weapon_CanUse(pWeapon) )
			continue;

		if ( pWeapon->IsLocked(this) )
			continue;

#ifdef MAPBASE
		// Skip weapons we already own
		if ( Weapon_OwnsThisType(pWeapon->GetClassname()) )
			continue;
#endif

		if ( GetActiveWeapon() )
		{
			// Already armed. Would picking up this weapon improve my situation?
#ifndef MAPBASE
			if( GetActiveWeapon()->m_iClassname == pWeapon->m_iClassname )
			{
				// No, I'm already using this type of weapon.
				continue;
			}
#endif

#ifdef MAPBASE
			if ( pWeapon->IsMeleeWeapon() && !GetActiveWeapon()->IsMeleeWeapon() )
			{
				// This weapon is a melee weapon and the weapon I have now is not.
				// Picking up this weapon might not improve my situation.
				continue;
			}

			if ( pWeapon->GetWeight() != 0 && GetActiveWeapon()->GetWeight() > pWeapon->GetWeight() )
			{
				// Discard if our target weapon supports weight but our current weapon has more of it.
				// 
				// (RIP going from AR2 to shotgun)
				continue;
			}
#else
			if( FClassnameIs( pWeapon, "weapon_pistol" ) )
			{
				// No, it's a pistol.
				continue;
			}
#endif
		}

		float fCurDist = (pWeapon->GetLocalOrigin() - GetLocalOrigin()).Length();

		// Give any reserved weapon a bonus
		if( pWeapon->HasSpawnFlags( SF_WEAPON_NO_PLAYER_PICKUP ) )
		{
			fCurDist *= 0.5f;
		}

		if ( pBestWeapon )
		{
#ifdef MAPBASE
			// NPCs now use weight to determine which weapon is best.
			// All HL2 weapons are weighted and are usually good enough.
			// 
			// This probably won't cause problems...
			if (pWeapon->GetWeight() > 1)
			{
#if 0
				float flRatio = MIN( (2.5f / (pWeapon->GetWeight() - (GetActiveWeapon() ? GetActiveWeapon()->GetWeight() : 0))), 1.0 );
				if (flRatio < 0)
					flRatio *= -1; flRatio += 1.0f;
				fCurDist *= flRatio;
#else
				fCurDist *= MIN( (2.5f / pWeapon->GetWeight()), 1.0 );
#endif
			}
#else
			// UNDONE: Better heuristic needed here
			//			Need to pick by power of weapons
			//			Don't want to pick a weapon right next to a NPC!

			// Give the AR2 a bonus to be selected by making it seem closer.
			if( FClassnameIs( pWeapon, "weapon_ar2" ) )
			{
				fCurDist *= 0.5;
			}
#endif

			// choose the last range attack weapon you find or the first available other weapon
			if ( ! (pWeapon->CapabilitiesGet() & bits_CAP_RANGE_ATTACK_GROUP) )
			{
				continue;
			}
			else if (fCurDist > fBestDist ) 
			{
				continue;
			}
		}

		if( Weapon_IsOnGround(pWeapon) )
		{
			// Weapon appears to be lying on the ground. Make sure this weapon is reachable
			// by tracing out a human sized hull just above the weapon.  If not, reject
			trace_t tr;

			Vector	vAboveWeapon = pWeapon->GetAbsOrigin();
			UTIL_TraceEntity( this, vAboveWeapon, vAboveWeapon + Vector( 0, 0, 1 ), MASK_SOLID, pWeapon, COLLISION_GROUP_NONE, &tr );

			if ( tr.startsolid || (tr.fraction < 1.0) )
				continue;
		}
		else if( bConservative )
		{
			// Skip it.
			continue;
		}

		if( FVisible(pWeapon) )
		{
			fBestDist   = fCurDist;
			pBestWeapon = pWeapon;
		}
	}

	if( pBestWeapon )
	{
		// Lock this weapon for my exclusive use. Lock it for just a couple of seconds because my AI 
		// might not actually be able to go pick it up right now.
		pBestWeapon->Lock( 2.0, this );
	}


	return pBestWeapon;
}

//-----------------------------------------------------------------------------
// Purpose: Give the player some ammo.
// Input  : iCount - Amount of ammo to give.
//			iAmmoIndex - Index of the ammo into the AmmoInfoArray
//			iMax - Max carrying capability of the player
// Output : Amount of ammo actually given
//-----------------------------------------------------------------------------
int CBaseCombatCharacter::GiveAmmo( int iCount, int iAmmoIndex, bool bSuppressSound)
{
	if (iCount <= 0)
		return 0;

	if ( !g_pGameRules->CanHaveAmmo( this, iAmmoIndex ) )
	{
		// game rules say I can't have any more of this ammo type.
		return 0;
	}

	if ( iAmmoIndex < 0 || iAmmoIndex >= MAX_AMMO_SLOTS )
		return 0;

	int iMax = GetAmmoDef()->MaxCarry(iAmmoIndex);
	int iAdd = MIN( iCount, iMax - m_iAmmo[iAmmoIndex] );
	if ( iAdd < 1 )
		return 0;

	// Ammo pickup sound
	if ( !bSuppressSound )
	{
		EmitSound( "BaseCombatCharacter.AmmoPickup" );
	}

	m_iAmmo.Set( iAmmoIndex, m_iAmmo[iAmmoIndex] + iAdd );

	return iAdd;
}

//-----------------------------------------------------------------------------
// Purpose: Give the player some ammo.
//-----------------------------------------------------------------------------
int CBaseCombatCharacter::GiveAmmo( int iCount, const char *szName, bool bSuppressSound )
{
	int iAmmoType = GetAmmoDef()->Index(szName);
	if (iAmmoType == -1)
	{
		Msg("ERROR: Attempting to give unknown ammo type (%s)\n",szName);
		return 0;
	}
	return GiveAmmo( iCount, iAmmoType, bSuppressSound );
}


ConVar	phys_stressbodyweights( "phys_stressbodyweights", "5.0" );
void CBaseCombatCharacter::VPhysicsUpdate( IPhysicsObject *pPhysics )
{
	ApplyStressDamage( pPhysics, false );
	BaseClass::VPhysicsUpdate( pPhysics );
}

float CBaseCombatCharacter::CalculatePhysicsStressDamage( vphysics_objectstress_t *pStressOut, IPhysicsObject *pPhysics )
{
	// stress damage hack.
	float mass = pPhysics->GetMass();
	CalculateObjectStress( pPhysics, this, pStressOut );
	float stress = (pStressOut->receivedStress * m_impactEnergyScale) / mass;

	// Make sure the stress isn't from being stuck inside some static object.
	// how many times your own weight can you hold up?
	if ( pStressOut->hasNonStaticStress && stress > phys_stressbodyweights.GetFloat() )
	{
		// if stuck, don't do this!
		if ( !(pPhysics->GetGameFlags() & FVPHYSICS_PENETRATING) )
			return 200;
	}

	return 0;
}

void CBaseCombatCharacter::ApplyStressDamage( IPhysicsObject *pPhysics, bool bRequireLargeObject )
{
#ifdef HL2_DLL
	if( Classify() == CLASS_PLAYER_ALLY || Classify() == CLASS_PLAYER_ALLY_VITAL )
	{
		// Bypass stress completely for allies and vitals.
		if( hl2_episodic.GetBool() )
			return;
	}
#endif//HL2_DLL

	vphysics_objectstress_t stressOut;
	float damage = CalculatePhysicsStressDamage( &stressOut, pPhysics );
	if ( damage > 0 )
	{
		if ( bRequireLargeObject && !stressOut.hasLargeObjectContact )
			return;

		//Msg("Stress! %.2f / %.2f\n", stressOut.exertedStress, stressOut.receivedStress );
		CTakeDamageInfo dmgInfo( GetWorldEntity(), GetWorldEntity(), vec3_origin, vec3_origin, damage, DMG_CRUSH );
		dmgInfo.SetDamageForce( Vector( 0, 0, -stressOut.receivedStress * GetCurrentGravity() * gpGlobals->frametime ) );
		dmgInfo.SetDamagePosition( GetAbsOrigin() );
		TakeDamage( dmgInfo );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const impactdamagetable_t
//-----------------------------------------------------------------------------
const impactdamagetable_t &CBaseCombatCharacter::GetPhysicsImpactDamageTable( void )
{
	return gDefaultNPCImpactDamageTable;
}

// how much to amplify impact forces
// This is to account for the ragdolls responding differently than
// the shadow objects.  Also this makes the impacts more dramatic.
ConVar	phys_impactforcescale( "phys_impactforcescale", "1.0" ); 
ConVar	phys_upimpactforcescale( "phys_upimpactforcescale", "0.375" ); 

void CBaseCombatCharacter::VPhysicsShadowCollision( int index, gamevcollisionevent_t *pEvent )
{
	int otherIndex = !index;
	CBaseEntity *pOther = pEvent->pEntities[otherIndex];
	IPhysicsObject *pOtherPhysics = pEvent->pObjects[otherIndex];
	if ( !pOther )
		return;

	// Ragdolls are marked as dying.
	if ( pOther->m_lifeState == LIFE_DYING )
		return;

	if ( pOther->GetMoveType() != MOVETYPE_VPHYSICS )
		return;
	
	if ( !pOtherPhysics->IsMoveable() )
		return;
	
	if ( pOther == GetGroundEntity() )
		return;

	// Player can't damage himself if he's was physics attacker *on this frame*
	// which can occur owing to ordering issues it appears.
	float flOtherAttackerTime = 0.0f;

#if defined( HL2_DLL ) && !defined( HL2MP )
	if ( HL2GameRules()->MegaPhyscannonActive() == true )
	{
		flOtherAttackerTime = 1.0f;
	}
#endif // HL2_DLL && !HL2MP

	if ( this == pOther->HasPhysicsAttacker( flOtherAttackerTime ) )
		return;

	int damageType = 0;
	float damage = 0;

	damage = CalculatePhysicsImpactDamage( index, pEvent, GetPhysicsImpactDamageTable(), m_impactEnergyScale, false, damageType );
	
	if ( damage <= 0 )
		return;
	
	// NOTE: We really need some rotational motion for some of these collisions.
	// REVISIT: Maybe resolve this collision on death with a different (not approximately infinite like AABB tensor)
	// inertia tensor to get torque?
	Vector damageForce = pEvent->postVelocity[index] * pEvent->pObjects[index]->GetMass() * phys_impactforcescale.GetFloat();
	
	IServerVehicle *vehicleOther = pOther->GetServerVehicle();
	if ( vehicleOther )
	{
		CBaseCombatCharacter *pPassenger = vehicleOther->GetPassenger();
		if ( pPassenger != NULL )
		{
			// flag as vehicle damage
			damageType |= DMG_VEHICLE;
			// if hit by vehicle driven by player, add some upward velocity to force
			float len = damageForce.Length();
			damageForce.z += len*phys_upimpactforcescale.GetFloat();
			//Msg("Force %.1f / %.1f\n", damageForce.Length(), damageForce.z );

			if ( pPassenger->IsPlayer() )
			{
				CBasePlayer *pPlayer = assert_cast<CBasePlayer *>(pPassenger);
				if( damage >= GetMaxHealth() )
				{
					pPlayer->RumbleEffect( RUMBLE_357, 0, RUMBLE_FLAG_RESTART );
				}
				else
				{
					pPlayer->RumbleEffect( RUMBLE_PISTOL, 0, RUMBLE_FLAG_RESTART );
				}
			}
		}
	}

	Vector damagePos;
	pEvent->pInternalData->GetContactPoint( damagePos );
	CTakeDamageInfo dmgInfo( pOther, pOther, damageForce, damagePos, damage, damageType );

	// FIXME: is there a better way for physics objects to keep track of what root entity responsible for them moving?
	CBasePlayer *pPlayer = pOther->HasPhysicsAttacker( 1.0 );
	if (pPlayer)
	{
		dmgInfo.SetAttacker( pPlayer );
	}

	// UNDONE: Find one near damagePos?
	m_nForceBone = 0;
	PhysCallbackDamage( this, dmgInfo, *pEvent, index );
}


//-----------------------------------------------------------------------------
// Purpose: this entity is exploding, or otherwise needs to inflict damage upon 
//			entities within a certain range.  only damage ents that can clearly 
//			be seen by the explosion!
// Input  :
// Output :
//-----------------------------------------------------------------------------	
void RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrc, float flRadius, int iClassIgnore, CBaseEntity *pEntityIgnore )
{
	// NOTE: I did this this way so I wouldn't have to change a whole bunch of
	// code unnecessarily. We need TF2 specific rules for RadiusDamage, so I moved
	// the implementation of radius damage into gamerules. All existing code calls
	// this method, which calls the game rules method
	g_pGameRules->RadiusDamage( info, vecSrc, flRadius, iClassIgnore, pEntityIgnore );

	// Let the world know if this was an explosion.
	if( info.GetDamageType() & DMG_BLAST )
	{
		// Even the tiniest explosion gets attention. Don't let the radius
		// be less than 128 units.
		float soundRadius = MAX( 128.0f, flRadius * 1.5 );

		CSoundEnt::InsertSound( SOUND_COMBAT | SOUND_CONTEXT_EXPLOSION, vecSrc, soundRadius, 0.25, info.GetInflictor() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Change active weapon and notify derived classes
//			
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::SetActiveWeapon( CBaseCombatWeapon *pNewWeapon )
{
	CBaseCombatWeapon *pOldWeapon = m_hActiveWeapon;
	if ( pNewWeapon != pOldWeapon )
	{
		m_hActiveWeapon = pNewWeapon;
		OnChangeActiveWeapon( pOldWeapon, pNewWeapon );
	}
}

//-----------------------------------------------------------------------------
// Consider the weapon's built-in accuracy, this character's proficiency with
// the weapon, and the status of the target. Use this information to determine
// how accurately to shoot at the target.
//-----------------------------------------------------------------------------
Vector CBaseCombatCharacter::GetAttackSpread( CBaseCombatWeapon *pWeapon, CBaseEntity *pTarget )
{
	if ( pWeapon )
		return pWeapon->GetBulletSpread(GetCurrentWeaponProficiency());
	return VECTOR_CONE_15DEGREES;
}

//-----------------------------------------------------------------------------
float CBaseCombatCharacter::GetSpreadBias( CBaseCombatWeapon *pWeapon, CBaseEntity *pTarget )
{
	if ( pWeapon )
		return pWeapon->GetSpreadBias(GetCurrentWeaponProficiency());
	return 1.0;
}

#ifdef GLOWS_ENABLE
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::AddGlowEffect( void )
{
	SetTransmitState( FL_EDICT_ALWAYS );
	m_bGlowEnabled.Set( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::RemoveGlowEffect( void )
{
	m_bGlowEnabled.Set( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::IsGlowEffectActive( void )
{
	return m_bGlowEnabled;
}

void CBaseCombatCharacter::SetGlowColor( float red, float green, float blue, float alpha )
{
	m_GlowColor.GetForModify().Init( red, green, blue );
	m_GlowAlpha.Set( alpha );
}
#endif // GLOWS_ENABLE

//-----------------------------------------------------------------------------
// Assume everyone is average with every weapon. Override this to make exceptions.
//-----------------------------------------------------------------------------
WeaponProficiency_t CBaseCombatCharacter::CalcWeaponProficiency( CBaseCombatWeapon *pWeapon )
{
	return WEAPON_PROFICIENCY_AVERAGE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
#define MAX_MISS_CANDIDATES 16
CBaseEntity *CBaseCombatCharacter::FindMissTarget( void )
{
	CBaseEntity *pMissCandidates[ MAX_MISS_CANDIDATES ];
	int numMissCandidates = 0;

	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	CBaseEntity *pEnts[256];
	Vector		radius( 100, 100, 100);
	Vector		vecSource = GetAbsOrigin();

	int numEnts = UTIL_EntitiesInBox( pEnts, 256, vecSource-radius, vecSource+radius, 0 );

	for ( int i = 0; i < numEnts; i++ )
	{
		if ( pEnts[i] == NULL )
			continue;

		// New rule for this system. Don't shoot what the player won't see.
		if ( pPlayer && !pPlayer->FInViewCone( pEnts[ i ] ) )
			continue;

		if ( numMissCandidates >= MAX_MISS_CANDIDATES )
			break;

		//See if it's a good target candidate
		if ( FClassnameIs( pEnts[i], "prop_dynamic" ) || 
			 FClassnameIs( pEnts[i], "prop_physics" ) || 
			 FClassnameIs( pEnts[i], "physics_prop" ) )
		{
			pMissCandidates[numMissCandidates++] = pEnts[i];
			continue;
		}
	}

	if( numMissCandidates == 0 )
		return NULL;

	return pMissCandidates[ random->RandomInt( 0, numMissCandidates - 1 ) ];
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::ShouldShootMissTarget( CBaseCombatCharacter *pAttacker )
{
	// Don't shoot at NPC's right now.
	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::InputKilledNPC( inputdata_t &inputdata )
{
	OnKilledNPC( inputdata.pActivator ? inputdata.pActivator->MyCombatCharacterPointer() : NULL );
}

#ifdef MAPBASE
//-----------------------------------------------------------------------------
// Purpose: Handle enemy kills. (this technically measures players too)
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::OnKilledNPC( CBaseCombatCharacter *pKilled )
{
	m_OnKilledEnemy.Set(pKilled, pKilled, this);

	// Fire an additional output if this was a player
	if (pKilled && pKilled->IsPlayer())
		m_OnKilledPlayer.Set(pKilled, pKilled, this);
}

//------------------------------------------------------------------------------
// Purpose: Give the NPC in question the weapon specified
//------------------------------------------------------------------------------
void CBaseCombatCharacter::InputGiveWeapon( inputdata_t &inputdata )
{
	// Give the NPC the specified weapon
	string_t iszWeaponName = inputdata.value.StringID();
	if ( iszWeaponName != NULL_STRING )
	{
		if (IsNPC())
		{
			if( Classify() == CLASS_PLAYER_ALLY_VITAL )
			{
				MyNPCPointer()->m_iszPendingWeapon = iszWeaponName;
			}
			else
			{
				MyNPCPointer()->GiveWeapon( iszWeaponName );
			}
		}
		else
		{
			CBaseCombatWeapon *pWeapon = Weapon_Create(STRING(iszWeaponName));
			if (pWeapon)
			{
				Weapon_Equip(pWeapon);
			}
			else
			{
				Warning( "Couldn't create weapon %s to give %s.\n", STRING(iszWeaponName), GetDebugName() );
				return;
			}
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::InputDropWeapon( inputdata_t &inputdata )
{
	CBaseCombatWeapon *pWeapon = FStrEq(inputdata.value.String(), "") ? GetActiveWeapon() : Weapon_OwnsThisType(inputdata.value.String());
	if (pWeapon)
	{
		Weapon_Drop(pWeapon);
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::InputPickupWeaponInstant( inputdata_t &inputdata )
{
	if (inputdata.value.Entity() && inputdata.value.Entity()->IsBaseCombatWeapon())
	{
		CBaseCombatWeapon *pWeapon = inputdata.value.Entity()->MyCombatWeaponPointer();
		if (pWeapon->GetOwner())
		{
			Msg("Ignoring PickupWeaponInstant on %s because %s already has an owner\n", GetDebugName(), pWeapon->GetDebugName());
			return;
		}

		if (CBaseCombatWeapon *pExistingWeapon = Weapon_OwnsThisType(pWeapon->GetClassname()))
		{
			// Drop our existing weapon then!
			Weapon_Drop(pExistingWeapon);
		}

		if (IsNPC())
		{
			Weapon_Equip(pWeapon);
			MyNPCPointer()->OnGivenWeapon(pWeapon);
		}
		else
		{
			Weapon_Equip(pWeapon);
		}

		pWeapon->OnPickedUp( this );
	}
	else
	{
		Warning("%s received PickupWeaponInstant with invalid entity %s\n", GetDebugName(), inputdata.value.Entity() ? inputdata.value.Entity()->GetDebugName() : "<<null>>");
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::InputHolsterWeapon( inputdata_t &inputdata )
{
	CBaseCombatWeapon *pWeapon = GetActiveWeapon();
	if (pWeapon)
	{
		pWeapon->Holster();
		//SetActiveWeapon( NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::InputHolsterAndDestroyWeapon( inputdata_t &inputdata )
{
	CBaseCombatWeapon *pWeapon = GetActiveWeapon();
	if (pWeapon)
	{
		pWeapon->Holster();
		SetActiveWeapon( NULL );

		if (pWeapon->GetActivity() == ACT_VM_HOLSTER)
		{
			// Remove when holster is finished
			pWeapon->ThinkSet( &CBaseEntity::SUB_Remove, gpGlobals->curtime + pWeapon->GetViewModelSequenceDuration() );
		}
		else
		{
			// Remove now
			UTIL_Remove( pWeapon );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::InputUnholsterWeapon( inputdata_t &inputdata )
{
	// NPCs can handle strings, but players fall back to SwitchToWeapon
	if (inputdata.value.StringID() != NULL_STRING)
		InputSwitchToWeapon( inputdata );

	CBaseCombatWeapon *pWeapon = GetActiveWeapon();
	if (pWeapon && pWeapon->IsEffectActive(EF_NODRAW))
	{
		pWeapon->Deploy();
	}
}

//------------------------------------------------------------------------------
// Purpose: Makes the NPC instantly switch to the specified weapon, creates it if it doesn't exist
//------------------------------------------------------------------------------
void CBaseCombatCharacter::InputSwitchToWeapon( inputdata_t &inputdata )
{
	for (int i = 0; i<MAX_WEAPONS; i++)
	{
		// These are both pooled, so if they're the same classname they should point to the same address
		if (m_hMyWeapons[i].Get() && m_hMyWeapons[i]->m_iClassname == inputdata.value.StringID())
		{
			Weapon_Switch( m_hMyWeapons[i] );
			return;
		}
	}

	// We must not have it
	if (IsNPC())
		MyNPCPointer()->GiveWeapon( inputdata.value.StringID(), false );
	else
	{
		CBaseCombatWeapon *pWeapon = Weapon_Create( inputdata.value.String() );
		if (pWeapon)
		{
			Weapon_Equip( pWeapon );
		}
		else
		{
			Warning( "Couldn't create weapon %s to give %s.\n", inputdata.value.String(), GetDebugName() );
		}
	}
}

#define FINDNAMEDENTITY_MAX_ENTITIES	32
//-----------------------------------------------------------------------------
// Purpose: FindNamedEntity has been moved from CAI_BaseNPC to CBaseCombatCharacter so players can use it.
//			Coincidentally, everything that it did on NPCs could be done on BaseCombatCharacters with no consequences.
// Input  :
// Output :
//-----------------------------------------------------------------------------
CBaseEntity *CBaseCombatCharacter::FindNamedEntity( const char *szName, IEntityFindFilter *pFilter )
{
	const char *name = szName;
	if (name[0] == '!')
		name++;

	if ( !stricmp( name, "player" ))
	{
		return AI_GetSinglePlayer();
	}
	else if ( !stricmp( name, "enemy" ) )
	{
		return GetEnemy();
	}
	else if ( !stricmp( name, "self" ) || !stricmp( name, "target1" ) )
	{
		return this;
	}
	else if ( !stricmp( name, "nearestfriend" ) || !strnicmp( name, "friend", 6 ) )
	{
		// Just look for the nearest friendly NPC within 500 units
		// (most of this was stolen from CAI_PlayerAlly::FindSpeechTarget())
		const Vector &	vAbsOrigin = GetAbsOrigin();
		float 			closestDistSq = Square(500.0);
		CBaseEntity *	pNearest = NULL;
		float			distSq;
		int				i;
		for ( i = 0; i < g_AI_Manager.NumAIs(); i++ )
		{
			CAI_BaseNPC *pNPC = (g_AI_Manager.AccessAIs())[i];

			if ( pNPC == this )
				continue;

			distSq = ( vAbsOrigin - pNPC->GetAbsOrigin() ).LengthSqr();
				
			if ( distSq > closestDistSq )
				continue;

			if ( IRelationType( pNPC ) == D_LI )
			{
				closestDistSq = distSq;
				pNearest = pNPC;
			}
		}

		if (stricmp(name, "friend_npc") != 0)
		{
			// Okay, find the nearest friendly client.
			for ( i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
				if ( pPlayer )
				{
					// Don't get players with notarget
					if (pPlayer->GetFlags() & FL_NOTARGET)
						continue;

					distSq = ( vAbsOrigin - pPlayer->GetAbsOrigin() ).LengthSqr();
					
					if ( distSq > closestDistSq )
						continue;

					if ( IRelationType( pPlayer ) == D_LI )
					{
						closestDistSq = distSq;
						pNearest = pPlayer;
					}
				}
			}
		}

		return pNearest;
	}
	else if (!stricmp( name, "weapon" ))
	{
		return GetActiveWeapon();
	}

	// HACKHACK: FindEntityProcedural can go through this now, so running this code could cause an infinite loop.
	// As a result, FindEntityProcedural currently identifies itself with this entity filter.
	else if (!pFilter || !dynamic_cast<CNullEntityFilter*>(pFilter))
	{
		// search for up to 32 entities with the same name and choose one randomly
		CBaseEntity *entityList[ FINDNAMEDENTITY_MAX_ENTITIES ];
		CBaseEntity *entity;
		int	iCount;

		entity = NULL;
		for( iCount = 0; iCount < FINDNAMEDENTITY_MAX_ENTITIES; iCount++ )
		{
			entity = gEntList.FindEntityByName( entity, szName, this, NULL, NULL, pFilter );
			if ( !entity )
			{
				break;
			}
			entityList[ iCount ] = entity;
		}

		if ( iCount > 0 )
		{
			int index = RandomInt( 0, iCount - 1 );
			entity = entityList[ index ];
			return entity;
		}
	}

	return NULL;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Overload our muzzle flash and send it to any actively held weapon
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::DoMuzzleFlash()
{
	// Our weapon takes our muzzle flash command
	CBaseCombatWeapon *pWeapon = GetActiveWeapon();
	if ( pWeapon )
	{
		pWeapon->DoMuzzleFlash();
		//NOTENOTE: We do not chain to the base here
	}
	else
	{
		BaseClass::DoMuzzleFlash();
	}
}

#ifdef MAPBASE_VSCRIPT
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
HSCRIPT CBaseCombatCharacter::ScriptGetActiveWeapon()
{
	return ToHScript( GetActiveWeapon() );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
HSCRIPT CBaseCombatCharacter::ScriptGetWeapon( int i )
{
	Assert( i >= 0 && i < MAX_WEAPONS );

	if ( i < 0 || i >= MAX_WEAPONS )
		return NULL;

	return ToHScript( GetWeapon( i ) );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
HSCRIPT CBaseCombatCharacter::ScriptGetWeaponByType( const char *pszWeapon, int iSubType )
{
	return ToHScript( Weapon_OwnsThisType( pszWeapon, iSubType ) );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::ScriptGetAllWeapons( HSCRIPT hTable )
{
	for (int i=0;i<MAX_WEAPONS;i++)
	{
		if (m_hMyWeapons[i]) 
		{
			g_pScriptVM->SetValue( hTable, m_hMyWeapons[i]->GetClassname(), ToHScript( m_hMyWeapons[i] ) );
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::ScriptDropWeapon( HSCRIPT hWeapon )
{
	CBaseCombatWeapon *pWeapon = HScriptToClass<CBaseCombatWeapon>( hWeapon );
	if (!pWeapon)
		return;

	if (pWeapon->GetOwner() == this)
	{
		// Drop the weapon
		Weapon_Drop( pWeapon );
	}
	else
	{
		CGMsg( 1, CON_GROUP_VSCRIPT, "ScriptDropWeapon: %s is not owned by %s", pWeapon->GetDebugName(), GetDebugName() );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::ScriptEquipWeapon( HSCRIPT hWeapon )
{
	CBaseCombatWeapon *pWeapon = HScriptToClass<CBaseCombatWeapon>( hWeapon );
	if (!pWeapon)
		return;

	if (pWeapon->GetOwner() == this)
	{
		// Switch to this weapon
		Weapon_Switch( pWeapon );
	}
	else
	{
		if (CBaseCombatWeapon *pExistingWeapon = Weapon_OwnsThisType( pWeapon->GetClassname() ))
		{
			// Drop our existing weapon then!
			Weapon_Drop( pExistingWeapon );
		}

		if (IsNPC())
		{
			Weapon_Equip( pWeapon );
			MyNPCPointer()->OnGivenWeapon( pWeapon );
		}
		else
		{
			Weapon_Equip( pWeapon );
		}

		pWeapon->OnPickedUp( this );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CBaseCombatCharacter::ScriptGiveAmmo( int iCount, int iAmmoIndex, bool bSuppressSound )
{
	return GiveAmmo( iCount, iAmmoIndex, bSuppressSound );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::ScriptRemoveAmmo( int iCount, int iAmmoIndex )
{
	if (iAmmoIndex == -1)
	{
		Warning( "%i is not a valid ammo type\n", iAmmoIndex );
		return;
	}

	RemoveAmmo( iCount, iAmmoIndex );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CBaseCombatCharacter::ScriptGetAmmoCount( int iType ) const
{
	Assert( iType == -1 || iType < MAX_AMMO_SLOTS );

	if ( iType < 0 || iType >= MAX_AMMO_SLOTS )
		return 0;

	return GetAmmoCount( iType );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::ScriptSetAmmoCount( int iType, int iCount )
{
	Assert( iType == -1 || iType < MAX_AMMO_SLOTS );

	if ( iType < 0 || iType >= MAX_AMMO_SLOTS )
		return;

	return SetAmmoCount( iCount, iType );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const Vector& CBaseCombatCharacter::ScriptGetAttackSpread( HSCRIPT hWeapon, HSCRIPT hTarget )
{
	CBaseEntity *pWeapon = ToEnt( hWeapon );
	if (!pWeapon || !pWeapon->IsBaseCombatWeapon())
	{
		Warning( "GetAttackSpread: %s is not a valid weapon\n", pWeapon ? pWeapon->GetDebugName() : "Null entity" );
		return vec3_origin;
	}

	// TODO: Make this a simple non-reference Vector?
	static Vector vec;
	vec = GetAttackSpread( pWeapon->MyCombatWeaponPointer(), ToEnt( hTarget ) );
	return vec;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
float CBaseCombatCharacter::ScriptGetSpreadBias( HSCRIPT hWeapon, HSCRIPT hTarget )
{
	CBaseEntity *pWeapon = ToEnt( hWeapon );
	if (!pWeapon || !pWeapon->IsBaseCombatWeapon())
	{
		Warning( "GetSpreadBias: %s is not a valid weapon\n", pWeapon ? pWeapon->GetDebugName() : "Null entity" );
		return 1.0f;
	}

	return GetSpreadBias( pWeapon->MyCombatWeaponPointer(), ToEnt( hTarget ) );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CBaseCombatCharacter::ScriptRelationType( HSCRIPT pTarget )
{
	return (int)IRelationType( ToEnt( pTarget ) );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CBaseCombatCharacter::ScriptRelationPriority( HSCRIPT pTarget )
{
	return IRelationPriority( ToEnt( pTarget ) );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::ScriptSetRelationship( HSCRIPT pTarget, int disposition, int priority )
{
	AddEntityRelationship( ToEnt( pTarget ), (Disposition_t)disposition, priority );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::ScriptSetClassRelationship( int classify, int disposition, int priority )
{
	AddClassRelationship( (Class_T)classify, (Disposition_t)disposition, priority);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
HSCRIPT CBaseCombatCharacter::ScriptGetVehicleEntity()
{
	return ToHScript( GetVehicleEntity() );
}
#endif

//-----------------------------------------------------------------------------
// Purpose: return true if given target cant be seen because of fog
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::IsHiddenByFog( const Vector &target ) const
{
	float range = EyePosition().DistTo( target );
	return IsHiddenByFog( range );
}

//-----------------------------------------------------------------------------
// Purpose: return true if given target cant be seen because of fog
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::IsHiddenByFog( CBaseEntity *target ) const
{
	if ( !target )
		return false;

	float range = EyePosition().DistTo( target->WorldSpaceCenter() );
	return IsHiddenByFog( range );
}

//-----------------------------------------------------------------------------
// Purpose: return true if given target cant be seen because of fog
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::IsHiddenByFog( float range ) const
{
	if ( GetFogObscuredRatio( range ) >= 1.0f )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: return 0-1 ratio where zero is not obscured, and 1 is completely obscured
//-----------------------------------------------------------------------------
float CBaseCombatCharacter::GetFogObscuredRatio( const Vector &target ) const
{
	float range = EyePosition().DistTo( target );
	return GetFogObscuredRatio( range );
}

//-----------------------------------------------------------------------------
// Purpose: return 0-1 ratio where zero is not obscured, and 1 is completely obscured
//-----------------------------------------------------------------------------
float CBaseCombatCharacter::GetFogObscuredRatio( CBaseEntity *target ) const
{
	if ( !target )
		return false;

	float range = EyePosition().DistTo( target->WorldSpaceCenter() );
	return GetFogObscuredRatio( range );
}

//-----------------------------------------------------------------------------
// Purpose: return 0-1 ratio where zero is not obscured, and 1 is completely obscured
//-----------------------------------------------------------------------------
float CBaseCombatCharacter::GetFogObscuredRatio( float range ) const
{
/* TODO: Get global fog from map somehow since nav mesh fog is gone
	fogparams_t fog;
	GetFogParams( &fog );

	if ( !fog.enable )
		return 0.0f;

	if ( range <= fog.start )
		return 0.0f;

	if ( range >= fog.end )
		return 1.0f;

	float ratio = (range - fog.start) / (fog.end - fog.start);
	ratio = MIN( ratio, fog.maxdensity );
	return ratio;
*/
	return 0.0f;
}


//-----------------------------------------------------------------------------
// Purpose: Invoke this to update our last known nav area 
// (since there is no think method chained to CBaseCombatCharacter)
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::UpdateLastKnownArea( void )
{
#ifdef NEXT_BOT
	if ( TheNavMesh->IsGenerating() )
	{
		ClearLastKnownArea();
		return;
	}

	if ( nb_last_area_update_tolerance.GetFloat() > 0.0f )
	{
		// skip this test if we're not standing on the world (ie: elevators that move us)
		if ( GetGroundEntity() == NULL || GetGroundEntity()->IsWorld() )
		{
			if ( m_lastNavArea && m_NavAreaUpdateMonitor.IsMarkSet() && !m_NavAreaUpdateMonitor.TargetMoved( this ) )
				return;

			m_NavAreaUpdateMonitor.SetMark( this, nb_last_area_update_tolerance.GetFloat() );
		}
	}

	// find the area we are directly standing in
	CNavArea *area = TheNavMesh->GetNearestNavArea( this, GETNAVAREA_CHECK_GROUND | GETNAVAREA_CHECK_LOS, 50.0f );
	if ( !area )
		return;

	// make sure we can actually use this area - if not, consider ourselves off the mesh
	if ( !IsAreaTraversable( area ) )
		return;

	if ( area != m_lastNavArea )
	{
		// player entered a new nav area
		if ( m_lastNavArea )
		{
			m_lastNavArea->DecrementPlayerCount( m_registeredNavTeam, entindex() );
			m_lastNavArea->OnExit( this, area );
		}

		m_registeredNavTeam = GetTeamNumber();
		area->IncrementPlayerCount( m_registeredNavTeam, entindex() );
		area->OnEnter( this, m_lastNavArea );

		OnNavAreaChanged( area, m_lastNavArea );

		m_lastNavArea = area;
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Return true if we can use (walk through) the given area 
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::IsAreaTraversable( const CNavArea *area ) const
{
	return area ? !area->IsBlocked( GetTeamNumber() ) : false;
}


//-----------------------------------------------------------------------------
// Purpose: Leaving the nav mesh
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::ClearLastKnownArea( void )
{
	OnNavAreaChanged( NULL, m_lastNavArea );
	
	if ( m_lastNavArea )
	{
		m_lastNavArea->DecrementPlayerCount( m_registeredNavTeam, entindex() );
		m_lastNavArea->OnExit( this, NULL );
		m_lastNavArea = NULL;
		m_registeredNavTeam = TEAM_INVALID;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handling editor removing the area we're standing upon
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::OnNavAreaRemoved( CNavArea *removedArea )
{
	if ( m_lastNavArea == removedArea )
	{
		ClearLastKnownArea();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Changing team, maintain associated data
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::ChangeTeam( int iTeamNum )
{
	// old team member no longer in the nav mesh
	ClearLastKnownArea();

#ifdef GLOWS_ENABLE
	RemoveGlowEffect();
#endif // GLOWS_ENABLE

	BaseClass::ChangeTeam( iTeamNum );
}


//-----------------------------------------------------------------------------
// Return true if we have ever been injured by a member of the given team
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::HasEverBeenInjured( int team /*= TEAM_ANY */ ) const
{
	if ( team == TEAM_ANY )
	{
		return ( m_hasBeenInjured == 0 ) ? false : true;
	}

	int teamMask = 1 << team;

	if ( m_hasBeenInjured & teamMask )
	{
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Return time since we were hurt by a member of the given team
//-----------------------------------------------------------------------------
float CBaseCombatCharacter::GetTimeSinceLastInjury( int team /*= TEAM_ANY */ ) const
{
	const float never = 999999999999.9f;

	if ( team == TEAM_ANY )
	{
		float time = never;

		// find most recent injury time
		for( int i=0; i<MAX_DAMAGE_TEAMS; ++i )
		{
			if ( m_damageHistory[i].team != TEAM_INVALID )
			{
				if ( m_damageHistory[i].interval.GetElapsedTime() < time )
				{
					time = m_damageHistory[i].interval.GetElapsedTime();
				}
			}
		}

		return time;
	}
	else
	{
		for( int i=0; i<MAX_DAMAGE_TEAMS; ++i )
		{
			if ( m_damageHistory[i].team == team )
			{
				return m_damageHistory[i].interval.GetElapsedTime();
			}
		}
	}

	return never;
}


