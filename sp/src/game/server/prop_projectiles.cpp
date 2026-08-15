#include "cbase.h"
#include "BasePropDoor.h"
#include "ai_basenpc.h"
#include "npcevent.h"
#include "engine/IEngineSound.h"
#include "locksounds.h"
#include "filters.h"
#include "physics.h"
#include "vphysics_interface.h"
#include "entityoutput.h"
#include "vcollide_parse.h"
#include "studio.h"
#include "explode.h"
#include "utlrbtree.h"
#include "tier1/strtools.h"
#include "physics_impact_damage.h"
#include "KeyValues.h"
#include "filesystem.h"
#include "scriptevent.h"
#include "entityblocker.h"
#include "soundent.h"
#include "EntityFlame.h"
#include "game.h"
#include "physics_prop_ragdoll.h"
#include "decals.h"
#include "hierarchy.h"
#include "shareddefs.h"
#include "physobj.h"
#include "physics_npc_solver.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "datacache/imdlcache.h"
#include "doors.h"
#include "physics_collisionevent.h"
#include "gamestats.h"
#include "vehicle_base.h"
#include "props.h"
#include "ai_interactions.h"
#ifdef MAPBASE
#include "mapbase/GlobalStrings.h"
#include "collisionutils.h"
#include "vstdlib/IKeyValuesSystem.h" // From Alien Swarm SDK

#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



//=============================================================================
// PHYSICS PROPS
//=============================================================================





LINK_ENTITY_TO_CLASS(prop_projectile, CProjectileProp);
BEGIN_DATADESC(CProjectileProp)

DEFINE_INPUTFUNC(FIELD_VOID, "EnableMotion", InputEnableMotion),
DEFINE_INPUTFUNC(FIELD_VOID, "DisableMotion", InputDisableMotion),
DEFINE_INPUTFUNC(FIELD_VOID, "DisableFloating", InputDisableFloating),
#ifdef MAPBASE
DEFINE_INPUTFUNC(FIELD_BOOLEAN, "SetDebris", InputSetDebris),
#endif

DEFINE_KEYFIELD(m_massScale, FIELD_FLOAT, "massscale"),
DEFINE_KEYFIELD(m_inertiaScale, FIELD_FLOAT, "inertiascale"),
DEFINE_KEYFIELD(m_damageType, FIELD_INTEGER, "Damagetype"),
DEFINE_KEYFIELD(m_iszOverrideScript, FIELD_STRING, "overridescript"),

DEFINE_KEYFIELD(m_damageToEnableMotion, FIELD_INTEGER, "damagetoenablemotion"),
DEFINE_KEYFIELD(m_flForceToEnableMotion, FIELD_FLOAT, "forcetoenablemotion"),

DEFINE_OUTPUT(m_MotionEnabled, "OnMotionEnabled"),
DEFINE_OUTPUT(m_OnPhysGunPickup, "OnPhysGunPickup"),
DEFINE_OUTPUT(m_OnPhysGunOnlyPickup, "OnPhysGunOnlyPickup"),
DEFINE_OUTPUT(m_OnPhysGunPull, "OnPhysGunPull"),
DEFINE_OUTPUT(m_OnPhysGunPunt, "OnPhysGunPunt"),
DEFINE_OUTPUT(m_OnPhysGunDrop, "OnPhysGunDrop"),
DEFINE_OUTPUT(m_OnPlayerUse, "OnPlayerUse"),
DEFINE_OUTPUT(m_OnPlayerPickup, "OnPlayerPickup"),
DEFINE_OUTPUT(m_OnOutOfWorld, "OnOutOfWorld"),

DEFINE_FIELD(m_bThrownByPlayer, FIELD_BOOLEAN),

DEFINE_FIELD(m_bFirstCollisionAfterLaunch, FIELD_BOOLEAN),
DEFINE_FIELD(m_handleSprite, FIELD_EHANDLE),
DEFINE_FIELD(m_handleTrail, FIELD_EHANDLE),
DEFINE_FIELD(m_vecDir, FIELD_VECTOR),
DEFINE_FIELD(m_flDamage, FIELD_FLOAT),
DEFINE_FIELD(m_iBallType, FIELD_INTEGER),

DEFINE_THINKFUNC(ClearFlagsThink),

END_DATADESC()

CProjectileProp::CProjectileProp(BALLTYPE t) {
	m_iBallType = t;
	SetMaterials(m_iBallType);

}

CProjectileProp::CProjectileProp()
{
	SetModelName(AllocPooledString("models/propper/doomedresearch_devroom/plasmaball_balloutline_small.mdl"));
	SetRenderMode(kRenderTransColor);
	SetRenderColor(231, 148, 58, 150);
}


