#include "EldenParry.h"
#include "Hooks/HitEventHandler.h"
#include "Hooks/PoiseAV.h"
#include "Settings.h"
#include "Utils.hpp"
#include <Windows.h>
using uniqueLocker = std::unique_lock<std::shared_mutex>;
using sharedLocker = std::shared_lock<std::shared_mutex>;

void EldenParry::init()
{
	_isAvailable = GetModuleHandleW(L"EldenParry.dll") != nullptr;

	if (!_isAvailable) {
		return;
	}
	logger::info("Obtaining precision API...");
	_precision_API = reinterpret_cast<PRECISION_API::IVPrecision1*>(PRECISION_API::RequestPluginAPI());
	if (_precision_API) {
		logger::info("Precision API successfully obtained.");
		EldenSettings::facts::isPrecisionAPIObtained = true;
		if (_precision_API->AddPreHitCallback(SKSE::GetPluginHandle(), precisionPrehitCallbackFunc) ==
			PRECISION_API::APIResult::OK) {
			logger::info("Successfully registered precision API prehit callback.");
		}
		// _precision_API->AddPostHitCallback(SKSE::GetPluginHandle(), HitEventHandler::PoiseCallback_Post);
	} else {
		logger::info("Precision API not found.");
	}
	logger::info("Obtaining Valhalla Combat API...");
	_ValhallaCombat_API = reinterpret_cast<VAL_API::IVVAL1*>(VAL_API::RequestPluginAPI());
	if (_ValhallaCombat_API) {
		logger::info("Valhalla Combat API successfully obtained.");
		EldenSettings::facts::isValhallaCombatAPIObtained = true;
	} else {
		logger::info("Valhalla Combat API not found.");
	}
	//read parry sound
	auto data = RE::TESDataHandler::GetSingleton();
	_parrySound_shd = data->LookupForm<RE::BGSSoundDescriptorForm>(0x80E, "Chocolate Poise - Reforged.esp");
	_parrySound_wpn = data->LookupForm<RE::BGSSoundDescriptorForm>(0x80F, "Chocolate Poise - Reforged.esp");
	logger::info("Parry sounds loaded: SHD={} WPN={}",
		fmt::ptr(_parrySound_shd),
		fmt::ptr(_parrySound_wpn));
	if (!_parrySound_shd || !_parrySound_wpn) {
		logger::error("Parry sound not found in Chocolate Poise - Reforged.esp");
	}

	//read fcombatHitConeAngle
	_GMST_fCombatHitConeAngle = RE::GameSettingCollection::GetSingleton()->GetSetting("fCombatHitConeAngle")->GetFloat();
	_parryAngle = _GMST_fCombatHitConeAngle;
}

void EldenParry::update()
{
	if (!_bUpdate) {
		return;
	}
	uniqueLocker lock(mtx_parryTimer);
	auto         it = _parryTimer.begin();
	if (it == _parryTimer.end()) {
		_bUpdate = false;
		return;
	}
	while (it != _parryTimer.end()) {
		if (!it->first) {
			it = _parryTimer.erase(it);
			continue;
		}
		if (it->second > EldenSettings::fParryWindow_End) {
			it = _parryTimer.erase(it);
			continue;
		}
		//*static float* g_deltaTime = (float*)RELOCATION_ID(523660, 410199).address();*/          // 2F6B948
		it->second += g_deltaTime;
		it++;
	}
}

float EldenParry::calculateRiposteReflex(RE::Actor* a_actor)
{
	float a_value = 0.0f;

	if (!Utils::isEquippedShield(a_actor)) {
		a_value += 0.1f;
	}
	if (const auto perk = RE::TESForm::LookupByEditorID<RE::BGSPerk>(Milf::GetSingleton()->perks.QuickReflexes_Perk); perk) {
		if (!a_actor->HasPerk(perk)) {
			a_value += 0.1f;
		}
	}
	return a_value;
}

