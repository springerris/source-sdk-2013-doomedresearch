#pragma once
#include "Sprite.h"
#include "SpriteTrail.h"

class CPlasmaSprite : public CSprite
{
	DECLARE_CLASS(CPlasmaSprite, CSprite);
	DECLARE_DATADESC();
public:
	void Precache(void);
	static CPlasmaSprite* SpriteCreate(const char* pSpriteName, const Vector& origin, bool animate);
	void BallTouch(CBaseEntity* pOther);
};

class CBulletSprite : public CSpriteTrail
{
	DECLARE_CLASS(CBulletSprite, CSpriteTrail);
	DECLARE_DATADESC();
public:
	void Precache(void);
	static CBulletSprite* SpriteTrailCreate(const char* pSpriteName, const Vector& origin, bool animate);
	void BallTouch(CBaseEntity* pOther);
};