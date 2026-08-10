#pragma once

#include "ActorValues/AVInterface.h"
#include "FormUtil.h"
class AVManager;

//static float* g_deltaTime = (float*)RELOCATION_ID(523660, 410199).address();          // 2F6B948, 30064C8
//static float* g_deltaTimeRealTime = (float*)RELOCATION_ID(523661, 410200).address();  // 2F6B94C, 30064CC

static float& g_deltaTime = (*(float*)RELOCATION_ID(523660, 410199).address());

class PoiseAV : public AVInterface
{
public:
	static void InstallHooks()
	{
		Hooks::Install();
	}

	static PoiseAV* GetSingleton()
	{
		static PoiseAV avInterface;
		return &avInterface;
	}

	inline static const char* g_avName = "Poise";

	float ApplyDamageModifiers(RE::Actor* a_target, float a_damage);
	bool  CanDamageActor(RE::Actor* a_actor);
	float GetBaseActorValue(RE::Actor* a_actor);
	float Score_GetBaseActorValue(RE::Actor* a_actor);
	float GetActorValueMax(RE::Actor* a_actor);
	bool  IsActorPerformingAction(RE::Actor* a_actor);
	float GetWardPoiseReduction(RE::Actor* a_actor);
	float ApplyAttackOfOpportunityMult(RE::Actor* a_target, float a_poiseDamage);
	float ApplyDifficultyScaling(RE::Actor* a_target, RE::Actor* a_aggressor, float a_poiseDamage);
	float ApplyLevelDifferenceScaling(RE::Actor* a_target, RE::Actor* a_aggressor, float a_poiseDamage);
	float CheckImpact(RE::Actor* a_target, float a_poiseDamage, AVManager* avManager);
	void  HandlePoiseBreak(
		RE::Actor* a_target,
		RE::Actor* a_aggressor,
		float      a_impactPercent);
	void DamageAndCheckPoise(RE::Actor* a_target, RE::Actor* a_aggressor, float a_poiseDamage, RE::HitData* a_hitData = nullptr);
	void Update(RE::Actor* a_actor, float a_delta);
	void GarbageCollection();

	void Cast_Spell(RE::Actor* a_actor, std::string a_spell, float a_mag)
	{
		if (const auto e_spell = RE::TESForm::LookupByEditorID<RE::MagicItem>(a_spell); e_spell) {
			if (const auto caster = a_actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant); caster) {
				caster->CastSpellImmediate(e_spell, false, a_actor, 1, false, a_mag, a_actor);
			}
		}
	};

	RE::TESFaction* ForceFullBodyStagger;

	void RetrieveFullBodyStaggerFaction()
	{
		ForceFullBodyStagger = RE::TESForm::LookupByID(0x10CED7)->As<RE::TESFaction>();
		if (!ForceFullBodyStagger)
			ForceFullBodyStagger = FormUtil::LookupByIdentifier<RE::TESFaction>("ChocolatePoise - Enderal.esp|0x800");
	}

	bool            appliedStagger = false;
	std::thread::id staggerThread;

	static void TryStagger(RE::Actor* a_target, float a_staggerMult, RE::Actor* a_aggressor)
	{
		a_target->SetGraphVariableBool("bPoise_IsStaggered", true);
		GetSingleton()->appliedStagger = true;
		GetSingleton()->staggerThread = std::this_thread::get_id();

		using func_t = decltype(&TryStagger);
		REL::Relocation<func_t> func{ REL::RelocationID(36700, 37710) };
		func(a_target, a_staggerMult, a_aggressor);
	}

	static bool GetBoolVariable(RE::Actor* a_actor, std::string a_string)
	{
		auto result = false;
		a_actor->GetGraphVariableBool(a_string, result);
		return result;
	}

	static void RemoveFromFaction(RE::Actor* a_actor, RE::TESFaction* a_faction)
	{
		using func_t = decltype(&RemoveFromFaction);
		REL::Relocation<func_t> func{ REL::RelocationID(36680, 37688) };
		func(a_actor, a_faction);
	}

	static RE::ActorValue GetActorValueIdFromName(char* a_name)
	{
		using func_t = decltype(&GetActorValueIdFromName);
		REL::Relocation<func_t> func{ REL::RelocationID(26570, 27203) };
		func(a_name);
	}

	static void ApplyPerkEntryPoint(INT32 entry, RE::Actor* actor_a, RE::Actor* actor_b, float* out)
	{
		using func_t = decltype(&ApplyPerkEntryPoint);
		REL::Relocation<func_t> func{ REL::RelocationID(23073, 23526) };  // 1.5.97 14032ECE0
		return func(entry, actor_a, actor_b, out);
	}

	std::unordered_map<RE::FormID, float> regenDelays;

protected:
	struct Hooks
	{
		struct PlayerCharacter_Update
		{
			static void thunk(RE::PlayerCharacter* a_player, float a_delta)
			{
				func(a_player, a_delta);
				GetSingleton()->Update(a_player, g_deltaTime);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct Actor_Update
		{
			static void thunk(RE::Actor* a_actor, float a_delta)
			{
				func(a_actor, a_delta);
				GetSingleton()->Update(a_actor, g_deltaTime);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct Filter_ApplyPerkEntryPoint_Aggressor
		{
			static void thunk(INT32 entry, RE::Actor* target, RE::Actor* aggressor, float& staggerMult)
			{
				auto poiseAV = GetSingleton();
				auto currentThread = std::this_thread::get_id();
				if (poiseAV->appliedStagger && poiseAV->staggerThread == currentThread)
					return;
				if (poiseAV->staggerThread != currentThread)
					logger::debug("Stagger attempted on another thread");
				func(entry, target, aggressor, staggerMult);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct Filter_ApplyPerkEntryPoint_Target
		{
			static void thunk(INT32 entry, RE::Actor* target, RE::Actor* aggressor, float& staggerMult)
			{
				auto poiseAV = GetSingleton();
				auto currentThread = std::this_thread::get_id();
				if (poiseAV->appliedStagger && poiseAV->staggerThread == currentThread)
					poiseAV->appliedStagger = false;
				else
					func(entry, target, aggressor, staggerMult);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		static void Install()
		{
			stl::write_vfunc<RE::PlayerCharacter, 0xAD, PlayerCharacter_Update>();
			stl::write_vfunc<RE::Character, 0xAD, Actor_Update>();
			stl::write_thunk_call<Filter_ApplyPerkEntryPoint_Aggressor>(REL::RelocationID(36700, 37710).address() + REL::Relocate(0x9A, 0xA1, 0x9A));  // 1.5.97 1405FA1B0
			stl::write_thunk_call<Filter_ApplyPerkEntryPoint_Target>(REL::RelocationID(36700, 37710).address() + REL::Relocate(0xAE, 0xB9, 0xAE));     // 1.5.97 1405FA1B0
		}
	};
};
