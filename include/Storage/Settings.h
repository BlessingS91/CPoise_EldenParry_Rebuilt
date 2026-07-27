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
	[[nodiscard]] static Settings* GetSingleton()
	{
		static Settings singleton;
		return &singleton;
	}

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

	struct
	{
		float BaseMult{ 100.0f };
		float MassMult{ 1.0f };
		float ArmorMult{ 0.020f };
		float ArmorMultMin{ 0.35f };
		float RegenRate{ 0.045f };
	} Health;

	struct
	{
		float BashMult{ 1.0f };
		float ArrowDamageMult{ 0.25f };
		float BowDrawSpeedMult{ 1.0f };
		float CrossbowMult = 1.15f;
		float CreatureMult{ 1.5f };
		float MeleeMult{ 1.0f };
		float UnarmedMult{ 1.0f };
		float NormalAttackMult{ 1.0 };
		float PowerAttackMult{ 1.0f };

		float ToPCMult{ 1.0f };
		float ToNPCMult{ 1.0f };

		float PoiseScaling{ 0.25f };

		float WeightContribution{ 0.005f };
		float GauntletWeightContribution{ 0.05f };
		float UnarmedSkillContribution{ 0.4f };
		float AttackOfOpportunityMult{ 1.5f };

		float NormalImpactThreshold{ 0.25f };
		float PowerfulImpactThreshold{ 0.50f };
		float SeismicImpactThreshold{ 0.75f };

	} Damage;

	struct
	{
		bool     SpecialBar{ true };
		uint32_t SpecialBarNormalColor{ 0xFFFF00 };    // Normal poise bar color
		uint32_t SpecialBarDepletedColor{ 0x808080 };  // Poise depleted color
	} TrueHUD;

	struct DebugSettings
	{
		bool LogWeaponCalcs = false;
		bool LogArmorCalcs = false;
		bool LogMagicEffectCalcs = false;
		bool LogStaggerCalcs = false;
	} Debug;

	json JSONSettings;

	float GetDamageMultiplier(RE::Actor* a_aggressor, RE::Actor* a_target);
	void  LoadGameSettings();
	void  LoadINI(const wchar_t* a_path);
	void  LoadJSON(const wchar_t* a_path);
	void  LoadSettings();

private:
	Settings() = default;
	Settings(const Settings&) = delete;
	Settings(Settings&&) = delete;

	~Settings() = default;

	Settings& operator=(const Settings&) = delete;
	Settings& operator=(Settings&&) = delete;
};
