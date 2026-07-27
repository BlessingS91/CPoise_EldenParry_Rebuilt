#include "Hooks/HitEventHandler.h"
#include <algorithm>

#include "Hooks/PoiseAV.h"
#include "Storage/Settings.h"

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

			auto multiplier = settings->JSONSettings["Weapons"]["Multipliers"][weaponType];

			if (multiplier != nullptr) {
				weaponMult = multiplier.get<float>();
			}

			break;
		}
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
	float weightFactor = ignoreWeight ? 0.0f : (a_weapon->weight * weightContrib);

	float basePoiseFactor = r2 + weightFactor;

	basePoiseFactor = std::clamp(basePoiseFactor, 0.0f, 1.0f);

	// 6. Map the final curve output to your target poise range (15.0f min to 70.0f max)
	float minPoise = 25.0f;
	float maxPoise = 75.0f;

	// Assuming r2 naturally spans from 0.0 to a theoretical ceiling,
	// we can lerp or scale it directly across your target poise bounds:
	float finalValue = std::lerp(minPoise, maxPoise, basePoiseFactor);

	if (settings->Debug.LogWeaponCalcs) {
		float outputValue = std::clamp(finalValue * weaponMult, 0.0f, 200.0f);

		logger::info(
			FMT_STRING(
				"[Weapon Poise] Weapon={} Type={} Damage={} Weight={} "
				"MinDamage={} MaxDamage={} Normalized={} "
				"CurveFactor={} WeightFactor={} BaseFactor={} "
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

	if (weapon->IsHandToHandMelee()) {
		float unarmedDamage = GetUnarmedDamage(aggressor);
		return unarmedDamage * settings->Damage.UnarmedMult;
	}

	float weaponDamage = GetWeaponDamage(weapon);

	if (settings->Debug.LogWeaponCalcs) {
		logger::info(
			FMT_STRING("[Melee Mult] Before={} Mult={} After={}"),
			weaponDamage,
			settings->Damage.MeleeMult,
			weaponDamage * settings->Damage.MeleeMult);
	}

	return weaponDamage * settings->Damage.MeleeMult;
}

float HitEventHandler::CalculateProjectileStagger(RE::Actor* aggressor, RE::Projectile* projectile)
{
	auto settings = Settings::GetSingleton();

	auto& data = projectile->GetProjectileRuntimeData();

	if (!data.weaponSource) {
		return 0.0f;
	}

	float bowDamage = GetWeaponDamage(data.weaponSource);

	float drawFactor = 1.0f;

	float bowSpeed = data.weaponSource->GetSpeed();
	if (bowSpeed > 0.0f) {
		drawFactor =
			1.0f +
			((1.0f / bowSpeed) - 1.0f) *
				settings->Damage.BowDrawSpeedMult;
	}

	float arrowDamage = 0.0f;

	if (data.ammoSource) {
		arrowDamage =
			data.ammoSource->GetRuntimeData().data.damage;
	}

	float arrowContribution =
		arrowDamage * settings->Damage.ArrowDamageMult;

	float stagger =
		(bowDamage * drawFactor) +
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
			"[Bow Calc] Weapon={} Ammo={} "
			"BowDamage={:.2f} BowSpeed={:.3f} DrawFactor={:.3f} "
			"ArrowDamage={:.2f} ArrowMult={:.3f} ArrowContribution={:.2f} "
			"BowBonus={:.2f} FinalStagger={:.2f}",
			data.weaponSource->GetName(),
			data.ammoSource ? data.ammoSource->GetName() : "NULL",
			bowDamage,
			bowSpeed,
			drawFactor,
			arrowDamage,
			settings->Damage.ArrowDamageMult,
			arrowDamage * settings->Damage.ArrowDamageMult,
			bowBonus,
			stagger);
	}

	return stagger;
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

bool HitEventHandler::IsCreature(RE::Actor* actor)
{
	if (!actor) {
		return false;
	}

	auto race = actor->GetRace();
	if (!race) {
		return false;
	}

	auto npcKeyword = RE::TESForm::LookupByID<RE::BGSKeyword>(0x00013794);

	if (!npcKeyword) {
		return false;
	}

	return !race->HasKeyword(npcKeyword);
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

	if (staggerOffset == 0.0f) {
		// Normal attack
		attackMult = settings->Damage.NormalAttackMult;
	} else if (staggerOffset == 1.0f) {
		// Power attack
		attackMult = settings->Damage.PowerAttackMult;
	}

	if (settings->Debug.LogStaggerCalcs) {
		logger::info(
			FMT_STRING("[Attack Data] Offset={} Before={} Mult={} After={}"),
			staggerOffset,
			stagger,
			attackMult,
			stagger * attackMult);
	}

	return stagger * attackMult;
}

float HitEventHandler::CalculateBashStagger(RE::Actor* aggressor)
{
	if (!aggressor) {
		return 0.0f;
	}

	auto settings = Settings::GetSingleton();

	float bashMultiplier = settings->Damage.BashMult;

	if (const auto perk = RE::TESForm::LookupByEditorID<RE::BGSPerk>(
			Milf::GetSingleton()->perks.SkullRattler_Perk);
		perk && aggressor->HasPerk(perk)) {
		bashMultiplier += 0.25f;
	}

	auto leftHand = aggressor->GetEquippedObject(true);
	auto rightHand = aggressor->GetEquippedObject(false);

	if (leftHand && leftHand->formType == RE::FormType::Armor) {
		return GetShieldDamage(leftHand->As<RE::TESObjectARMO>()) * bashMultiplier;
	}

	if (rightHand && rightHand->formType == RE::FormType::Weapon) {
		return GetWeaponDamage(rightHand->As<RE::TESObjectWEAP>()) * bashMultiplier;
	}

	return GetMiscDamage() * bashMultiplier;
}

float HitEventHandler::ApplyArmorReduction(RE::Actor* target, float stagger)
{
	auto settings = Settings::GetSingleton();
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

	return stagger;
}

float HitEventHandler::ApplyBlockingMultiplier(RE::HitData* hitData, RE::Actor* aggressor, RE::Actor* target, float stagger)
{
	auto settings = Settings::GetSingleton();

	float baseMult = 1.0f - hitData->percentBlocked;

	if (settings->Debug.LogArmorCalcs) {
		logger::info(FMT_STRING(
						 "Block Calc: PercentBlocked={} InitialBlockMult={}"),
			hitData->percentBlocked,
			baseMult);
	}

	PoiseAV::ApplyPerkEntryPoint(34, aggressor, target, &baseMult);
	PoiseAV::ApplyPerkEntryPoint(33, target, aggressor, &baseMult);

	if (settings->Debug.LogArmorCalcs) {
		logger::info(FMT_STRING(
						 "Block Calc After Perks: FinalBlockMult={} StaggerBefore={} StaggerAfter={}"),
			baseMult,
			stagger,
			stagger * baseMult);
	}

	return stagger * baseMult;
}

float HitEventHandler::RecalculateStagger(RE::Actor* target, RE::Actor* aggressor, RE::HitData* hitData)
{
	auto settings = Settings::GetSingleton();

	float stagger = 0.0f;

	auto sourceRef = hitData->sourceRef.get().get();

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
	else if (hitData->weapon) {
		auto weapon = hitData->weapon->As<RE::TESObjectWEAP>();

		if (!weapon) {
			return 0.0f;
		}

		if (weapon->IsHandToHandMelee()) {
			if (!aggressor) {
				return 0.0f;
			}

			stagger =
				GetUnarmedDamage(aggressor) *
				settings->Damage.UnarmedMult;
		} else {
			stagger =
				CalculateWeaponStagger(aggressor, weapon);
		}
	}
	// ==========================
	// No Skill / Physical Damage
	// ==========================
	else if (hitData->skill == RE::ActorValue::kNone) {
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

		// ==========================
		// Environmental Damage
		// ==========================
		if (!aggressor) {
			logger::critical("[kNone ENVIRONMENTAL] No aggressor");

			stagger =
				hitData->physicalDamage *
				settings->Damage.CreatureMult;

			logger::critical(
				FMT_STRING(
					"[kNone ENVIRONMENTAL RESULT] Physical={} Mult={} Final={}"),
				hitData->physicalDamage,
				settings->Damage.CreatureMult,
				stagger);
		}

		// ==========================
		// Actor Physical Hit
		// ==========================
		else {
			logger::critical(
				FMT_STRING(
					"[kNone ACTOR] Aggressor={} FormID={:08X}"),
				aggressor->GetName(),
				aggressor->GetFormID());

			auto race = aggressor->GetRace();

			logger::critical(
				FMT_STRING(
					"[kNone ACTOR RACE] Race={}"),
				race ? race->GetName() : "NULL");

			logger::critical("[kNone] Before GetEquippedObject LEFT");

			auto leftHand = aggressor->GetEquippedObject(true);

			logger::critical("[kNone] After GetEquippedObject LEFT");

			logger::critical("[kNone] Before GetEquippedObject RIGHT");

			auto rightHand = aggressor->GetEquippedObject(false);

			logger::critical("[kNone] After GetEquippedObject RIGHT");

			logger::critical(
				FMT_STRING(
					"[kNone EQUIPMENT] "
					"Left={} LeftID={:08X} LeftType={} "
					"Right={} RightID={:08X} RightType={}"),
				leftHand ? leftHand->GetName() : "NULL",
				leftHand ? leftHand->GetFormID() : 0,
				leftHand ? magic_enum::enum_name(leftHand->GetFormType()) : "NULL",
				rightHand ? rightHand->GetName() : "NULL",
				rightHand ? rightHand->GetFormID() : 0,
				rightHand ? magic_enum::enum_name(rightHand->GetFormType()) : "NULL");

			RE::TESObjectARMO* shield = nullptr;

			// ==========================
			// Left Hand Shield Check
			// ==========================
			if (leftHand) {
				logger::critical(
					FMT_STRING(
						"[kNone LEFT CHECK] FormType={}"),
					magic_enum::enum_name(leftHand->GetFormType()));

				if (leftHand->GetFormType() == RE::FormType::Armor) {
					auto armor = leftHand->As<RE::TESObjectARMO>();

					logger::critical(
						FMT_STRING(
							"[kNone LEFT ARMOR] Armor={} IsNull={}"),
						armor ? armor->GetName() : "NULL",
						armor == nullptr);

					if (armor && armor->IsShield()) {
						logger::critical(
							"[kNone LEFT SHIELD FOUND] {}",
							armor->GetName());

						shield = armor;
					}
				}
			}

			// ==========================
			// Right Hand Shield Check
			// ==========================
			if (!shield && rightHand) {
				logger::critical(
					FMT_STRING(
						"[kNone RIGHT CHECK] FormType={}"),
					magic_enum::enum_name(rightHand->GetFormType()));

				if (rightHand->GetFormType() == RE::FormType::Armor) {
					auto armor = rightHand->As<RE::TESObjectARMO>();

					logger::critical(
						FMT_STRING(
							"[kNone RIGHT ARMOR] Armor={} IsNull={}"),
						armor ? armor->GetName() : "NULL",
						armor == nullptr);

					if (armor && armor->IsShield()) {
						logger::critical(
							"[kNone RIGHT SHIELD FOUND] {}",
							armor->GetName());

						shield = armor;
					}
				}
			}

			// ==========================
			// Shield Strike
			// ==========================
			if (shield) {
				logger::critical(
					"[kNone BRANCH] SHIELD STRIKE");

				float shieldDamage =
					GetShieldDamage(shield);

				stagger =
					shieldDamage *
					settings->Damage.BashMult;

				logger::critical(
					FMT_STRING(
						"[Shield Result] Shield={} Weight={} "
						"ShieldDamage={} BashMult={} Final={}"),
					shield->GetName(),
					shield->weight,
					shieldDamage,
					settings->Damage.BashMult,
					stagger);
			}

			// ==========================
			// Creature
			// ==========================
			else {
				bool creature = IsCreature(aggressor);

				logger::critical(
					FMT_STRING(
						"[kNone CREATURE CHECK] Result={}"),
					creature);

				if (creature) {
					logger::critical(
						"[kNone BRANCH] CREATURE ATTACK");

					stagger =
						hitData->physicalDamage *
						settings->Damage.CreatureMult;

					logger::critical(
						FMT_STRING(
							"[Creature Result] Physical={} Mult={} Final={}"),
						hitData->physicalDamage,
						settings->Damage.CreatureMult,
						stagger);
				}

				// ==========================
				// Unknown Actor
				// ==========================
				else {
					logger::critical(
						"[kNone BRANCH] UNKNOWN ACTOR");

					logger::critical(
						FMT_STRING(
							"[Unknown kNone] Target={} Aggressor={} "
							"TotalDamage={} PhysicalDamage={} Flags={} Source={}"),
						target ? target->GetName() : "NULL",
						aggressor ? aggressor->GetName() : "NULL",
						hitData->totalDamage,
						hitData->physicalDamage,
						hitData->flags.underlying(),
						sourceRef ? sourceRef->GetName() : "NULL");

					stagger = 0.0f;
				}
			}
		}
	}

	// ==========================
	// Unknown Attack
	// ==========================
	else {
		logger::debug("Unknown attack type");
		return 0.0f;
	}

	// Attack Multiplier
	stagger = ApplyAttackMultiplier(hitData, stagger);

	// Armor Multipliers
	stagger = ApplyArmorReduction(target, stagger);

	// Blocking Multiplier
	stagger = ApplyBlockingMultiplier(hitData, aggressor, target, stagger);

	// 			stagger *= block_score;
	// 		}
	// 	}
	// }
	//Final Log
	if (settings->Debug.LogStaggerCalcs) {
		logger::info(
			FMT_STRING("[RecalculateStagger END] Target={} Aggressor={} FinalStagger={}"),
			target ? target->GetName() : "NULL",
			aggressor ? aggressor->GetName() : "NULL",
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

	if (settings->Debug.LogStaggerCalcs) {
		logger::info(
			FMT_STRING(
				"[PreProcessHit] Target={} Aggressor={} Weapon={} Damage={} Flags={} Skill={} PhysicalDamage={}"),
			target ? target->GetName() : "NULL",
			hitData->aggressor ? hitData->aggressor.get()->GetName() : "NULL",
			hitData->weapon && hitData->weapon->GetFormType() == RE::FormType::Weapon ?
				hitData->weapon->GetName() :
				(hitData->weapon ? "<NonWeapon>" : "NULL"),
			hitData->totalDamage,
			hitData->flags.underlying(),
			std::string(magic_enum::enum_name(hitData->skill)),
			hitData->physicalDamage);
	}

	// Get the actor responsible for the hit.
	auto poiseAV = PoiseAV::GetSingleton();
	auto aggressor = hitData->aggressor ? hitData->aggressor.get().get() : nullptr;

	// Actor cannot receive poise damage
	if (!poiseAV->CanDamageActor(target)) {
		return;
	}
	// No attacker = environmental hit, skip for now
	if (!aggressor) {
		return;
	}
	if (!(target->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth) <= hitData->totalDamage)) {
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

		poiseAV->DamageAndCheckPoise(target, aggressor, poiseDamage, hitData);
	}

	// Disable Skyrim's vanilla stagger calculation.
	// This system replaces it with the custom poise-based stagger system above.
	hitData->stagger = 0;
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