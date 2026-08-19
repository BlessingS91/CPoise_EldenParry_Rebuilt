#pragma once
#include "lib/PrecisionAPI.h"
#include "lib/ValhallaCombatAPI.h"
#include <memory>
// #include "Hooks/HitEventHandler.h"
#include <mutex>
#include <shared_mutex>

#include <unordered_set>
using PRECISION_API::PostHitCallback;
using std::string;

class Milf
{
public:
	[[nodiscard]] static Milf* GetSingleton();

	void Load();

	struct Core
	{
		void Load(CSimpleIniA& a_ini);

		bool useScoreSystem{ true };
	} core;

	struct Scores
	{
		void Load(CSimpleIniA& a_ini);

		float scoreDiffThreshold{ 20.0f };

		float weaponSkillWeight{ 1.0f };

		float oneHandDaggerScore{ 0.0f };
		float oneHandSwordScore{ 20.0f };
		float oneHandAxeScore{ 25.0f };
		float oneHandMaceScore{ 25.0f };
		float oneHandKatanaScore{ 30.0f };
		float oneHandRapierScore{ 15.0f };
		float oneHandClawsScore{ 10.0f };
		float oneHandWhipScore{ -100.0f };
		float twoHandSwordScore{ 40.0f };
		float twoHandAxeScore{ 50.0f };
		float twoHandWarhammerScore{ 50.0f };
		float twoHandPikeScore{ 30.0f };
		float twoHandHalberdScore{ 45.0f };
		float twoHandQuarterstaffScore{ 50.0f };

		float altmerScore{ -15.0f };
		float argonianScore{ 0.0f };
		float bosmerScore{ -10.0f };
		float bretonScore{ -10.0f };
		float dunmerScore{ -5.0f };
		float imperialScore{ 0.0f };
		float khajiitScore{ 5.0f };
		float nordScore{ 10.0f };
		float orcScore{ 20.0f };
		float redguardScore{ 10.0f };

		float femaleScore{ -10.0f };

		float powerAttackScore{ 25.0f };

		float playerScore{ 0.0f };
	} scores;

	struct Perks
	{
		void              Load(CSimpleIniA& a_ini);
		RE::BSFixedString EldenParry_Perk1 = "ORD_Bck20_TimedBlock_Perk_20_OrdASISExclude";
		RE::BSFixedString EldenParry_Perk2 = "ORD_Bck20_TimedBlock_Perk_50_OrdASISExclude";
		RE::BSFixedString EldenParry_Spell1 = "ORD_Bck_TimedBlock_Spell_Proc";
		RE::BSFixedString EldenParry_Spell2 = "ORD_Bck_TimedBlock_Spell_Proc_2";
		RE::BSFixedString DragonsTail_Perk = "ORD_Bck60_DragonTail_Perk_60_OrdASISExclude";
		RE::BSFixedString Deliverance_Perk = "ORD_Bck90_Deliverance_Perk_90_OrdASISExclude";
		RE::BSFixedString QuickReflexes_Perk = "ORD_Bck40_QuickReflexes_Perk_40_QuickReflexes";
		RE::BSFixedString SkullRattler_Perk = "ORD_Bck50_SkullRattler_Perk_50_WasDeadlyBash";
		RE::BSFixedString AlikrDance_Perk = "ORD_Bck30_PokeTheDragon_Perk_30_OrdASISExclude";

	} perks;

private:
	Milf() = default;
	Milf(const Milf&) = delete;
	Milf(Milf&&) = delete;
	~Milf() = default;

	Milf& operator=(const Milf&) = delete;
	Milf& operator=(Milf&&) = delete;

