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

void ActiveEffectHandler::ProcessValueModifier(
	RE::Actor*     a_target,
	RE::ActorValue a_actorValue,
	float          a_magnitudeDelta,
	RE::Actor*     a_aggressor)
{
	if (!a_target ||
		a_target == a_aggressor ||
		(!a_aggressor && a_magnitudeDelta >= 0) ||
		std::abs(a_magnitudeDelta) <= 0.001f) {
		return;
	}

	auto settings = Settings::GetSingleton();

	float effectMultiplier = CalculateEffectMultiplier(a_actorValue, a_magnitudeDelta > 0);

	if (effectMultiplier <= 0.0f) {
		return;
	}

	auto poiseAV = PoiseAV::GetSingleton();

	if (!poiseAV->CanDamageActor(a_target)) {
		return;
	}

	float poiseDamage = effectMultiplier * a_magnitudeDelta;

	if (poiseDamage <= 0.0f) {
		return;
	}

	float baseMult = 1.0f;

	// Apply perk modifiers
	if (a_aggressor) {
		PoiseAV::ApplyPerkEntryPoint(
			34,
			a_aggressor->As<RE::Character>(),
			a_target->As<RE::Character>(),
			&baseMult);

		PoiseAV::ApplyPerkEntryPoint(
			33,
			a_target->As<RE::Character>(),
			a_aggressor->As<RE::Character>(),
			&baseMult);

		poiseDamage *= baseMult;
	}

	// Soft cap extreme magic spikes
	float preClampDamage = poiseDamage;

	if (poiseDamage > 50.0f) {
		float excessDamage = poiseDamage - 50.0f;
		poiseDamage = 50.0f + (excessDamage * 0.5f);
	}

	float preResistDamage = poiseDamage;

	// Apply magic resistance
	poiseDamage = ApplyMagicPoiseResistance(a_target, poiseDamage);

	float preLevelDamage = poiseDamage;

	// Apply level scaling
	if (a_aggressor) {
		poiseDamage *= settings->GetDamageMultiplier(a_aggressor, a_target);
	}

	float preDifficultyDamage = poiseDamage;

	// Apply difficulty scaling
	// poiseDamage *= settings->GetDifficultyMultiplier();

	// Prevent meaningless zero damage ticks
	poiseDamage = (std::max)(poiseDamage, 0.075f);

	if (settings->Debug.LogMagicEffectCalcs && poiseDamage >= 1.0f) {
		logger::info(
			"[Magic Effect Poise] Target={}({:08X}) Aggressor={}({:08X}) AV={} "
			"RawMagnitude={} EffectMultiplier={} BaseMult={} "
			"PreClamp={} PreResist={} PreLevel={} PreDifficulty={} Final={}",
			a_target->GetName(),
			a_target->GetFormID(),
			a_aggressor ? a_aggressor->GetName() : "NULL",
			a_aggressor ? a_aggressor->GetFormID() : 0,
			std::string(magic_enum::enum_name(a_actorValue)),
			a_magnitudeDelta,
			effectMultiplier,
			baseMult,
			preClampDamage,
			preResistDamage,
			preLevelDamage,
			preDifficultyDamage,
			poiseDamage);
	}

	poiseAV->DamageAndCheckPoise(a_target, a_aggressor, poiseDamage);
}

float ActiveEffectHandler::ApplyMagicPoiseResistance(RE::Actor* a_target, float a_damage)
{
	if (!a_target) {
		return a_damage;
	}

	auto settings = Settings::GetSingleton();

	auto avOwner = a_target->AsActorValueOwner();

	float magicResist =
		avOwner->GetActorValue(RE::ActorValue::kResistMagic);

	float fireResist =
		avOwner->GetActorValue(RE::ActorValue::kResistFire);

	float frostResist =
		avOwner->GetActorValue(RE::ActorValue::kResistFrost);

	float shockResist =
		avOwner->GetActorValue(RE::ActorValue::kResistShock);

	// Resist Magic = 65% of total resistance
	// Elemental resistances combined = 35% of total resistance
	float elementalAverage =
		(fireResist + frostResist + shockResist) / 3.0f;

	float effectiveResist =
		(magicResist * 0.65f) +
		(elementalAverage * 0.35f);

	effectiveResist = std::clamp(effectiveResist, -100.0f, 100.0f);

	float reduction = 0.0f;
	float finalMult = 1.0f;

	if (effectiveResist >= 0.0f) {
		// Smooth diminishing returns curve
		// Prevents low resistance values from becoming extreme reductions.
		float normalizedResist =
			effectiveResist / 100.0f;

		float curveStrength =
			1.5f * settings->Magic.ResistanceMult;

		float r1 =
			normalizedResist * curveStrength;

		reduction =
			r1 / (1.0f + r1);

		finalMult =
			1.0f - reduction;
	} else {
		// Negative resistance increases poise damage
		float weakness =
			std::abs(effectiveResist) / 100.0f;

		finalMult =
			1.0f + weakness;

		reduction =
			-weakness;
	}

	if (settings->Debug.LogMagicEffectCalcs && a_damage >= 1.0f) {
		logger::info(
			"[Magic Poise Resist] Target={} Magic={} Fire={} Frost={} Shock={} Effective={} Reduction={} Mult={} Before={} After={}",
			a_target->GetName(),
			magicResist,
			fireResist,
			frostResist,
			shockResist,
			effectiveResist,
			reduction,
			finalMult,
			a_damage,
			a_damage * finalMult);
	}

	return a_damage * finalMult;
}