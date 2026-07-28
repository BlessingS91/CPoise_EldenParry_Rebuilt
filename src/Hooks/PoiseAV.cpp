#include "Hooks/PoiseAV.h"

#include "ActorValues/AVManager.h"
#include "Storage/ActorCache.h"
#include "Storage/Settings.h"
#include "UI/PoiseAVHUD.h"

#undef max
#include <limits>
//#undef min

float PoiseAV::ApplyDamageModifiers(RE::Actor* a_aggressor, RE::Actor* a_target, float a_damage)
{
	auto settings = Settings::GetSingleton();

	if (!a_aggressor || !a_target) {
		return a_damage;
	}

	float baseMult = 1.0f;

	ApplyPerkEntryPoint(34, a_aggressor, a_target, &baseMult);
	ApplyPerkEntryPoint(33, a_target, a_aggressor, &baseMult);

	a_damage *= baseMult;

	if (a_target->IsPlayerRef()) {
		a_damage *= settings->Damage.ToPCMult;
	} else {
		a_damage *= settings->Damage.ToNPCMult;
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
			if (auto actorState = a_actor->AsActorState()) {
				result = !actorState->actorState2.staggered;
			}
			break;
		}
	}

	if (settings->Debug.LogActorCalcs) {
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
		return settings->Health.BaseMult;
	}

	std::string editorID;

	if (auto race = a_actor->GetRace()) {
		editorID = race->GetFormEditorID();
	}

	float health = settings->Health.BaseMult;
	float mass = 1.0f;

	auto raceMass = settings->JSONSettings["Races"][editorID];

	if (!editorID.empty() && raceMass != nullptr) {
		mass = static_cast<float>(raceMass);
	} else {
		mass = a_actor->AsActorValueOwner()->GetBaseActorValue(RE::ActorValue::kMass);
	}

	mass = std::clamp(mass, 0.5f, 10.0f);

	float massMultiplier = mass * settings->Health.MassMult;

	health *= massMultiplier;

	if (settings->Debug.LogActorCalcs && health != settings->Health.BaseMult) {
		logger::info(
			FMT_STRING("[Poise Health Calc] Actor={} Race={} Base={} Mass={} MassMult={} Final={}"),
			a_actor->GetName(),
			editorID,
			settings->Health.BaseMult,
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

	bool isCasting = false;

	a_actor->GetGraphVariableBool("IsInCastState", isCasting);

	if (isCasting) {
		return true;
	}

	bool castRight = false;
	bool castLeft = false;
	bool castDual = false;

	a_actor->GetGraphVariableBool("IsCastingRight", castRight);
	a_actor->GetGraphVariableBool("IsCastingLeft", castLeft);
	a_actor->GetGraphVariableBool("IsCastingDual", castDual);

	return castRight || castLeft || castDual;
}

float PoiseAV::ApplyAttackOfOpportunityMult(RE::Actor* a_target, float a_poiseDamage)
{
	auto settings = Settings::GetSingleton();

	if (!IsActorPerformingAction(a_target)) {
		return a_poiseDamage;
	}

	float beforeDamage = a_poiseDamage;

	a_poiseDamage *= settings->Damage.AttackOfOpportunityMult;

	if (settings->Debug.LogStaggerCalcs) {
		logger::info(
			FMT_STRING("[Attack of Oppourtunity Mult] Target={} Mult={} Before={} After={}"),
			a_target->GetName(),
			settings->Damage.AttackOfOpportunityMult,
			beforeDamage,
			a_poiseDamage);
	}

	return a_poiseDamage;
}

float PoiseAV::ApplyDifficultyScaling(RE::Actor* a_target, RE::Actor* a_aggressor, float a_poiseDamage)
{
	auto settings = Settings::GetSingleton();

	if (!a_target || !a_aggressor || a_target == a_aggressor || a_poiseDamage <= 0.0f) {
		return a_poiseDamage;
	}

	float rawDifficultyMult = settings->GetDamageMultiplier(a_aggressor, a_target);

	float damageMultiplier =
		1.0f + (rawDifficultyMult - 1.0f) * settings->Damage.PoiseScaling;

	float beforeDamage = a_poiseDamage;

	a_poiseDamage *= damageMultiplier;

	if (a_target->IsPlayerRef()) {
		a_poiseDamage *= settings->Damage.ToPCMult;
	} else {
		a_poiseDamage *= settings->Damage.ToNPCMult;
	}

	if (settings->Debug.LogStaggerCalcs) {
		logger::info(
			FMT_STRING("[Difficulty Scaling] Target={} Before={} Difficulty={} Scaling={} FinalMult={} After={}"),
			a_target->GetName(),
			beforeDamage,
			rawDifficultyMult,
			settings->Damage.PoiseScaling,
			damageMultiplier,
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

	if (poiseDamagePercent >= settings->Damage.NormalImpactThreshold &&
		poiseDamagePercent < settings->Damage.PowerfulImpactThreshold) {
		impactType = "Normal";
		Cast_Spell(a_target, "BHR_Normal_Impact", 0.0f);

	} else if (poiseDamagePercent >= settings->Damage.PowerfulImpactThreshold &&
			   poiseDamagePercent < settings->Damage.SeismicImpactThreshold) {
		impactType = "Powerful";
		Cast_Spell(a_target, "BHR_Powerful_Impact", 0.0f);

	} else if (poiseDamagePercent >= settings->Damage.SeismicImpactThreshold) {
		impactType = "Seismic";
		Cast_Spell(a_target, "BHR_Seismic_Impact", 0.0f);
	}

	if (settings->Debug.LogStaggerCalcs) {
		logger::info(
			FMT_STRING(
				"[Impact Check] Target={} Damage={} Percent={} Impact={} Thresholds={}/{}/{}"),
			a_target->GetName(),
			a_poiseDamage,
			poiseDamagePercent,
			impactType,
			settings->Damage.NormalImpactThreshold,
			settings->Damage.PowerfulImpactThreshold,
			settings->Damage.SeismicImpactThreshold);
	}
	return poiseDamagePercent;
}

void PoiseAV::HandlePoiseBreak(RE::Actor* a_target, RE::Actor* a_aggressor, float a_poiseDamage, float a_poiseDamagePercent, float a_currentPoise, AVManager* avManager)
{
	if (!a_target || !avManager) {
		return;
	}

	auto settings = Settings::GetSingleton();

	a_target->AddToFaction(ForceFullBodyStagger, 0);

	if (GetBoolVariable(a_target, "bKaputt_IsInKillMove")) {
		return;
	}

	float safeStaggerMag = std::clamp(a_poiseDamagePercent, 0.0f, 2.0f);

	if (settings->Debug.LogStaggerCalcs) {
		float maxPoise;

		{
			std::lock_guard<std::shared_mutex> lk(avManager->mtx);
			maxPoise = avManager->GetActorValueMax(g_avName, a_target);
		}

		logger::info(
			FMT_STRING(
				"[Poise Break] Target={} "
				"PoiseDamage={} "
				"PoiseRemaining={} "
				"MaxPoise={} "
				"DamagePercent={} "
				"StaggerMagnitude={}"),
			a_target->GetName(),
			a_poiseDamage,
			a_currentPoise,
			maxPoise,
			a_poiseDamagePercent,
			safeStaggerMag);
	}

	TryStagger(a_target, safeStaggerMag, a_aggressor);

	a_target->SetGraphVariableBool("bPoise_IsStaggered", false);
}

void PoiseAV::DamageAndCheckPoise(RE::Actor* a_target, RE::Actor* a_aggressor, float a_poiseDamage, [[maybe_unused]] RE::HitData* a_hitData)
{
	if (!a_target || std::abs(a_poiseDamage) <= 0.001f) {
		return;
	}

	auto settings = Settings::GetSingleton();
	auto avManager = AVManager::GetSingleton();

	float poiseDamagePercent = 0.0f;
	float initialPoiseDamage = a_poiseDamage;

	if (settings->Debug.LogStaggerCalcs) {
		logger::info(
			FMT_STRING("[Poise Damage START] Target={}({:08X}) Aggressor={}({:08X}) IncomingDamage={}"),
			a_target->GetName(),
			a_target->GetFormID(),
			a_aggressor ? a_aggressor->GetName() : "NULL",
			a_aggressor ? a_aggressor->GetFormID() : 0,
			a_poiseDamage);
	}

	//Attack of Oppourtunity Mult
	a_poiseDamage = ApplyAttackOfOpportunityMult(a_target, a_poiseDamage);
	float afterAoO = a_poiseDamage;
	// Difficulty / PC-NPC scaling
	if (a_poiseDamage > 0.0f && a_aggressor && a_target != a_aggressor) {
		a_poiseDamage = ApplyDifficultyScaling(a_target, a_aggressor, a_poiseDamage);

		poiseDamagePercent = CheckImpact(a_target, a_poiseDamage, avManager);
	}

	if (settings->Debug.LogStaggerCalcs) {
		float finalPoiseDamage = a_poiseDamage;
		logger::info(
			FMT_STRING(
				"[Final Poise Calculation] Target={} Initial={} AfterAoO={} Final={} PercentOfMax={}"),
			a_target->GetName(),
			initialPoiseDamage,
			afterAoO,
			finalPoiseDamage,
			poiseDamagePercent);
	}

	// Apply final calculated poise damage.
	a_poiseDamage = std::max(a_poiseDamage, 0.0f);

	{
		std::lock_guard<std::shared_mutex> lk(avManager->mtx);
		avManager->DamageActorValue(g_avName, a_target, a_poiseDamage);
	}

	float poise;

	{
		std::lock_guard<std::shared_mutex> lk(avManager->mtx);
		poise = avManager->GetActorValue(g_avName, a_target);
	}

	// Check if the hit depleted the target's remaining poise.
	if (poise <= 0.0f) {
		HandlePoiseBreak(a_target, a_aggressor, a_poiseDamage, poiseDamagePercent, poise, avManager);
	}

	if (settings->Debug.LogStaggerCalcs) {
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

void PoiseAV::Update(RE::Actor* a_actor, [[maybe_unused]] float a_delta)
{
	auto settings = Settings::GetSingleton();
	if (!a_actor) {
		return;
	}
	auto& runtime = a_actor->GetActorRuntimeData();

	if (settings->Debug.LogActorCalcs) {
		logger::info(
			FMT_STRING("[Poise Update Check] Actor={} Process={} High={} 3D={}"),
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
			float maxPoise;
			{
				std::lock_guard<std::shared_mutex> lk(avManager->mtx);
				maxPoise = avManager->GetActorValueMax(g_avName, a_actor);
			}

			{
				std::lock_guard<std::shared_mutex> lk(avManager->mtx);
				avManager->RestoreActorValue(g_avName, a_actor, maxPoise);
			}

			if (PoiseAVHUD::trueHUDInterface && settings->TrueHUD.SpecialBar) {
				PoiseAVHUD::trueHUDInterface->FlashActorSpecialBar(SKSE::GetPluginHandle(), a_actor->GetHandle(), true);
			}
			RemoveFromFaction(a_actor, ForceFullBodyStagger);
		} else {
			if (!GetBoolVariable(a_actor, "bKaputt_IsInKillMove")) {
				TryStagger(a_actor, 0.5f, nullptr);
				a_actor->SetGraphVariableBool("bPoise_IsStaggered", false);
			}
		}
	} else {
		// Delta-time scaled regen point calculation
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
	auto                               avManager = AVManager::GetSingleton();
	std::lock_guard<std::shared_mutex> lk(avManager->mtx);

	json temporaryJson = avManager->avStorage;
	for (auto& el : avManager->avStorage.items()) {
		std::string sformID = el.key();
		try {
			if (auto form = RE::TESForm::LookupByID(static_cast<RE::FormID>(std::stoul(sformID)))) {
				if (auto actor = form->As<RE::Actor>()) {
					if (actor->GetActorRuntimeData().currentProcess && actor->GetActorRuntimeData().currentProcess->InHighProcess() && actor->Is3DLoaded())
						continue;
				}
			}
			temporaryJson.erase(sformID);
		} catch (std::invalid_argument const&) {
			logger::error("Bad input: std::invalid_argument thrown");
		} catch (std::out_of_range const&) {
			logger::error("Integer overflow: std::out_of_range thrown");
		}
	}
	avManager->avStorage = temporaryJson;
}
