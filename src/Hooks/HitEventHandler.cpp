#include "Hooks/HitEventHandler.h"
#include <algorithm>

#include "Hooks/PoiseAV.h"
#include "Storage/Settings.h"

float HitEventHandler::GetWeaponDamage(RE::TESObjectWEAP* a_weapon)
{
	auto settings = Settings::GetSingleton();
	for (int index = a_weapon->numKeywords - 1; index >= 0; index--) {
		if (a_weapon->keywords[index]) {
			std::string keyword = a_weapon->keywords[index]->formEditorID.c_str();
			auto        pos = keyword.find("WeapType");
			if (pos != 0)
				continue;
			std::string type = keyword.substr(pos + 8, keyword.length());
			if (type == "Bow" && a_weapon->weaponData.animationType == RE::WEAPON_TYPE::kCrossbow)
				type = "Crossbow";
			if (!type.empty()) {
				auto weaponDamage = settings->JSONSettings["Weapons"]["Damage"][type];
				if (weaponDamage != nullptr)
					return std::lerp(static_cast<float>(weaponDamage), a_weapon->weight, settings->Damage.WeightContribution);
			}
		}
	}
	return a_weapon->weight;
}

float HitEventHandler::GetUnarmedDamage(RE::Actor* a_actor)
{
	auto settings = Settings::GetSingleton();

	auto unarmedDamage = std::lerp(static_cast<float>(settings->JSONSettings["Weapons"]["Damage"]["HandToHandMelee"]), a_actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kUnarmedDamage), settings->Damage.UnarmedSkillContribution);
	auto gauntlet = a_actor->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kHands);

	return gauntlet ? std::lerp(unarmedDamage, unarmedDamage + gauntlet->weight, settings->Damage.GauntletWeightContribution) : unarmedDamage;
}

float HitEventHandler::GetShieldDamage(RE::TESObjectARMO* a_shield)
{
	auto settings = Settings::GetSingleton();
	auto shieldDamage = settings->JSONSettings["Weapons"]["Damage"]["Shield"];
	if (shieldDamage != nullptr)
		return std::lerp(static_cast<float>(shieldDamage), a_shield->weight, settings->Damage.WeightContribution);
	return a_shield->weight;
}

float HitEventHandler::GetMiscDamage()
{
	auto settings = Settings::GetSingleton();
	auto miscDamage = settings->JSONSettings["Weapons"]["Damage"]["Misc"];
	if (miscDamage != nullptr)
		return static_cast<float>(miscDamage);
	return 5.0f;
}

float HitEventHandler::ModActorBashMult(RE::Actor* aggressor)
{
	float a_value = 0.0f;

	if (const auto perk = RE::TESForm::LookupByEditorID<RE::BGSPerk>(Milf::GetSingleton()->perks.SkullRattler_Perk); perk) {
		if (aggressor->HasPerk(perk)) {
			a_value = 0.25f;
		}
	}

	return a_value;
}

