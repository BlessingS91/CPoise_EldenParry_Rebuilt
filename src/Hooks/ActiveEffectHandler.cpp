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
		float poiseDamage = CalculateEffectMultiplier(a_actorValue, a_magnitudeDelta > 0) * a_magnitudeDelta;

		if (a_aggressor) {
			float baseMult = 1.0f;
			PoiseAV::ApplyPerkEntryPoint(34, a_aggressor->As<RE::Character>(), a_target->As<RE::Character>(), &baseMult);
			PoiseAV::ApplyPerkEntryPoint(33, a_target->As<RE::Character>(), a_aggressor->As<RE::Character>(), &baseMult);
			poiseDamage *= baseMult;
			if (poiseDamage > 0) {
				poiseDamage *= settings->GetDamageMultiplier(a_aggressor, a_target);
				if (a_target != a_aggressor) {
					if (a_target->IsPlayerRef())
						poiseDamage *= settings->Damage.ToPCMult;
					else
						poiseDamage *= settings->Damage.ToNPCMult;
				}
			}
		}

		poiseAV->DamageAndCheckPoise(a_target, a_aggressor, poiseDamage);
	}
}

//Resistance Calcs
float ActiveEffectHandler::GetResistanceMultiplier(RE::Actor* a_target, RE::ActorValue a_actorValue)
{
	if (!a_target) {
		return 1.0f;
	}

	auto actorValues = a_target->AsActorValueOwner();

	float resistance = 0.0f;

	switch (a_actorValue) {
	case RE::ActorValue::kResistFire:
		resistance = actorValues->GetActorValue(RE::ActorValue::kResistFire);
		break;

	case RE::ActorValue::kResistFrost:
		resistance = actorValues->GetActorValue(RE::ActorValue::kResistFrost);
		break;

	case RE::ActorValue::kResistShock:
		resistance = actorValues->GetActorValue(RE::ActorValue::kResistShock);
		break;

	case RE::ActorValue::kPoisonResist:
		resistance = actorValues->GetActorValue(RE::ActorValue::kPoisonResist);
		break;

	case RE::ActorValue::kResistMagic:
		resistance = actorValues->GetActorValue(RE::ActorValue::kResistMagic);
		break;

	default:
		// Generic magic effects use Magic Resistance
		resistance = actorValues->GetActorValue(RE::ActorValue::kResistMagic);
		break;
	}

	resistance = std::clamp(resistance, 0.0f, 100.0f);

	return 1.0f - (resistance / 100.0f);
}