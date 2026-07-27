#include "Hooks/HitEventHandler.h"
#include <algorithm>

#include "Hooks/PoiseAV.h"
#include "Storage/Settings.h"

float HitEventHandler::GetWeaponDamage(RE::TESObjectWEAP* a_weapon)
{
	/* old weapon json
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
	}*/

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

			auto multiplier = settings->JSONSettings["Weapons"]["Multipliers"][weaponType];

			if (multiplier != nullptr) {
				weaponMult = multiplier.get<float>();
			}

			break;
		}
	}

	if (!a_weapon || !_minWeapon || !_maxWeapon) {
		return a_weapon ? a_weapon->weight : 0.0f;  // Fallback if data isn't ready
	}

	// 1. Get settings singleton for configuration multipliers
	float weightContrib = settings ? settings->Damage.WeightContribution : 0.005f;

	// 2. Get base attack damage stats from the cached baseline weapons
	float minDamage = _minWeapon->GetAttackDamage();
	float maxDamage = _maxWeapon->GetAttackDamage() * 7.0f;
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

	float r1 = normalizedDamage * 2.5f;
	float r2 = r1 / (1.0f + r1);

	// 5. Multiply the rescaled damage factor directly by the flat weight contribution multiplier
	float basePoiseFactor = r2 + (a_weapon->weight * weightContrib);
	basePoiseFactor = std::clamp(basePoiseFactor, 0.0f, 1.0f);

	// 6. Map the final curve output to your target poise range (15.0f min to 70.0f max)
	float minPoise = 25.0f;
	float maxPoise = 75.0f;

	// Assuming r2 naturally spans from 0.0 to a theoretical ceiling,
	// we can lerp or scale it directly across your target poise bounds:
	float finalValue = std::lerp(minPoise, maxPoise, basePoiseFactor);

	if (settings->Debug.LogWeaponCalcs) {
		logger::info(
			FMT_STRING(
				"[Weapon Poise] Weapon={} Type={} Damage={} Weight={} "
				"MinDamage={} MaxDamage={} Normalized={} "
				"ARRFactor={} WeightFactor={} BaseFactor={} "
				"WeaponMult={} Final={}"),
			a_weapon->GetName(),
			weaponType,
			currentDamage,
			a_weapon->weight,
			minDamage,
			maxDamage,
			normalizedDamage,
			r2,
			a_weapon->weight * weightContrib,
			basePoiseFactor,
			weaponMult,
			finalValue);
	}

	return std::clamp(finalValue * weaponMult, 0.0f, 200.0f);
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
	auto settings = Settings::GetSingleton();

	float stagger = 0.0f;

	auto sourceRef = hitData->sourceRef.get().get();

	logger::info(
		"[Recalculate START] Target={} Aggressor={} WeaponPtr={} Skill={}",
		target ? target->GetName() : "NULL",
		aggressor ? aggressor->GetName() : "NULL",
		fmt::ptr(hitData->weapon),
		static_cast<int>(hitData->skill));

	// ==========================
	// Bow / Projectile Attacks
	// ==========================
	if (sourceRef && sourceRef->AsProjectile()) {
		auto  projectile = sourceRef->AsProjectile();
		auto& projectileData = projectile->GetProjectileRuntimeData();

		if (projectileData.ammoSource && projectileData.weaponSource) {
			// Bow damage is already converted through GetWeaponDamage()
			// using the normal weapon poise curve.
			float bowDamage = GetWeaponDamage(projectileData.weaponSource);

			// Arrow damage acts as the ranged equivalent of melee weapon weight.
			// It provides additional impact force without replacing the bow's
			// primary damage scaling.
			float arrowDamage =
				static_cast<float>(projectileData.ammoSource->data.damage);

			// Arrow impact contribution.
			// Lower values make bow quality matter more.
			// Higher values make arrow upgrades matter more.
			float arrowContribution = arrowDamage * settings->Damage.ArrowContribution;

			// Combine bow force and projectile impact.
			stagger = bowDamage + arrowContribution;

			// Skyrim actor value bonus for bow stagger.
			float bowStaggerBonus =
				aggressor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kBowStaggerBonus);

			stagger *= 1.0f + bowStaggerBonus;

			if (settings->Debug.LogWeaponCalcs) {
				logger::info(
					FMT_STRING(
						"[Bow Calc] Weapon={} BowDamage={} ArrowDamage={} "
						"ArrowContribution={} BowBonus={} FinalStagger={}"),
					projectileData.weaponSource->GetName(),
					bowDamage,
					arrowDamage,
					arrowContribution,
					bowStaggerBonus,
					stagger);
			}
		}
	}
	// ==========================
	// Weapon Attacks
	// ==========================
	if (hitData->weapon && hitData->weapon->formType != RE::FormType::Weapon) {
		logger::info(
			"[Skipping Non Weapon Hit] FormType={} Skill={} Flags={}",
			hitData->weapon->GetFormEditorID(),
			static_cast<int>(hitData->skill),
			hitData->flags.underlying());

		return 0.0f;
	}

	else if (hitData->weapon) {
		auto weapon = hitData->weapon->As<RE::TESObjectWEAP>();

		if (!weapon) {
			logger::info(
				"[Skipping Non Weapon Hit] FormType={} Skill={} Flags={}",
				static_cast<int>(hitData->weapon->GetFormType()),
				static_cast<int>(hitData->skill),
				hitData->flags.underlying());
			return 0.0f;
		}

		if (weapon->IsHandToHandMelee()) {
			float unarmedDamage = GetUnarmedDamage(aggressor);
			stagger = unarmedDamage * settings->Damage.UnarmedMult;
		} else {
			float weaponDamage = GetWeaponDamage(weapon);
			stagger = weaponDamage * settings->Damage.MeleeMult;
		}
	}

	// ==========================
	// Creature Attacks
	// ==========================
	else if (hitData->skill == RE::ActorValue::kNone) {
		stagger =
			hitData->physicalDamage *
			settings->Damage.CreatureMult;
	}

	// ==========================
	// Bash Attacks
	// ==========================
	else if (hitData->skill == RE::ActorValue::kBlock) {
		auto leftHand = aggressor->GetEquippedObject(true);
		auto rightHand = aggressor->GetEquippedObject(false);

		float bashMultiplier =
			settings->Damage.BashMult +
			ModActorBashMult(aggressor);

		// Shield bash
		if (leftHand && leftHand->formType == RE::FormType::Armor) {
			stagger =
				GetShieldDamage(leftHand->As<RE::TESObjectARMO>()) * bashMultiplier;
		}

		// Weapon bash
		else if (rightHand && rightHand->formType == RE::FormType::Weapon) {
			stagger =
				GetWeaponDamage(rightHand->As<RE::TESObjectWEAP>()) * bashMultiplier;
		}

		// Generic bash
		else {
			stagger =
				GetMiscDamage() * bashMultiplier;
		}
	}

	// ==========================
	// Unknown Attack
	// ==========================
	else {
		logger::debug("Unknown attack type");
		return 0.0f;
	}

	// Power Attack Multiplier
	auto attackData = hitData->attackData ? hitData->attackData.get() : nullptr;

	if (attackData) {
		float attackMult = 1.0f + attackData->data.staggerOffset;

		if (attackData->data.staggerOffset >= 1.0f) {
			attackMult *= settings->Damage.PowerAttackMult;
		}

		if (settings->Debug.LogStaggerCalcs) {
			logger::info(
				FMT_STRING("[Attack Data] staggerOffset={} FinalMult={}"),
				attackData->data.staggerOffset,
				attackMult);
		}

		stagger *= attackMult;
	}

	// Blocking Mult
	float baseMult = 1.0f - hitData->percentBlocked;

	if (Settings::GetSingleton()->Debug.LogStaggerCalcs) {
		logger::info(FMT_STRING(
						 "Block Calc: PercentBlocked={} InitialBlockMult={}"),
			hitData->percentBlocked,
			baseMult);
	}

	PoiseAV::ApplyPerkEntryPoint(34, aggressor, target, &baseMult);
	PoiseAV::ApplyPerkEntryPoint(33, target, aggressor, &baseMult);

	if (Settings::GetSingleton()->Debug.LogStaggerCalcs) {
		logger::info(FMT_STRING(
						 "Block Calc After Perks: FinalBlockMult={} StaggerBefore={} StaggerAfter={}"),
			baseMult,
			stagger,
			stagger * baseMult);
	}

	stagger *= baseMult;

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

	// --- Pure Hyperbolic Armor Reduction (ARR) ---
	float totalArmor = (std::max)(0.0f,
		static_cast<float>(target->GetActorRuntimeData().armorRating));

	float r1 = (totalArmor / 100.0f) * settings->Health.ArmorMult * 5.0f;

	float armorReduction = r1 / (1.0f + r1);

	float armorMult = 1.0f - armorReduction;

	bool minCapApplied = false;

	if (armorMult < settings->Health.ArmorMultMin) {
		armorMult = settings->Health.ArmorMultMin;
		minCapApplied = true;
	}

	float preArmorStagger = stagger;

	stagger *= armorMult;

	if (settings->Debug.LogArmorCalcs) {
		logger::info(
			FMT_STRING(
				"[Armor Calc] Target={} Armor={} "
				"BaseStagger={} r1={} Reduction={} "
				"RawMult={} FinalMult={} MinCap={} FinalStagger={}"),
			target->GetName(),
			totalArmor,
			preArmorStagger,
			r1,
			armorReduction,
			1.0f - armorReduction,
			armorMult,
			minCapApplied,
			stagger);
	}

	if (stagger > 0.00) {
		// Apply additional block-based stagger reduction only when the hit was blocked.
		if (hitData->flags.all(RE::HitData::Flag::kBlocked)) {
			// Check if the target has the perk that enables this custom blocking behavior.
			if (const auto perk = RE::TESForm::LookupByEditorID<RE::BGSPerk>(Milf::GetSingleton()->perks.AlikrDance_Perk); perk && target->HasPerk(perk)) {
				// Determine whether the block was performed with a weapon or a shield.
				bool weaponblock = hitData->flags.all(RE::HitData::Flag::kBlockWithWeapon);

				// Convert the Block skill into a stagger multiplier.
				// Higher Block skill results in greater stagger reduction.
				auto block_score = 1.0f - (target->AsActorValueOwner()->GetActorValue(RE::ActorValue::kBlock) / 100.0f);

				// Enforce minimum stagger values to prevent blocking from completely
				// negating stagger. Weapon blocks have a higher minimum than shield blocks.
				if (weaponblock) {
					if (block_score < 0.30) {
						block_score = 0.30;
					}
				} else {
					if (block_score < 0.15) {
						block_score = 0.15;
					}
				}

				// Apply the final block reduction multiplier.
				stagger *= block_score;
			}
		}
	}

	if (settings->Debug.LogStaggerCalcs) {
		logger::info(
			FMT_STRING("[RecalculateStagger END] Target={} Aggressor={} FinalStagger={}"),
			target ? target->GetName() : "NULL",
			aggressor ? aggressor->GetName() : "NULL",
			stagger);
	}
	return stagger;
}

