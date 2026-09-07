#include "Hooks/HitEventHandler.h"
#include "Hooks/ActiveEffectHandler.h"
#include "Hooks/PoiseAV.h"
#include "Storage/Settings.h"
#include <algorithm>
#undef min
#undef max
#include <limits>

float HitEventHandler::GetWeaponDamage(RE::TESObjectWEAP* a_weapon, bool ignoreWeight)
{
	if (!a_weapon || !_minWeapon || !_maxWeapon) {
		return a_weapon ? a_weapon->weight : 0.0f;  // Fallback if data isn't ready
	}

	auto  settings = Settings::GetSingleton();
	float weaponMult = 1.0f;

	//weapon type calc

	std::string weaponType = "Unknown";

	for (int index = a_weapon->numKeywords - 1; index >= 0; index--) {
		if (a_weapon->keywords[index]) {
			std::string keyword = a_weapon->keywords[index]->formEditorID.c_str();

			auto pos = keyword.find("WeapType");
			if (pos != 0)
				continue;

			weaponType = keyword.substr(pos + 8, keyword.length());

			if (weaponType == "Bow" && a_weapon->weaponData.animationType == RE::WEAPON_TYPE::kCrossbow)
				weaponType = "Crossbow";

			auto weaponMultiplier = _weaponMultiplierCache.find(weaponType);

			if (weaponMultiplier != _weaponMultiplierCache.end()) {
				weaponMult = weaponMultiplier->second;
			}

			break;
		}
	}

	// 1. Get settings singleton for configuration multipliers
	float weightContrib = settings ? settings->Weapon.WeightContribution : 0.005f;

	// 2. Get base attack damage stats from the cached baseline weapons
	float minDamage = _minWeapon->GetAttackDamage();
	float maxDamage = _maxWeapon->GetAttackDamage() * settings->Global.EquipmentReferenceMultiplier;
	float currentDamage = a_weapon->GetAttackDamage();

	// 3. Prevent division by zero
	float damageRange = maxDamage - minDamage;
	if (damageRange <= 0.0f) {
		return 25.0f;
	}

	// 4. Apply the exact Armor Rating Rescaled algorithm for damage:
	float normalizedDamage = std::clamp(
		(currentDamage - minDamage) / damageRange,
		0.0f,
		1.0f);

	// Controls how strongly each point of weapon damage
	// contributes to the weapon's poise value.
	float damageContribution =
		settings->Weapon.DamageContribution;

	normalizedDamage *= damageContribution;

	normalizedDamage =
		std::clamp(normalizedDamage, 0.0f, 1.0f);

	float r1 = normalizedDamage * settings->Global.WeaponScalingCurve;

	float r2 = r1 / (1.0f + r1);

	// 5. Multiply the rescaled damage factor directly by the flat weight contribution multiplier
	float weightFactor = ignoreWeight ? 0.0f : (a_weapon->weight * weightContrib);

	float basePoiseFactor = r2 + weightFactor;

	basePoiseFactor = std::clamp(basePoiseFactor, 0.0f, 1.0f);

	// 6. Map the final curve output to your target poise range (15.0f min to 70.0f max)
	float minPoise = 25.0f;
	float maxPoise = 100.0f;

	// Assuming r2 naturally spans from 0.0 to a theoretical ceiling,
	// we can lerp or scale it directly across your target poise bounds:
	float finalValue = std::lerp(minPoise, maxPoise, basePoiseFactor);

	if (settings->Debug.LogWeaponCalcs) {
		float outputValue = std::clamp(finalValue * weaponMult, 0.0f, 200.0f);

		logger::info(
			FMT_STRING(
				"[Weapon Poise] Weapon={} Type={} Damage={} Weight={} "
				"MinDamage={} MaxDamage={} Normalized={} "
				"DamageContribution={} CurveFactor={} WeightFactor={} "
				"BaseFactor={} WeaponMult={} Final={}"),
			a_weapon->GetName(),
			weaponType,
			currentDamage,
			a_weapon->weight,
			minDamage,
			maxDamage,
			normalizedDamage,
			damageContribution,
			r2,
			a_weapon->weight * weightContrib,
			basePoiseFactor,
			weaponMult,
			outputValue);
	}

	return std::clamp(finalValue * weaponMult, 0.0f, 200.0f);
}

float HitEventHandler::CalculateWeaponStagger(RE::Actor* aggressor, RE::TESObjectWEAP* weapon)
{
	auto settings = Settings::GetSingleton();

	if (!weapon) {
		return 0.0f;
	}

	// Unarmed attacks already return their fully calculated poise damage
	if (weapon->IsHandToHandMelee()) {
		return GetUnarmedDamage(aggressor);
	}

	float weaponDamage = GetWeaponDamage(weapon);

	if (settings->Debug.LogWeaponCalcs) {
		logger::info(
			FMT_STRING("[Weapon Mult] Before={} Mult={} After={}"),
			weaponDamage,
			settings->Weapon.MeleeMult,
			weaponDamage * settings->Weapon.MeleeMult);
	}

	return weaponDamage * settings->Weapon.MeleeMult;
}

float HitEventHandler::CalculateProjectileStagger(RE::Actor* aggressor, RE::Projectile* projectile)
{
	auto settings = Settings::GetSingleton();

	auto& data = projectile->GetProjectileRuntimeData();

	if (!data.weaponSource) {
		return 0.0f;
	}

	bool isCrossbow =
		data.weaponSource->weaponData.animationType ==
		RE::WEAPON_TYPE::kCrossbow;

	float weaponDamage = GetWeaponDamage(data.weaponSource);

	float weaponFactor = 1.0f;

	float drawFactor = 1.0f;

	if (isCrossbow) {
		weaponFactor = settings->Weapon.CrossbowMult;
	} else {
		weaponFactor = settings->Weapon.BowDamageMult;

		float bowSpeed = data.weaponSource->GetSpeed();

		if (bowSpeed > 0.0f) {
			drawFactor =
				1.0f +
				((1.0f / bowSpeed) - 1.0f) *
					settings->Weapon.BowDrawSpeedMult;
		}
	}

	float arrowDamage = 0.0f;

	if (data.ammoSource) {
		arrowDamage =
			data.ammoSource->GetRuntimeData().data.damage;
	}

	float arrowContribution =
		arrowDamage *
		settings->Weapon.ArrowDamageMult;

	float stagger =
		(weaponDamage * drawFactor * weaponFactor) +
		arrowContribution;

	float bowBonus = 0.0f;

	if (aggressor && aggressor->AsActorValueOwner()) {
		bowBonus =
			aggressor->AsActorValueOwner()->GetActorValue(
				RE::ActorValue::kBowStaggerBonus);
	}

	stagger *= 1.0f + bowBonus;

	if (settings->Debug.LogWeaponCalcs) {
		logger::info(
			FMT_STRING(
				"[Projectile Calc] Weapon={} Type={} Ammo={} "
				"WeaponDamage={:.2f} WeaponFactor={:.2f} "
				"DrawFactor={:.3f} ArrowDamage={:.2f} "
				"ArrowMult={:.3f} ArrowContribution={:.2f} "
				"BowBonus={:.2f} Final={:.2f}"),
			data.weaponSource->GetName(),
			isCrossbow ? "Crossbow" : "Bow",
			data.ammoSource ? data.ammoSource->GetName() : "NULL",
			weaponDamage,
			weaponFactor,
			drawFactor,
			arrowDamage,
			settings->Weapon.ArrowDamageMult,
			arrowContribution,
			bowBonus,
			stagger);
	}

	return stagger;
}

