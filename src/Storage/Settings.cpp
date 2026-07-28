#include "Storage/Settings.h"
#include "UI/PoiseAVHUD.h"
#include <fstream>

float Settings::GetDamageMultiplier(RE::Actor* a_aggressor, RE::Actor* a_target)
{
	if (a_aggressor && (a_aggressor->IsPlayerRef() || a_aggressor->IsPlayerTeammate())) {
		switch (static_cast<RE::DIFFICULTY>(RE::PlayerCharacter::GetSingleton()->GetGameStatsData().difficulty)) {
		case RE::DIFFICULTY::kNovice:
			return fDiffMultHPByPCVE;
		case RE::DIFFICULTY::kApprentice:
			return fDiffMultHPByPCE;
		case RE::DIFFICULTY::kAdept:
			return fDiffMultHPByPCN;
		case RE::DIFFICULTY::kExpert:
			return fDiffMultHPByPCH;
		case RE::DIFFICULTY::kMaster:
			return fDiffMultHPByPCVH;
		case RE::DIFFICULTY::kLegendary:
			return fDiffMultHPByPCL;
		}
	} else if (a_target && (a_target->IsPlayerRef() || a_target->IsPlayerTeammate())) {
		switch (static_cast<RE::DIFFICULTY>(RE::PlayerCharacter::GetSingleton()->GetGameStatsData().difficulty)) {
		case RE::DIFFICULTY::kNovice:
			return fDiffMultHPToPCVE;
		case RE::DIFFICULTY::kApprentice:
			return fDiffMultHPToPCE;
		case RE::DIFFICULTY::kAdept:
			return fDiffMultHPToPCN;
		case RE::DIFFICULTY::kExpert:
			return fDiffMultHPToPCH;
		case RE::DIFFICULTY::kMaster:
			return fDiffMultHPToPCVH;
		case RE::DIFFICULTY::kLegendary:
			return fDiffMultHPToPCL;
		}
	}
	return 1.0f;
}

void Settings::LoadGameSettings()
{
	auto gameSettingCollection = RE::GameSettingCollection::GetSingleton();
	if (!gameSettingCollection)
		return;

	auto getSettingFloat = [gameSettingCollection](const char* a_name) {
		auto setting = gameSettingCollection->GetSetting(a_name);
		return setting ? setting->GetFloat() : 1.0f;
	};

	fDiffMultHPByPCVE = getSettingFloat("fDiffMultHPByPCVE");
	fDiffMultHPByPCE = getSettingFloat("fDiffMultHPByPCE");
	fDiffMultHPByPCN = getSettingFloat("fDiffMultHPByPCN");
	fDiffMultHPByPCH = getSettingFloat("fDiffMultHPByPCH");
	fDiffMultHPByPCVH = getSettingFloat("fDiffMultHPByPCVH");
	fDiffMultHPByPCL = getSettingFloat("fDiffMultHPByPCL");

	fDiffMultHPToPCVE = getSettingFloat("fDiffMultHPToPCVE");
	fDiffMultHPToPCE = getSettingFloat("fDiffMultHPToPCE");
	fDiffMultHPToPCN = getSettingFloat("fDiffMultHPToPCN");
	fDiffMultHPToPCH = getSettingFloat("fDiffMultHPToPCH");
	fDiffMultHPToPCVH = getSettingFloat("fDiffMultHPToPCVH");
	fDiffMultHPToPCL = getSettingFloat("fDiffMultHPToPCL");
}