// Retrieves Skyrim's difficulty-based damage multiplier.
// If the victim is the player, returns the "damage taken by player" multiplier (fDiffMultHPToPC).
// Otherwise, returns the "damage dealt by player" multiplier (fDiffMultHPByPC).
// Used to keep poise damage scaling consistent with Skyrim's difficulty settings.
inline float getDamageMult(bool is_victim_player)
{
	auto              difficulty = RE::PlayerCharacter::GetSingleton()->GetGameStatsData().difficulty;
	const std::vector diff_str = { "VE", "E", "N", "H", "VH", "L" };

	auto setting_name = fmt::format("fDiffMultHP{}PC{}", is_victim_player ? "To" : "By", diff_str[difficulty]);
	auto setting = RE::GameSettingCollection::GetSingleton()->GetSetting(setting_name.c_str());

	return setting->data.f;
}

void HitEventHandler::PreProcessHit(RE::Actor* target, RE::HitData* hitData)
{
	if (!target || !hitData) {
		return;
	}

	logger::info(
		"[HitData] Target={} Aggressor={} Weapon={} Damage={} Stagger={} Flags={} Skill={} PhysicalDamage={}",
		target ? target->GetName() : "NULL",
		hitData->aggressor ? hitData->aggressor.get()->GetName() : "NULL",
		hitData->weapon && hitData->weapon->GetFormType() == RE::FormType::Weapon ?
			hitData->weapon->GetName() :
			(hitData->weapon ? "<NonWeapon>" : "NULL"),
		hitData->totalDamage,
		hitData->stagger,
		hitData->flags.underlying(),
		std::string(magic_enum::enum_name(hitData->skill)),
		hitData->physicalDamage);

	auto poiseAV = PoiseAV::GetSingleton();

	// Get the actor responsible for the hit.
	auto aggressor = hitData->aggressor ? hitData->aggressor.get().get() : nullptr;

	if (!aggressor && hitData->totalDamage <= 0.0f) {
		return;
	}

	if (aggressor && poiseAV->CanDamageActor(target)) {
		// Skip poise damage on killing blows.
		// Prevents a final hit from also triggering unnecessary stagger/poise effects.
		if (!(target->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth) <= hitData->totalDamage)) {
			// Convert the incoming hit into custom poise damage.
			// Handles weapon damage, attack type multipliers, blocking,
			// armor scaling, and other stagger calculations.DamageAndCheckPoise
			auto poiseDamage = RecalculateStagger(target, aggressor, hitData);

			// Apply damage to the target's poise health and check for stagger/break events.
			logger::info(
				"[Poise Call] Target={} Aggressor={} HitData={} Damage={}",
				target->GetName(),
				aggressor ? aggressor->GetName() : "NULL",
				fmt::ptr(hitData),
				poiseDamage);
			poiseAV->DamageAndCheckPoise(target, aggressor, poiseDamage, hitData);
		}
	}

	// Disable Skyrim's vanilla stagger calculation.
	// This system replaces it with the custom poise-based stagger system above.
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