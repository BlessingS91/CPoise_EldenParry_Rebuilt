#include "Hooks/PoiseAV.h"

#include "ActorValues/AVManager.h"
#include "Storage/ActorCache.h"
#include "Storage/Settings.h"
#include "UI/PoiseAVHUD.h"

#undef max
#include <limits>
//#undef min

bool PoiseAV::CanDamageActor(RE::Actor* a_actor)
{
	if (a_actor && a_actor->GetActorRuntimeData().currentProcess && !a_actor->IsChild()) {
		switch (Settings::GetSingleton()->Modes.StaggerMode) {
		case 0:
			return true;
		case 1:
			// return !GetBoolVariable(a_actor, "IsStaggering");
			auto actorState = a_actor->AsActorState();
			if (!actorState) {
				return false;
			}

			return !actorState->actorState2.staggered;
		}
	}
	return false;
}

float PoiseAV::GetBaseActorValue(RE::Actor* a_actor)
{
	auto settings = Settings::GetSingleton();

	std::string editorID;

	//null safety
	if (auto race = a_actor->GetRace()) {
		editorID = race->GetFormEditorID();
	}

	// Base poise pool
	float health = settings->Health.BaseMult;

	float mass = 1.0f;

	// Pull custom race mass from JSON if available
	if (!editorID.empty() && settings->JSONSettings["Races"][editorID] != nullptr) {
		mass = static_cast<float>(settings->JSONSettings["Races"][editorID]);
	} else {
		// Otherwise use Skyrim race mass
		mass = a_actor->AsActorValueOwner()->GetBaseActorValue(RE::ActorValue::kMass);
	}

	mass = std::clamp(mass, 0.5f, 10.0f);

	// Final mass factor applied to base poise
	float massMultiplier = mass * settings->Health.MassMult;

	health *= massMultiplier;

	if (settings->Debug.LogStaggerCalcs && health != settings->Health.BaseMult) {
		logger::debug(
			FMT_STRING("[Poise Calc] Actor={} Race={} Base={} Mass={} MassMult={} Final={}"),
			a_actor->GetName(),
			editorID,
			settings->Health.BaseMult,
			mass,
			settings->Health.MassMult,
			health);
	}

	return std::clamp(health, 0.0f, std::numeric_limits<float>::max());
}

//For Leonkingzz Elden Parry system, no extra math needed just use GetBaseActorValue calculations.
float PoiseAV::Score_GetBaseActorValue(RE::Actor* a_actor)
{
	return GetBaseActorValue(a_actor);
}

float PoiseAV::GetActorValueMax([[maybe_unused]] RE::Actor* a_actor)
{
	return GetBaseActorValue(a_actor);
}

