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
			return !a_actor->AsActorState()->actorState2.staggered;
		}
	}
	return false;
}

float PoiseAV::GetBaseActorValue(RE::Actor* a_actor)
{
	auto        settings = Settings::GetSingleton();
	std::string editorID = a_actor->GetRace()->GetFormEditorID();

	float health;
	if (!editorID.empty() && (settings->JSONSettings["Races"][editorID] != nullptr))
		health = (float)settings->JSONSettings["Races"][editorID];
	else
		health = a_actor->AsActorValueOwner()->GetBaseActorValue(RE::ActorValue::kMass);

	health *= settings->Health.BaseMult;
	health += ActorCache::GetSingleton()->GetOrCreateCachedWeight(a_actor) * Settings::GetSingleton()->Health.ArmorMult;

	//if (auto levelledModifier = (RE::ExtraLevCreaModifier*)a_actor->extraList.GetByType(RE::ExtraDataType::kLevCreaModifier)) {
	//	auto modifier = levelledModifier->modifier;
	//	if (modifier.any(RE::LEV_CREA_MODIFIER::kEasy)) {
	//		health *= 0.75;
	//	} else if (modifier.any(RE::LEV_CREA_MODIFIER::kMedium)) {
	//		health *= 1.00;
	//	} else if (modifier.any(RE::LEV_CREA_MODIFIER::kHard)) {
	//		health *= 1.25;
	//	} else if (modifier.any(RE::LEV_CREA_MODIFIER::kVeryHard)) {
	//		health *= 1.50;
	//	}
	//}

	//return std::clamp(health, 0.0f, FLT_MAX);
	return std::clamp(health, 0.0f, std::numeric_limits<float>::max());
}

float PoiseAV::Score_GetBaseActorValue(RE::Actor* a_actor)
{
	auto        settings = Settings::GetSingleton();
	std::string editorID = a_actor->GetRace()->GetFormEditorID();

	float health;
	if (!editorID.empty() && (settings->JSONSettings["Races"][editorID] != nullptr))
		health = (float)settings->JSONSettings["Races"][editorID];
	else
		health = a_actor->AsActorValueOwner()->GetBaseActorValue(RE::ActorValue::kMass);

	health *= settings->Health.BaseMult;

	//if (auto levelledModifier = (RE::ExtraLevCreaModifier*)a_actor->extraList.GetByType(RE::ExtraDataType::kLevCreaModifier)) {
	//	auto modifier = levelledModifier->modifier;
	//	if (modifier.any(RE::LEV_CREA_MODIFIER::kEasy)) {
	//		health *= 0.75;
	//	} else if (modifier.any(RE::LEV_CREA_MODIFIER::kMedium)) {
	//		health *= 1.00;
	//	} else if (modifier.any(RE::LEV_CREA_MODIFIER::kHard)) {
	//		health *= 1.25;
	//	} else if (modifier.any(RE::LEV_CREA_MODIFIER::kVeryHard)) {
	//		health *= 1.50;
	//	}
	//}

	return health;
}

float PoiseAV::GetActorValueMax([[maybe_unused]] RE::Actor* a_actor)
{
	return GetBaseActorValue(a_actor);
}

