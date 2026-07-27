#include "Hooks/ActiveEffectHandler.h"

#include "Hooks/PoiseAV.h"
#include "Storage/Settings.h"

float ActiveEffectHandler::CalculateEffectMultiplier(RE::ActorValue a_actorValue, bool a_detrimental)
{
	auto        settings = Settings::GetSingleton();
	std::string sEffectType = a_detrimental ? "Damage" : "Recovery";
	auto        baseAVString = std::string(magic_enum::enum_name(a_actorValue));
	if (baseAVString.size() != 0) {
		baseAVString = baseAVString.substr(1);
		auto actorValue = settings->JSONSettings["Magic Effects"]["Actor Values"][sEffectType][baseAVString];

		if (actorValue != nullptr)
			return static_cast<float>(actorValue);
	}

	return 0;
}

void ActiveEffectHandler::ProcessValueModifier(RE::Actor* a_target, RE::ActorValue a_actorValue, float a_magnitudeDelta, RE::Actor* a_aggressor)
{
	if (!a_target ||
		a_target == a_aggressor ||
		(!a_aggressor && a_magnitudeDelta >= 0) ||
		std::abs(a_magnitudeDelta) <= 0.001f) {
		return;
	}

	float effectMultiplier = CalculateEffectMultiplier(a_actorValue, a_magnitudeDelta > 0);

	if (effectMultiplier <= 0.0f) {
		return;
	}

	auto poiseAV = PoiseAV::GetSingleton();
	auto settings = Settings::GetSingleton();

	if (!poiseAV->CanDamageActor(a_target)) {
		return;
	}
	float poiseDamage = effectMultiplier * a_magnitudeDelta;

	if (poiseDamage <= 0.0f) {
		return;
	}

	float rawDifficultyMult = 1.0f;
	float difficultyMult = 1.0f;
	float baseMult = 1.0f;

	if (a_aggressor) {
		rawDifficultyMult = settings->GetDamageMultiplier(a_aggressor, a_target);
		difficultyMult = 1.0f + (rawDifficultyMult - 1.0f) * settings->Damage.PoiseScaling;

		PoiseAV::ApplyPerkEntryPoint(34, a_aggressor->As<RE::Character>(), a_target->As<RE::Character>(), &baseMult);
		PoiseAV::ApplyPerkEntryPoint(33, a_target->As<RE::Character>(), a_aggressor->As<RE::Character>(), &baseMult);

		poiseDamage *= baseMult;
		poiseDamage *= difficultyMult;

		if (a_target->IsPlayerRef())
			poiseDamage *= settings->Damage.ToPCMult;
		else
			poiseDamage *= settings->Damage.ToNPCMult;
	}

	if (poiseDamage <= 0.0f) {
		return;
	}

	if (settings->Debug.LogMagicEffectCalcs) {
		logger::info(
			"[Magic Effect Poise] Target={}({:08X}) Aggressor={}({:08X}) AV={} Type={} RawMagnitude={} EffectMultiplier={} BaseMult={} DifficultyRaw={} DifficultyFinal={} PoiseScaling={} FinalDamage={}",
			a_target->GetName(),
			a_target->GetFormID(),
			a_aggressor ? a_aggressor->GetName() : "NULL",
			a_aggressor ? a_aggressor->GetFormID() : 0,
			std::string(magic_enum::enum_name(a_actorValue)),
			a_magnitudeDelta > 0 ? "Damage" : "Recovery",
			a_magnitudeDelta,
			effectMultiplier,
			baseMult,
			rawDifficultyMult,
			difficultyMult,
			settings->Damage.PoiseScaling,
			poiseDamage);
	}
	poiseAV->DamageAndCheckPoise(a_target, a_aggressor, poiseDamage);
}