void CProjectileProp::SetMaterials(BALLTYPE t) {
	switch (t)
	{
	case YLW:
		m_iszSpriteName = MAKE_STRING("sprites/glow01.spr");
		m_iszTrailName = MAKE_STRING("sprites/laserbeam.vmt");
		break;
	case YLW_HUGE:
		m_iszSpriteName = MAKE_STRING("sprites/glow01.spr");
		m_iszTrailName = MAKE_STRING("sprites/laserbeam.vmt");
		break;
	case CYAN:
		m_iszSpriteName = MAKE_STRING("sprites/glow01.spr");
		m_iszTrailName = MAKE_STRING("sprites/laserbeam.vmt");
		break;
	default:
		break;
	}
}


bool CProjectileProp::IsGib()
{
	return (m_spawnflags & SF_PHYSPROP_IS_GIB) ? true : false;
}



CProjectileProp::~CProjectileProp()
{
}



void CProjectileProp::HandleAnyCollisionInteractions(int index, gamevcollisionevent_t* pEvent)
{
	// If we're supposed to impale, and we've hit an NPC, impale it
	if (HasInteraction(PROPINTER_PHYSGUN_FIRST_IMPALE))
	{
		Vector vel = pEvent->preVelocity[index];

		Vector forward;
		QAngle angImpaleForward;
		if (GetPropDataAngles("impale_forward", angImpaleForward))
		{
			Vector vecImpaleForward;
			AngleVectors(angImpaleForward, &vecImpaleForward);
			VectorRotate(vecImpaleForward, EntityToWorldTransform(), forward);
		}
		else
		{
			GetVectors(&forward, NULL, NULL);
		}

		float speed = DotProduct(forward, vel);
		if (speed < 1000.0f)
		{
			// not going to stick, so remove any ragdolls we've got
			CheckRemoveRagdolls();
			return;
		}
		CBaseEntity* pHitEntity = pEvent->pEntities[!index];
		if (pHitEntity->IsWorld())
		{
			Vector normal;
			float sign = index ? -1.0f : 1.0f;
			pEvent->pInternalData->GetSurfaceNormal(normal);
			float dot = DotProduct(forward, normal);
			if ((sign * dot) < DOT_45DEGREE)
				return;
			// Impale sticks to the wall if we hit end on
			HandleInteractionStick(index, pEvent);
		}
		else if (pHitEntity->MyNPCPointer())
		{
			CAI_BaseNPC* pNPC = pHitEntity->MyNPCPointer();
			IPhysicsObject* pObj = VPhysicsGetObject();

			// do not impale NPCs if the impaler is friendly
			CBasePlayer* pAttacker = HasPhysicsAttacker(25.0f);
			if (pAttacker && pNPC->IRelationType(pAttacker) == D_LI)
			{
				return;
			}

			Vector vecPos;
			pObj->GetPosition(&vecPos, NULL);

			// Find the bone for the hitbox we hit
			trace_t tr;
			UTIL_TraceLine(vecPos, vecPos + pEvent->preVelocity[index] * 1.5, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);
			Vector vecImpalePos = tr.endpos;
			int iBone = -1;
			if (tr.hitbox)
			{
				Vector vecBonePos;
				QAngle vecBoneAngles;
				iBone = pNPC->GetHitboxBone(tr.hitbox);
				pNPC->GetBonePosition(iBone, vecBonePos, vecBoneAngles);

				Teleport(&vecBonePos, NULL, NULL);
				vecImpalePos = vecBonePos;
			}

			// Kill the NPC and make an attached ragdoll
			pEvent->pInternalData->GetContactPoint(vecImpalePos);
			CBaseEntity* pRagdoll = CreateServerRagdollAttached(pNPC, vec3_origin, -1, COLLISION_GROUP_INTERACTIVE_DEBRIS, pObj, this, 0, vecImpalePos, iBone, vec3_origin);
			if (pRagdoll)
			{
				Vector vecVelocity = pEvent->preVelocity[index] * pObj->GetMass();
				PhysCallbackImpulse(pObj, vecVelocity, vec3_origin);
				UTIL_Remove(pNPC);
				AddSpawnFlags(SF_PHYSPROP_HAS_ATTACHED_RAGDOLLS);
			}
		}
	}
}

