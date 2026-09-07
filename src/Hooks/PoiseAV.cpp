#include "Hooks/PoiseAV.h"

#include "ActorValues/AVManager.h"
#include "Hooks/HitEventHandler.h"
#include "Storage/ActorCache.h"
#include "Storage/Settings.h"
#include "UI/PoiseAVHUD.h"

#undef max
#include <limits>
//#undef min

float PoiseAV::ApplyDamageModifiers(RE::Actor* a_target, float a_damage)
{
	auto settings = Settings::GetSingleton();

	if (!a_target) {
		return a_damage;
	}

	if (a_target->IsPlayerRef()) {
		a_damage *= settings->Global.ToPCMult;
	} else {
		a_damage *= settings->Global.ToNPCMult;
	}

	return a_damage;
}

bool PoiseAV::CanDamageActor(RE::Actor* a_actor)
{
	auto settings = Settings::GetSingleton();

	bool result = false;

	if (a_actor && a_actor->GetActorRuntimeData().currentProcess && !a_actor->IsChild()) {
		switch (settings->Modes.StaggerMode) {
		case 0:
			result = true;
			break;

		case 1:
			result = true;

			if (auto actorState = a_actor->AsActorState()) {
				result = !actorState->actorState2.staggered;
			}
			break;
		}
	}

	if (settings->Debug.LogActorCalcs && result != false) {
		logger::info(
			FMT_STRING("[CanDamageActor] Actor={} Mode={} Result={}"),
			a_actor ? a_actor->GetName() : "NULL",
			settings->Modes.StaggerMode,
			result);
	}

	return result;
}

float PoiseAV::GetBaseActorValue(RE::Actor* a_actor)
{
	auto settings = Settings::GetSingleton();

	if (!a_actor) {
		return settings->Health.BaseHealth;
	}

	std::string editorID;

	if (auto race = a_actor->GetRace()) {
		editorID = race->GetFormEditorID();
	}

	float health = settings->Health.BaseHealth;
	float mass = 1.0f;

	auto& races = settings->JSONSettings["Races"];

	mass = a_actor->AsActorValueOwner()->GetBaseActorValue(RE::ActorValue::kMass);

	if (!editorID.empty()) {
		auto raceIt = races.find(editorID);

		if (raceIt != races.end() && raceIt->is_number()) {
			mass = raceIt->get<float>();
		}
	}

	mass = std::clamp(mass, 0.5f, 10.0f);

	float massMultiplier = mass * settings->Health.MassMult;

	health *= massMultiplier;

	if (settings->Debug.LogActorCalcs && health != settings->Health.BaseHealth) {
		logger::info(
			FMT_STRING("[Poise Health Calc] Actor={} Race={} Base={} Mass={} MassMult={} Final={}"),
			a_actor->GetName(),
			editorID,
			settings->Health.BaseHealth,
			mass,
			settings->Health.MassMult,
			health);
	}

	return std::max(health, 0.0f);
}
//For Leonkingzz Elden Parry system, no extra math needed just use GetBaseActorValue calculations.
float PoiseAV::Score_GetBaseActorValue(RE::Actor* a_actor)
{
	return GetBaseActorValue(a_actor);
}

float PoiseAV::GetActorValueMax(RE::Actor* a_actor)

{
	return GetBaseActorValue(a_actor);
}