void EldenParry::startTimingParry(RE::Actor* a_actor)
{
	uniqueLocker lock(mtx_parryTimer);
	auto         it = _parryTimer.find(a_actor);
	if (it != _parryTimer.end()) {
		it->second = 0;
	} else {
		_parryTimer.insert({ a_actor, calculateRiposteReflex(a_actor) });
	}

	_bUpdate = true;
}

void EldenParry::finishTimingParry(RE::Actor* a_actor)
{
	uniqueLocker lock(mtx_parryTimer);
	_parryTimer.erase(a_actor);
}

/// <summary>
/// Check if the object is in the blocker's blocking angle.
/// </summary>
/// <param name="a_blocker"></param>
/// <param name="a_obj"></param>
/// <returns>True if the object is in blocker's blocking angle.</returns>
bool EldenParry::inBlockAngle(RE::Actor* a_blocker, RE::TESObjectREFR* a_obj)
{
	auto angle = a_blocker->GetHeadingAngle(a_obj->GetPosition(), false);
	return (angle <= _parryAngle && angle >= -_parryAngle);
}
/// <summary>
/// Check if the actor is in parry state i.e. they are able to parry the incoming attack/projectile.
/// </summary>
/// <param name="a_actor"></param>
/// <returns></returns>
bool EldenParry::inParryState(RE::Actor* a_actor)
{
	sharedLocker lock(mtx_parryTimer);
	auto         it = _parryTimer.find(a_actor);
	if (it != _parryTimer.end()) {
		return it->second >= EldenSettings::fParryWindow_Start;
	}
	return false;
}

bool EldenParry::canParry(RE::Actor* a_parrier, RE::TESObjectREFR* a_obj)
{
	//logger::info("{}",a_parrier->GetName());
	return inParryState(a_parrier) && inBlockAngle(a_parrier, a_obj);
}

bool EldenParry::processMeleeParry(RE::Actor* a_attacker, RE::Actor* a_parrier)
{
	if (canParry(a_parrier, a_attacker)) {
		playParryEffects(a_parrier);
		Utils::triggerStagger(a_parrier, a_attacker);
		if (EldenSettings::facts::isValhallaCombatAPIObtained) {
			_ValhallaCombat_API->processStunDamage(VAL_API::STUNSOURCE::parry, nullptr, a_parrier, a_attacker, 0);
		}
		if (a_parrier->IsPlayerRef()) {
			RE::PlayerCharacter::GetSingleton()->AddSkillExperience(RE::ActorValue::kBlock, EldenSettings::fMeleeParryExp);
		}
		if (EldenSettings::bSuccessfulParryNoCost) {
			negateParryCost(a_parrier);
		}
		send_melee_parry_event(a_attacker);
		return true;
	}

	return false;
}

/// <summary>
/// Process a projectile parry; Return if the parry is successful.
/// </summary>
/// <param name="a_parrier"></param>
/// <param name="a_projectile"></param>
/// <param name="a_projectile_collidable"></param>
/// <returns>True if the projectile parry is successful.</returns>
bool EldenParry::processProjectileParry(RE::Actor* a_parrier, RE::Projectile* a_projectile, RE::hkpCollidable* a_projectile_collidable)
{
	if (canParry(a_parrier, a_projectile)) {
		RE::TESObjectREFR* shooter = nullptr;
		if (a_projectile->GetProjectileRuntimeData().shooter && a_projectile->GetProjectileRuntimeData().shooter.get()) {
			shooter = a_projectile->GetProjectileRuntimeData().shooter.get().get();
		}

		Utils::resetProjectileOwner(a_projectile, a_parrier, a_projectile_collidable);

		if (shooter && shooter->Is3DLoaded()) {
			Utils::RetargetProjectile(a_projectile, shooter);
		} else {
			Utils::ReflectProjectile(a_projectile);
		}

		playParryEffects(a_parrier);
		if (a_parrier->IsPlayerRef()) {
			RE::PlayerCharacter::GetSingleton()->AddSkillExperience(RE::ActorValue::kBlock, EldenSettings::fProjectileParryExp);
		}
		if (EldenSettings::bSuccessfulParryNoCost) {
			negateParryCost(a_parrier);
		}
		send_ranged_parry_event();
		return true;
	}
	return false;
}