bool HitEventHandler::IsShieldStrike(RE::Actor* actor, RE::HitData* hitData)
{
	if (!actor || !hitData) {
		return false;
	}

	if (hitData->skill != RE::ActorValue::kNone) {
		return false;
	}

	auto leftHand = actor->GetEquippedObject(true);

	auto shield = leftHand ? leftHand->As<RE::TESObjectARMO>() : nullptr;

	if (!shield || !shield->IsShield()) {
		return false;
	}

	const auto flags = hitData->flags.underlying();

	if (flags != 196608 && flags != 196609) {
		return false;
	}

	if (Settings::GetSingleton()->Debug.LogWeaponCalcs) {
		logger::info(
			FMT_STRING(
				"[Shield Strike FOUND] Actor={} Shield={} Flags={}"),
			actor->GetName(),
			shield->GetName(),
			flags);
	}

	return true;
}

float HitEventHandler::GetShieldDamage(RE::Actor* a_actor)
{
	auto settings = Settings::GetSingleton();

	if (!a_actor || !_minShield || !_maxShield) {
		return 0.0f;
	}

	auto leftHand = a_actor->GetEquippedObject(true);
	auto shield = leftHand ? leftHand->As<RE::TESObjectARMO>() : nullptr;

	if (!shield || !shield->IsShield()) {
		return 0.0f;
	}

	// ==========================
	// Shield Armor Scaling
	// ==========================

	float minArmor = _minShield->GetArmorRating();

	float maxArmor =
		_maxShield->GetArmorRating() *
		settings->Global.EquipmentReferenceMultiplier;

	float minWeight = _minShield->GetWeight();
	float maxWeight = _maxShield->GetWeight();

	float armorRange = maxArmor - minArmor;

	if (armorRange <= 0.0f) {
		armorRange = 1.0f;
	}

	float weightRange = maxWeight - minWeight;

	if (weightRange <= 0.0f) {
		weightRange = 1.0f;
	}

	float normalizedArmor =
		std::clamp(
			(shield->GetArmorRating() - minArmor) / armorRange,
			0.0f,
			1.0f);

	float normalizedWeight =
		std::clamp(
			(shield->GetWeight() - minWeight) / weightRange,
			0.0f,
			1.0f);

	// Same ARR curve as weapons/gauntlets

	float armorR1 =
		normalizedArmor * settings->Global.ArmorScalingCurve;

	float armorR2 =
		armorR1 / (1.0f + armorR1);

	float baseFactor =
		std::clamp(
			(armorR2 * settings->Shield.ArmorContribution) +
				(normalizedWeight * settings->Shield.WeightContribution),
			0.0f,
			1.0f);

	// Shield impact range
	// Hide -> Daedric

	float poiseDamage =
		std::lerp(
			25.0f,
			100.0f,
			baseFactor);

	// ==========================
	// Block Skill Scaling
	// ==========================

	auto avOwner = a_actor->AsActorValueOwner();

	float blockSkill = avOwner ?
	                       avOwner->GetActorValue(RE::ActorValue::kBlock) :
	                       0.0f;

	float blockMultiplier =
		std::clamp(
			0.75f + (blockSkill / 100.0f) * 0.5f,
			0.75f,
			1.25f);

	poiseDamage *= blockMultiplier;

	if (settings->Debug.LogWeaponCalcs) {
		logger::info(
			FMT_STRING(
				"[Shield Strike Calc] "
				"Shield={} Armor={} Weight={} "
				"ArmorNorm={} WeightNorm={} "
				"ArmorCurve={} "
				"BlockSkill={} BlockMult={} "
				"BaseFactor={} Final={}"),
			shield->GetName(),
			shield->GetArmorRating(),
			shield->GetWeight(),
			normalizedArmor,
			normalizedWeight,
			armorR2,
			blockSkill,
			blockMultiplier,
			baseFactor,
			poiseDamage);
	}

	return std::clamp(
		poiseDamage,
		0.0f,
		200.0f);
}