bool PoiseAV::IsActorPerformingAction(RE::Actor* a_actor)
{
	if (!a_actor) {
		return false;
	}

	if (auto actorState = a_actor->AsActorState()) {
		if (actorState->GetAttackState() != RE::ATTACK_STATE_ENUM::kNone) {
			return true;
		}
	}

	if (!a_actor->Get3D()) {
		return false;
	}

	bool castLeft = false;
	bool castRight = false;
	bool castDual = false;

	a_actor->GetGraphVariableBool("IsCastingLeft", castLeft);
	a_actor->GetGraphVariableBool("IsCastingRight", castRight);
	a_actor->GetGraphVariableBool("IsCastingDual", castDual);

	auto hasWardKeyword = [](RE::SpellItem* spell) {
		if (!spell) {
			return false;
		}

		auto keyword = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("MagicWard");
		return keyword && spell->HasKeyword(keyword);
	};

	auto leftSpell = a_actor->GetEquippedObject(true);
	auto rightSpell = a_actor->GetEquippedObject(false);

	bool leftWard =
		hasWardKeyword(leftSpell ? leftSpell->As<RE::SpellItem>() : nullptr);

	bool rightWard =
		hasWardKeyword(rightSpell ? rightSpell->As<RE::SpellItem>() : nullptr);

	// True dual cast
	if (castDual) {
		if (leftWard || rightWard) {
			if (Settings::GetSingleton()->Debug.LogStaggerCalcs) {
				logger::info("[AoO Check] {} dual casting ward, ignoring action", a_actor->GetName());
			}
			return false;
		}

		return true;
	}

	// Independent left/right casting
	if (castLeft && leftWard) {
		if (Settings::GetSingleton()->Debug.LogStaggerCalcs) {
			logger::info("[AoO Check] {} left ward casting, ignoring action", a_actor->GetName());
		}
		return false;
	}

	if (castRight && rightWard) {
		if (Settings::GetSingleton()->Debug.LogStaggerCalcs) {
			logger::info("[AoO Check] {} right ward casting, ignoring action", a_actor->GetName());
		}
		return false;
	}

	return castLeft || castRight;
}

float PoiseAV::ApplyAttackOfOpportunityMult(RE::Actor* a_target, float a_poiseDamage)
{
	auto settings = Settings::GetSingleton();

	if (!IsActorPerformingAction(a_target)) {
		return a_poiseDamage;
	}

	float beforeDamage = a_poiseDamage;

	a_poiseDamage *= settings->Attack.AttackOfOpportunityMult;

	if (settings->Debug.LogStaggerCalcs &&
		settings->Debug.LogActorCalcs &&
		beforeDamage != a_poiseDamage) {
		logger::info(
			FMT_STRING(
				"[Attack of Opportunity] "
				"Target={} "
				"Triggered={} "
				"AttackState={} "
				"Casting={} "
				"DamageBefore={} "
				"DamageAfter={}"),
			a_target->GetName(),
			IsActorPerformingAction(a_target),
			a_target->AsActorState() ?
				static_cast<int>(a_target->AsActorState()->GetAttackState()) :
				-1,
			GetBoolVariable(a_target, "IsInCastState"),
			beforeDamage,
			a_poiseDamage);
	}

	return a_poiseDamage;
}

float PoiseAV::GetWardPoiseReduction(RE::Actor* a_actor)
{
	if (!a_actor) {
		return 0.0f;
	}

	auto keyword = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("MagicWard");

	if (!keyword) {
		return 0.0f;
	}

	bool castLeft = false;
	bool castRight = false;
	bool castDual = false;

	a_actor->GetGraphVariableBool("IsCastingLeft", castLeft);
	a_actor->GetGraphVariableBool("IsCastingRight", castRight);
	a_actor->GetGraphVariableBool("IsCastingDual", castDual);

	auto settings = Settings::GetSingleton();

	float restoration = a_actor->AsActorValueOwner()->GetBaseActorValue(RE::ActorValue::kRestoration);

	auto checkWard = [&](RE::SpellItem* spell, const char* hand) {
		if (!spell || !spell->HasKeyword(keyword)) {
			return 0.0f;
		}

		if (spell->effects.empty() || !spell->effects[0]) {
			return 0.0f;
		}

		float magnitude = spell->effects[0]->effectItem.magnitude;

		float spellPart = magnitude * 0.005f;
		float restorationPart = restoration * 0.004f;

		float reduction = std::clamp(spellPart + restorationPart, 0.2f, 0.8f);

		if (settings->Debug.LogMagicEffectCalcs) {
			logger::info(
				"[Ward Poise Reduction] Actor={} Hand={} Spell={} Reduction={}%%",
				a_actor->GetName(),
				hand,
				spell->GetName(),
				reduction * 100.0f);
		}

		return reduction;
	};

	float reduction = 0.0f;

	auto leftSpell = a_actor->GetEquippedObject(true);
	auto rightSpell = a_actor->GetEquippedObject(false);

	if (castDual) {
		// True dual cast: either hand contributes
		reduction = std::max(
			checkWard(leftSpell ? leftSpell->As<RE::SpellItem>() : nullptr, "Dual Left"),
			checkWard(rightSpell ? rightSpell->As<RE::SpellItem>() : nullptr, "Dual Right"));
	} else {
		// Independent hand casts
		if (castLeft) {
			reduction = std::max(
				reduction,
				checkWard(leftSpell ? leftSpell->As<RE::SpellItem>() : nullptr, "Left"));
		}

		if (castRight) {
			reduction = std::max(
				reduction,
				checkWard(rightSpell ? rightSpell->As<RE::SpellItem>() : nullptr, "Right"));
		}
	}

	return reduction;
}

