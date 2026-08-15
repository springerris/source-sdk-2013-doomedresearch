//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A special kind of beam effect that traces from its start position to
//			its end position and stops if it hits anything.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "EnvLaser.h"
#include "Sprite.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( env_laser, CEnvLaser );

BEGIN_DATADESC( CEnvLaser )

	DEFINE_KEYFIELD( m_iszLaserTarget, FIELD_STRING, "LaserTarget" ),
	DEFINE_FIELD( m_pSprite, FIELD_CLASSPTR ),
	DEFINE_KEYFIELD( m_iszSpriteName, FIELD_STRING, "EndSprite" ),
	DEFINE_KEYFIELD(m_vecLaserOrigin, FIELD_VECTOR,"LaserTargetCoords"),
	DEFINE_FIELD( m_firePosition, FIELD_VECTOR ),
	DEFINE_KEYFIELD( m_flStartFrame, FIELD_FLOAT, "framestart" ),
	DEFINE_KEYFIELD(m_flRealWidth, FIELD_FLOAT, "realwidth"),
	
	DEFINE_KEYFIELD(m_flLength, FIELD_FLOAT, "laserlength"),

	// Function Pointers
	DEFINE_FUNCTION( StrikeThink ),

	// Input functions
	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOn", InputTurnOn ),
	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOff", InputTurnOff ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Toggle", InputToggle ),

#ifdef MAPBASE
	DEFINE_OUTPUT( m_OnTouchedByEntity, "OnTouchedByEntity" ),
#endif