void EldenParry::processGuardBash(RE::Actor* a_basher, RE::Actor* a_blocker)
{
	if (!a_blocker->IsBlocking() || !inBlockAngle(a_blocker, a_basher) || a_blocker->AsActorState()->GetAttackState() == RE::ATTACK_STATE_ENUM::kBash) {
		return;
	}
	Utils::triggerStagger(a_basher, a_blocker);
	playGuardBashEffects(a_basher);
	RE::PlayerCharacter::GetSingleton()->AddSkillExperience(RE::ActorValue::kBlock, EldenSettings::fGuardBashExp);
}

void EldenParry::playParryEffects(RE::Actor* a_parrier)
{
	if (EldenSettings::bEnableParrySoundEffect) {
		if (Utils::isEquippedShield(a_parrier)) {
			if (_parrySound_shd) {
				Utils::playSound(a_parrier, _parrySound_shd);
			}
		} else {
			if (_parrySound_wpn) {
				Utils::playSound(a_parrier, _parrySound_wpn);
			}
		}
	}
	if (EldenSettings::bEnableParrySparkEffect) {
		blockSpark::playBlockSpark(a_parrier);
	}
	if (a_parrier->IsPlayerRef()) {
		if (EldenSettings::bEnableSlowTimeEffect) {
			Utils::slowTime(0.2f, 0.3f);
		}
		if (EldenSettings::bEnableScreenShakeEffect) {
			inlineUtils::shakeCamera(1.5, a_parrier->GetPosition(), 0.4f);
		}
	}
}

void EldenParry::applyParryCost(RE::Actor* a_actor)
{
	//logger::logger::info("apply parry cost for {}", a_actor->GetName());
	std::lock_guard<std::shared_mutex> lock(mtx_parryCostQueue);
	std::lock_guard<std::shared_mutex> lock2(mtx_parrySuccessActors);
	if (_parryCostQueue.contains(a_actor)) {
		if (!_parrySuccessActors.contains(a_actor)) {
			inlineUtils::damageAv(a_actor, RE::ActorValue::kStamina, _parryCostQueue[a_actor]);
		}
		_parryCostQueue.erase(a_actor);
	}
	_parrySuccessActors.erase(a_actor);
}

void EldenParry::cacheParryCost(RE::Actor* a_actor, float a_cost)
{
	//logger::logger::info("cache parry cost for {}: {}", a_actor->GetName(), a_cost);
	std::lock_guard<std::shared_mutex> lock(mtx_parryCostQueue);
	_parryCostQueue[a_actor] = a_cost;
}

void EldenParry::negateParryCost(RE::Actor* a_actor)
{
	//logger::logger::info("negate parry cost for {}", a_actor->GetName());
	std::lock_guard<std::shared_mutex> lock(mtx_parrySuccessActors);
	_parrySuccessActors.insert(a_actor);
}

void EldenParry::playGuardBashEffects(RE::Actor* a_actor)
{
	if (EldenSettings::bEnableParrySoundEffect && _parrySound_shd) {
		Utils::playSound(a_actor, _parrySound_shd);
	}
	if (EldenSettings::bEnableParrySparkEffect) {
		blockSpark::playBlockSpark(a_actor);
	}
	if (a_actor->IsPlayerRef()) {
		if (EldenSettings::bEnableSlowTimeEffect) {
			Utils::slowTime(0.2f, 0.3f);
		}
		if (EldenSettings::bEnableScreenShakeEffect) {
			inlineUtils::shakeCamera(1.5, a_actor->GetPosition(), 0.4f);
		}
	}
}