float PoiseAV::ApplyDifficultyScaling(RE::Actor* a_target, RE::Actor* a_aggressor, float a_poiseDamage)
{
	auto settings = Settings::GetSingleton();

	if (!a_target || !a_aggressor || a_target == a_aggressor || a_poiseDamage <= 0.0f) {
		return a_poiseDamage;
	}

	float rawDifficultyMult = settings->GetDamageMultiplier(a_aggressor, a_target);

	float damageMultiplier =
		1.0f + (rawDifficultyMult - 1.0f) * settings->Global.DifficultyScaling;

	float beforeDamage = a_poiseDamage;

	a_poiseDamage *= damageMultiplier;

	if (settings->Debug.LogStaggerCalcs && a_poiseDamage > 1.0f) {
		logger::info(
			FMT_STRING("[Difficulty Scaling] Target={} Before={} Difficulty={} Scaling={} FinalMult={} After={}"),
			a_target->GetName(),
			beforeDamage,
			rawDifficultyMult,
			settings->Global.DifficultyScaling,
			damageMultiplier,
			a_poiseDamage);
	}

	return a_poiseDamage;
}

float PoiseAV::ApplyLevelDifferenceScaling(RE::Actor* a_target, RE::Actor* a_aggressor, float a_poiseDamage)
{
	if (!a_target || !a_aggressor || a_poiseDamage <= 0.0f) {
		return a_poiseDamage;
	}

	auto settings = Settings::GetSingleton();

	int attackerLevel = a_aggressor->GetLevel();
	int targetLevel = a_target->GetLevel();

	int levelDifference = attackerLevel - targetLevel;

	// Attacker higher level = deal more poise damage
	// Attacker lower level = deal less poise damage
	float levelMult = 1.0f + (levelDifference * settings->Global.LevelDifferenceMult);

	// Prevent extreme scaling
	levelMult = std::clamp(levelMult, 0.7f, 1.3f);

	float beforeDamage = a_poiseDamage;

	a_poiseDamage *= levelMult;

	if (settings->Debug.LogStaggerCalcs &&
		beforeDamage > 1.0f &&
		std::abs(levelMult - 1.0f) > 0.001f) {
		logger::info(
			FMT_STRING("[Level Scaling] Attacker={} Level={} Target={} Level={} Difference={} Mult={} Before={} After={}"),
			a_aggressor->GetName(),
			attackerLevel,
			a_target->GetName(),
			targetLevel,
			levelDifference,
			levelMult,
			beforeDamage,
			a_poiseDamage);
	}

	return a_poiseDamage;
}

