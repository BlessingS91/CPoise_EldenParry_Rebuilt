#pragma once

#include <cstdint>

#include "RE/Skyrim.h"

#ifdef ChocolatePoise_EXPORTS
#	define POISE_API __declspec(dllexport)
#else
#	define POISE_API __declspec(dllimport)
#endif

extern "C"
{
	POISE_API float Poise_GetArmorReducedStagger(
		uint32_t formID,
		float    stagger);

	POISE_API float Poise_GetEffectiveMagicResistance(
		RE::Actor* a_target);

	POISE_API float Poise_GetHandDamage(
		uint32_t formID,
		bool     a_leftHand);
}

using Poise_GetArmorReducedStagger_t = float (*)(uint32_t, float);
using Poise_GetEffectiveMagicResistance_t = float (*)(RE::Actor*);
using Poise_GetHandDamage_t = float (*)(uint32_t, bool);