void EldenParry::send_melee_parry_event(RE::Actor* a_attacker)
{
	SKSE::ModCallbackEvent modEvent{
		RE::BSFixedString("EP_MeleeParryEvent"),
		RE::BSFixedString(),
		0.0f,
		a_attacker
	};

	SKSE::GetModCallbackEventSource()->SendEvent(&modEvent);
	//logger::info("Sent melee parry event");
}

void EldenParry::send_ranged_parry_event()
{
	SKSE::ModCallbackEvent modEvent{
		RE::BSFixedString("EP_RangedParryEvent"),
		RE::BSFixedString(),
		0.0f,
		nullptr
	};

	SKSE::GetModCallbackEventSource()->SendEvent(&modEvent);
	//logger::info("Sent ranged parry event");
}

PRECISION_API::PreHitCallbackReturn EldenParry::precisionPrehitCallbackFunc(const PRECISION_API::PrecisionHitData& a_precisionHitData)
{
	PRECISION_API::PreHitCallbackReturn returnData;
	if (!a_precisionHitData.target || !a_precisionHitData.target->Is(RE::FormType::ActorCharacter)) {
		return returnData;
	}
	if (EldenParry::GetSingleton()->processMeleeParry(a_precisionHitData.attacker, a_precisionHitData.target->As<RE::Actor>())) {
		returnData.bIgnoreHit = true;
	}
	return returnData;
}

const RE::TESObjectWEAP* const EldenParry::GetAttackWeapon(RE::AIProcess* const aiProcess)
{
	if (aiProcess && aiProcess->high && aiProcess->high->attackData &&
		!aiProcess->high->attackData->data.flags.all(RE::AttackData::AttackFlag::kBashAttack)) {
		const RE::TESForm* equipped = aiProcess->high->attackData->IsLeftAttack() ? aiProcess->GetEquippedLeftHand() : aiProcess->GetEquippedRightHand();

		if (equipped) {
			return equipped->As<RE::TESObjectWEAP>();
		}
	}

	return nullptr;
}

