#pragma once

#include <SimpleIni.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace RE
{
	enum class DIFFICULTY : std::int32_t
	{
		kNovice = 0,
		kApprentice = 1,
		kAdept = 2,
		kExpert = 3,
		kMaster = 4,
		kLegendary = 5
	};
}

class Settings
{
public:
	std::unordered_map<std::string, float> RaceWeightCache;
	float                                  MinRaceWeight{ 0.0f };
	float                                  MaxRaceWeight{ 0.0f };
	[[nodiscard]] static Settings*         GetSingleton()
	{
		static Settings singleton;
		return &singleton;
	}

	// ==========================
	// Difficulty
	// ==========================
	float fDiffMultHPByPCVE;
	float fDiffMultHPByPCE;
	float fDiffMultHPByPCN;
	float fDiffMultHPByPCH;
	float fDiffMultHPByPCVH;
	float fDiffMultHPByPCL;

	float fDiffMultHPToPCVE;
	float fDiffMultHPToPCE;
	float fDiffMultHPToPCN;
	float fDiffMultHPToPCH;
	float fDiffMultHPToPCVH;
	float fDiffMultHPToPCL;

	struct
	{
		int StaggerMode{ 1 };

	} Modes;

	// ==========================
	// Poise Health
	// ==========================
	struct
	{
		float BaseHealth{ 100.0f };

		float MassMult{ 1.0f };

		float RegenRate{ 0.045f };

		float RegenDelay{ 1.0f };

	} Health;

	// ==========================
	// Armor Calculations
	// ==========================
	struct
	{
		float ArmorMult{ 1.0f };

		float WeightContribution{ 0.005f };

	} Armor;

	// ==========================
	// Weapon Damage
	// ==========================
	struct
	{
		float MeleeMult{ 1.0f };

		float WeightContribution{ 0.005f };

		float BowDrawSpeedMult{ 1.0f };

		float ArrowDamageMult{ 0.25f };

		float CrossbowMult{ 1.15f };

		float BowDamageMult{ 1.0f };

	} Weapon;

	// ==========================
	// Unarmed Damage
	// ==========================
	struct
	{
		// How much gauntlet armor rating contributes to poise
		float ArmorContribution{ 1.0f };

		// How much gauntlet weight contributes to poise
		float WeightContribution{ 0.15f };

		// How much the selected skill contributes (0.40 = +40% at skill 100)
		float SkillContribution{ 0.40f };

		// Additional multiplier applied when wearing Heavy Armor gauntlets
		float HeavyGauntletContribution{ 1.0f };

		// Additional multiplier applied when wearing Light Armor gauntlets
		float LightGauntletContribution{ 0.80f };

		// Skill used for scaling
		// 0=None, 1=OneHanded, 2=TwoHanded, ...
		int SkillType{ 0 };

	} Unarmed;

	// ==========================
	// Creature Damage
	// ==========================
	struct
	{
		float DamageMultiplier{ 1.5f };

		float ScalingCurve{ 4.0f };

	} Creature;

	// ==========================
	// Bash / Blocking
	// ==========================
	struct
	{
		float BashMult{ 1.0f };

		float BlockingMult{ 1.0f };

		float PowerAttackBlockingMult{ 0.5 };

	} Blocking;

	// ==========================
	// Shield Damage
	// ==========================
	struct
	{
		// How much shield armor rating contributes to poise damage
		float ArmorContribution{ 1.0f };

		// How much shield weight contributes to poise damage
		float WeightContribution{ 0.15f };

	} Shield;

	// ==========================
	// Attack Modifiers
	// ==========================
	struct
	{
		float NormalAttackMult{ 1.0f };

		float PowerAttackMult{ 1.0f };

		float AttackOfOpportunityMult{ 1.5f };

	} Attack;

	// ==========================
	// Magic Damage
	// ==========================
	struct
	{
		float ResistanceMult{ 1.0f };

		float DamageMult{ 1.0f };

	} Magic;

	// ==========================
	// Environment / Traps
	// ==========================
	struct
	{
		float TrapMult{ 3.0f };

	} Environment;

	// ==========================
	// NPC / Player Multipliers
	// ==========================
	struct
	{
		float ToPCMult{ 1.0f };

		float ToNPCMult{ 1.0f };

		float DifficultyScaling{ 0.25f };

		float WeaponScalingCurve{ 2.5f };

		float ArmorScalingCurve{ 2.5f };

		float EquipmentReferenceMultiplier{ 5.0f };

		float LevelDifferenceMult{ 0.01f };

	} Global;

	// ==========================
	// Impact Thresholds
	// ==========================
	struct
	{
		float Normal{ 0.25f };

		float Large{ 0.50f };

		float Massive{ 0.75f };

	} Impact;

	struct
	{
		bool SpecialBar{ true };

		uint32_t SpecialBarNormalColor{ 0xFFFF00 };

		uint32_t SpecialBarDepletedColor{ 0x808080 };

	} TrueHUD;

	struct DebugSettings
	{
		bool LogWeaponCalcs{ false };

		bool LogUnarmedCalcs{ false };

		bool LogArmorCalcs{ false };

		bool LogMagicEffectCalcs{ false };

		bool LogStaggerCalcs{ false };

		bool LogActorCalcs{ false };

		bool LogPerkCalcs{ false };

	} Debug;

	json JSONSettings;

	float GetDamageMultiplier(
		RE::Actor* a_aggressor,
		RE::Actor* a_target);

	void LoadGameSettings();

	void LoadINI(
		const wchar_t* a_path);

	void LoadJSON(
		const wchar_t* a_path);

	void LoadSettings();

private:
	Settings() = default;

	Settings(const Settings&) = delete;
	Settings(Settings&&) = delete;

	~Settings() = default;

	Settings& operator=(const Settings&) = delete;
	Settings& operator=(Settings&&) = delete;
};