CProjectileProp* CProjectileProp::ShootProjectile(BALLTYPE t, float vel, Vector from, QAngle angle, CBaseEntity* owner)
{
	CProjectileProp* pProp = dynamic_cast<CProjectileProp*>(CreateEntityByName("prop_projectile"));
	if (pProp) {


		pProp->SetAbsOrigin(from);
		pProp->SetAbsAngles(angle);
		pProp->SetOwnerEntity(owner);
		pProp->m_iBallType = t;
		pProp->SetMaterials(pProp->m_iBallType);
#ifdef DEBUG
		DevMsg("PROJECTILE SPRITES: %s %s\n", pProp->m_iszSpriteName.ToCStr(), pProp->m_iszTrailName.ToCStr());
#endif // DEBUG
		pProp->Precache();
		DispatchSpawn(pProp);
		pProp->SetupSprites();
	}
	return pProp;
}


LINK_ENTITY_TO_CLASS(prop_projectile_shooter, CProjectilePropShooter);

// Start of our data description for the class
BEGIN_DATADESC(CProjectilePropShooter)

// Links our member variable to our keyvalue from Hammer
DEFINE_KEYFIELD(m_iBalltype, FIELD_INTEGER, "projectiletype"),

// Links our input name from Hammer to our input member function
DEFINE_INPUTFUNC(FIELD_VOID, "ShootProjectile", InputShootProjectile),

END_DATADESC()

CProjectilePropShooter::CProjectilePropShooter()
{
	SetOwnerEntity(this->GetRootMoveParent());
}

void CProjectilePropShooter::InputShootProjectile(inputdata_t& inputdata)
{
	ShootProjectile();
}

void CProjectilePropShooter::ShootProjectile()
{
	CProjectileProp::ShootProjectile(this->m_iBalltype, PROJECTILE_SPEED, this->GetAbsOrigin(), this->GetAbsAngles(), this->GetOwnerEntity());
}


void CProjectileProp::SetupSprites()
{
	// Start up the eye glow

	m_handleSprite = CSprite::SpriteCreate(STRING(m_iszSpriteName), this->GetLocalOrigin(), false);
	m_handleTrail = CSpriteTrail::SpriteTrailCreate(STRING(m_iszSpriteName), this->GetLocalOrigin(), false);

	if (m_handleSprite && m_handleTrail)
	{
		m_handleSprite->FollowEntity(this);
		m_handleSprite->SetTransparency(kRenderWorldGlow, 231, 148, 58, 200, kRenderFxNoDissipation);
		m_handleSprite->SetBrightness(255);

		m_handleSprite->FollowEntity(this);
		m_handleTrail->SetTransparency(kRenderTransAdd, 231, 148, 58, 200, kRenderFxNone);
		m_handleTrail->SetBrightness(255);
		m_handleTrail->SetLifeTime(0.4);
		m_handleSprite->SetScale(0.5);
		m_handleTrail->SetStartWidth(16.0);
		m_handleTrail->TurnOn();

		switch (m_iBallType)
		{
		case YLW:
			m_handleSprite->SetScale(0.5);
			m_handleTrail->SetStartWidth(16.0);
			break;
		case YLW_HUGE:
			m_handleSprite->SetScale(1.0);
			m_handleTrail->SetStartWidth(32.0);
			break;
		case CYAN:
			m_handleSprite->SetScale(0.5);
			m_handleTrail->SetStartWidth(16.0);
			break;
		default:
			break;
		}
	}

}