float EldenParry::GetScore(RE::Actor* actor, const Milf::Scores& scoreSettings)
{
	float score = 0.0f;

	auto race = actor->GetRace();
	auto raceFormID = race ? race->formID : 0;

	auto weaponAI = actor->GetActorRuntimeData().currentProcess;

	if (weaponAI && weaponAI->high && weaponAI->high->attackData) {
		const RE::TESForm* weaponL = weaponAI->high->attackData->IsLeftAttack() ? weaponAI->GetEquippedLeftHand() : weaponAI->GetEquippedRightHand();

		// auto weapon = weaponL->IsWeapon() ? weaponL->As<RE::TESObjectWEAP>() :

		// RE::TESObjectWEAP* weapon = nullptr;

		// bool WeaponShield = false;

		// a_aggressor->GetActorRuntimeData().currentProcess

		// Utils::UGetAttackWeapon(a_aggressor->GetActorRuntimeData().currentProcess)
		// Utils::isHumanoid(actor)

		// return equipped->As<RE::TESObjectWEAP>();

		if (weaponL) {
			if (weaponL->IsWeapon()) {
				auto weapon = (weaponL->As<RE::TESObjectWEAP>());
				switch (weapon->GetWeaponType()) {
				case RE::WEAPON_TYPE::kOneHandSword:
					score += scoreSettings.oneHandSwordScore;
					break;
				case RE::WEAPON_TYPE::kOneHandAxe:
					score += scoreSettings.oneHandAxeScore;
					break;
				case RE::WEAPON_TYPE::kOneHandMace:
					score += scoreSettings.oneHandMaceScore;
					break;
				case RE::WEAPON_TYPE::kOneHandDagger:
					score += scoreSettings.oneHandDaggerScore;
					break;
				case RE::WEAPON_TYPE::kTwoHandAxe:
					score += scoreSettings.twoHandAxeScore;
					break;
				case RE::WEAPON_TYPE::kTwoHandSword:
					score += scoreSettings.twoHandSwordScore;
					break;
				case RE::WEAPON_TYPE::kHandToHandMelee:
					score += -100.0f;
					break;
				case RE::WEAPON_TYPE::kBow:
					score += -50.0f;
					break;
				case RE::WEAPON_TYPE::kCrossbow:
					score += -40.0f;
					break;
				case RE::WEAPON_TYPE::kStaff:
					score += 5.0f;
					break;
				}
				const auto actorValue = weapon->weaponData.skill.get();
				switch (actorValue) {
				case RE::ActorValue::kOneHanded:
					score += (scoreSettings.weaponSkillWeight *
							  actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kOneHanded));
					break;
				case RE::ActorValue::kTwoHanded:
					score += (scoreSettings.weaponSkillWeight *
							  actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kTwoHanded));
					break;
				default:
					// Do nothing
					break;
				}

			} else {
				score += 70.0f;
				score += (scoreSettings.weaponSkillWeight *
						  actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kBlock));
			}

		} else {
			// score += (scoreSettings.weaponSkillWeight *
			// 			  actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kUnarmedDamage));
			if (weaponAI) {
				score += (2.0f *
						  weaponAI->GetUserData()->CalcUnarmedDamage());

				// weaponAI->cachedValues->cachedDPS
			}
		}

	} else {
		// score += (scoreSettings.weaponSkillWeight *
		// 			  actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kUnarmedDamage));
		if (weaponAI) {
			score += (2.0f *
					  weaponAI->GetUserData()->CalcUnarmedDamage());

			// weaponAI->cachedValues->cachedDPS
		}
	}

	score += (0.35f * actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina));

	if (raceFormID == 0x13743 || raceFormID == 0x88840) {
		score += scoreSettings.altmerScore;
	} else if (raceFormID == 0x13740 || raceFormID == 0x8883A) {
		score += scoreSettings.argonianScore;
	} else if (raceFormID == 0x13749 || raceFormID == 0x88884) {
		score += scoreSettings.bosmerScore;
	} else if (raceFormID == 0x13741 || raceFormID == 0x8883C) {
		score += scoreSettings.bretonScore;
	} else if (raceFormID == 0x13742 || raceFormID == 0x8883D) {
		score += scoreSettings.dunmerScore;
	} else if (raceFormID == 0x13744 || raceFormID == 0x88844) {
		score += scoreSettings.imperialScore;
	} else if (raceFormID == 0x13745 || raceFormID == 0x88845) {
		score += scoreSettings.khajiitScore;
	} else if (raceFormID == 0x13746 || raceFormID == 0x88794) {
		score += scoreSettings.nordScore;
	} else if (raceFormID == 0x13747 || raceFormID == 0xA82B9) {
		score += scoreSettings.orcScore;
	} else if (raceFormID == 0x13748 || raceFormID == 0x88846) {
		score += scoreSettings.redguardScore;
	} else {
		score += (PoiseAV::GetSingleton()->Score_GetBaseActorValue(actor));
	}

	if (Utils::isHumanoid(actor)) {
		const auto actorBase = actor->GetActorBase();
		if (actorBase && actorBase->IsFemale()) {
			score += scoreSettings.femaleScore;
		}
	}

	if (inlineUtils::isPowerAttacking(actor)) {
		score += scoreSettings.powerAttackScore;
	}

	if (actor->IsPlayerRef()) {
		score += scoreSettings.playerScore;
	}

	return score;
}

float EldenParry::AttackerBeatsParry(RE::Actor* attacker, RE::Actor* target)
{
	// if (!Milf::GetSingleton()->core.useScoreSystem)
	// {
	// 	// The score-based system has been disabled in INI, so attackers can never overpower parries
	// 	return false;
	// }
	const float attackerScore = GetScore(attacker, Milf::GetSingleton()->scores);
	const float targetScore = GetScore(target, Milf::GetSingleton()->scores);

	return (((targetScore - attackerScore) / targetScore) * 100.0f);  // >= Milf::GetSingleton()->scores.scoreDiffThreshold);
}

Milf* Milf::GetSingleton()
{
	static Milf singleton;
	return std::addressof(singleton);
}