float HitEventHandler::GetUnarmedDamage(RE::Actor* a_actor)
{
	auto settings = Settings::GetSingleton();

	if (!a_actor) {
		return 0.0f;
	}

	// Base Skyrim unarmed damage
	float unarmedDamage =
		a_actor->AsActorValueOwner()->GetActorValue(
			RE::ActorValue::kUnarmedDamage);

	if (unarmedDamage <= 0.0f) {
		return 0.0f;
	}

	// Same normalization curve as weapons
	float minDamage = _minWeapon->GetAttackDamage();
	float maxDamage = _maxWeapon->GetAttackDamage() * settings->Global.EquipmentReferenceMultiplier;

	float normalizedDamage =
		std::clamp(
			(unarmedDamage - minDamage) /
				(maxDamage - minDamage),
			0.0f,
			1.0f);

	float r1 = normalizedDamage * settings->Global.WeaponScalingCurve;

	float r2 = r1 / (1.0f + r1);

	float poiseDamage =
		std::lerp(
			25.0f,
			100.0f,
			r2);

	// ==========================
	// Gauntlet Armor Scaling
	// ==========================

	auto gauntlet =
		a_actor->GetWornArmor(
			RE::BGSBipedObjectForm::BipedObjectSlot::kHands);

	if (gauntlet && _minGauntlet && _maxGauntlet) {
		float minArmor = _minGauntlet->GetArmorRating();
		float maxArmor = _maxGauntlet->GetArmorRating() * settings->Global.EquipmentReferenceMultiplier;

		float minWeight = _minGauntlet->GetWeight();
		float maxWeight = _maxGauntlet->GetWeight();

		float armorRange = maxArmor - minArmor;
		if (armorRange <= 0.0f)
			armorRange = 1.0f;

		float weightRange = maxWeight - minWeight;
		if (weightRange <= 0.0f)
			weightRange = 1.0f;

		// Normalize armor and weight independently
		float normalizedArmor =
			std::clamp(
				(gauntlet->GetArmorRating() - minArmor) / armorRange,
				0.0f,
				1.0f);

		float normalizedWeight =
			std::clamp(
				(gauntlet->GetWeight() - minWeight) / weightRange,
				0.0f,
				1.0f);

		// Same rescale curve used by weapons
		float armorR1 = normalizedArmor * settings->Global.ArmorScalingCurve;

		float armorR2 = armorR1 / (1.0f + armorR1);

		// Armor and weight each have their own INI contribution
		float baseFactor =
			std::clamp(
				(armorR2 * settings->Unarmed.ArmorContribution) +
					(normalizedWeight * settings->Unarmed.WeightContribution),
				0.0f,
				1.0f);

		// Convert into poise value
		float gauntletBonus =
			std::lerp(
				0.0f,
				25.0f,
				baseFactor);

		// Heavy/Light specific scaling
		float armorTypeContribution =
			gauntlet->IsHeavyArmor() ?
				settings->Unarmed.HeavyGauntletContribution :
				settings->Unarmed.LightGauntletContribution;

		poiseDamage +=
			gauntletBonus *
			armorTypeContribution;

		if (settings->Debug.LogWeaponCalcs) {
			logger::info(
				FMT_STRING(
					"[Gauntlet Poise] "
					"Name={} Heavy={} "
					"Armor={} Weight={} "
					"ArmorNorm={} WeightNorm={} "
					"ArmorCurve={} "
					"ArmorContribution={} "
					"WeightContribution={} "
					"BaseFactor={} "
					"ArmorTypeContribution={} "
					"FinalBonus={}"),
				gauntlet->GetName(),
				gauntlet->IsHeavyArmor(),
				gauntlet->GetArmorRating(),
				gauntlet->GetWeight(),
				normalizedArmor,
				normalizedWeight,
				armorR2,
				settings->Unarmed.ArmorContribution,
				settings->Unarmed.WeightContribution,
				baseFactor,
				armorTypeContribution,
				gauntletBonus * armorTypeContribution);
		}
	}

	// ==========================
	// Skill Scaling
	// ==========================
	float skillValue = 0.0f;

	auto actorValueOwner = a_actor->AsActorValueOwner();

	if (actorValueOwner) {
		switch (settings->Unarmed.SkillType) {
		case 0:  // None
			break;

		case 1:  // One-Handed
			skillValue = actorValueOwner->GetActorValue(RE::ActorValue::kOneHanded);
			break;

		case 2:  // Two-Handed
			skillValue = actorValueOwner->GetActorValue(RE::ActorValue::kTwoHanded);
			break;

		case 3:  // Archery
			skillValue = actorValueOwner->GetActorValue(RE::ActorValue::kArchery);
			break;

		case 4:  // Block
			skillValue = actorValueOwner->GetActorValue(RE::ActorValue::kBlock);
			break;

		case 5:  // Heavy Armor
			skillValue = actorValueOwner->GetActorValue(RE::ActorValue::kHeavyArmor);
			break;

		case 6:  // Light Armor
			skillValue = actorValueOwner->GetActorValue(RE::ActorValue::kLightArmor);
			break;

		case 7:  // Pickpocket
			skillValue = actorValueOwner->GetActorValue(RE::ActorValue::kPickpocket);
			break;

		case 8:  // Lockpicking
			skillValue = actorValueOwner->GetActorValue(RE::ActorValue::kLockpicking);
			break;

		case 9:  // Sneak
			skillValue = actorValueOwner->GetActorValue(RE::ActorValue::kSneak);
			break;

		case 10:  // Alchemy
			skillValue = actorValueOwner->GetActorValue(RE::ActorValue::kAlchemy);
			break;

		case 11:  // Speech
			skillValue = actorValueOwner->GetActorValue(RE::ActorValue::kSpeech);
			break;

		case 12:  // Alteration
			skillValue = actorValueOwner->GetActorValue(RE::ActorValue::kAlteration);
			break;

		case 13:  // Conjuration
			skillValue = actorValueOwner->GetActorValue(RE::ActorValue::kConjuration);
			break;

		case 14:  // Destruction
			skillValue = actorValueOwner->GetActorValue(RE::ActorValue::kDestruction);
			break;

		case 15:  // Illusion
			skillValue = actorValueOwner->GetActorValue(RE::ActorValue::kIllusion);
			break;

		case 16:  // Restoration
			skillValue = actorValueOwner->GetActorValue(RE::ActorValue::kRestoration);
			break;

		case 17:  // Enchanting
			skillValue = actorValueOwner->GetActorValue(RE::ActorValue::kEnchanting);
			break;

		case 18:  // Smithing
			skillValue = actorValueOwner->GetActorValue(RE::ActorValue::kSmithing);
			break;

		default:
			break;
		}
	}

	float skillMultiplier =
		1.0f +
		(skillValue / 100.0f) *
			settings->Unarmed.SkillContribution;

	poiseDamage *= skillMultiplier;

	// ==========================
	// Final H2H multiplier
	// ==========================
	float multiplier = 1.0f;

	auto handToHandMultiplier = _weaponMultiplierCache.find("HandToHandMelee");

	if (handToHandMultiplier != _weaponMultiplierCache.end()) {
		multiplier = handToHandMultiplier->second;
	}

	poiseDamage *= multiplier;

	if (settings->Debug.LogWeaponCalcs) {
		logger::info(
			"[Unarmed Calc] Actor={} AV={} Normalized={} Curve={} Skill={} SkillMult={} Gauntlet={} Final={}",
			a_actor->GetName(),
			unarmedDamage,
			normalizedDamage,
			r2,
			skillValue,
			skillMultiplier,
			gauntlet ? gauntlet->weight : 0.0f,
			poiseDamage);
	}

	return std::clamp(
		poiseDamage,
		0.0f,
		200.0f);
}