float PoiseAV::CheckImpact(RE::Actor* a_target, float a_poiseDamage, AVManager* avManager)
{
	if (!a_target || !avManager || a_poiseDamage <= 0.0f) {
		return 0.0f;
	}

	auto settings = Settings::GetSingleton();

	float maxPoise;

	{
		std::lock_guard<std::shared_mutex> lk(avManager->mtx);
		maxPoise = avManager->GetActorValueMax(g_avName, a_target);
	}

	float poiseDamagePercent = 0.0f;

	if (maxPoise > 0.0f) {
		poiseDamagePercent = a_poiseDamage / maxPoise;
	}

	std::string impactType = "None";

	if (poiseDamagePercent >= settings->Impact.Normal &&
		poiseDamagePercent < settings->Impact.Large) {
		impactType = "Normal";
		Cast_Spell(a_target, "BHR_Normal_Impact", 0.0f);

	} else if (poiseDamagePercent >= settings->Impact.Large &&
			   poiseDamagePercent < settings->Impact.Massive) {
		impactType = "Powerful";
		Cast_Spell(a_target, "BHR_Powerful_Impact", 0.0f);

	} else if (poiseDamagePercent >= settings->Impact.Massive) {
		impactType = "Seismic";
		Cast_Spell(a_target, "BHR_Seismic_Impact", 0.0f);
	}

	if (settings->Debug.LogStaggerCalcs) {
		if (a_poiseDamage > 1.0f) {
			logger::info(
				FMT_STRING(
					"[Impact Check] Target={} Damage={} Percent={} Impact={} Thresholds={}/{}/{}"),
				a_target->GetName(),
				a_poiseDamage,
				poiseDamagePercent,
				impactType,
				settings->Impact.Normal,
				settings->Impact.Large,
				settings->Impact.Massive);
		}
	}
	return poiseDamagePercent;
}

void PoiseAV::HandlePoiseBreak(RE::Actor* a_target, RE::Actor* a_aggressor, float a_impactPercent)
{
	if (!a_target) {
		return;
	}

	if (GetBoolVariable(a_target, "bKaputt_IsInKillMove")) {
		return;
	}

	auto settings = Settings::GetSingleton();

	a_target->AddToFaction(ForceFullBodyStagger, 0);

	float staggerMagnitude = std::clamp(
		0.5f + a_impactPercent,
		0.5f,
		2.0f);

	if (settings->Debug.LogStaggerCalcs) {
		logger::info(
			FMT_STRING(
				"[Poise Break] Target={} ImpactPercent={} Magnitude={}"),
			a_target->GetName(),
			a_impactPercent,
			staggerMagnitude);
	}

	TryStagger(
		a_target,
		staggerMagnitude,
		a_aggressor);
}

