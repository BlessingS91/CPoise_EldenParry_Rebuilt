#include "Storage/Settings.h"

#include "UI/PoiseAVHUD.h"

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
	fDiffMultHPByPCVE = gameSettingCollection->GetSetting("fDiffMultHPByPCVE")->GetFloat();
	fDiffMultHPByPCE = gameSettingCollection->GetSetting("fDiffMultHPByPCE")->GetFloat();
	fDiffMultHPByPCN = gameSettingCollection->GetSetting("fDiffMultHPByPCN")->GetFloat();
	fDiffMultHPByPCH = gameSettingCollection->GetSetting("fDiffMultHPByPCH")->GetFloat();
	fDiffMultHPByPCVH = gameSettingCollection->GetSetting("fDiffMultHPByPCVH")->GetFloat();
	fDiffMultHPByPCL = gameSettingCollection->GetSetting("fDiffMultHPByPCL")->GetFloat();
	fDiffMultHPToPCVE = gameSettingCollection->GetSetting("fDiffMultHPToPCVE")->GetFloat();
	fDiffMultHPToPCE = gameSettingCollection->GetSetting("fDiffMultHPToPCE")->GetFloat();
	fDiffMultHPToPCN = gameSettingCollection->GetSetting("fDiffMultHPToPCN")->GetFloat();
	fDiffMultHPToPCH = gameSettingCollection->GetSetting("fDiffMultHPToPCH")->GetFloat();
	fDiffMultHPToPCVH = gameSettingCollection->GetSetting("fDiffMultHPToPCVH")->GetFloat();
	fDiffMultHPToPCL = gameSettingCollection->GetSetting("fDiffMultHPToPCL")->GetFloat();
}