void Milf::Load()
{
	constexpr auto path = "Data\\SKSE\\Plugins\\EldenRiposteSystem.ini";

	CSimpleIniA ini;
	ini.SetUnicode();

	ini.LoadFile(path);

	core.Load(ini);
	scores.Load(ini);
	perks.Load(ini);

	ini.SaveFile(path);
}

void Milf::Core::Load(CSimpleIniA& a_ini)
{
	static const char* section = "Core";

	detail::get_value(a_ini, useScoreSystem, section, "UseScoreSystem",
		";Use the score-based system to allow certain attacks to go through and ignore parries.");
}

void Milf::Scores::Load(CSimpleIniA& a_ini)
{
	static const char* section = "Scores";

	detail::get_value(a_ini, scoreDiffThreshold, section, "ScoreDiffThreshold",
		";If the difference in scores is at least equal to this threshold, attacks are not parried.");

	detail::get_value(a_ini, weaponSkillWeight, section, "WeaponSkillWeight",
		";Weapon Skill is multiplied by this weight and then added to the score.");

	detail::get_value(a_ini, oneHandDaggerScore, section, "OneHandDaggerScore",
		";Bonus score for attacks with daggers.");
	detail::get_value(a_ini, oneHandSwordScore, section, "OneHandSwordScore",
		";Bonus score for attacks with one-handed swords.");
	detail::get_value(a_ini, oneHandAxeScore, section, "OneHandAxeScore",
		";Bonus score for attacks with one-handed axes.");
	detail::get_value(a_ini, oneHandMaceScore, section, "OneHandMaceScore",
		";Bonus score for attacks with one-handed maces.");
	detail::get_value(a_ini, oneHandKatanaScore, section, "OneHandKatanaScore",
		";Bonus score for attacks with katanas (from Animated Armoury).");
	detail::get_value(a_ini, oneHandRapierScore, section, "OneHandRapierScore",
		";Bonus score for attacks with rapiers (from Animated Armoury).");
	detail::get_value(a_ini, oneHandClawsScore, section, "OneHandClawsScore",
		";Bonus score for attacks with claws (from Animated Armoury).");
	detail::get_value(a_ini, oneHandWhipScore, section, "OneHandWhipScore",
		";Bonus score for attacks with whips (from Animated Armoury).");
	detail::get_value(a_ini, twoHandSwordScore, section, "TwoHandSwordScore",
		";Bonus score for attacks with two-handed swords.");
	detail::get_value(a_ini, twoHandAxeScore, section, "TwoHandAxeScore",
		";Bonus score for attacks with two-handed axes.");
	detail::get_value(a_ini, twoHandWarhammerScore, section, "TwoHandWarhammerScore",
		";Bonus score for attacks with two-handed warhammers.");
	detail::get_value(a_ini, twoHandPikeScore, section, "TwoHandPikeScore",
		";Bonus score for attacks with two-handed pikes (from Animated Armoury).");
	detail::get_value(a_ini, twoHandHalberdScore, section, "TwoHandHalberdScore",
		";Bonus score for attacks with two-handed halberds (from Animated Armoury).");
	detail::get_value(a_ini, twoHandQuarterstaffScore, section, "TwoHandQuarterstaffScore",
		";Bonus score for attacks with two-handed quarterstaffs (from Animated Armoury).");

	detail::get_value(a_ini, altmerScore, section, "AltmerScore",
		";Bonus score for Altmer.");
	detail::get_value(a_ini, argonianScore, section, "ArgonianScore",
		";Bonus score for Argonians.");
	detail::get_value(a_ini, bosmerScore, section, "BosmerScore",
		";Bonus score for Bosmer.");
	detail::get_value(a_ini, bretonScore, section, "BretonScore",
		";Bonus score for Bretons.");
	detail::get_value(a_ini, dunmerScore, section, "DunmerScore",
		";Bonus score for Dunmer.");
	detail::get_value(a_ini, imperialScore, section, "ImperialScore",
		";Bonus score for Imperials.");
	detail::get_value(a_ini, khajiitScore, section, "KhajiitScore",
		";Bonus score for Khajiit.");
	detail::get_value(a_ini, nordScore, section, "NordScore",
		";Bonus score for Nords.");
	detail::get_value(a_ini, orcScore, section, "OrcScore",
		";Bonus score for Orcs.");
	detail::get_value(a_ini, redguardScore, section, "RedguardScore",
		";Bonus score for Redguard.");

	detail::get_value(a_ini, femaleScore, section, "FemaleScore",
		";Bonus score for female characters.");

	detail::get_value(a_ini, powerAttackScore, section, "PowerAttackScore",
		";Bonus score for power attacks.");

	detail::get_value(a_ini, playerScore, section, "PlayerScore", ";Bonus score for the Player.");
}