void PoiseAV::DamageAndCheckPoise(RE::Actor* a_target, RE::Actor* a_aggressor, float a_poiseDamage, [[maybe_unused]] RE::HitData* a_hitData)
{
	if (!a_target || std::abs(a_poiseDamage) <= 0.001f) {
		return;
	}

	if (!CanDamageActor(a_target)) {
		return;
	}

	auto settings = Settings::GetSingleton();
	auto avManager = AVManager::GetSingleton();

	float poiseDamagePercent = 0.0f;
	float initialPoiseDamage = a_poiseDamage;

	if (settings->Debug.LogStaggerCalcs && a_poiseDamage > 1.0f) {
		logger::info(
			FMT_STRING("[Poise Damage START] Target={}({:08X}) Aggressor={}({:08X}) IncomingDamage={}"),
			a_target->GetName(),
			a_target->GetFormID(),
			a_aggressor ? a_aggressor->GetName() : "NULL",
			a_aggressor ? a_aggressor->GetFormID() : 0,
			a_poiseDamage);
	}

	// Attack of Opportunity
	a_poiseDamage = ApplyAttackOfOpportunityMult(a_target, a_poiseDamage);
	float afterAoO = a_poiseDamage;

	float wardReduction = GetWardPoiseReduction(a_target);

	float afterWard = a_poiseDamage;

	if (wardReduction > 0.0f) {
		float beforeWard = a_poiseDamage;

		a_poiseDamage *= (1.0f - wardReduction);

		if (settings->Debug.LogMagicEffectCalcs) {
			logger::info(
				"[Ward Poise Reduction] Target={} Reduction={}%% Before={} After={}",
				a_target->GetName(),
				wardReduction * 100.0f,
				beforeWard,
				a_poiseDamage);
		}
	}

	// Level difference scaling
	if (a_poiseDamage > 0.0f && a_aggressor && a_target != a_aggressor) {
		a_poiseDamage = ApplyLevelDifferenceScaling(a_target, a_aggressor, a_poiseDamage);
	}
	float afterLevelScaling = a_poiseDamage;

	// Your custom difficulty scaling
	if (a_poiseDamage > 0.0f && a_aggressor && a_target != a_aggressor) {
		a_poiseDamage = ApplyDifficultyScaling(a_target, a_aggressor, a_poiseDamage);
	}
	float afterDifficulty = a_poiseDamage;

	// Global PC/NPC damage multiplier
	a_poiseDamage = ApplyDamageModifiers(
		a_target,
		a_poiseDamage);

	float afterDamageModifiers = a_poiseDamage;

	// For Honor Stamina multiplier
	if (a_poiseDamage > 0.0f && a_aggressor && a_target != a_aggressor) {
		const float staminaMultiplier =
			HitEventHandler::GetSingleton()->GetStaminaMultiplier(a_aggressor);

		const float beforeStamina = a_poiseDamage;

		if (settings->Debug.LogPerkCalcs &&
			std::abs(staminaMultiplier - 1.0f) > 0.001f) {
			logger::info(
				FMT_STRING(
					"[For Honor Stamina] "
					"Aggressor={} "
					"Multiplier={} "
					"Before={} "
					"After={}"),
				a_aggressor->GetName(),
				staminaMultiplier,
				beforeStamina,
				beforeStamina * staminaMultiplier);
		}

		a_poiseDamage *= staminaMultiplier;
	}

	float afterStamina = a_poiseDamage;

	// Vanilla stagger perks should be LAST

	// Vanilla stagger perks should be LAST
	if (a_poiseDamage > 0.0f && a_aggressor && a_target != a_aggressor) {
		auto logStaggerPerks = [](RE::Actor* actor, RE::BGSEntryPointPerkEntry::EntryPoint entryPoint, const char* label) {
			if (!actor) {
				return;
			}

			class StaggerPerkVisitor : public RE::PerkEntryVisitor
			{
			public:
				StaggerPerkVisitor(RE::Actor* a_actor, const char* a_label) :
					actor(a_actor),
					label(a_label)
				{}

				RE::PerkEntryVisitor::ReturnType Visit(RE::BGSPerkEntry* a_entry) override
				{
					auto perkEntry = skyrim_cast<RE::BGSEntryPointPerkEntry*>(a_entry);

					if (!perkEntry) {
						return RE::PerkEntryVisitor::ReturnType::kContinue;
					}

					auto perk = perkEntry->perk;

					logger::info(
						FMT_STRING("[{}] Actor={} Perk={} EntryPoint={}"),
						label,
						actor->GetName(),
						perk ? perk->GetName() : "NULL",
						static_cast<int>(perkEntry->entryData.entryPoint.underlying()));

					return RE::PerkEntryVisitor::ReturnType::kContinue;
				}

				RE::Actor*  actor;
				const char* label;
			};

			StaggerPerkVisitor visitor(actor, label);
			actor->ForEachPerkEntry(entryPoint, visitor);
		};

		float incomingMult = 1.0f;
		float targetMult = 1.0f;

		// Detailed perk enumeration only
		if (settings->Debug.LogPerkCalcs) {
			logStaggerPerks(
				a_aggressor,
				static_cast<RE::BGSEntryPointPerkEntry::EntryPoint>(34),
				"Incoming");
		}

		// Always apply vanilla incoming stagger perks
		ApplyPerkEntryPoint(
			34,
			a_aggressor->As<RE::Character>(),
			a_target->As<RE::Character>(),
			&incomingMult);

		// Detailed perk enumeration only
		if (settings->Debug.LogPerkCalcs) {
			logStaggerPerks(
				a_target,
				static_cast<RE::BGSEntryPointPerkEntry::EntryPoint>(33),
				"Target");
		}

		// Always apply vanilla target stagger perks
		ApplyPerkEntryPoint(
			33,
			a_target->As<RE::Character>(),
			a_aggressor->As<RE::Character>(),
			&targetMult);

		float vanillaStaggerMult = incomingMult * targetMult;

		if (settings->Debug.LogPerkCalcs &&
			std::abs(vanillaStaggerMult - 1.0f) > 0.001f) {
			logger::info(
				FMT_STRING(
					"[Vanilla Perk Final] "
					"IncomingMult={} TargetMult={} Combined={} Before={} After={}"),
				incomingMult,
				targetMult,
				vanillaStaggerMult,
				a_poiseDamage,
				a_poiseDamage * vanillaStaggerMult);
		}

		// Always apply final vanilla multiplier
		a_poiseDamage *= vanillaStaggerMult;
	}

	// Apply final calculated poise damage.
	a_poiseDamage = std::max(a_poiseDamage, 0.0f);
	poiseDamagePercent = CheckImpact(a_target, a_poiseDamage, avManager);
	if (settings->Debug.LogStaggerCalcs && a_poiseDamage > 1.0f) {
		logger::info(
			FMT_STRING(
				"[Poise Stages] Target={} Initial={} AoO={} Ward={} Level={} Difficulty={} PC/NPC={} Final={}"),
			*a_target->GetName(),
			initialPoiseDamage,
			afterAoO,
			afterWard,
			afterLevelScaling,
			afterDifficulty,
			afterDamageModifiers,
			a_poiseDamage);
	}

	{
		std::lock_guard<std::shared_mutex> lk(avManager->mtx);
		avManager->DamageActorValue(g_avName, a_target, a_poiseDamage);
	}

	if (a_poiseDamage > 0.0f) {
		regenDelays[a_target->GetFormID()] = settings->Health.RegenDelay;
	}

	float poise;

	{
		std::lock_guard<std::shared_mutex> lk(avManager->mtx);
		poise = avManager->GetActorValue(g_avName, a_target);
	}

	// Check if the hit depleted the target's remaining poise.
	if (poise <= 0.0f) {
		HandlePoiseBreak(
			a_target,
			a_aggressor,
			poiseDamagePercent);
	}

	if (settings->Debug.LogStaggerCalcs && a_poiseDamage > 1.0f) {
		float currentPoise;
		float maxPoise;

		{
			std::lock_guard<std::shared_mutex> lk(avManager->mtx);
			currentPoise = avManager->GetActorValue(g_avName, a_target);
			maxPoise = avManager->GetActorValueMax(g_avName, a_target);
		}

		logger::info(
			FMT_STRING(
				"[Poise Damage] Target={} Damage={} CurrentPoise={} / {}"),
			a_target->GetName(),
			a_poiseDamage,
			currentPoise,
			maxPoise);
	}
}

