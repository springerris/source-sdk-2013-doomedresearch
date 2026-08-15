//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================//
#if !defined( C_BASEHLPLAYER_H )
#define C_BASEHLPLAYER_H
#ifdef _WIN32
#pragma once
#endif


#include "c_baseplayer.h"
#include "c_hl2_playerlocaldata.h"

#if !defined( HL2MP ) && defined ( MAPBASE )
#include "mapbase/mapbase_playeranimstate.h"
#endif

class C_BossEncounterDrawData {
	DECLARE_SIMPLE_DATADESC();
	// Prediction data copying
	DECLARE_CLASS_NOBASE(C_BossEncounterDrawData);
	DECLARE_EMBEDDED_NETWORKVAR();
public:
	char percent;
	char title[128];
	C_BossEncounterDrawData(char inTitle[128], char inPercent) {
		percent = inPercent;
		Q_strncpy(title, inTitle, sizeof(inTitle));
	}
	C_BossEncounterDrawData() {
		percent = 0;
		Q_strncpy(title, "", sizeof(""));
	}
};


class C_BaseHLPlayer : public C_BasePlayer
{
public:
	DECLARE_CLASS( C_BaseHLPlayer, C_BasePlayer );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

						C_BaseHLPlayer();
	//C_BossEncounterDrawData* GetBossEncounterDrawData() { return m_bossDrawData; };
	float* GetBossEncounterDrawP() { return m_bossDrawDataP; }
	float* GetBossEncounterDrawScale() { return m_bossDrawDataScale; }
	int* GetBossEncounterDrawPos() { return m_bossDrawDataPos; }
	int GetBossCount() { return m_iBossCount; }
	virtual void		OnDataChanged( DataUpdateType_t updateType );
	virtual void		AddEntity( void );

	void				Weapon_DropPrimary( void );
		
	float				GetFOV();
	void				Zoom( float FOVOffset, float time );
	float				GetZoom( void );
	bool				IsZoomed( void )	{ return m_HL2Local.m_bZooming; }

	//Tony; minor cosmetic really, fix confusion by simply renaming this one; everything calls IsSprinting(), and this isn't really even used.
	bool				IsSprintActive( void ) { return m_HL2Local.m_bitsActiveDevices & bits_SUIT_DEVICE_SPRINT; }
	bool				IsFlashlightActive( void ) { return m_HL2Local.m_bitsActiveDevices & bits_SUIT_DEVICE_FLASHLIGHT; }
	bool				IsBreatherActive( void ) { return m_HL2Local.m_bitsActiveDevices & bits_SUIT_DEVICE_BREATHER; }

#ifdef MAPBASE
	bool				IsCustomDevice0Active( void ) { return m_HL2Local.m_bitsActiveDevices & bits_SUIT_DEVICE_CUSTOM0; }
	bool				IsCustomDevice1Active( void ) { return m_HL2Local.m_bitsActiveDevices & bits_SUIT_DEVICE_CUSTOM1; }
	bool				IsCustomDevice2Active( void ) { return m_HL2Local.m_bitsActiveDevices & bits_SUIT_DEVICE_CUSTOM2; }
#endif

	virtual int			DrawModel( int flags );
	virtual	void		BuildTransformations( CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed );

	LadderMove_t		*GetLadderMove() { return &m_HL2Local.m_LadderMove; }
	virtual void		ExitLadder();
	bool				IsSprinting() const { return m_fIsSprinting; }
	
	// Input handling
	virtual bool	CreateMove( float flInputSampleTime, CUserCmd *pCmd );
	void			PerformClientSideObstacleAvoidance( float flFrameTime, CUserCmd *pCmd );
	void			PerformClientSideNPCSpeedModifiers( float flFrameTime, CUserCmd *pCmd );

	bool				IsWeaponLowered( void ) { return m_HL2Local.m_bWeaponLowered; }

#ifdef MAPBASE
	int				GetProtagonistIndex() const { return m_nProtagonistIndex; }
#endif

#ifdef SP_ANIM_STATE
	virtual const Vector&	GetRenderOrigin();
	virtual const QAngle&	GetRenderAngles( void );
	virtual CStudioHdr		*OnNewModel();
#endif

public:
	//CUtlVector<C_BossEncounterDrawData> m_bossList;
	C_HL2PlayerLocalData		m_HL2Local;
	EHANDLE				m_hClosestNPC;
	float				m_flSpeedModTime;
	bool				m_fIsSprinting;

private:
	C_BaseHLPlayer( const C_BaseHLPlayer & ); // not defined, not accessible
	
	bool				TestMove( const Vector &pos, float fVertDist, float radius, const Vector &objPos, const Vector &objDir );

	float				m_flZoomStart;
	float				m_flZoomEnd;
	float				m_flZoomRate;
	float				m_flZoomStartTime;

	bool				m_bPlayUseDenySound;		// Signaled by PlayerUse, but can be unset by HL2 ladder code...
	float				m_flSpeedMod;
	float				m_flExitSpeedMod;
public: 
	char m_bossDrawDataTitle[MAX_BOSSES_DISPLAYED][64];
	char m_bossDrawDataTitle1[64];
	char m_bossDrawDataTitle2[64];
	char m_bossDrawDataTitle3[64];
	char m_bossDrawDataTitle4[64];
protected:
	float m_bossDrawDataP[MAX_BOSSES_DISPLAYED];
	float m_bossDrawDataScale[MAX_BOSSES_DISPLAYED];
	int m_bossDrawDataPos[MAX_BOSSES_DISPLAYED];
	
	int 	m_iBossCount;

#ifdef MAPBASE
	int					m_nProtagonistIndex;
	//C_BossEncounterDrawData 	m_bossDrawData[MAX_BOSSES_DISPLAYED];


#endif
	
#ifdef MAPBASE_MP
	CSinglePlayerAnimState *m_pPlayerAnimState;
#elif MAPBASE
	// At the moment, we network the render angles since almost none of the player anim stuff is done on the client in SP.
	// If any of this is ever adapted for MP, this method should be replaced with replicating/moving the anim state to the client.
	float				m_flAnimRenderYaw;
	float				m_flAnimRenderZ;
	QAngle				m_angAnimRender;
#endif

friend class CHL2GameMovement;
};


#endif