//-----------------------------------------------------------------------------
// Purpose: Create a physics object for this prop
//-----------------------------------------------------------------------------
void CProjectileProp::Spawn()
{

	SetModelName(AllocPooledString("models/propper/doomedresearch_devroom/plasmaball_balloutline.mdl"));

	BaseClass::Spawn();

	if (IsMarkedForDeletion())
		return;

	if (HasSpawnFlags(SF_PHYSPROP_DEBRIS) || HasInteraction(PROPINTER_PHYSGUN_CREATE_FLARE))
	{
		SetCollisionGroup(HasSpawnFlags(SF_PHYSPROP_FORCE_TOUCH_TRIGGERS) ? COLLISION_GROUP_DEBRIS_TRIGGER : COLLISION_GROUP_DEBRIS);
	}

	if (HasSpawnFlags(SF_PHYSPROP_NO_ROTORWASH_PUSH))
	{
		AddEFlags(EFL_NO_ROTORWASH_PUSH);
	}

	CreateVPhysics();

	if (!PropDataOverrodeBlockLOS())
	{
		CalculateBlockLOS();
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CProjectileProp::Precache(void)
{
	if (GetModelName() == NULL_STRING)
	{
		//Msg("%s at (%.3f, %.3f, %.3f) has no model name!\n", GetClassname(), GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z);
	}
	else
	{
		PrecacheModel(STRING(m_iszSpriteName));
		PrecacheModel(STRING(m_iszTrailName));
		PrecacheModel(STRING(GetModelName()));
		BaseClass::Precache();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CProjectileProp::CreateVPhysics()
{
	// Create the object in the physics system
	bool asleep = HasSpawnFlags(SF_PHYSPROP_START_ASLEEP) ? true : false;

	solid_t tmpSolid;
	PhysModelParseSolid(tmpSolid, this, GetModelIndex());

	if (m_massScale > 0)
	{
		tmpSolid.params.mass *= m_massScale;
	}

	if (m_inertiaScale > 0)
	{
		tmpSolid.params.inertia *= m_inertiaScale;
		if (tmpSolid.params.inertia < 0.5)
			tmpSolid.params.inertia = 0.5;
	}

	PhysGetMassCenterOverride(this, modelinfo->GetVCollide(GetModelIndex()), tmpSolid);
	if (HasSpawnFlags(SF_PHYSPROP_NO_COLLISIONS))
	{
		tmpSolid.params.enableCollisions = false;
	}
	PhysSolidOverride(tmpSolid, m_iszOverrideScript);

	IPhysicsObject* pPhysicsObject = VPhysicsInitNormal(SOLID_VPHYSICS, 0, asleep, &tmpSolid);

	if (!pPhysicsObject)
	{
		SetSolid(SOLID_NONE);
		SetMoveType(MOVETYPE_NONE);
		Warning("ERROR!: Can't create physics object for %s\n", STRING(GetModelName()));
	}
	else
	{
		if (m_damageType == 1)
		{
			PhysSetGameFlags(pPhysicsObject, FVPHYSICS_DMG_SLICE);
		}
		if (HasSpawnFlags(SF_PHYSPROP_MOTIONDISABLED) || m_damageToEnableMotion > 0 || m_flForceToEnableMotion > 0)
		{
			pPhysicsObject->EnableMotion(false);
		}
	}

	// fix up any noncompliant blades.
	if (HasInteraction(PROPINTER_PHYSGUN_LAUNCH_SPIN_Z))
	{
		if (!(VPhysicsGetObject()->GetGameFlags() & FVPHYSICS_DMG_SLICE))
		{
			PhysSetGameFlags(pPhysicsObject, FVPHYSICS_DMG_SLICE);

#if 0
			if (g_pDeveloper->GetInt())
			{
				// Highlight them in developer mode.
				m_debugOverlays |= (OVERLAY_TEXT_BIT | OVERLAY_BBOX_BIT);
			}
#endif
		}
	}

	if (HasInteraction(PROPINTER_PHYSGUN_DAMAGE_NONE))
	{
		PhysSetGameFlags(pPhysicsObject, FVPHYSICS_NO_IMPACT_DMG);
	}

	if (HasSpawnFlags(SF_PHYSPROP_PREVENT_PICKUP))
	{
		PhysSetGameFlags(pPhysicsObject, FVPHYSICS_NO_PLAYER_PICKUP);
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CProjectileProp::CanBePickedUpByPhyscannon(void)
{
	if (HasSpawnFlags(SF_PHYSPROP_PREVENT_PICKUP))
		return false;

	IPhysicsObject* pPhysicsObject = VPhysicsGetObject();
	if (pPhysicsObject && pPhysicsObject->IsMoveable() == false)
	{
		if (HasSpawnFlags(SF_PHYSPROP_ENABLE_ON_PHYSCANNON) == false)
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CProjectileProp::OverridePropdata(void)
{
#ifdef MAPBASE
	return EntIsClass(this, gm_isz_class_PropPhysicsOverride);
#else
	return (FClassnameIs(this, "prop_physics_override"));
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Enable physics motion and collision response (on by default)
//-----------------------------------------------------------------------------
void CProjectileProp::InputEnableMotion(inputdata_t& inputdata)
{
	EnableMotion();
}

//-----------------------------------------------------------------------------
// Purpose: Disable any physics motion or collision response
//-----------------------------------------------------------------------------
void CProjectileProp::InputDisableMotion(inputdata_t& inputdata)
{
	IPhysicsObject* pPhysicsObject = VPhysicsGetObject();
	if (pPhysicsObject != NULL)
	{
		pPhysicsObject->EnableMotion(false);
	}
}

// Turn off floating simulation (and cost)
void CProjectileProp::InputDisableFloating(inputdata_t& inputdata)
{
	PhysEnableFloating(VPhysicsGetObject(), false);
}

#ifdef MAPBASE
//-----------------------------------------------------------------------------
// Purpose: Adds or removes the debris spawnflag.
//-----------------------------------------------------------------------------
void CProjectileProp::InputSetDebris(inputdata_t& inputdata)
{
	if (inputdata.value.Bool())
	{
		AddSpawnFlags(SF_PHYSPROP_DEBRIS);
		SetCollisionGroup(HasSpawnFlags(SF_PHYSPROP_FORCE_TOUCH_TRIGGERS) ? COLLISION_GROUP_DEBRIS_TRIGGER : COLLISION_GROUP_DEBRIS);
	}
	else
	{
		RemoveSpawnFlags(SF_PHYSPROP_DEBRIS);
		SetCollisionGroup(COLLISION_GROUP_INTERACTIVE); // Is this the default collision group?
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CProjectileProp::EnableMotion(void)
{
	IPhysicsObject* pPhysicsObject = VPhysicsGetObject();
	if (pPhysicsObject)
	{
		Vector pos;
		QAngle angles;

		if (GetEnableMotionPosition(&pos, &angles))
		{
			ClearEnableMotionPosition();
			//pPhysicsObject->SetPosition( pos, angles, true );
			Teleport(&pos, &angles, NULL);
		}

		pPhysicsObject->EnableMotion(true);
		pPhysicsObject->Wake();

		m_MotionEnabled.FireOutput(this, this, 0);
	}
	CheckRemoveRagdolls();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CProjectileProp::OnPhysGunPickup(CBasePlayer* pPhysGunUser, PhysGunPickup_t reason)
{
	BaseClass::OnPhysGunPickup(pPhysGunUser, reason);

	IPhysicsObject* pPhysicsObject = VPhysicsGetObject();
	if (pPhysicsObject && !pPhysicsObject->IsMoveable())
	{
		if (!HasSpawnFlags(SF_PHYSPROP_ENABLE_ON_PHYSCANNON))
			return;

		EnableMotion();

		if (HasInteraction(PROPINTER_PHYSGUN_WORLD_STICK))
		{
			SetCollisionGroup(COLLISION_GROUP_INTERACTIVE_DEBRIS);
		}
	}

	m_OnPhysGunPickup.FireOutput(pPhysGunUser, this);

	if (reason == PICKED_UP_BY_CANNON)
	{
		m_OnPhysGunOnlyPickup.FireOutput(pPhysGunUser, this);
	}

	if (reason == PUNTED_BY_CANNON)
	{
		m_OnPhysGunPunt.FireOutput(pPhysGunUser, this);
	}

	if (reason == PICKED_UP_BY_CANNON || reason == PICKED_UP_BY_PLAYER)
	{
		m_OnPlayerPickup.FireOutput(pPhysGunUser, this);
	}

	CheckRemoveRagdolls();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CProjectileProp::OnPhysGunPull(CBasePlayer* pPhysGunUser) {
	m_OnPhysGunPull.FireOutput(pPhysGunUser, this);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CProjectileProp::OnPhysGunDrop(CBasePlayer* pPhysGunUser, PhysGunDrop_t Reason)
{
	BaseClass::OnPhysGunDrop(pPhysGunUser, Reason);

	if (Reason == LAUNCHED_BY_CANNON)
	{
		if (HasInteraction(PROPINTER_PHYSGUN_LAUNCH_SPIN_Z))
		{
			AngularImpulse angVel(0, 0, 5000.0);
			VPhysicsGetObject()->AddVelocity(NULL, &angVel);

			// no angular drag on this object anymore
			float angDrag = 0.0f;
			VPhysicsGetObject()->SetDragCoefficient(NULL, &angDrag);
		}

		PhysSetGameFlags(VPhysicsGetObject(), FVPHYSICS_WAS_THROWN);
		m_bFirstCollisionAfterLaunch = true;
	}
	else if (Reason == THROWN_BY_PLAYER)
	{
		// Remember the player threw us for NPC response purposes
		m_bThrownByPlayer = true;
	}

	m_OnPhysGunDrop.FireOutput(pPhysGunUser, this);

	if (HasInteraction(PROPINTER_PHYSGUN_NOTIFY_CHILDREN))
	{
		CUtlVector<CBaseEntity*> children;
		GetAllChildren(this, children);
		for (int i = 0; i < children.Count(); i++)
		{
			CBaseEntity* pent = children.Element(i);

			IParentPropInteraction* pPropInter = dynamic_cast<IParentPropInteraction*>(pent);
			if (pPropInter)
			{
				pPropInter->OnParentPhysGunDrop(pPhysGunUser, Reason);
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get the specified key's angles for this prop from the QC's physgun_interactions
//-----------------------------------------------------------------------------
bool CProjectileProp::GetPropDataAngles(const char* pKeyName, QAngle& vecAngles)
{
	KeyValues* modelKeyValues = new KeyValues("");
	if (modelKeyValues->LoadFromBuffer(modelinfo->GetModelName(GetModel()), modelinfo->GetModelKeyValueText(GetModel())))
	{
		KeyValues* pkvPropData = modelKeyValues->FindKey("physgun_interactions");
		if (pkvPropData)
		{
			char const* pszBase = pkvPropData->GetString(pKeyName);
			if (pszBase && pszBase[0])
			{
				UTIL_StringToVector(vecAngles.Base(), pszBase);
				modelKeyValues->deleteThis();
				return true;
			}
		}
	}

	modelKeyValues->deleteThis();
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CProjectileProp::GetCarryDistanceOffset(void)
{
	KeyValues* modelKeyValues = new KeyValues("");
	if (modelKeyValues->LoadFromBuffer(modelinfo->GetModelName(GetModel()), modelinfo->GetModelKeyValueText(GetModel())))
	{
		KeyValues* pkvPropData = modelKeyValues->FindKey("physgun_interactions");
		if (pkvPropData)
		{
			float flDistance = pkvPropData->GetFloat("carry_distance_offset", 0);
			modelKeyValues->deleteThis();
			return flDistance;
		}
	}

	modelKeyValues->deleteThis();
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CProjectileProp::ObjectCaps()
{
	int caps = BaseClass::ObjectCaps() | FCAP_WCEDIT_POSITION;

	if (HasSpawnFlags(SF_PHYSPROP_ENABLE_PICKUP_OUTPUT))
	{
		caps |= FCAP_IMPULSE_USE;
	}
	else if (CBasePlayer::CanPickupObject(this, 35, 128))
	{
		caps |= FCAP_IMPULSE_USE;

		if (hl2_episodic.GetBool() && HasInteraction(PROPINTER_PHYSGUN_CREATE_FLARE))
		{
			caps |= FCAP_USE_IN_RADIUS;
		}
	}

	if (HasSpawnFlags(SF_PHYSPROP_RADIUS_PICKUP))
	{
		caps |= FCAP_USE_IN_RADIUS;
	}

	return caps;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pActivator - 
//			*pCaller - 
//			useType - 
//			value - 
//-----------------------------------------------------------------------------
void CProjectileProp::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	CBasePlayer* pPlayer = ToBasePlayer(pActivator);
	if (pPlayer)
	{
		if (HasSpawnFlags(SF_PHYSPROP_ENABLE_PICKUP_OUTPUT))
		{
			m_OnPlayerUse.FireOutput(this, this);
		}

		pPlayer->PickupObject(this);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPhysics - 
//-----------------------------------------------------------------------------
void CProjectileProp::VPhysicsUpdate(IPhysicsObject* pPhysics)
{
	BaseClass::VPhysicsUpdate(pPhysics);

	if (!IsInWorld())
	{
		m_OnOutOfWorld.FireOutput(this, this);
		UTIL_Remove(this);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CProjectileProp::ClearFlagsThink(void)
{
	// collision may have destroyed the physics object, recheck
	if (VPhysicsGetObject())
	{
		PhysClearGameFlags(VPhysicsGetObject(), FVPHYSICS_WAS_THROWN);
	}
	SetContextThink(NULL, 0, "PROP_CLEARFLAGS");
}


//-----------------------------------------------------------------------------
// Compute impulse to apply to the enabled entity.
//-----------------------------------------------------------------------------
void CProjectileProp::ComputeEnablingImpulse(int index, gamevcollisionevent_t* pEvent)
{
	// Surface speed of the object that hit us = v + w x r
	// NOTE: w is specified in local space
	Vector vecContactPoint, vecLocalContactPoint;
	pEvent->pInternalData->GetContactPoint(vecContactPoint);

	// Compute the angular component of velocity
	IPhysicsObject* pImpactObject = pEvent->pObjects[!index];
	pImpactObject->WorldToLocal(&vecLocalContactPoint, vecContactPoint);
	vecLocalContactPoint -= pImpactObject->GetMassCenterLocalSpace();

	Vector vecLocalContactVelocity, vecContactVelocity;

	AngularImpulse vecAngularVelocity = pEvent->preAngularVelocity[!index];
	vecAngularVelocity *= M_PI / 180.0f;
	CrossProduct(vecAngularVelocity, vecLocalContactPoint, vecLocalContactVelocity);
	pImpactObject->LocalToWorldVector(&vecContactVelocity, vecLocalContactVelocity);

	// Add in the center-of-mass velocity
	vecContactVelocity += pEvent->preVelocity[!index];

	// Compute the force + torque to apply
	vecContactVelocity *= pImpactObject->GetMass();

	Vector vecForce;
	AngularImpulse vecTorque;
	pEvent->pObjects[index]->CalculateForceOffset(vecContactVelocity, vecContactPoint, &vecForce, &vecTorque);

	PhysCallbackImpulse(pEvent->pObjects[index], vecForce, vecTorque);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CProjectileProp::VPhysicsCollision(int index, gamevcollisionevent_t* pEvent)
{
	BaseClass::VPhysicsCollision(index, pEvent);

	IPhysicsObject* pPhysObj = pEvent->pObjects[!index];

	if (m_flForceToEnableMotion)
	{
		CBaseEntity* pOther = static_cast<CBaseEntity*>(pPhysObj->GetGameData());

		// Don't allow the player to bump an object active if we've requested not to
		if ((pOther && pOther->IsPlayer() && HasSpawnFlags(SF_PHYSPROP_PREVENT_PLAYER_TOUCH_ENABLE)) == false)
		{
			// Large enough to enable motion?
			float flForce = pEvent->collisionSpeed * pPhysObj->GetMass();

			if (flForce >= m_flForceToEnableMotion)
			{
				ComputeEnablingImpulse(index, pEvent);
				EnableMotion();
				m_flForceToEnableMotion = 0;
			}
		}
	}

	if (m_bFirstCollisionAfterLaunch)
	{
		HandleFirstCollisionInteractions(index, pEvent);
	}

	if (HasPhysicsAttacker(2.0f))
	{
		HandleAnyCollisionInteractions(index, pEvent);
	}

	if (!HasSpawnFlags(SF_PHYSPROP_DONT_TAKE_PHYSICS_DAMAGE))
	{
		int damageType = 0;

		IBreakableWithPropData* pBreakableInterface = assert_cast<IBreakableWithPropData*>(this);
		float damage = CalculateDefaultPhysicsDamage(index, pEvent, m_impactEnergyScale, true, damageType, pBreakableInterface->GetPhysicsDamageTable());
		if (damage > 0)
		{
			// Take extra damage after we're punted by the physcannon
			if (m_bFirstCollisionAfterLaunch && !m_bThrownByPlayer)
			{
				damage *= 10;
			}

			CBaseEntity* pHitEntity = pEvent->pEntities[!index];
			if (!pHitEntity)
			{
				// hit world
				pHitEntity = GetContainingEntity(INDEXENT(0));
			}
			Vector damagePos;
			pEvent->pInternalData->GetContactPoint(damagePos);
			Vector damageForce = pEvent->postVelocity[index] * pEvent->pObjects[index]->GetMass();
			if (damageForce == vec3_origin)
			{
				// This can happen if this entity is motion disabled, and can't move.
				// Use the velocity of the entity that hit us instead.
				damageForce = pEvent->postVelocity[!index] * pEvent->pObjects[!index]->GetMass();
			}

			// FIXME: this doesn't pass in who is responsible if some other entity "caused" this collision
			PhysCallbackDamage(this, CTakeDamageInfo(pHitEntity, pHitEntity, damageForce, damagePos, damage, damageType), *pEvent, index);
		}
	}

	if (m_bThrownByPlayer || m_bFirstCollisionAfterLaunch)
	{
		// If we were thrown by a player, and we've hit an NPC, let the NPC know
		CBaseEntity* pHitEntity = pEvent->pEntities[!index];
		if (pHitEntity && pHitEntity->MyNPCPointer())
		{
			pHitEntity->MyNPCPointer()->DispatchInteraction(g_interactionHitByPlayerThrownPhysObj, this, NULL);
			m_bThrownByPlayer = false;
		}
	}

	if (m_bFirstCollisionAfterLaunch)
	{
		m_bFirstCollisionAfterLaunch = false;

		// Setup the think function to remove the flags
		RegisterThinkContext("PROP_CLEARFLAGS");
		SetContextThink(&CProjectileProp::ClearFlagsThink, gpGlobals->curtime, "PROP_CLEARFLAGS");
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CProjectileProp::OnTakeDamage(const CTakeDamageInfo& info)
{
	// note: if motion is disabled, OnTakeDamage can't apply physics force
	int ret = BaseClass::OnTakeDamage(info);

	if (IsOnFire())
	{
		if ((info.GetDamageType() & DMG_BURN) && (info.GetDamageType() & DMG_DIRECT))
		{
			// Burning! scare things in my path if I'm moving.
			Vector vel;

			if (VPhysicsGetObject())
			{
				VPhysicsGetObject()->GetVelocity(&vel, NULL);

				int dangerRadius = 256; // generous radius to begin with

				if (hl2_episodic.GetBool())
				{
					// In Episodic, burning items (such as destroyed APCs) are making very large
					// danger sounds which frighten NPCs. This danger sound was designed to frighten
					// NPCs away from burning objects that are about to explode (barrels, etc). 
					// So if this item has no more health (ie, has died but hasn't exploded), 
					// make a smaller danger sound, just to keep NPCs away from the flames. 
					// I suspect this problem didn't appear in HL2 simply because we didn't have 
					// NPCs in such close proximity to destroyed NPCs. (sjb)
					if (GetHealth() < 1)
					{
						// This item has no health, but still exists. That means that it may keep
						// burning, but isn't likely to explode, so don't frighten over such a large radius.
						dangerRadius = 120;
					}
				}

				trace_t tr;
				UTIL_TraceLine(WorldSpaceCenter(), WorldSpaceCenter() + vel, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);
				CSoundEnt::InsertSound(SOUND_DANGER, tr.endpos, dangerRadius, 1.0, this, SOUNDENT_CHANNEL_REPEATED_DANGER);
			}
		}
	}

	// If we have a force to enable motion, and we're still disabled, check to see if this should enable us
	if (m_flForceToEnableMotion)
	{
		// Large enough to enable motion?
		float flForce = info.GetDamageForce().Length();
		if (flForce >= m_flForceToEnableMotion)
		{
			EnableMotion();
			m_flForceToEnableMotion = 0;
		}
	}

	// Check our health against the threshold:
	if (m_damageToEnableMotion > 0 && GetHealth() < m_damageToEnableMotion)
	{
		// only do this once
		m_damageToEnableMotion = 0;

		// The damage that enables motion may have been enough damage to kill me if I'm breakable
		// in which case my physics object is gone.
		if (VPhysicsGetObject() != NULL)
		{
			EnableMotion();
			VPhysicsTakeDamage(info);
		}
	}

	return ret;
}


//-----------------------------------------------------------------------------
// Mass / mass center
//-----------------------------------------------------------------------------
void CProjectileProp::GetMassCenter(Vector* pMassCenter)
{
	if (!VPhysicsGetObject())
	{
		pMassCenter->Init();
		return;
	}

	Vector vecLocal = VPhysicsGetObject()->GetMassCenterLocalSpace();
	VectorTransform(vecLocal, EntityToWorldTransform(), *pMassCenter);
}

float CProjectileProp::GetMass() const
{
	return VPhysicsGetObject() ? VPhysicsGetObject()->GetMass() : 1.0f;
}


//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CProjectileProp::DrawDebugTextOverlays(void)
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT)
	{
		if (VPhysicsGetObject())
		{
			char tempstr[512];
			Q_snprintf(tempstr, sizeof(tempstr), "Mass: %.2f kg / %.2f lb (%s)", VPhysicsGetObject()->GetMass(), kg2lbs(VPhysicsGetObject()->GetMass()), GetMassEquivalent(VPhysicsGetObject()->GetMass()));
			EntityText(text_offset, tempstr, 0);
			text_offset++;

			{
				vphysics_objectstress_t stressOut;
				float stress = CalculateObjectStress(VPhysicsGetObject(), this, &stressOut);
				Q_snprintf(tempstr, sizeof(tempstr), "Stress: %.2f (%.2f / %.2f)", stress, stressOut.exertedStress, stressOut.receivedStress);
				EntityText(text_offset, tempstr, 0);
				text_offset++;
			}

			if (!VPhysicsGetObject()->IsMoveable())
			{
				Q_snprintf(tempstr, sizeof(tempstr), "Motion Disabled");
				EntityText(text_offset, tempstr, 0);
				text_offset++;
			}

			if (m_iszBasePropData != NULL_STRING)
			{
				Q_snprintf(tempstr, sizeof(tempstr), "Base PropData: %s", STRING(m_iszBasePropData));
				EntityText(text_offset, tempstr, 0);
				text_offset++;
			}

			if (m_iNumBreakableChunks != 0)
			{
				IBreakableWithPropData* pBreakableInterface = assert_cast<IBreakableWithPropData*>(this);
				Q_snprintf(tempstr, sizeof(tempstr), "Breakable Chunks: %d (Max Size %d)", m_iNumBreakableChunks, pBreakableInterface->GetMaxBreakableSize());
				EntityText(text_offset, tempstr, 0);
				text_offset++;
			}

			Q_snprintf(tempstr, sizeof(tempstr), "Skin: %d", m_nSkin.Get());
			EntityText(text_offset, tempstr, 0);
			text_offset++;

			Q_snprintf(tempstr, sizeof(tempstr), "Health: %d, collision group %d", GetHealth(), GetCollisionGroup());
			EntityText(text_offset, tempstr, 0);
			text_offset++;
		}
	}

	return text_offset;
}