void Settings::LoadINI(const wchar_t* a_path)
{
	CSimpleIniA ini;
	ini.SetUnicode();

	if (ini.LoadFile(a_path) < 0) {
		logger::error(FMT_STRING("Failed to load INI file at path: {}"), SKSE::stl::utf16_to_utf8(a_path).value_or(""));
		return;
	}

	auto getFloatVal = [&](const char* primarySec, const char* primaryKey, const char* altSec, const char* altKey, float defaultVal) -> float {
		if (ini.GetValue(primarySec, primaryKey)) {
			return static_cast<float>(ini.GetDoubleValue(primarySec, primaryKey, defaultVal));
		}
		if (ini.GetValue(altSec, altKey)) {
			return static_cast<float>(ini.GetDoubleValue(altSec, altKey, defaultVal));
		}
		return defaultVal;
	};

	auto getBoolVal = [&](const char* primarySec, const char* primaryKey, const char* altSec, const char* altKey, bool defaultVal) -> bool {
		if (ini.GetValue(primarySec, primaryKey)) {
			return ini.GetBoolValue(primarySec, primaryKey, defaultVal);
		}
		if (ini.GetValue(altSec, altKey)) {
			return ini.GetBoolValue(altSec, altKey, defaultVal);
		}
		return defaultVal;
	};

	// Modes
	Modes.StaggerMode = static_cast<int>(ini.GetLongValue("Modes", "StaggerMode", Modes.StaggerMode));

	// Health
	Health.BaseMult = getFloatVal("Health", "BaseMult", "Health Settings", "Base Mult", Health.BaseMult);
	Health.MassMult = getFloatVal("Health", "MassMult", "Health Settings", "Mass Mult", Health.MassMult);
	Health.ArmorMult = getFloatVal("Health", "ArmorMult", "Health Settings", "Armor Mult", Health.ArmorMult);
	Health.ArmorMultMin = getFloatVal("Health", "ArmorMultMin", "Health Settings", "Armor Mult Min", Health.ArmorMultMin);
	Health.RegenRate = getFloatVal("Health", "RegenRate", "Health Settings", "Regen Rate", Health.RegenRate);

	// Weapon Damage
	Weapon.MeleeMult = getFloatVal("Weapon", "MeleeMult", "Damage Settings", "Melee Mult", Weapon.MeleeMult);
	Weapon.WeightContribution = getFloatVal("Weapon", "WeightContribution", "General Damage Settings", "Weight Contribution", Weapon.WeightContribution);
	Weapon.BowDrawSpeedMult = getFloatVal("Weapon", "BowDrawSpeedMult", "Damage Settings", "Bow Draw Speed Mult", Weapon.BowDrawSpeedMult);
	Weapon.ArrowDamageMult = getFloatVal("Weapon", "ArrowDamageMult", "Damage Settings", "Arrow Damage Mult", Weapon.ArrowDamageMult);
	Weapon.CrossbowMult = getFloatVal("Weapon", "CrossbowMult", "Damage Settings", "Crossbow Mult", Weapon.CrossbowMult);
	Weapon.MaxDamageMultiplier =
		getFloatVal(
			"Weapon",
			"MaxDamageMultiplier",
			"Damage Settings",
			"Max Damage Multiplier",
			Weapon.MaxDamageMultiplier);

	// Unarmed Damage
	Unarmed.ArmorContribution =
		getFloatVal(
			"Unarmed",
			"ArmorContribution",
			"Unarmed Damage Settings",
			"Armor Contribution",
			Unarmed.ArmorContribution);

	Unarmed.WeightContribution =
		getFloatVal(
			"Unarmed",
			"WeightContribution",
			"Unarmed Damage Settings",
			"Weight Contribution",
			Unarmed.WeightContribution);

	Unarmed.SkillContribution =
		getFloatVal(
			"Unarmed",
			"SkillContribution",
			"Unarmed Damage Settings",
			"Skill Contribution",
			Unarmed.SkillContribution);

	Unarmed.HeavyGauntletContribution =
		getFloatVal(
			"Unarmed",
			"HeavyGauntletContribution",
			"Unarmed Damage Settings",
			"Heavy Gauntlet Contribution",
			Unarmed.HeavyGauntletContribution);

	Unarmed.LightGauntletContribution =
		getFloatVal(
			"Unarmed",
			"LightGauntletContribution",
			"Unarmed Damage Settings",
			"Light Gauntlet Contribution",
			Unarmed.LightGauntletContribution);

	Unarmed.SkillType =
		static_cast<int>(
			ini.GetLongValue(
				"Unarmed",
				"SkillType",
				Unarmed.SkillType));

	Unarmed.MaxArmorMultiplier =
		getFloatVal(
			"Unarmed",
			"MaxArmorMultiplier",
			"Unarmed Damage Settings",
			"Max Armor Multiplier",
			Unarmed.MaxArmorMultiplier);

	// Creature Damage
	Creature.DamageMultiplier = getFloatVal("Creature", "DamageMultiplier", "Damage Settings", "Creature Damage Multiplier", Creature.DamageMultiplier);

	// Bash / Blocking
	Blocking.BashMult = getFloatVal("Blocking", "BashMult", "Damage Settings", "Bash Mult", Blocking.BashMult);
	Blocking.BlockingMult = getFloatVal("Blocking", "BlockingMult", "General Damage Settings", "Blocking Multiplier", Blocking.BlockingMult);

	// Shields
	Shield.ArmorContribution =
		getFloatVal(
			"Shield",
			"ArmorContribution",
			"Shield Settings",
			"Armor Contribution",
			Shield.ArmorContribution);

	Shield.WeightContribution =
		getFloatVal(
			"Shield",
			"WeightContribution",
			"Shield Settings",
			"Weight Contribution",
			Shield.WeightContribution);

	Shield.MaxArmorMultiplier =
		getFloatVal(
			"Shield",
			"MaxArmorMultiplier",
			"Shield Settings",
			"Max Armor Multiplier",
			Shield.MaxArmorMultiplier);

	// Attack Modifiers
	Attack.NormalAttackMult = getFloatVal("Attack", "NormalAttackMult", "Damage Settings", "Normal Attack Mult", Attack.NormalAttackMult);
	Attack.PowerAttackMult = getFloatVal("Attack", "PowerAttackMult", "Damage Settings", "Power Attack Mult", Attack.PowerAttackMult);
	Attack.AttackOfOpportunityMult = getFloatVal("Attack", "AttackOfOpportunityMult", "Damage", "AttackOfOpportunityMult", Attack.AttackOfOpportunityMult);

	// Magic Damage
	Magic.ResistanceMult = getFloatVal("Magic", "ResistanceMult", "General Damage Settings", "Magic Resistance Multiplier", Magic.ResistanceMult);

	// Environment / Traps
	Environment.TrapMult = getFloatVal("Environment", "TrapMult", "Damage", "TrapMult", Environment.TrapMult);

	// NPC / Player Multipliers
	Global.ToPCMult = getFloatVal("Global", "ToPCMult", "General Damage Settings", "Player Multiplier", Global.ToPCMult);
	Global.ToNPCMult = getFloatVal("Global", "ToNPCMult", "General Damage Settings", "NPC Multiplier", Global.ToNPCMult);
	// Difficulty Scaling Influence
	Global.DifficultyScaling = getFloatVal(
		"Global",
		"DifficultyScaling",
		"General Damage Settings",
		"Difficulty Scaling",
		Global.DifficultyScaling);
	// Impact Thresholds
	Impact.Normal = getFloatVal("Impact Thresholds", "Normal", "Impact Thresholds", "Normal Impact", Impact.Normal);
	Impact.Powerful = getFloatVal("Impact Thresholds", "Powerful", "Impact Thresholds", "Powerful Impact", Impact.Powerful);
	Impact.Seismic = getFloatVal("Impact Thresholds", "Seismic", "Impact Thresholds", "Seismic Impact", Impact.Seismic);

	// TrueHUD Integration
	TrueHUD.SpecialBar = getBoolVal("TrueHUD", "SpecialBar", "True HUD integration", "TrueHUD special bar usage", TrueHUD.SpecialBar);
	bool ignoreValhalla = getBoolVal("TrueHUD", "IgnoreValhallaCombat", "True HUD integration", "Ignore Valhalla Combat", false);

	if (GetModuleHandleA("valhallaCombat.dll") && !ignoreValhalla) {
		TrueHUD.SpecialBar = false;
	}

	if (auto normalColor = ini.GetValue("TrueHUD", "SpecialBarNormalColor", nullptr)) {
		TrueHUD.SpecialBarNormalColor = std::stoul(normalColor, nullptr, 16);
	}
	if (auto depletedColor = ini.GetValue("TrueHUD", "SpecialBarDepletedColor", nullptr)) {
		TrueHUD.SpecialBarDepletedColor = std::stoul(depletedColor, nullptr, 16);
	}

	// Debug
	Debug.LogWeaponCalcs = ini.GetBoolValue("Debug", "LogWeaponCalcs", Debug.LogWeaponCalcs);
	Debug.LogUnarmedCalcs = ini.GetBoolValue("Debug", "LogUnarmedCalcs", Debug.LogUnarmedCalcs);
	Debug.LogArmorCalcs = ini.GetBoolValue("Debug", "LogArmorCalcs", Debug.LogArmorCalcs);
	Debug.LogMagicEffectCalcs = ini.GetBoolValue("Debug", "LogMagicEffectCalcs", Debug.LogMagicEffectCalcs);
	Debug.LogStaggerCalcs = ini.GetBoolValue("Debug", "LogStaggerCalcs", Debug.LogStaggerCalcs);
	Debug.LogActorCalcs = ini.GetBoolValue("Debug", "LogActorCalcs", Debug.LogActorCalcs);

	// Logging
	logger::info(FMT_STRING("INI Loaded Successfully:"));
	logger::info(FMT_STRING("  [Modes] StaggerMode={}"), Modes.StaggerMode);
	logger::info(FMT_STRING("  [Health] BaseMult={} MassMult={} ArmorMult={} ArmorMultMin={} RegenRate={}"),
		Health.BaseMult, Health.MassMult, Health.ArmorMult, Health.ArmorMultMin, Health.RegenRate);
	logger::info(FMT_STRING("  [Weapon] MeleeMult={} WeightContribution={} BowDrawSpeedMult={} ArrowDamageMult={} CrossbowMult={} MaxDamageMultiplier={}"),
		Weapon.MeleeMult,
		Weapon.WeightContribution,
		Weapon.BowDrawSpeedMult,
		Weapon.ArrowDamageMult,
		Weapon.CrossbowMult,
		Weapon.MaxDamageMultiplier);
	logger::info(
		FMT_STRING(
			"  [Unarmed] ArmorContribution={} WeightContribution={} "
			"SkillContribution={} HeavyGauntletContribution={} "
			"LightGauntletContribution={} SkillType={} MaxArmorMultiplier={}"),
		Unarmed.ArmorContribution,
		Unarmed.WeightContribution,
		Unarmed.SkillContribution,
		Unarmed.HeavyGauntletContribution,
		Unarmed.LightGauntletContribution,
		Unarmed.SkillType,
		Unarmed.MaxArmorMultiplier);
	logger::info(FMT_STRING("  [Creature] DamageMult={}"), Creature.DamageMultiplier);
	logger::info(FMT_STRING("  [Blocking] BashMult={} BlockingMult={}"), Blocking.BashMult, Blocking.BlockingMult);
	logger::info(
		FMT_STRING(
			"  [Shield] ArmorContribution={} WeightContribution={} MaxArmorMultiplier={}"),
		Shield.ArmorContribution,
		Shield.WeightContribution,
		Shield.MaxArmorMultiplier);
	logger::info(FMT_STRING("  [Attack] NormalAttackMult={} PowerAttackMult={} AttackOfOpportunityMult={}"),
		Attack.NormalAttackMult, Attack.PowerAttackMult, Attack.AttackOfOpportunityMult);
	logger::info(FMT_STRING("  [Magic] ResistanceMult={}"), Magic.ResistanceMult);
	logger::info(FMT_STRING("  [Environment] TrapMult={}"), Environment.TrapMult);
	logger::info(FMT_STRING("  [Global] ToPCMult={} ToNPCMult={} DifficultyScaling={}"),
		Global.ToPCMult,
		Global.ToNPCMult,
		Global.DifficultyScaling);
	logger::info(FMT_STRING("  [Impact] Normal={} Powerful={} Seismic={}"),
		Impact.Normal, Impact.Powerful, Impact.Seismic);
	logger::info(FMT_STRING("  [TrueHUD] SpecialBar={} (IgnoreValhalla={})"),
		TrueHUD.SpecialBar, ignoreValhalla);
	logger::info(FMT_STRING("  [TrueHUD Colors] Normal=0x{:X} Depleted=0x{:X}"),
		TrueHUD.SpecialBarNormalColor, TrueHUD.SpecialBarDepletedColor);
	logger::info(FMT_STRING("  [Debug] Weapon={} Unarmed={} Armor={} MagicEffects={} Stagger={} Actors={}"),
		Debug.LogWeaponCalcs, Debug.LogUnarmedCalcs, Debug.LogArmorCalcs, Debug.LogMagicEffectCalcs, Debug.LogStaggerCalcs, Debug.LogActorCalcs);
}