void PoiseAV::DamageAndCheckPoise(RE::Actor* a_target, RE::Actor* a_aggressor, float a_poiseDamage, [[maybe_unused]] RE::HitData* a_hitData)
{
	if (!a_target || std::abs(a_poiseDamage) <= 0.001f) {
		return;
	}

	auto settings = Settings::GetSingleton();
	auto avManager = AVManager::GetSingleton();

	float poiseDamagePercent = 0.0f;

	if (settings->Debug.LogStaggerCalcs) {
		logger::info(
			FMT_STRING("[Poise Damage START] Target={}({:08X}) Aggressor={}({:08X}) IncomingDamage={}"),
			a_target ? a_target->GetName() : "NULL",
			a_target ? a_target->GetFormID() : 0,
			a_aggressor ? a_aggressor->GetName() : "NULL",
			a_aggressor ? a_aggressor->GetFormID() : 0,
			a_poiseDamage);
	}

	// Attack checks
	bool isAttacking = false;

	if (auto actorState = a_target->AsActorState()) {
		logger::info(
			"[Poise Debug] ActorState={}",
			actorState ? "VALID" : "NULL");
		isAttacking = actorState->GetAttackState() != RE::ATTACK_STATE_ENUM::kNone;
	}

	bool isCasting = false;

	if (a_target && a_target->Get3D()) {
		a_target->GetGraphVariableBool("IsInCastState", isCasting);
	}

	if (!isCasting) {
		bool castRight = false, castLeft = false, castDual = false;
		a_target->GetGraphVariableBool("IsCastingRight", castRight);
		a_target->GetGraphVariableBool("IsCastingLeft", castLeft);
		a_target->GetGraphVariableBool("IsCastingDual", castDual);
		isCasting = (castRight || castLeft || castDual);
	}
	//Attack of Oppourtunity Mult
	if (isAttacking || isCasting) {
		const auto  actionSettings = Settings::GetSingleton();
		const float actionMultiplier = (actionSettings && actionSettings->Damage.AttackOfOpportunityMult > 0.0f) ? actionSettings->Damage.AttackOfOpportunityMult : 1.5f;

		float beforeActionDamage = a_poiseDamage;
		a_poiseDamage *= actionMultiplier;

		logger::debug(
			FMT_STRING("[Poise Multiplier - Action] Target: {} | Attacking: {} | Casting: {} | Mult: {} | Before: {} | After: {}"),
			a_target->GetName(),
			isAttacking,
			isCasting,
			actionMultiplier,
			beforeActionDamage,
			a_poiseDamage);
	}

	if (a_poiseDamage > 0.0f && a_target && a_aggressor && a_target != a_aggressor) {
		// Store base raw multiplier to avoid redundant calls
		float rawDifficultyMult = settings->GetDamageMultiplier(a_aggressor, a_target);
		float damageMultiplier = 1.0f + (rawDifficultyMult - 1.0f) * settings->Damage.PoiseScaling;

		logger::debug(
			FMT_STRING("Poise Scaling: Before={} DifficultyMult={} PoiseScaling={} FinalMult={}"),
			a_poiseDamage,
			rawDifficultyMult,
			settings->Damage.PoiseScaling,
			damageMultiplier);

		a_poiseDamage *= damageMultiplier;

		if (a_target->IsPlayerRef()) {
			a_poiseDamage *= settings->Damage.ToPCMult;
		} else {
			a_poiseDamage *= settings->Damage.ToNPCMult;
		}

		// Division-by-zero guard against NaN/Infinity crashes
		float maxPoise;

		{
			std::lock_guard<std::shared_mutex> lk(avManager->mtx);
			maxPoise = avManager->GetActorValueMax(g_avName, a_target);
		}
		if (maxPoise > 0.0f) {
			poiseDamagePercent = a_poiseDamage / maxPoise;
		} else {
			poiseDamagePercent = 0.0f;
		}

		if (poiseDamagePercent >= settings->Damage.NormalImpactThreshold &&
			poiseDamagePercent < settings->Damage.PowerfulImpactThreshold) {
			Cast_Spell(a_target, "BHR_Normal_Impact", 0.0f);

		} else if (poiseDamagePercent >= settings->Damage.PowerfulImpactThreshold &&
				   poiseDamagePercent < settings->Damage.SeismicImpactThreshold) {
			Cast_Spell(a_target, "BHR_Powerful_Impact", 0.0f);

		} else if (poiseDamagePercent >= settings->Damage.SeismicImpactThreshold) {
			Cast_Spell(a_target, "BHR_Seismic_Impact", 0.0f);
		}

		logger::debug(
			FMT_STRING("Impact Check: Target={} PoiseDamage={} PoisePercent={} Thresholds[N/P/S]={}/{}/{}"),
			a_target->GetName(),
			a_poiseDamage,
			poiseDamagePercent,
			settings->Damage.NormalImpactThreshold,
			settings->Damage.PowerfulImpactThreshold,
			settings->Damage.SeismicImpactThreshold);
	}

	// Apply incoming poise damage to the target's poise actor value.
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
		// Force full-body stagger behavior when poise is broken.
		a_target->AddToFaction(ForceFullBodyStagger, 0);

		if (!GetBoolVariable(a_target, "bKaputt_IsInKillMove")) {
			// Convert the percentage of poise depleted into stagger strength.
			// Clamp prevents extreme values from being passed into Skyrim's stagger system.
			float safeStaggerMag = std::clamp(poiseDamagePercent, 0.0f, 2.0f);

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
					poise,
					maxPoise,
					poiseDamagePercent,
					safeStaggerMag);
			}

			// Trigger stagger animation from the custom poise system.
			TryStagger(a_target, safeStaggerMag, a_aggressor);

			// Reset custom graph state after triggering stagger.
			a_target->SetGraphVariableBool("bPoise_IsStaggered", false);
		}
	}

	if (Settings::GetSingleton()->Debug.LogStaggerCalcs) {
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
	if (a_actor->GetActorRuntimeData().currentProcess && a_actor->GetActorRuntimeData().currentProcess->InHighProcess() && a_actor->Is3DLoaded()) {
		auto settings = Settings::GetSingleton();

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
		auto avManager = AVManager::GetSingleton();

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