void PoiseAV::DamageAndCheckPoise(RE::Actor* a_target, RE::Actor* a_aggressor, float a_poiseDamage, [[maybe_unused]] RE::HitData* a_hitData)
{
	auto                               settings = Settings::GetSingleton();
	auto                               avManager = AVManager::GetSingleton();
	std::lock_guard<std::shared_mutex> lk(avManager->mtx);

	float poiseDamagePercent = 0.0f;

	if (a_poiseDamage > 0 && a_target != a_aggressor) {
		float damageMultiplier = settings->GetDamageMultiplier(a_aggressor, a_target);

		damageMultiplier = 1.0f + (damageMultiplier - 1.0f) * settings->Damage.PoiseScaling;

		logger::debug(
			FMT_STRING("Poise Scaling: Before={} DifficultyMult={} PoiseScaling={} FinalMult={}"),
			a_poiseDamage,
			settings->GetDamageMultiplier(a_aggressor, a_target),
			settings->Damage.PoiseScaling,
			damageMultiplier);

		a_poiseDamage *= damageMultiplier;

		if (a_target->IsPlayerRef()) {
			a_poiseDamage *= settings->Damage.ToPCMult;
		} else {
			a_poiseDamage *= settings->Damage.ToNPCMult;
		}

		poiseDamagePercent = a_poiseDamage / avManager->GetActorValueMax(g_avName, a_target);

		if (a_hitData && a_hitData->flags && a_hitData->flags.all(RE::HitData::Flag::kBlocked)) {
			if (poiseDamagePercent >= 0.1 && poiseDamagePercent < 0.25) {
				Cast_Spell(a_target, "BHR_Normal_Impact", 0.0f);

			} else if (poiseDamagePercent >= 0.25 && poiseDamagePercent < 0.5) {
				Cast_Spell(a_target, "BHR_Powerful_Impact", 0.0f);

			} else if (poiseDamagePercent >= 0.5) {
				Cast_Spell(a_target, "BHR_Seismic_Impact", 0.0f);
			}
		}
	}

	// logger::info("DACP Branch. {} attempting to damage poise. current value: {} ", a_target->GetName(), avManager->GetActorValue(g_avName, a_target));

	avManager->DamageActorValue(g_avName, a_target, a_poiseDamage);

	// logger::info("DACP Branch. {} poise damaged. current value: {} ", a_target->GetName(), avManager->GetActorValue(g_avName, a_target));

	auto poise = avManager->GetActorValue(g_avName, a_target);
	if (poise == 0.0f) {
		// logger::info("DACP Branch. {} poise is depleted. attemtping stagger. Value: {} ", a_target->GetName(), avManager->GetActorValue(g_avName, a_target));
		a_target->AddToFaction(ForceFullBodyStagger, 0);
		// auto poiseDamagePercent = a_poiseDamage / avManager->GetActorValueMax(g_avName, a_target);

		// logger::info("DACP Branch. {} Poisedamage percent is equal to Stagger Mag: {} ", a_target->GetName(), poiseDamagePercent);

		// Stagger duration is relative to the power of the attacking weapon
		logger::debug(FMT_STRING("Poise Damage Percent {}"), poiseDamagePercent);

		if (!GetBoolVariable(a_target, "bKaputt_IsInKillMove")) {
			TryStagger(a_target, poiseDamagePercent, a_aggressor);
			a_target->SetGraphVariableBool("bPoise_IsStaggered", false);
		}
	}
	logger::debug(FMT_STRING("Target {} Poise Damage {} Poise Health {} / {}"), a_target->GetName(), a_poiseDamage, avManager->GetActorValue(g_avName, a_target), avManager->GetActorValueMax(g_avName, a_target));
}

void PoiseAV::Update(RE::Actor* a_actor, [[maybe_unused]] float a_delta)
{
	if (a_actor->GetActorRuntimeData().currentProcess && a_actor->GetActorRuntimeData().currentProcess->InHighProcess() && a_actor->Is3DLoaded()) {
		auto settings = Settings::GetSingleton();

		if (PoiseAVHUD::trueHUDInterface && settings->TrueHUD.SpecialBar) {
			if (!CanDamageActor(a_actor)) {
				PoiseAVHUD::trueHUDInterface->OverrideSpecialBarColor(a_actor->GetHandle(), TRUEHUD_API::BarColorType::BarColor, 0x808080);

				// logger::info("Update hook active. {} is staggered. Overridding poise bar ", a_actor->GetName());

			} else {
				PoiseAVHUD::trueHUDInterface->RevertSpecialBarColor(a_actor->GetHandle(), TRUEHUD_API::BarColorType::BarColor);
			}
		}

		auto                               avManager = AVManager::GetSingleton();
		std::lock_guard<std::shared_mutex> lk(avManager->mtx);
		if (avManager->GetActorValue(g_avName, a_actor) == 0.0f) {
			// logger::info("Update Branch. {} poise is depleted. Value: {} ", a_actor->GetName(), avManager->GetActorValue(g_avName, a_actor));

			if (a_actor->AsActorState()->actorState2.staggered) {
				// logger::info("Update Branch. {} isStaggered. Restoring Max Poise ", a_actor->GetName());

				avManager->RestoreActorValue(g_avName, a_actor, avManager->GetActorValueMax(g_avName, a_actor));

				// logger::info("Update Branch. {} Current Poise: {:.2f} ", a_actor->GetName(), avManager->GetActorValue(g_avName, a_actor));

				if (PoiseAVHUD::trueHUDInterface && settings->TrueHUD.SpecialBar) {
					PoiseAVHUD::trueHUDInterface->FlashActorSpecialBar(SKSE::GetPluginHandle(), a_actor->GetHandle(), true);
					// logger::info("Update Branch. {} flashing HUD Bar due to depleted poise ", a_actor->GetName());
				}
				RemoveFromFaction(a_actor, ForceFullBodyStagger);
			} else {
				// logger::info("Update Branch. {} isnot Staggered. Attempting stagger", a_actor->GetName());

				if (!GetBoolVariable(a_actor, "bKaputt_IsInKillMove")) {
					TryStagger(a_actor, 0.5f, nullptr);
					a_actor->SetGraphVariableBool("bPoise_IsStaggered", false);
				}
			}
		} else {
			avManager->RestoreActorValue(g_avName, a_actor, avManager->GetActorValueMax(g_avName, a_actor) * settings->Health.RegenRate * a_delta);
			// logger::info("Update Branch. {} poise not depleted. Restoring poise : {} ", a_actor->GetName(), avManager->GetActorValueMax(g_avName, a_actor) * settings->Health.RegenRate * a_delta);
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
				if (auto actor = RE::TESForm::LookupByID(static_cast<RE::FormID>(std::stoul(sformID)))->As<RE::Actor>()) {
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