float HitEventHandler::GetBashDamage(RE::TESObjectARMO* a_bashingItem)
{
	float multiplier = 1.0f;

	auto shieldMultiplier = _weaponMultiplierCache.find("Shield");

	if (shieldMultiplier != _weaponMultiplierCache.end()) {
		multiplier = shieldMultiplier->second;
	}

	return a_bashingItem->weight * multiplier;
}

float HitEventHandler::GetMiscDamage()
{
	float multiplier = 1.0f;

	auto miscMultiplier = _weaponMultiplierCache.find("Misc");

	if (miscMultiplier != _weaponMultiplierCache.end()) {
		multiplier = miscMultiplier->second;
	}

	return 5.0f * multiplier;
}

float HitEventHandler::GetStaffDamage(RE::Actor* a_actor, RE::TESObjectWEAP* a_staff)
{
	if (!a_actor || !a_staff) {
		return 0.0f;
	}

	auto* enchantment = a_staff->formEnchanting;

	if (!enchantment) {
		return 0.0f;
	}

	auto settings = Settings::GetSingleton();

	float totalDamage = 0.0f;

	for (const auto& effect : enchantment->effects) {
		if (!effect || !effect->baseEffect) {
			continue;
		}

		const auto actorValue =
			effect->baseEffect->data.primaryAV;

		const float magnitude =
			effect->effectItem.magnitude;

		std::string avName{
			magic_enum::enum_name(actorValue)
		};

		if (avName.empty()) {
			continue;
		}

		// Remove k prefix
		// kHealth -> Health
		avName.erase(0, 1);

		const bool detrimental =
			magnitude > 0.0f;

		const auto jsonAV =
			settings->JSONSettings
				["Magic Effects"]
				["Actor Values"]
				[detrimental ? "Damage" : "Recovery"]
				[avName];

		if (jsonAV == nullptr) {
			continue;
		}

		float poiseDamage =
			static_cast<float>(jsonAV) *
			magnitude;

		if (poiseDamage <= 0.0f) {
			continue;
		}

		// ====================================================
		// Attacker perk modifiers
		// ====================================================

		float baseMult = 1.0f;

		PoiseAV::ApplyPerkEntryPoint(
			34,
			a_actor->As<RE::Character>(),
			a_actor->As<RE::Character>(),
			&baseMult);

		poiseDamage *= baseMult;

		// ====================================================
		// Attacker damage multiplier
		// ====================================================

		poiseDamage *=
			settings->GetDamageMultiplier(
				a_actor,
				a_actor);

		// ====================================================
		// Magic damage multiplier
		// ====================================================

		poiseDamage *=
			settings->Magic.DamageMult;

		// ====================================================
		// Magic scaling
		// ====================================================

		if (poiseDamage > 25.0f) {
			const float excess =
				poiseDamage - 25.0f;

			poiseDamage =
				25.0f +
				(excess /
					(1.0f +
						(excess / 100.0f)));
		}

		totalDamage += poiseDamage;

		if (settings->Debug.LogMagicEffectCalcs) {
			logger::info(
				"[Staff Magic Damage] Actor={} Staff={} "
				"AV={} Magnitude={} "
				"EffectDamage={} Total={}",
				a_actor->GetName(),
				a_staff->GetName(),
				avName,
				magnitude,
				poiseDamage,
				totalDamage);
		}
	}

	return std::clamp(
		totalDamage,
		0.0f,
		200.0f);
}

bool HitEventHandler::IsCreature(RE::Actor* actor)
{
	if (!actor) {
		return false;
	}

	auto race = actor->GetRace();
	if (!race) {
		return false;
	}

	static auto creatureKeyword =
		RE::TESForm::LookupByID<RE::BGSKeyword>(0x13795);  // ActorTypeCreature

	static auto animalKeyword =
		RE::TESForm::LookupByID<RE::BGSKeyword>(0x13798);  // ActorTypeAnimal

	static auto dwarvenKeyword =
		RE::TESForm::LookupByID<RE::BGSKeyword>(0x1397A);  // ActorTypeDwarven

	bool hasCreature =
		creatureKeyword &&
		race->HasKeyword(creatureKeyword);

	bool hasAnimal =
		animalKeyword &&
		race->HasKeyword(animalKeyword);

	bool hasDwarven =
		dwarvenKeyword &&
		race->HasKeyword(dwarvenKeyword);

	bool result =
		hasCreature ||
		hasAnimal ||
		hasDwarven;

	auto settings = Settings::GetSingleton();

	if (settings->Debug.LogWeaponCalcs && result) {
		logger::info(
			FMT_STRING(
				"[IsCreature] Actor={} Race={} Creature={} Animal={} Dwarven={} Result={}"),
			actor->GetName(),
			race->GetFormEditorID(),
			hasCreature,
			hasAnimal,
			hasDwarven,
			result);
	}

	return result;
}

float HitEventHandler::ApplyAttackMultiplier(RE::HitData* hitData, float stagger)
{
	auto settings = Settings::GetSingleton();

	auto attackData =
		hitData->attackData ? hitData->attackData.get() : nullptr;

	if (!attackData) {
		return stagger;
	}

	float staggerOffset = attackData->data.staggerOffset;

	float attackMult = 1.0f;

	if (staggerOffset == 1.0f) {
		// Power attacks naturally deal 2x poise damage
		attackMult = 2.0f * settings->Attack.PowerAttackMult;
	} else {
		// Normal attacks
		attackMult = settings->Attack.NormalAttackMult;
	}

	float result = stagger * attackMult;

	if (settings->Debug.LogStaggerCalcs) {
		logger::info(
			FMT_STRING(
				"[Custom Attack Mult] Type={} Offset={} Before={} Mult={} After={}"),
			staggerOffset == 1.0f ? "Power" : "Normal",
			staggerOffset,
			stagger,
			attackMult,
			result);
	}

	return result;
}