void PoiseAV::Update(RE::Actor* a_actor, float a_delta)
{
	auto settings = Settings::GetSingleton();
	if (!a_actor) {
		return;
	}
	auto& runtime = a_actor->GetActorRuntimeData();

	if (settings->Debug.LogActorCalcs &&
		(!runtime.currentProcess ||
			!runtime.currentProcess->InHighProcess() ||
			!a_actor->Is3DLoaded())) {
		logger::info(
			FMT_STRING("[Poise Update Failed] Actor={} Process={} High={} 3D={}"),
			a_actor->GetName(),
			runtime.currentProcess != nullptr,
			runtime.currentProcess ? runtime.currentProcess->InHighProcess() : false,
			a_actor->Is3DLoaded());
	}

	if (!runtime.currentProcess || !runtime.currentProcess->InHighProcess() || !a_actor->Is3DLoaded()) {
		return;
	}

	auto* avManager = AVManager::GetSingleton();
	//auto* settings = Settings::GetSingleton();

	if (PoiseAVHUD::trueHUDInterface && settings->TrueHUD.SpecialBar) {
		if (!CanDamageActor(a_actor)) {
			PoiseAVHUD::trueHUDInterface->OverrideSpecialBarColor(
				a_actor->GetHandle(),
				TRUEHUD_API::BarColorType::BarColor,
				settings->TrueHUD.SpecialBarDepletedColor);
		} else {
			PoiseAVHUD::trueHUDInterface->OverrideSpecialBarColor(
				a_actor->GetHandle(),
				TRUEHUD_API::BarColorType::BarColor,
				settings->TrueHUD.SpecialBarNormalColor);
		}
	}

	float currentPoise;
	{
		std::lock_guard<std::shared_mutex> lk(avManager->mtx);
		currentPoise = avManager->GetActorValue(g_avName, a_actor);
	}

	if (currentPoise <= 0.0f) {
		auto actorState = a_actor->AsActorState();

		if (actorState && actorState->actorState2.staggered) {
			{
				std::lock_guard<std::shared_mutex> lk(avManager->mtx);
				avManager->RestoreActorValueToMax(g_avName, a_actor);
			}

			if (PoiseAVHUD::trueHUDInterface && settings->TrueHUD.SpecialBar) {
				PoiseAVHUD::trueHUDInterface->FlashActorSpecialBar(SKSE::GetPluginHandle(), a_actor->GetHandle(), true);
			}
			RemoveFromFaction(a_actor, ForceFullBodyStagger);
		} else {
			if (!GetBoolVariable(a_actor, "bKaputt_IsInKillMove") &&
				actorState &&
				!actorState->actorState2.staggered) {
				TryStagger(a_actor, 0.5f, nullptr);
			}
		}
	} else {
		auto formID = a_actor->GetFormID();

		auto it = regenDelays.find(formID);
		if (it != regenDelays.end()) {
			it->second -= a_delta;

			if (it->second > 0.0f) {
				return;
			}

			regenDelays.erase(it);
		}

		float maxPoise;
		{
			std::lock_guard<std::shared_mutex> lk(avManager->mtx);
			maxPoise = avManager->GetActorValueMax(g_avName, a_actor);
		}

		float regenAmount = maxPoise * settings->Health.RegenRate * a_delta;

		{
			std::lock_guard<std::shared_mutex> lk(avManager->mtx);
			avManager->RestoreActorValue(g_avName, a_actor, regenAmount);
		}
	}
}