void Settings::LoadJSON(const wchar_t* a_path)
{
	std::ifstream i(a_path);
	if (i.is_open()) {
		try {
			i >> JSONSettings;
		} catch (const std::exception& e) {
			logger::error(FMT_STRING("Failed to parse JSON file at {}: {}"),
				SKSE::stl::utf16_to_utf8(a_path).value_or(""), e.what());
		}
	}
}

void Settings::LoadSettings()
{
	LoadGameSettings();
	LoadINI(L"Data/SKSE/Plugins/ChocolatePoise.ini");
	LoadJSON(L"Data/SKSE/Plugins/ChocolatePoise.json");

	if (PoiseAVHUD::trueHUDInterface) {
		if (PoiseAVHUD::trueHUDInterface->RequestSpecialResourceBarsControl(SKSE::GetPluginHandle()) == TRUEHUD_API::APIResult::OK) {
			if (TrueHUD.SpecialBar)
				PoiseAVHUD::trueHUDInterface->RegisterSpecialResourceFunctions(SKSE::GetPluginHandle(), PoiseAVHUD::GetCurrentSpecial, PoiseAVHUD::GetMaxSpecial, true);
			else
				PoiseAVHUD::trueHUDInterface->ReleaseSpecialResourceBarControl(SKSE::GetPluginHandle());
		}
	}
}