	struct detail
	{
		// Thanks to: https://github.com/powerof3/CLibUtil
		template <class T>
		static T& get_value(CSimpleIniA& a_ini, T& a_value, const char* a_section, const char* a_key, const char* a_comment,
			const char* a_delimiter = R"(|)")
		{
			if constexpr (std::is_same_v<T, bool>) {
				a_value = a_ini.GetBoolValue(a_section, a_key, a_value);
				a_ini.SetBoolValue(a_section, a_key, a_value, a_comment);
			} else if constexpr (std::is_floating_point_v<T>) {
				a_value = static_cast<float>(a_ini.GetDoubleValue(a_section, a_key, a_value));
				a_ini.SetDoubleValue(a_section, a_key, a_value, a_comment);
			} else if constexpr (std::is_enum_v<T>) {
				a_value = string::template to_num<T>(
					a_ini.GetValue(a_section, a_key, std::to_string(std::to_underlying(a_value)).c_str()));
				a_ini.SetValue(a_section, a_key, std::to_string(std::to_underlying(a_value)).c_str(), a_comment);
			} else if constexpr (std::is_arithmetic_v<T>) {
				a_value = string::template to_num<T>(a_ini.GetValue(a_section, a_key, std::to_string(a_value).c_str()));
				a_ini.SetValue(a_section, a_key, std::to_string(a_value).c_str(), a_comment);
			} else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
				a_value = string::split(a_ini.GetValue(a_section, a_key, string::join(a_value, a_delimiter).c_str()),
					a_delimiter);
				a_ini.SetValue(a_section, a_key, string::join(a_value, a_delimiter).c_str(), a_comment);
			} else {
				a_value = a_ini.GetValue(a_section, a_key, a_value.c_str());
				a_ini.SetValue(a_section, a_key, a_value.c_str(), a_comment);
			}
			return a_value;
		}
	};
};

class EldenParry
{
public:
	float GetScore(RE::Actor* actor, const Milf::Scores& scoreSettings);

	float AttackerBeatsParry(RE::Actor* attacker, RE::Actor* target);

	const RE::TESObjectWEAP* const GetAttackWeapon(RE::AIProcess* const aiProcess);

	static EldenParry* GetSingleton()
	{
		static EldenParry singleton;
		return std::addressof(singleton);
	}

	void init();

	bool IsAvailable() const { return _isAvailable; }

	/// <summary>
	/// Try to process a parry by the parrier.
	/// </summary>
	/// <param name="a_attacker"></param>
	/// <param name="a_parrier"></param>
	/// <returns>True if the parry is successful.</returns>
	bool processMeleeParry(RE::Actor* a_attacker, RE::Actor* a_parrier);

	bool processProjectileParry(RE::Actor* a_blocker, RE::Projectile* a_projectile, RE::hkpCollidable* a_projectile_collidable);

	void processGuardBash(RE::Actor* a_basher, RE::Actor* a_blocker);

	PRECISION_API::IVPrecision1* _precision_API;
	VAL_API::IVVAL1*             _ValhallaCombat_API;

	void applyParryCost(RE::Actor* a_actor);
	void cacheParryCost(RE::Actor* a_actor, float a_cost);

	void negateParryCost(RE::Actor* a_actor);

	void playGuardBashEffects(RE::Actor* a_actor);

	void  startTimingParry(RE::Actor* a_actor);
	void  finishTimingParry(RE::Actor* a_actor);
	float calculateRiposteReflex(RE::Actor* a_actor);

	void send_melee_parry_event(RE::Actor* a_attacker);
	void send_ranged_parry_event();

	void update();

private:
	bool _isAvailable{ false };

	void playParryEffects(RE::Actor* a_parrier);

	bool                                       inParryState(RE::Actor* a_parrier);
	bool                                       canParry(RE::Actor* a_parrier, RE::TESObjectREFR* a_obj);
	bool                                       inBlockAngle(RE::Actor* a_blocker, RE::TESObjectREFR* a_obj);
	static PRECISION_API::PreHitCallbackReturn precisionPrehitCallbackFunc(const PRECISION_API::PrecisionHitData& a_precisionHitData);

	std::unordered_map<RE::Actor*, float> _parryCostQueue;
	std::unordered_set<RE::Actor*>        _parrySuccessActors;
	std::unordered_map<RE::Actor*, float> _parryTimer;

	RE::BGSSoundDescriptorForm* _parrySound_shd;
	RE::BGSSoundDescriptorForm* _parrySound_wpn;
	float                       _GMST_fCombatHitConeAngle;
	float                       _parryAngle;

	std::shared_mutex mtx_parryCostQueue;
	std::shared_mutex mtx_parrySuccessActors;
	std::shared_mutex mtx_parryTimer;

	bool _bUpdate;
};