void Milf::Perks::Load(CSimpleIniA& a_ini)
{
	static const char* section = "Perks";

	auto DS = GetSingleton();

	DS->perks.EldenParry_Perk1 = a_ini.GetValue(section, "EldenParry_Perk1", DS->perks.EldenParry_Perk1.c_str());
	DS->perks.EldenParry_Perk2 = a_ini.GetValue(section, "EldenParry_Perk2", DS->perks.EldenParry_Perk2.c_str());
	DS->perks.EldenParry_Spell1 = a_ini.GetValue(section, "EldenParry_Spell1", DS->perks.EldenParry_Spell1.c_str());
	DS->perks.EldenParry_Spell2 = a_ini.GetValue(section, "EldenParry_Spell2", DS->perks.EldenParry_Spell2.c_str());
	DS->perks.QuickReflexes_Perk = a_ini.GetValue(section, "QuickReflexes_Perk", DS->perks.QuickReflexes_Perk.c_str());
	DS->perks.DragonsTail_Perk = a_ini.GetValue(section, "DragonsTail_Perk", DS->perks.DragonsTail_Perk.c_str());
	DS->perks.Deliverance_Perk = a_ini.GetValue(section, "Deliverance_Perk", DS->perks.Deliverance_Perk.c_str());
	DS->perks.SkullRattler_Perk = a_ini.GetValue(section, "SkullRattler_Perk", DS->perks.SkullRattler_Perk.c_str());
	DS->perks.AlikrDance_Perk = a_ini.GetValue(section, "AlikrDance_Perk", DS->perks.AlikrDance_Perk.c_str());

	a_ini.SetValue(section, "EldenParry_Perk1", DS->perks.EldenParry_Perk1.c_str(), ";EDID of the Elden parry Perk - 1st Level");
	a_ini.SetValue(section, "EldenParry_Perk2", DS->perks.EldenParry_Perk2.c_str(), ";EDID of the Elden parry Perk - 2nd Level");
	a_ini.SetValue(section, "EldenParry_Spell1", DS->perks.EldenParry_Spell1.c_str(), ";EDID of the Elden parry buff - 1st Level");
	a_ini.SetValue(section, "EldenParry_Spell2", DS->perks.EldenParry_Spell2.c_str(), ";EDID of the Elden parry buff - 2nd Level");
	a_ini.SetValue(section, "QuickReflexes_Perk", DS->perks.QuickReflexes_Perk.c_str(), ";EDID of the Quick Reflexes Perk");
	a_ini.SetValue(section, "DragonsTail_Perk", DS->perks.DragonsTail_Perk.c_str(), ";EDID of the Dragon's Tail Perk");
	a_ini.SetValue(section, "Deliverance_Perk", DS->perks.Deliverance_Perk.c_str(), ";EDID of the Deliverance Perk");
	a_ini.SetValue(section, "SkullRattler_Perk", DS->perks.SkullRattler_Perk.c_str(), ";EDID of the Skull Rattler Perk");
	a_ini.SetValue(section, "AlikrDance_Perk", DS->perks.AlikrDance_Perk.c_str(), ";EDID of the Alik'r Dance Perk");
}