float HitEventHandler::CalculateBashStagger(RE::Actor* aggressor)
{
	if (!aggressor) {
		return 0.0f;
	}

	auto settings = Settings::GetSingleton();

	float bashMultiplier = settings->Blocking.BashMult;

	if (const auto perk = RE::TESForm::LookupByEditorID<RE::BGSPerk>(
			Milf::GetSingleton()->perks.SkullRattler_Perk);
		perk && aggressor->HasPerk(perk)) {
		bashMultiplier += 0.25f;
	}

	auto leftHand = aggressor->GetEquippedObject(true);
	auto rightHand = aggressor->GetEquippedObject(false);

	if (leftHand && leftHand->formType == RE::FormType::Armor) {
		return GetBashDamage(leftHand->As<RE::TESObjectARMO>()) * bashMultiplier;
	}

	if (rightHand && rightHand->formType == RE::FormType::Weapon) {
		return GetWeaponDamage(rightHand->As<RE::TESObjectWEAP>()) * bashMultiplier;
	}

	return GetMiscDamage() * bashMultiplier;
}

float HitEventHandler::ApplyArmorReduction(RE::Actor* target, float stagger)
{
	if (!target || stagger <= 0.0f) {
		return stagger;
	}

	auto settings = Settings::GetSingleton();

	// ============================================================
	// Armor Constants
	// ============================================================

	constexpr float kArmorCap = 1000.0f;
	constexpr float kMaxReduction = 0.60f;
	constexpr float kMaxArmorWeight = 100.0f;

	// ============================================================
	// Armor Rating
	// ============================================================

	float totalArmor =
		(std::max)(0.0f,
			static_cast<float>(
				target->GetActorRuntimeData().armorRating));

	// ============================================================
	// Armor Effectiveness
	// ============================================================

	float armorEffectiveness =
		settings->Armor.ArmorMult;

	// Normalize armor against a fixed 1000 AR cap.
	//
	// 0 AR    = 0.0
	// 500 AR  = 0.5
	// 1000 AR = 1.0
	// 1000+   = 1.0
	float armorRatio =
		std::clamp(
			(totalArmor * armorEffectiveness) /
				kArmorCap,
			0.0f,
			1.0f);

	// ============================================================
	// Armor Weight
	// ============================================================
	//
	// Weight acts as a secondary multiplier rather than an
	// independent source of armor effectiveness.
	//
	// Weight is capped at 100 total worn armor weight.
	//
	// WeightMultiplier =
	//     1.0 + (CappedArmorWeight * WeightContribution)
	//
	// With WeightContribution = 0.0025:
	//
	// 0 weight   = 1.00x
	// 25 weight  = 1.0625x
	// 50 weight  = 1.125x
	// 75 weight  = 1.1875x
	// 100 weight = 1.25x
	// 100+       = 1.25x

	float totalArmorWeight = 0.0f;

	auto inventory = target->GetInventory();

	for (const auto& [item, entry] : inventory) {
		if (!item || !entry.second) {
			continue;
		}

		auto armor =
			item->As<RE::TESObjectARMO>();

		if (!armor || !entry.second->IsWorn()) {
			continue;
		}

		totalArmorWeight +=
			armor->GetWeight();
	}

	// 100 weight is the maximum considered.
	totalArmorWeight =
		std::clamp(
			totalArmorWeight,
			0.0f,
			kMaxArmorWeight);

	float weightContribution =
		settings->Armor.WeightContribution;

	float weightMultiplier =
		1.0f +
		(totalArmorWeight * weightContribution);

	// Prevent invalid negative multipliers.
	weightMultiplier =
		(std::max)(0.0f,
			weightMultiplier);

	// ============================================================
	// Combine Armor Rating + Weight
	// ============================================================
	//
	// Armor Rating remains the primary source of mitigation.
	// Weight modifies the effectiveness of the existing armor.
	//
	// Example:
	//
	// 800 AR + 0 weight
	//     -> armorRatio * 1.00
	//
	// 800 AR + 100 weight
	//     -> armorRatio * 1.25

	float baseArmorFactor =
		std::clamp(
			armorRatio * weightMultiplier,
			0.0f,
			1.0f);

	// ============================================================
	// Hyperbolic Scaling
	// ============================================================

	float curveStrength =
		settings->Global.ArmorScalingCurve;

	float r1 =
		baseArmorFactor *
		curveStrength;

	float curvedArmor =
		r1 /
		(1.0f + r1);

	// ============================================================
	// Curve Normalization
	// ============================================================
	//
	// Normalize against the theoretical maximum curve value.
	// This preserves the 60% maximum reduction.

	float maxCurve =
		curveStrength /
		(1.0f + curveStrength);

	float normalizedCurve =
		maxCurve > 0.0f ?
			curvedArmor / maxCurve :
			0.0f;

	normalizedCurve =
		std::clamp(
			normalizedCurve,
			0.0f,
			1.0f);

	// ============================================================
	// Final Armor Reduction
	// ============================================================

	float armorReduction =
		normalizedCurve *
		kMaxReduction;

	float armorMultiplier =
		1.0f -
		armorReduction;

	float preArmorStagger =
		stagger;

	stagger *=
		armorMultiplier;

	// ============================================================
	// Debug
	// ============================================================

	if (settings->Debug.LogArmorCalcs) {
		logger::info(
			FMT_STRING(
				"[Armor Calc] Target={} "
				"Armor={} ArmorEffectiveness={} "
				"ArmorRatio={} "
				"Weight={} MaxWeight={} "
				"WeightContribution={} "
				"WeightMult={} "
				"BaseFactor={} "
				"Curve={} CurvedArmor={} "
				"NormalizedCurve={} "
				"Reduction={} FinalMult={} "
				"Before={} After={}"),
			target->GetName(),
			totalArmor,
			armorEffectiveness,
			armorRatio,
			totalArmorWeight,
			kMaxArmorWeight,
			weightContribution,
			weightMultiplier,
			baseArmorFactor,
			curveStrength,
			curvedArmor,
			normalizedCurve,
			armorReduction,
			armorMultiplier,
			preArmorStagger,
			stagger);
	}

	return stagger;
}

