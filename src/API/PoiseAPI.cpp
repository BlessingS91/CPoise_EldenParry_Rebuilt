#include "PoiseAPI.h"

#include "Hooks/ActiveEffectHandler.h"
#include "Hooks/HitEventHandler.h"

POISE_API float Poise_GetArmorReducedStagger(
	uint32_t formID,
	float    stagger)
{
	auto* dataHandler = RE::TESDataHandler::GetSingleton();

	if (!dataHandler || stagger <= 0.0f) {
		return 0.0f;
	}

	auto* form = RE::TESForm::LookupByID(formID);

	if (!form) {
		return 0.0f;
	}

	auto* actor = form->As<RE::Actor>();

	if (!actor) {
		return 0.0f;
	}

	float reducedStagger =
		HitEventHandler::GetSingleton()->ApplyArmorReduction(
			actor,
			stagger);

	return std::clamp(
		((stagger - reducedStagger) / stagger) * 100.0f,
		0.0f,
		100.0f);
}

POISE_API float Poise_GetEffectiveMagicResistance(
	RE::Actor* a_target)
{
	return ActiveEffectHandler::GetSingleton()->GetEffectiveMagicResistance(
		a_target);
}

POISE_API float Poise_GetHandDamage(
	uint32_t formID,
	bool     a_leftHand)
{
	auto* form = RE::TESForm::LookupByID(formID);

	if (!form) {
		return 0.0f;
	}

	auto* actor = form->As<RE::Actor>();

	if (!actor) {
		return 0.0f;
	}

	return HitEventHandler::GetSingleton()->GetHandDamage(
		actor,
		a_leftHand);
}