void PoiseAV::GarbageCollection()
{
	auto*                              avManager = AVManager::GetSingleton();
	std::lock_guard<std::shared_mutex> lk(avManager->mtx);

	for (auto it = avManager->avStorage.begin(); it != avManager->avStorage.end();) {
		try {
			auto  formID = static_cast<RE::FormID>(std::stoul(it.key()));
			auto* form = RE::TESForm::LookupByID(formID);
			auto* actor = form ? form->As<RE::Actor>() : nullptr;

			if (actor) {
				auto& runtime = actor->GetActorRuntimeData();

				if (runtime.currentProcess &&
					runtime.currentProcess->InHighProcess() &&
					actor->Is3DLoaded()) {
					++it;
					continue;
				}
			}

			it = avManager->avStorage.erase(it);
		} catch (const std::invalid_argument&) {
			logger::error("Bad input: invalid FormID in AV storage");
			it = avManager->avStorage.erase(it);
		} catch (const std::out_of_range&) {
			logger::error("FormID out of range in AV storage");
			it = avManager->avStorage.erase(it);
		}
	}

	// Clean stale poise regeneration delay entries.
	for (auto it = regenDelays.begin(); it != regenDelays.end();) {
		auto* form = RE::TESForm::LookupByID(it->first);
		auto* actor = form ? form->As<RE::Actor>() : nullptr;

		if (actor) {
			auto& runtime = actor->GetActorRuntimeData();

			if (runtime.currentProcess &&
				runtime.currentProcess->InHighProcess() &&
				actor->Is3DLoaded()) {
				++it;
				continue;
			}
		}

		it = regenDelays.erase(it);
	}
}