float HitEventHandler::ApplyBlockingMultiplier(RE::HitData* hitData, RE::Actor* target, float stagger)
{
	if (!hitData || !target || hitData->percentBlocked <= 0.0f) {
		return stagger;
	}

	auto settings = Settings::GetSingleton();

	float blockAmount = hitData->percentBlocked;

	// Power attacks use a separate blocking multiplier.
	bool isPowerAttack =
		hitData->flags.any(RE::HitData::Flag::kPowerAttack);

	if (isPowerAttack) {
		blockAmount *= settings->Blocking.PowerAttackBlockingMult;
	}

	float blockMult =
		1.0f - (blockAmount * settings->Blocking.BlockingMult);

	blockMult = std::clamp(blockMult, 0.0f, 1.0f);

	if (settings->Debug.LogArmorCalcs) {
		logger::info(
			FMT_STRING(
				"Block Calc: PercentBlocked={} PowerAttack={} "
				"EffectiveBlock={} BlockingMult={} PowerAttackBlockingMult={} "
				"FinalMult={} Before={} After={}"),
			hitData->percentBlocked,
			isPowerAttack,
			blockAmount,
			settings->Blocking.BlockingMult,
			settings->Blocking.PowerAttackBlockingMult,
			blockMult,
			stagger,
			stagger * blockMult);
	}

	return stagger * blockMult;
}

float HitEventHandler::RecalculateStagger(RE::Actor* target, RE::Actor* aggressor, RE::HitData* hitData)
{
	auto settings = Settings::GetSingleton();

	float stagger = 0.0f;

	auto sourceRef = hitData->sourceRef.get().get();

	bool isCreature = aggressor && IsCreature(aggressor);

	if (settings->Debug.LogStaggerCalcs) {
		logger::info(
			FMT_STRING("[Recalculate START] Target={} Aggressor={} Weapon={} Skill={}"),
			target ? target->GetName() : "NULL",
			aggressor ? aggressor->GetName() : "NULL",
			hitData->weapon ? hitData->weapon->GetName() : "NULL",
			magic_enum::enum_name(hitData->skill));
	}

	// ==========================
	// Bash Attacks
	// ==========================
	if (hitData->skill == RE::ActorValue::kBlock) {
		stagger = CalculateBashStagger(aggressor);
	}

	// ==========================
	// Bow / Projectile Attacks
	// ==========================
	else if (auto projectile = sourceRef ? sourceRef->AsProjectile() : nullptr) {
		stagger = CalculateProjectileStagger(
			aggressor,
			projectile);
	}
	// ==========================
	// Weapon / Unarmed Attacks
	// ==========================
	else if (hitData->weapon && (!aggressor || !isCreature)) {
		// Shield strikes can carry a weapon pointer.
		if (hitData->skill == RE::ActorValue::kNone &&
			IsShieldStrike(aggressor, hitData)) {
			stagger = GetShieldDamage(aggressor);
		} else {
			auto weapon = hitData->weapon->As<RE::TESObjectWEAP>();

			if (!weapon) {
				return 0.0f;
			}

			if (weapon->IsHandToHandMelee()) {
				if (!aggressor) {
					return 0.0f;
				}

				stagger = GetUnarmedDamage(aggressor);
			} else {
				stagger = CalculateWeaponStagger(aggressor, weapon);
			}
		}
	}
	// ==========================
	// Creature Attacks
	// ==========================
	else if (isCreature) {
		float baseStagger = 0.0f;

		RE::TESRace* race = aggressor->GetRace();

		float attackMult = 1.0f;

		// ==========================
		// Base Creature Damage
		// ==========================

		float unarmedDamage = 0.0f;

		if (race) {
			unarmedDamage = race->data.unarmedDamage;
		}

		// Clamp raw unarmed damage before weight scaling
		unarmedDamage =
			std::clamp(
				unarmedDamage,
				20.0f,
				50.0f);

		// Find attack-specific damage multiplier from ATKD
		if (hitData->attackData) {
			attackMult = hitData->attackData->data.damageMult;
		}

		// ==========================
		// Creature Weight Scaling
		// ==========================

		float creatureWeight = 1.0f;

		if (race) {
			auto it = settings->RaceWeightCache.find(
				race->GetFormEditorID());

			if (it != settings->RaceWeightCache.end()) {
				creatureWeight = it->second;
			}
		}

		// Normalize creature weight:
		// 0.5 = minimum (1.0x)
		// 4.0 = maximum (2.0x)
		float normalizedWeight =
			std::clamp(
				(creatureWeight - 0.5f) / (4.0f - 0.5f),
				0.0f,
				1.0f);

		// Apply weight scaling curve
		float curvedWeight =
			std::pow(
				normalizedWeight,
				settings->Creature.ScalingCurve);

		// Convert normalized weight into damage multiplier
		float creatureMultiplier =
			std::lerp(
				1.0f,
				2.5f,
				curvedWeight);

		// Apply creature weight to base unarmed damage
		baseStagger =
			unarmedDamage *
			creatureMultiplier;

		// Clamp weighted base stagger before attack multiplier
		baseStagger =
			std::clamp(
				baseStagger,
				20.0f,
				80.0f);

		// Apply attack-specific multiplier AFTER the clamp
		baseStagger *= attackMult;

		// Apply global creature damage multiplier
		stagger =
			baseStagger *
			settings->Creature.DamageMultiplier;

		if (settings->Debug.LogWeaponCalcs) {
			logger::info(
				FMT_STRING(
					"[Creature Calc] "
					"Aggressor={} Race={} "
					"UnarmedDamage={} AttackMult={} "
					"Weight={} Normalized={} "
					"Curve={} CreatureMult={} "
					"ClampedBase={} DamageMult={} Final={}"),
				aggressor->GetName(),
				race ? race->GetName() : "NULL",
				unarmedDamage,
				attackMult,
				creatureWeight,
				normalizedWeight,
				settings->Creature.ScalingCurve,
				creatureMultiplier,
				baseStagger,
				settings->Creature.DamageMultiplier,
				stagger);
		}
	}

	// ==========================
	// No Skill / Physical Damage
	// ==========================
	else if (hitData->skill == RE::ActorValue::kNone) {
		if (settings->Debug.LogWeaponCalcs) {
			logger::critical(
				FMT_STRING(
					"[kNone ENTER] Target={} Aggressor={} TotalDamage={} PhysicalDamage={} "
					"Flags={} Source={} Weapon={}"),
				target ? target->GetName() : "NULL",
				aggressor ? aggressor->GetName() : "NULL",
				hitData->totalDamage,
				hitData->physicalDamage,
				hitData->flags.underlying(),
				sourceRef ? sourceRef->GetName() : "NULL",
				hitData->weapon ? hitData->weapon->GetName() : "NULL");
		}

		// ==========================
		// Environment / Traps
		// ==========================
		if (!aggressor) {
			if (settings->Debug.LogWeaponCalcs) {
				logger::info("[kNone BRANCH] ENVIRONMENT");
			}

			stagger =
				hitData->totalDamage *
				settings->Environment.TrapMult;

			if (settings->Debug.LogWeaponCalcs) {
				logger::info(
					FMT_STRING(
						"[Environment Result] TotalDamage={} PhysicalDamage={} TrapMult={} Final={}"),
					hitData->totalDamage,
					hitData->physicalDamage,
					settings->Environment.TrapMult,
					stagger);
			}
		}

		// ==========================
		// Humanoid Unarmed
		// ==========================
		else {
			RE::TESRace* race = nullptr;
			float        unarmedDamage = 0.0f;

			if (settings->Debug.LogWeaponCalcs) {
				race = aggressor->GetRace();
				unarmedDamage =
					aggressor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kUnarmedDamage);

				logger::info(
					FMT_STRING(
						"[Unarmed Data] Aggressor={} Race={} Weapon={} UnarmedAV={} PhysicalDamage={}"),
					aggressor->GetName(),
					race ? race->GetName() : "NULL",
					hitData->weapon ? hitData->weapon->GetName() : "NONE",
					unarmedDamage,
					hitData->physicalDamage);
			}

			stagger = GetUnarmedDamage(aggressor);

			if (settings->Debug.LogWeaponCalcs) {
				logger::info(
					FMT_STRING(
						"[Unarmed Result] Aggressor={} Race={} Weapon={} UnarmedAV={} PhysicalDamage={} Final={}"),
					aggressor->GetName(),
					race ? race->GetName() : "NULL",
					hitData->weapon ? hitData->weapon->GetName() : "NONE",
					unarmedDamage,
					hitData->physicalDamage,
					stagger);
			}
		}
	}

	// ==========================
	// Poise Mitigation Calcs
	// ==========================
	stagger = ApplyAttackMultiplier(hitData, stagger);

	stagger = ApplyArmorReduction(target, stagger);

	stagger = ApplyBlockingMultiplier(hitData, target, stagger);

	if (settings->Debug.LogStaggerCalcs) {
		float staggerOffset = 0.0f;

		if (hitData->attackData) {
			staggerOffset = hitData->attackData->data.staggerOffset;
		}

		logger::info(
			FMT_STRING(
				"[RecalculateStagger END] Target={} Aggressor={} AttackType={} FinalStagger={}"),
			target ? target->GetName() : "NONE",
			aggressor ? aggressor->GetName() : "NONE",
			staggerOffset == 1.0f ? "Power" : "Normal",
			stagger);
	}
	return stagger;
}