END_DATADESC()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaser::Spawn( void )
{
	if ( !GetModelName() )
	{
		SetThink( &CEnvLaser::SUB_Remove );
		return;
	}

	SetSolid( SOLID_NONE );							// Remove model & collisions
	SetThink( &CEnvLaser::StrikeThink );

	SetEndWidth( GetWidth() );				// Note: EndWidth is not scaled

	PointsInit( GetLocalOrigin(), GetLocalOrigin() );
	SetAbsAngles(GetLocalAngles());
	Precache( );

	if ( !m_pSprite && m_iszSpriteName != NULL_STRING )
	{
		m_pSprite = CSprite::SpriteCreate( STRING(m_iszSpriteName), GetAbsOrigin(), TRUE );
	}
	else
	{
		m_pSprite = NULL;
	}

	if ( m_pSprite )
	{
		m_pSprite->SetParent( GetMoveParent() );
		m_pSprite->SetTransparency( kRenderGlow, m_clrRender->r, m_clrRender->g, m_clrRender->b, m_clrRender->a, m_nRenderFX );
	}

	if ( GetEntityName() != NULL_STRING && !(m_spawnflags & SF_BEAM_STARTON) )
	{
		TurnOff();
	}
	else
	{
		TurnOn();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaser::Precache( void )
{
	SetModelIndex( PrecacheModel( STRING( GetModelName() ) ) );
	if ( m_iszSpriteName != NULL_STRING )
		PrecacheModel( STRING(m_iszSpriteName) );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEnvLaser::KeyValue( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "width"))
	{
		SetWidth( atof(szValue) );
	}
	else if (FStrEq(szKeyName, "NoiseAmplitude"))
	{
		SetNoise( atoi(szValue) );
	}
	else if (FStrEq(szKeyName, "TextureScroll"))
	{
		SetScrollRate( atoi(szValue) );
	}
	else if (FStrEq(szKeyName, "texture"))
	{
		SetModelName( AllocPooledString(szValue) );
	}
	else
	{
		BaseClass::KeyValue( szKeyName, szValue );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Returns whether the laser is currently active.
//-----------------------------------------------------------------------------
int CEnvLaser::IsOn( void )
{
	if ( IsEffectActive( EF_NODRAW ) )
		return 0;
	return 1;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaser::InputTurnOn( inputdata_t &inputdata )
{
	if (!IsOn())
	{
		TurnOn();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaser::InputTurnOff( inputdata_t &inputdata )
{
	if (IsOn())
	{
		TurnOff();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaser::InputToggle( inputdata_t &inputdata )
{
	if ( IsOn() )
	{
		TurnOff();
	}
	else
	{
		TurnOn();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaser::TurnOff( void )
{
	AddEffects( EF_NODRAW );
	if ( m_pSprite )
		m_pSprite->TurnOff();

	SetNextThink( TICK_NEVER_THINK );
	SetThink( NULL );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaser::TurnOn( void )
{
	RemoveEffects( EF_NODRAW );
	if ( m_pSprite )
		m_pSprite->TurnOn();

	m_flFireTime = gpGlobals->curtime;

	SetThink( &CEnvLaser::StrikeThink );

	//
	// Call StrikeThink here to update the end position, otherwise we will see
	// the beam in the wrong place for one frame since we cleared the nodraw flag.
	//
	StrikeThink();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaser::FireAtPoint( trace_t &tr )
{
	SetAbsEndPos( tr.endpos );
	if ( m_pSprite )
	{
		UTIL_SetOrigin( m_pSprite, tr.endpos );
	}

	// Apply damage and do sparks every 1/10th of a second.
	if ( gpGlobals->curtime >= m_flFireTime + 0.1 )
	{
#ifdef MAPBASE
		if ( tr.fraction != 1.0 && tr.m_pEnt && !tr.m_pEnt->IsWorld() )
		{
			m_OnTouchedByEntity.FireOutput( tr.m_pEnt, this );
		}
#endif
		BeamDamage( &tr );
		DoSparks( GetAbsStartPos(), tr.endpos );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaser::StrikeThink( void )
{
	CBaseEntity *pEnd = RandomTargetname( STRING( m_iszLaserTarget ) );
	// DR: if no targetname is specified, just fire the beam forward for the specified length

	Vector fwd;
	QAngle angles = GetAbsAngles();
	//Vector localOrigin = GetLocalOrigin();
	//localOrigin.x += m_flLength;
	
#ifdef DEBUG
	//if(gpGlobals->tickcount % 66 == 0)	DevMsg("BEAM %s %p ANGLES %f %f %f \n", this->GetEntityName().ToCStr(), this, angles.x, angles.y, angles.z);
#endif // DEBUG
	//AngleVectors(angles,&fwd);
	this->GetVectors(&fwd, NULL, NULL);
	Vector vecFireAt;
	//EntityToWorldSpace(localOrigin, &vecFireAt);
	//
	
	vecFireAt = this->GetAbsOrigin() + (fwd * m_flLength);

	if ( pEnd )
	{
#ifdef DEBUG
		if (gpGlobals->tickcount % 66 == 0)	DevMsg("BEAM NOT USING LENGTH");
#endif // DEBUG
		vecFireAt = pEnd->GetAbsOrigin();
	}
	//DebugDrawLine(GetAbsOrigin(), vecFireAt, 255, 5, 5, true, 0.5);
	trace_t tr;
#ifdef DEBUG
	/*
	if (gpGlobals->tickcount % 66 == 0) DevMsg("BEAM %s %p START %f %f %f \n", this->GetEntityName().ToCStr(), this, GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z);
	if (gpGlobals->tickcount % 66 == 0) DevMsg("BEAM %s %p FWD %f %f %f \n", this->GetEntityName().ToCStr(), this, fwd.x, fwd.y, fwd.z);
	if (gpGlobals->tickcount % 66 == 0) DevMsg("BEAM %s %p LENGTH %f \n", this->GetEntityName().ToCStr(), this, m_flLength);
	if (gpGlobals->tickcount % 66 == 0) DevMsg("BEAM %s %p END %f %f %f \n", this->GetEntityName().ToCStr(), this, vecFireAt.x, vecFireAt.y, vecFireAt.z);
	*/
#endif // DEBUG
	if (m_flRealWidth <= FLT_EPSILON) {
		UTIL_TraceLine(GetAbsOrigin(), vecFireAt, MASK_SOLID, NULL, COLLISION_GROUP_NONE, &tr);
	}
	else {
		Vector mins = Vector(-m_flRealWidth / 2.0, -m_flRealWidth / 2.0, -m_flRealWidth / 2.0);
		Vector maxs = Vector(m_flRealWidth / 2.0, m_flRealWidth / 2.0, m_flRealWidth / 2.0);
		UTIL_TraceHull(GetAbsOrigin() + fwd * m_flRealWidth, vecFireAt, mins, maxs, MASK_SOLID, this->GetOwnerEntity(), COLLISION_GROUP_NONE, &tr);
	}
	tr.startpos = tr.startpos - fwd * m_flRealWidth / 2;
	tr.endpos = tr.endpos + fwd * m_flRealWidth/2;
	FireAtPoint( tr );
	SetNextThink( gpGlobals->curtime );
}


