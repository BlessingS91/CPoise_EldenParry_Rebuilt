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
	ini.LoadFile(a_path);
	const auto get_value = [&]<class T>(T& a_value, const char* a_section, const char* a_key, const char* a_comment) {
		if constexpr (std::is_same_v<bool, T>) {
			a_value = ini.GetBoolValue(a_section, a_key, a_value);
			ini.SetBoolValue(a_section, a_key, a_value, a_comment);
		} else {
			a_value = static_cast<float>(ini.GetDoubleValue(a_section, a_key, a_value));
			ini.SetDoubleValue(a_section, a_key, a_value, a_comment);
		}
	};

	Modes.StaggerMode = ini.GetLongValue("Modes", "StaggerMode", Modes.StaggerMode);

	Health.BaseMult = static_cast<float>(ini.GetDoubleValue("Health", "BaseMult", Health.BaseMult));
	Health.ArmorMult = static_cast<float>(ini.GetDoubleValue("Health", "ArmorMult", Health.ArmorMult));
	Health.RegenRate = static_cast<float>(ini.GetDoubleValue("Health", "RegenRate", Health.RegenRate));

	Damage.BashMult = static_cast<float>(ini.GetDoubleValue("Damage", "BashMult", Damage.BashMult));
	Damage.BowMult = static_cast<float>(ini.GetDoubleValue("Damage", "BowMult", Damage.BowMult));
	Damage.CreatureMult = static_cast<float>(ini.GetDoubleValue("Damage", "CreatureMult", Damage.CreatureMult));
	Damage.MeleeMult = static_cast<float>(ini.GetDoubleValue("Damage", "MeleeMult", Damage.MeleeMult));

	Damage.UnarmedMult = static_cast<float>(ini.GetDoubleValue("Damage", "UnarmedMult", Damage.UnarmedMult));

	Damage.ToPCMult = static_cast<float>(ini.GetDoubleValue("Damage", "ToPCMult", Damage.ToPCMult));
	Damage.ToNPCMult = static_cast<float>(ini.GetDoubleValue("Damage", "ToNPCMult", Damage.ToNPCMult));

	Damage.PoiseScaling = static_cast<float>(ini.GetDoubleValue("Damage", "PoiseScaling", Damage.PoiseScaling));

	Damage.WeightContribution = static_cast<float>(ini.GetDoubleValue("Damage", "WeightContribution", Damage.WeightContribution));
	Damage.GauntletWeightContribution = static_cast<float>(ini.GetDoubleValue("Damage", "GauntletWeightContribution", Damage.GauntletWeightContribution));
	Damage.UnarmedSkillContribution = static_cast<float>(ini.GetDoubleValue("Damage", "UnarmedSkillContribution", Damage.UnarmedSkillContribution));

	TrueHUD.SpecialBar = ini.GetBoolValue("TrueHUD", "SpecialBar", TrueHUD.SpecialBar) && (!GetModuleHandleA("valhallaCombat.dll") || ini.GetBoolValue("TrueHUD", "IgnoreValhallaCombat", false));

	get_value(Health.BaseMult, "Health Settings", "Base Mult", ";Base Health Multiplier");
	get_value(Health.ArmorMult, "Health Settings", "Armor Mult", ";Armor Multiplier");
	get_value(Health.RegenRate, "Health Settings", "Regen Rate", ";Poise regeneration rate");

	get_value(Damage.BashMult, "Damage Settings", "Bash Mult", ";Multiplier for Bash Attacks");
	get_value(Damage.BowMult, "Damage Settings", "Bow Mult", ";Poise Damage Multiplier for Bow attacks");
	get_value(Damage.CreatureMult, "Damage Settings", "Creature Mult", ";Poise damage from creatures");
	get_value(Damage.MeleeMult, "Damage Settings", "Melee Mult", ";Poise damage multiplier from melee attacks");

	get_value(Damage.ToPCMult, "General Damage Settings", "Player Multiplier", ";Poise damage multiplier for the Player");
	get_value(Damage.ToNPCMult, "General Damage Settings", "NPC Multiplier", ";Poise damage multiplier to NPCs");
	get_value(Damage.WeightContribution, "General Damage Settings", "Weight Contribution", ";How much influence weight has to your poise damage");

	get_value(Damage.PoiseScaling, "General Damage Settings", "Poise Scaling", ";How much difficulty damage affects poise");

	get_value(Damage.UnarmedMult, "Unarmed Damage Settings", "Unarmed Damage Mult", ";Poise damage from unarmed attacks");
	get_value(Damage.GauntletWeightContribution, "Unarmed Damage Settings", "Gauntlet Weight Contribution", ";How much influence the gaunlet weight has to poise damage");
	get_value(Damage.UnarmedSkillContribution, "Unarmed Damage Settings", "Unarmed Skill Contribution", ";How much influence the Unarmed skill has to poise damage");

	get_value(Damage.NormalImpactThreshold,
		"Impact Thresholds",
		"Normal Impact",
		";Minimum poise damage percentage for normal impact");

	get_value(Damage.PowerfulImpactThreshold,
		"Impact Thresholds",
		"Powerful Impact",
		";Minimum poise damage percentage for powerful impact");

	get_value(Damage.SeismicImpactThreshold,
		"Impact Thresholds",
		"Seismic Impact",
		";Minimum poise damage percentage for seismic impact");

	get_value(TrueHUD.SpecialBar, "True HUD integration", "TrueHUD special bar usage", ";true or false");
	get_value(TrueHUD.SpecialBar, "True HUD integration", "Ignore Valhalla Combat", ";true or false");

	(void)ini.SaveFile(a_path);
}

void Settings::LoadJSON(const wchar_t* a_path)
{
	std::ifstream i(a_path);
	i >> JSONSettings;
}

void Settings::LoadSettings()
{
	LoadGameSettings();
	LoadINI(L"Data/SKSE/Plugins/ChocolatePoise.ini");
	LoadJSON(L"Data/SKSE/Plugins/ChocolatePoise.json");

	logger::debug(
		FMT_STRING("Loaded Impact Thresholds: Normal={} Powerful={} Seismic={}"),
		Damage.NormalImpactThreshold,
		Damage.PowerfulImpactThreshold,
		Damage.SeismicImpactThreshold);

	if (PoiseAVHUD::trueHUDInterface) {
		if (PoiseAVHUD::trueHUDInterface->RequestSpecialResourceBarsControl(SKSE::GetPluginHandle()) == TRUEHUD_API::APIResult::OK) {
			if (TrueHUD.SpecialBar)
				PoiseAVHUD::trueHUDInterface->RegisterSpecialResourceFunctions(SKSE::GetPluginHandle(), PoiseAVHUD::GetCurrentSpecial, PoiseAVHUD::GetMaxSpecial, true);
			else
				PoiseAVHUD::trueHUDInterface->ReleaseSpecialResourceBarControl(SKSE::GetPluginHandle());
		}
	}
}