float HitEventHandler::RecalculateStagger(RE::Actor* target, RE::Actor* aggressor, RE::HitData* hitData)
{
	auto  settings = Settings::GetSingleton();
	float stagger = 0.0;

	auto sourceRef = hitData->sourceRef.get().get();
	if (sourceRef) {
		if (sourceRef->AsProjectile() && sourceRef->AsProjectile()->GetProjectileRuntimeData().ammoSource && sourceRef->AsProjectile()->GetProjectileRuntimeData().weaponSource) {
			stagger = GetWeaponDamage(sourceRef->AsProjectile()->GetProjectileRuntimeData().weaponSource) * settings->Damage.BowMult;
			stagger *= 1.0f + aggressor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kBowStaggerBonus);
		} else
			logger::debug("Missed attack with sourceRef");
	} else if (hitData->weapon) {
		if (hitData->weapon->As<RE::TESObjectWEAP>()->IsHandToHandMelee()) {
			stagger = GetUnarmedDamage(aggressor) * settings->Damage.UnarmedMult;
		} else {
			stagger = GetWeaponDamage(hitData->weapon) * settings->Damage.MeleeMult;
		}
	} else if (hitData->skill == RE::ActorValue::kNone) {
		stagger = hitData->physicalDamage * settings->Damage.CreatureMult;
	} else if (hitData->skill == RE::ActorValue::kBlock) {
		auto leftHand = aggressor->GetEquippedObject(true);
		auto rightHand = aggressor->GetEquippedObject(false);
		if (leftHand && leftHand->formType == RE::FormType::Armor) {
			stagger = GetShieldDamage(leftHand->As<RE::TESObjectARMO>()) * (settings->Damage.BashMult + (ModActorBashMult(aggressor)));
		} else if (rightHand && rightHand->formType == RE::FormType::Weapon) {
			stagger = GetWeaponDamage(rightHand->As<RE::TESObjectWEAP>()) * (settings->Damage.BashMult + (ModActorBashMult(aggressor)));
		} else {
			stagger = GetMiscDamage() * (settings->Damage.BashMult + (ModActorBashMult(aggressor)));
		}
	} else {
		logger::debug("Unknown attack");
		return 0.0f;
	}

	auto attackData = hitData->attackData ? hitData->attackData.get() : nullptr;
	if (attackData) {
		stagger *= 1 + attackData->data.staggerOffset;
		logger::debug(FMT_STRING("Stagger Mult {}"), 1 * attackData->data.staggerOffset);
	}

	float baseMult = 1.0f - hitData->percentBlocked;
	logger::debug(FMT_STRING("Percent Blocked {}"), hitData->percentBlocked);

	PoiseAV::ApplyPerkEntryPoint(34, aggressor, target, &baseMult);
	PoiseAV::ApplyPerkEntryPoint(33, target, aggressor, &baseMult);

	stagger *= baseMult;

	// Explicit > 0.0f checks prevent division-by-zero (NaN/Infinity crash).
	// Uses raw physical damage ratio to prevent double-dipping armor mitigation.
	if (hitData->totalDamage > 0.0f && hitData->physicalDamage > 0.0f) {
		float damageRatio = hitData->totalDamage / hitData->physicalDamage;
		damageRatio = std::clamp(damageRatio, 0.0f, 3.0f);

		stagger *= damageRatio;
	}

	// --- Pure Hyperbolic Armor Reduction (ARR) ---
	float totalArmor = (std::max)(0.0f, static_cast<float>(target->GetActorRuntimeData().armorRating));

	// 1. Calculate r1 (1:1 mirror with Armor Rating Rescaled Health math)
	float r1 = (totalArmor / 100.0f) * settings->Health.ArmorMult;

	// 2. Hyperbolic reduction percentage (r2)
	float armorReduction = r1 / (1.0f + r1);

	float armorMult = 1.0f - armorReduction;
	if (armorMult < settings->Health.ArmorMultMin) {
		armorMult = settings->Health.ArmorMultMin;
	}

	stagger *= armorMult;

	if (stagger > 0.00) {
		if (hitData->flags.all(RE::HitData::Flag::kBlocked)) {
			if (const auto perk = RE::TESForm::LookupByEditorID<RE::BGSPerk>(Milf::GetSingleton()->perks.AlikrDance_Perk); perk && target->HasPerk(perk)) {
				bool weaponblock = hitData->flags.all(RE::HitData::Flag::kBlockWithWeapon);
				auto block_score = 1.0f - (target->AsActorValueOwner()->GetActorValue(RE::ActorValue::kBlock) / 100.0f);
				if (weaponblock) {
					if (block_score < 0.30) {
						block_score = 0.30;
					}
				} else {
					if (block_score < 0.15) {
						block_score = 0.15;
					}
				}
				stagger *= block_score;
			}
		}
	}

	return stagger;
}

inline float getDamageMult(bool is_victim_player)
{
	auto              difficulty = RE::PlayerCharacter::GetSingleton()->GetGameStatsData().difficulty;
	const std::vector diff_str = { "VE", "E", "N", "H", "VH", "L" };
	auto              setting_name = fmt::format("fDiffMultHP{}PC{}", is_victim_player ? "To" : "By", diff_str[difficulty]);
	auto              setting = RE::GameSettingCollection::GetSingleton()->GetSetting(setting_name.c_str());
	return setting->data.f;
}

void HitEventHandler::PreProcessHit(RE::Actor* target, RE::HitData* hitData)
{
	auto poiseAV = PoiseAV::GetSingleton();
	auto aggressor = hitData->aggressor ? hitData->aggressor.get().get() : nullptr;
	if (aggressor && poiseAV->CanDamageActor(target)) {
		if (!(target->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth) <= hitData->totalDamage)) {
			auto poiseDamage = RecalculateStagger(target, aggressor, hitData);
			poiseAV->DamageAndCheckPoise(target, aggressor, poiseDamage, hitData);
		}
		// auto poiseDamage = RecalculateStagger(target, aggressor, hitData);
		// poiseAV->DamageAndCheckPoise(target, aggressor, poiseDamage);
	}
	hitData->stagger = static_cast<uint32_t>(0.00);
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