void HitEventHandler::PreProcessHit(RE::Actor* target, RE::HitData* hitData)
{
	if (!target || !hitData) {
		return;
	}

	auto settings = Settings::GetSingleton();
	auto poiseAV = PoiseAV::GetSingleton();

	auto aggressor =
		hitData->aggressor ?
			hitData->aggressor.get().get() :
			nullptr;

	// Actor cannot receive poise damage
	if (!poiseAV->CanDamageActor(target)) {
		return;
	}

	// Skip lethal hits
	if (target->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth) <= hitData->totalDamage) {
		hitData->stagger = 0;
		return;
	}

	if (settings->Debug.LogStaggerCalcs) {
		logger::info(
			FMT_STRING(
				"[PreProcessHit START] Target={} Aggressor={} TotalDamage={} PhysicalDamage={} Skill={}"),
			target->GetName(),
			aggressor ? aggressor->GetName() : "ENVIRONMENT",
			hitData->totalDamage,
			hitData->physicalDamage,
			magic_enum::enum_name(hitData->skill));
	}

	auto poiseDamage = RecalculateStagger(target, aggressor, hitData);

	if (settings->Debug.LogStaggerCalcs) {
		logger::info(
			FMT_STRING(
				"[PreProcessHit - End] Target={} Aggressor={} HitData={} Damage={}"),
			target->GetName(),
			aggressor ? aggressor->GetName() : "NULL",
			fmt::ptr(hitData),
			poiseDamage);
	}

	if (poiseDamage > 0.0f) {
		poiseAV->DamageAndCheckPoise(
			target,
			aggressor,
			poiseDamage,
			hitData);
	}

	// Disable Skyrim vanilla stagger calculation
	hitData->stagger = 0;
}

// API CALLS