void Settings::LoadINI(const wchar_t* a_path)
{
	CSimpleIniA ini;
	ini.SetUnicode();

	if (ini.LoadFile(a_path) < 0) {
		logger::error(FMT_STRING("Failed to load INI file at path: {}"), SKSE::stl::utf16_to_utf8(a_path).value_or(""));
		return;
	}

	// Modes
	Modes.StaggerMode = static_cast<int>(ini.GetLongValue("Modes", "StaggerMode", Modes.StaggerMode));

	// Health
	Health.BaseMult = static_cast<float>(
		ini.GetValue("Health", "BaseMult", nullptr) ? ini.GetDoubleValue("Health", "BaseMult", Health.BaseMult) : ini.GetDoubleValue("Health Settings", "Base Mult", Health.BaseMult));

	Health.MassMult = static_cast<float>(
		ini.GetValue("Health", "BaseMult", nullptr) ? ini.GetDoubleValue("Health", "MassMult", Health.MassMult) : ini.GetDoubleValue("Health Settings", "Mass Mult", Health.MassMult));

	Health.ArmorMult = static_cast<float>(
		ini.GetValue("Health", "ArmorMult", nullptr) ? ini.GetDoubleValue("Health", "ArmorMult", Health.ArmorMult) : ini.GetDoubleValue("Health Settings", "Armor Mult", Health.ArmorMult));

	Health.ArmorMultMin = static_cast<float>(
		ini.GetValue("Health", "ArmorMultMin", nullptr) ? ini.GetDoubleValue("Health", "ArmorMultMin", Health.ArmorMultMin) : ini.GetDoubleValue("Health Settings", "Armor Mult Min", Health.ArmorMultMin));

	Health.RegenRate = static_cast<float>(
		ini.GetValue("Health", "RegenRate", nullptr) ? ini.GetDoubleValue("Health", "RegenRate", Health.RegenRate) : ini.GetDoubleValue("Health Settings", "Regen Rate", Health.RegenRate));

	// Damage
	Damage.BashMult = static_cast<float>(
		ini.GetValue("Damage", "BashMult", nullptr) ? ini.GetDoubleValue("Damage", "BashMult", Damage.BashMult) : ini.GetDoubleValue("Damage Settings", "Bash Mult", Damage.BashMult));

	Damage.BowMult = static_cast<float>(
		ini.GetValue("Damage", "BowMult", nullptr) ? ini.GetDoubleValue("Damage", "BowMult", Damage.BowMult) : ini.GetDoubleValue("Damage Settings", "Bow Mult", Damage.BowMult));

	Damage.CreatureMult = static_cast<float>(
		ini.GetValue("Damage", "CreatureMult", nullptr) ? ini.GetDoubleValue("Damage", "CreatureMult", Damage.CreatureMult) : ini.GetDoubleValue("Damage Settings", "Creature Mult", Damage.CreatureMult));

	Damage.MeleeMult = static_cast<float>(
		ini.GetValue("Damage", "MeleeMult", nullptr) ? ini.GetDoubleValue("Damage", "MeleeMult", Damage.MeleeMult) : ini.GetDoubleValue("Damage Settings", "Melee Mult", Damage.MeleeMult));

	Damage.UnarmedMult = static_cast<float>(
		ini.GetValue("Damage", "UnarmedMult", nullptr) ? ini.GetDoubleValue("Damage", "UnarmedMult", Damage.UnarmedMult) : ini.GetDoubleValue("Unarmed Damage Settings", "Unarmed Damage Mult", Damage.UnarmedMult));

	Damage.ToPCMult = static_cast<float>(
		ini.GetValue("Damage", "ToPCMult", nullptr) ? ini.GetDoubleValue("Damage", "ToPCMult", Damage.ToPCMult) : ini.GetDoubleValue("General Damage Settings", "Player Multiplier", Damage.ToPCMult));

	Damage.ToNPCMult = static_cast<float>(
		ini.GetValue("Damage", "ToNPCMult", nullptr) ? ini.GetDoubleValue("Damage", "ToNPCMult", Damage.ToNPCMult) : ini.GetDoubleValue("General Damage Settings", "NPC Multiplier", Damage.ToNPCMult));

	Damage.PoiseScaling = static_cast<float>(
		ini.GetValue("Damage", "PoiseScaling", nullptr) ? ini.GetDoubleValue("Damage", "PoiseScaling", Damage.PoiseScaling) : ini.GetDoubleValue("General Damage Settings", "Poise Scaling", Damage.PoiseScaling));

	Damage.WeightContribution = static_cast<float>(
		ini.GetValue("Damage", "WeightContribution", nullptr) ? ini.GetDoubleValue("Damage", "WeightContribution", Damage.WeightContribution) : ini.GetDoubleValue("General Damage Settings", "Weight Contribution", Damage.WeightContribution));

	Damage.GauntletWeightContribution = static_cast<float>(
		ini.GetValue("Damage", "GauntletWeightContribution", nullptr) ? ini.GetDoubleValue("Damage", "GauntletWeightContribution", Damage.GauntletWeightContribution) : ini.GetDoubleValue("Unarmed Damage Settings", "Gauntlet Weight Contribution", Damage.GauntletWeightContribution));

	Damage.UnarmedSkillContribution = static_cast<float>(
		ini.GetValue("Damage", "UnarmedSkillContribution", nullptr) ? ini.GetDoubleValue("Damage", "UnarmedSkillContribution", Damage.UnarmedSkillContribution) : ini.GetDoubleValue("Unarmed Damage Settings", "Unarmed Skill Contribution", Damage.UnarmedSkillContribution));

	Damage.AttackOfOpportunityMult = static_cast<float>(ini.GetDoubleValue("Damage", "AttackOfOpportunityMult ", 1.5));

	// Impact Thresholds
	Damage.NormalImpactThreshold = static_cast<float>(
		ini.GetValue("Impact Thresholds", "Normal Impact", nullptr) ? ini.GetDoubleValue("Impact Thresholds", "Normal Impact", Damage.NormalImpactThreshold) : ini.GetDoubleValue("Impact Thresholds", "NormalImpact", Damage.NormalImpactThreshold));

	Damage.PowerfulImpactThreshold = static_cast<float>(
		ini.GetValue("Impact Thresholds", "Powerful Impact", nullptr) ? ini.GetDoubleValue("Impact Thresholds", "Powerful Impact", Damage.PowerfulImpactThreshold) : ini.GetDoubleValue("Impact Thresholds", "PowerfulImpact", Damage.PowerfulImpactThreshold));

	Damage.SeismicImpactThreshold = static_cast<float>(
		ini.GetValue("Impact Thresholds", "Seismic Impact", nullptr) ? ini.GetDoubleValue("Impact Thresholds", "Seismic Impact", Damage.SeismicImpactThreshold) : ini.GetDoubleValue("Impact Thresholds", "SeismicImpact", Damage.SeismicImpactThreshold));

	// TrueHUD Integration
	TrueHUD.SpecialBar = ini.GetValue("TrueHUD", "SpecialBar", nullptr) ? ini.GetBoolValue("TrueHUD", "SpecialBar", TrueHUD.SpecialBar) : ini.GetBoolValue("True HUD integration", "TrueHUD special bar usage", TrueHUD.SpecialBar);

	bool ignoreValhalla = ini.GetValue("TrueHUD", "IgnoreValhallaCombat", nullptr) ? ini.GetBoolValue("TrueHUD", "IgnoreValhallaCombat", false) : ini.GetBoolValue("True HUD integration", "Ignore Valhalla Combat", false);

	if (GetModuleHandleA("valhallaCombat.dll") && !ignoreValhalla) {
		TrueHUD.SpecialBar = false;
	}

	logger::info(FMT_STRING("INI Loaded Successfully:"));
	logger::info(FMT_STRING("  [Modes] StaggerMode={}"), Modes.StaggerMode);
	logger::info(FMT_STRING("  [Health] BaseMult={} ArmorMult={} ArmorMultMin={} RegenRate={}"),
		Health.BaseMult, Health.ArmorMult, Health.ArmorMultMin, Health.RegenRate);
	logger::info(FMT_STRING("  [Damage] BashMult={} BowMult={} CreatureMult={} MeleeMult={} UnarmedMult={}"),
		Damage.BashMult, Damage.BowMult, Damage.CreatureMult, Damage.MeleeMult, Damage.UnarmedMult);
	logger::info(FMT_STRING("  [Damage General] ToPCMult={} ToNPCMult={} PoiseScaling={} WeightContrib={} GauntletWeight={} UnarmedSkill={}"),
		Damage.ToPCMult, Damage.ToNPCMult, Damage.PoiseScaling, Damage.WeightContribution, Damage.GauntletWeightContribution, Damage.UnarmedSkillContribution);
	logger::info(FMT_STRING("  [Impact Thresholds] Normal={} Powerful={} Seismic={}"),
		Damage.NormalImpactThreshold, Damage.PowerfulImpactThreshold, Damage.SeismicImpactThreshold);
	logger::info(FMT_STRING("  [TrueHUD] SpecialBar={} (IgnoreValhalla={})"),
		TrueHUD.SpecialBar, ignoreValhalla);
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