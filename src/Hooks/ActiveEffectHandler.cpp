#include "Hooks/ActiveEffectHandler.h"

#include "Hooks/PoiseAV.h"
#include "Storage/Settings.h"

float ActiveEffectHandler::CalculateEffectMultiplier(RE::ActorValue a_actorValue, bool a_detrimental)
{
	auto        settings = Settings::GetSingleton();
	std::string sEffectType = a_detrimental ? "Damage" : "Recovery";
	std::string baseAVString = std::string(magic_enum::enum_name(a_actorValue));
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
	auto poiseAV = PoiseAV::GetSingleton();
	auto settings = Settings::GetSingleton();

	if (a_target != a_aggressor && (a_aggressor || a_magnitudeDelta < 0) && poiseAV->CanDamageActor(a_target) && a_magnitudeDelta != 0) {
		float effectMultiplier = CalculateEffectMultiplier(a_actorValue, a_magnitudeDelta > 0);
		float poiseDamage = effectMultiplier * a_magnitudeDelta;
		float rawDifficultyMult = settings->GetDamageMultiplier(a_aggressor, a_target);
		float difficultyMult = 1.0f + (rawDifficultyMult - 1.0f) * settings->Damage.PoiseScaling;
		float baseMult = 1.0f;

		if (a_aggressor) {
			PoiseAV::ApplyPerkEntryPoint(34, a_aggressor->As<RE::Character>(), a_target->As<RE::Character>(), &baseMult);
			PoiseAV::ApplyPerkEntryPoint(33, a_target->As<RE::Character>(), a_aggressor->As<RE::Character>(), &baseMult);
			poiseDamage *= baseMult;
			if (poiseDamage > 0) {
				poiseDamage *= difficultyMult;

				if (a_target != a_aggressor) {
					if (a_target->IsPlayerRef())
						poiseDamage *= settings->Damage.ToPCMult;
					else
						poiseDamage *= settings->Damage.ToNPCMult;
				}
			}
		}

		logger::debug(
			"[Poise Damage] Target={} Aggressor={} AV={} RawDamage={} EffectMultiplier={} BaseMult={} RawDifficultyMult={} DifficultyMult={} PoiseScaling={} FinalPoiseDamage={}",
			a_target ? a_target->GetName() : "NULL",
			a_aggressor ? a_aggressor->GetName() : "NULL",
			std::string(magic_enum::enum_name(a_actorValue)),
			a_magnitudeDelta,
			effectMultiplier,
			baseMult,
			rawDifficultyMult,
			difficultyMult,
			settings->Damage.PoiseScaling,
			poiseDamage);

		poiseAV->DamageAndCheckPoise(a_target, a_aggressor, poiseDamage);
	}
}
