#pragma once

#include "basegrenade_shared.h"
class CParticleSystem;

enum DRGrenade_t
{
	SLIME,
	DRGrenade_t_size
};

static char* DRGrenade_t_mapped[DRGrenade_t_size] = { "SLIME" };


class CBaseGrenadeDR : public CBaseGrenade
{
	DECLARE_CLASS(CBaseGrenadeDR, CBaseGrenade);

public:
	CBaseGrenadeDR(void);


	static bool DRGetGrenadeVector(CBaseEntity* pEdict, const Vector& vecStartPos, const Vector& vecTarget, Vector* vecOut, float flSpeed, bool bcanFail);
	static Vector DRVecCheckThrowTolerance(CBaseEntity* pEdict, const Vector& vecSpot1, Vector vecSpot2, float flSpeed, float flTolerance, bool bcanFail);
	static void DRLaunchGrenadeAtTarget(CBaseEntity* pEdict, const Vector& vecSpot1, Vector vecSpot2, float flSpeed, float flTolerance, DRGrenade_t grenadeT, bool bcanFail);


	virtual void		Spawn(void) override;
	virtual void		Precache(void) override;
	virtual void		Event_Killed(const CTakeDamageInfo& info) override;

	virtual	unsigned int	PhysicsSolidMaskForEntity(void) const { return (BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_WATER); }

	void				Detonate(void) override;
	void				Think(void) override;

private:
	DECLARE_DATADESC();

	void	InitHissSound(void);

	CHandle< CParticleSystem >	m_hSpitEffect;
	CSoundPatch* m_pHissSound;
	bool			m_bPlaySound;
};