// Character Sheet
float HitEventHandler::GetHandDamage(RE::Actor* a_actor, bool a_leftHand)
{
	if (!a_actor) {
		return 0.0f;
	}

	auto* equippedObject =
		a_actor->GetEquippedObject(a_leftHand);

	// ============================================================
	// Two-Handed Weapon
	// ============================================================
	//
	// Two-handed weapons occupy both hand slots, so both left
	// and right hand API requests should return the same damage.
	//
	{
		auto* rightObject =
			a_actor->GetEquippedObject(false);

		if (auto* weapon = rightObject ?
		                       rightObject->As<RE::TESObjectWEAP>() :
		                       nullptr) {
			const bool isTwoHanded =
				weapon->IsTwoHandedSword() ||
				weapon->IsTwoHandedAxe() ||
				weapon->IsBow() ||
				weapon->IsCrossbow();

			if (isTwoHanded) {
				if (weapon->IsHandToHandMelee()) {
					return GetUnarmedDamage(a_actor);
				}

				// ====================================================
				// Bow / Crossbow
				// ====================================================
				if (weapon->IsBow() || weapon->IsCrossbow()) {
					float damage =
						GetWeaponDamage(weapon);

					auto* ammo =
						a_actor->GetCurrentAmmo();

					if (ammo) {
						damage +=
							ammo->GetRuntimeData().data.damage *
							Settings::GetSingleton()
								->Weapon.ArrowDamageMult;
					}

					return damage;
				}

				// ====================================================
				// Normal two-handed weapon
				// ====================================================
				return GetWeaponDamage(weapon);
			}
		}
	}

	// ============================================================
	// No Equipped Object
	// ============================================================
	if (!equippedObject) {
		return GetUnarmedDamage(a_actor);
	}

	// ============================================================
	// Weapon
	// ============================================================
	if (auto* weapon =
			equippedObject->As<RE::TESObjectWEAP>()) {
		if (weapon->IsHandToHandMelee()) {
			return GetUnarmedDamage(a_actor);
		}
		// ============================================================
		// Staff
		// ============================================================
		if (weapon->IsStaff()) {
			return GetStaffDamage(a_actor, weapon);
		}
		// ========================================================
		// Bow / Crossbow
		// ========================================================
		if (weapon->IsBow() || weapon->IsCrossbow()) {
			float damage =
				GetWeaponDamage(weapon);

			auto* ammo =
				a_actor->GetCurrentAmmo();

			if (ammo) {
				damage +=
					ammo->GetRuntimeData().data.damage *
					Settings::GetSingleton()
						->Weapon.ArrowDamageMult;
			}

			return damage;
		}

		return GetWeaponDamage(weapon);
	}

	// ============================================================
	// Armor / Shield
	// ============================================================
	if (auto* armor =
			equippedObject->As<RE::TESObjectARMO>()) {
		if (armor->IsShield()) {
			return GetShieldDamage(a_actor);
		}

		return 0.0f;
	}

	// ============================================================
	// Spell
	// ============================================================
	if (auto* spell =
			equippedObject->As<RE::SpellItem>()) {
		auto settings =
			Settings::GetSingleton();

		float totalDamage = 0.0f;

		for (const auto& effect : spell->effects) {
			if (!effect || !effect->baseEffect) {
				continue;
			}

			const auto actorValue =
				effect->baseEffect->data.primaryAV;

			const float magnitude =
				effect->effectItem.magnitude;

			std::string avName{
				magic_enum::enum_name(actorValue)
			};

			if (avName.empty()) {
				continue;
			}

			// Remove k prefix
			// kHealth -> Health
			avName.erase(0, 1);

			const bool detrimental =
				magnitude > 0.0f;

			const auto jsonAV =
				settings->JSONSettings
					["Magic Effects"]
					["Actor Values"]
					[detrimental ? "Damage" : "Recovery"]
					[avName];

			if (jsonAV == nullptr) {
				continue;
			}

			float poiseDamage =
				static_cast<float>(jsonAV) *
				magnitude;

			if (poiseDamage <= 0.0f) {
				continue;
			}

			// ====================================================
			// Attacker perk modifiers
			// ====================================================

			float baseMult = 1.0f;

			PoiseAV::ApplyPerkEntryPoint(
				34,
				a_actor->As<RE::Character>(),
				a_actor->As<RE::Character>(),
				&baseMult);

			poiseDamage *= baseMult;

			// ====================================================
			// Attacker damage multiplier
			// ====================================================

			poiseDamage *=
				settings->GetDamageMultiplier(
					a_actor,
					a_actor);

			// ====================================================
			// Magic damage multiplier
			// ====================================================

			poiseDamage *=
				settings->Magic.DamageMult;

			// ====================================================
			// Magic scaling
			// ====================================================

			if (poiseDamage > 25.0f) {
				const float excess =
					poiseDamage - 25.0f;

				poiseDamage =
					25.0f +
					(excess /
						(1.0f +
							(excess / 100.0f)));
			}

			totalDamage += poiseDamage;

			if (settings->Debug.LogMagicEffectCalcs) {
				logger::info(
					"[Magic Hand Damage] Actor={} Hand={} "
					"Spell={} AV={} Magnitude={} "
					"EffectDamage={} Total={}",
					a_actor->GetName(),
					a_leftHand ? "Left" : "Right",
					spell->GetName(),
					avName,
					magnitude,
					poiseDamage,
					totalDamage);
			}
		}

		return std::clamp(
			totalDamage,
			0.0f,
			200.0f);
	}

	return 0.0f;
}

// For Honor Stamina
float HitEventHandler::GetStaminaMultiplier(RE::Actor* aggressor)
{
	if (!_hasForHonorStamina || !_getStaminaMultiplier || !aggressor) {
		return 1.0f;
	}

	return _getStaminaMultiplier(aggressor);
}
// void HitEventHandler::PoiseCallback_Post(const PRECISION_API::PrecisionHitData& a_precisionHitData, const RE::HitData& hitData)
// {
// 	if (!a_precisionHitData.target || !a_precisionHitData.target->Is(RE::FormType::ActorCharacter)) {
// 		return;
// 	}
// 	auto handler = GetSingleton();
// 	RE::HitData* hitData_ptr = hitData;

// 	handler->PreProcessHit(a_precisionHitData.target->As<RE::Actor>(), hitData_ptr);

// 	return;
// }

//old leonkingzz stuff
// if (stagger > 0.00) {
// 	// Apply additional block-based stagger reduction only when the hit was blocked.
// 	if (hitData->flags.all(RE::HitData::Flag::kBlocked)) {
// 		// Check if the target has the perk that enables this custom blocking behavior.
// 		if (const auto perk = RE::TESForm::LookupByEditorID<RE::BGSPerk>(Milf::GetSingleton()->perks.AlikrDance_Perk); perk && target->HasPerk(perk)) {
// 			// Determine whether the block was performed with a weapon or a shield.
// 			bool weaponblock = hitData->flags.all(RE::HitData::Flag::kBlockWithWeapon);

// 			// Convert the Block skill into a stagger multiplier.
// 			// Higher Block skill results in greater stagger reduction.
// 			auto block_score = 1.0f - (target->AsActorValueOwner()->GetActorValue(RE::ActorValue::kBlock) / 100.0f);

// 			// Enforce minimum stagger values to prevent blocking from completely
// 			// negating stagger. Weapon blocks have a higher minimum than shield blocks.
// 			if (weaponblock) {
// 				if (block_score < 0.30) {
// 					block_score = 0.30;
// 				}
// 			} else {
// 				if (block_score < 0.15) {
// 					block_score = 0.15;
// 				}
// 			}

// 			// Apply the final block reduction multiplier.

///////////////////////////UNUSED FROM VANILLA CHOCOLATE POISE, MIGHT COME BACK///////////////////////////////////////////////////
/*
	// Additional Damage Scaling:
	// Scales stagger based on non-physical damage sources (ex: enchantments, poisons,
	// elemental effects) by comparing total damage against physical damage.
	// Currently disabled because this system does not account for resistances.
	
	if (hitData->totalDamage > 0.0f && hitData->physicalDamage > 0.0f) {
		float damageRatio = hitData->totalDamage / hitData->physicalDamage;
		damageRatio = std::clamp(damageRatio, 0.0f, 3.0f);

		stagger *= damageRatio;
	}
	*/