#include "Hooks/ActiveEffectHandler.h"

#include "Hooks/PoiseAV.h"
#include "Storage/Settings.h"

//#include "ClibUtil/editorID.hpp"

float ActiveEffectHandler::CalculateEffectMultiplier(
	RE::ActorValue a_actorValue,
	bool           a_detrimental)
{
	auto settings = Settings::GetSingleton();

	const std::string effectType =
		a_detrimental ? "Damage" : "Recovery";

	std::string avName{ magic_enum::enum_name(a_actorValue) };

	if (avName.empty()) {
		return 0.0f;
	}

	// Remove k prefix (kHealth -> Health)
	avName.erase(0, 1);

	const auto jsonValue =
		settings->JSONSettings["Magic Effects"]
							  ["Actor Values"]
							  [effectType]
							  [avName];

	return jsonValue != nullptr ?
	           static_cast<float>(jsonValue) :
	           0.0f;
}

void ActiveEffectHandler::ProcessValueModifier(
	RE::Actor*     a_target,
	RE::ActorValue a_actorValue,
	float          a_magnitudeDelta,
	RE::Actor*     a_aggressor)
{
	auto settings = Settings::GetSingleton();

	if (!a_target ||
		std::abs(a_magnitudeDelta) <= 0.001f) {
		return;
	}

	std::string avName(magic_enum::enum_name(a_actorValue));

	if (avName.empty()) {
		return;
	}

	avName.erase(0, 1);

	const bool detrimental = a_magnitudeDelta > 0.0f;

	auto jsonAV =
		settings->JSONSettings["Magic Effects"]["Actor Values"]
							  [detrimental ? "Damage" : "Recovery"][avName];

	if (jsonAV == nullptr) {
		return;
	}

	auto poiseAV = PoiseAV::GetSingleton();

	if (!poiseAV->CanDamageActor(a_target)) {
		return;
	}

	const float effectMultiplier = static_cast<float>(jsonAV);

	const bool logMagic =
		settings->Debug.LogMagicEffectCalcs &&
		std::abs(a_magnitudeDelta) >= 0.1f;

	if (logMagic) {
		logger::info(
			"[Magic Entry] Target={} Aggressor={} SameActor={} AV={} Magnitude={}",
			a_target->GetName(),
			a_aggressor ? a_aggressor->GetName() : "NULL",
			a_aggressor == a_target,
			avName,
			a_magnitudeDelta);
	}

	float poiseDamage =
		effectMultiplier *
		a_magnitudeDelta;

	if (logMagic) {
		logger::info(
			"[Magic Base] Magnitude={} Mult={} Damage={}",
			a_magnitudeDelta,
			effectMultiplier,
			poiseDamage);
	}

	if (poiseDamage <= 0.0f) {
		return;
	}

	//
	// Perk modifiers
	//

	if (a_aggressor) {
		float baseMult = 1.0f;

		PoiseAV::ApplyPerkEntryPoint(
			34,
			a_aggressor->As<RE::Character>(),
			a_target->As<RE::Character>(),
			&baseMult);

		PoiseAV::ApplyPerkEntryPoint(
			33,
			a_target->As<RE::Character>(),
			a_aggressor->As<RE::Character>(),
			&baseMult);

		const float before = poiseDamage;

		poiseDamage *= baseMult;

		if (logMagic) {
			logger::info(
				"[Perk Mult] Mult={} Before={} After={}",
				baseMult,
				before,
				poiseDamage);
		}

		if (poiseDamage > 0.0f) {
			const float damageMultiplier =
				settings->GetDamageMultiplier(
					a_aggressor,
					a_target);

			const float damageBefore = poiseDamage;

			poiseDamage *= damageMultiplier;

			if (logMagic) {
				logger::info(
					"[Damage Mult] Mult={} Before={} After={}",
					damageMultiplier,
					damageBefore,
					poiseDamage);
			}
		}
	}

	//
	// Magic damage multiplier
	//

	const float magicDamageBefore = poiseDamage;

	poiseDamage *= settings->Magic.DamageMult;

	if (logMagic) {
		logger::info(
			"[Magic Damage Mult] Mult={} Before={} After={}",
			settings->Magic.DamageMult,
			magicDamageBefore,
			poiseDamage);
	}

	//
	// Magic scaling
	//

	if (poiseDamage > 25.0f) {
		const float before = poiseDamage;

		const float excess = poiseDamage - 25.0f;

		poiseDamage =
			25.0f +
			(excess / (1.0f + (excess / 100.0f)));

		if (logMagic) {
			logger::info(
				"[Magic Cap] Before={} After={}",
				before,
				poiseDamage);
		}
	}

	const float beforeResist = poiseDamage;

	poiseDamage =
		ApplyMagicPoiseResistance(
			a_target,
			poiseDamage);

	if (logMagic) {
		logger::info(
			"[Magic Final] Target={} AV={} Final={} BeforeResist={}",
			a_target->GetName(),
			avName,
			poiseDamage,
			beforeResist);
	}

	poiseAV->DamageAndCheckPoise(
		a_target,
		a_aggressor,
		poiseDamage);
}

float ActiveEffectHandler::ApplyMagicPoiseResistance(
	RE::Actor* a_target,
	float      a_damage)
{
	if (!a_target) {
		return a_damage;
	}

	auto settings = Settings::GetSingleton();
	auto avOwner = a_target->AsActorValueOwner();

	const float magicResist =
		avOwner->GetActorValue(RE::ActorValue::kResistMagic);

	const float fireResist =
		avOwner->GetActorValue(RE::ActorValue::kResistFire);

	const float frostResist =
		avOwner->GetActorValue(RE::ActorValue::kResistFrost);

	const float shockResist =
		avOwner->GetActorValue(RE::ActorValue::kResistShock);

	const float elementalAverage =
		(fireResist + frostResist + shockResist) / 3.0f;

	const float effectiveResist = std::clamp(
		(magicResist * 0.65f) +
			(elementalAverage * 0.35f),
		-100.0f,
		100.0f);

	float finalMult = 1.0f;
	float reduction = 0.0f;

	if (effectiveResist >= 0.0f) {
		const float normalized =
			effectiveResist / 100.0f;

		const float scaled =
			normalized *
			(1.5f * settings->Magic.ResistanceMult);

		reduction =
			scaled / (1.0f + scaled);

		finalMult =
			1.0f - reduction;
	} else {
		const float weakness =
			std::abs(effectiveResist) / 100.0f;

		finalMult =
			1.0f + weakness;

		reduction =
			-weakness;
	}

	const float finalDamage =
		a_damage * finalMult;

	if (settings->Debug.LogMagicEffectCalcs &&
		a_damage >= 1.0f) {
		logger::info(
			"[Magic Poise Resist] Target={} Magic={} Fire={} Frost={} Shock={} Effective={} Reduction={} Mult={} Before={} After={}",
			a_target->GetName(),
			magicResist,
			fireResist,
			frostResist,
			shockResist,
			effectiveResist,
			reduction,
			finalMult,
			a_damage,
			finalDamage);
	}

	return finalDamage;
}

//API Function
float ActiveEffectHandler::GetEffectiveMagicResistance(RE::Actor* a_target)
{
	if (!a_target) {
		return 0.0f;
	}

	auto avOwner = a_target->AsActorValueOwner();

	const float magicResist =
		avOwner->GetActorValue(RE::ActorValue::kResistMagic);

	const float fireResist =
		avOwner->GetActorValue(RE::ActorValue::kResistFire);

	const float frostResist =
		avOwner->GetActorValue(RE::ActorValue::kResistFrost);

	const float shockResist =
		avOwner->GetActorValue(RE::ActorValue::kResistShock);

	const float elementalAverage =
		(fireResist + frostResist + shockResist) / 3.0f;

	return std::clamp(
		(magicResist * 0.65f) +
			(elementalAverage * 0.35f),
		-100.0f,
		100.0f);
}

//Unused Trap stuff
// bool ActiveEffectHandler::IsTrapEffect(RE::EffectSetting* a_mgef)
// {
// 	if (!a_mgef) {
// 		return false;
// 	}

// 	const auto formID = a_mgef->GetFormID();

// 	if (auto it = _trapEffectCache.find(formID);
// 		it != _trapEffectCache.end()) {
// 		return it->second;
// 	}

// 	auto settings = Settings::GetSingleton();

// 	//
// 	// JSON ActorValue filter FIRST
// 	//

// 	auto actorValue = a_mgef->data.primaryAV;

// 	std::string avName(
// 		magic_enum::enum_name(actorValue));

// 	if (avName.empty()) {
// 		_trapEffectCache.emplace(formID, false);
// 		return false;
// 	}

// 	avName.erase(0, 1);

// 	auto jsonAV =
// 		settings->JSONSettings["Magic Effects"]
// 							  ["Actor Values"]
// 							  ["Damage"]
// 							  [avName];

// 	if (jsonAV == nullptr ||
// 		static_cast<float>(jsonAV) <= 0.0f) {
// 		_trapEffectCache.emplace(formID, false);
// 		return false;
// 	}

// 	//
// 	// ONLY NOW check Trap EditorID / Keywords
// 	//

// 	auto containsTrap = [](std::string a_string) {
// 		std::ranges::transform(
// 			a_string,
// 			a_string.begin(),
// 			[](unsigned char c) {
// 				return static_cast<char>(std::tolower(c));
// 			});

// 		return a_string.contains("trap") ||
// 		       a_string.contains("magictrap");
// 	};

// 	bool isTrap = false;

// 	auto editorID = clib_util::editorID::get_editorID(a_mgef);

// 	if (!editorID.empty() && containsTrap(editorID)) {
// 		isTrap = true;
// 	}

// 	if (!isTrap) {
// 		for (std::uint32_t i = 0; i < a_mgef->numKeywords; ++i) {
// 			auto* keyword = a_mgef->keywords[i];

// 			if (!keyword) {
// 				continue;
// 			}

// 			auto keywordID =
// 				clib_util::editorID::get_editorID(keyword);

// 			if (!keywordID.empty() && containsTrap(keywordID)) {
// 				isTrap = true;
// 				break;
// 			}
// 		}
// 	}

// 	_trapEffectCache.emplace(formID, isTrap);

// 	return isTrap;
// }

// bool ActiveEffectHandler::IsActorAffectedByTrap(RE::Actor* a_actor)
// {
// 	if (!a_actor) {
// 		return false;
// 	}

// 	auto* magicTarget = a_actor->AsMagicTarget();
// 	if (!magicTarget) {
// 		return false;
// 	}

// 	auto* activeEffects = magicTarget->GetActiveEffectList();
// 	if (!activeEffects) {
// 		return false;
// 	}

// 	auto settings = Settings::GetSingleton();

// 	for (auto* effect : *activeEffects) {
// 		if (!effect ||
// 			!effect->effect ||
// 			!effect->effect->baseEffect) {
// 			continue;
// 		}

// 		auto* mgef = effect->effect->baseEffect;

// 		if (!IsTrapEffect(mgef)) {
// 			continue;
// 		}

// 		if (settings->Debug.LogMagicEffectCalcs) {
// 			auto editorID = clib_util::editorID::get_editorID(mgef);

// 			logger::info(
// 				"[Active TRAP MGEF] Actor={} Name={} EditorID={} FormID={:08X}",
// 				a_actor->GetName(),
// 				mgef->GetName(),
// 				editorID.empty() ? "NULL" : editorID.c_str(),
// 				mgef->GetFormID());

// 			for (std::uint32_t i = 0; i < mgef->numKeywords; ++i) {
// 				auto* keyword = mgef->keywords[i];
// 				if (!keyword) {
// 					continue;
// 				}

// 				auto keywordID = clib_util::editorID::get_editorID(keyword);

// 				logger::info(
// 					"    Keyword={} EditorID={} FormID={:08X}",
// 					keyword->GetName(),
// 					keywordID.empty() ? "NULL" : keywordID.c_str(),
// 					keyword->GetFormID());
// 			}
// 		}

// 		return true;
// 	}

// 	return false;
// }

// void ActiveEffectHandler::DumpTrapEffects()
// {
// 	auto containsTrap = [](std::string a_string) {
// 		std::ranges::transform(
// 			a_string,
// 			a_string.begin(),
// 			[](unsigned char c) {
// 				return static_cast<char>(std::tolower(c));
// 			});

// 		return a_string.contains("trap") ||
// 		       a_string.contains("magictrap");
// 	};

// 	auto dataHandler = RE::TESDataHandler::GetSingleton();

// 	if (!dataHandler) {
// 		return;
// 	}

// 	auto settings = Settings::GetSingleton();

// 	logger::info("========== FILTERED TRAP MGEF DUMP START ==========");

// 	for (auto* mgef : dataHandler->GetFormArray<RE::EffectSetting>()) {
// 		if (!mgef) {
// 			continue;
// 		}

// 		//
// 		// JSON ActorValue filter
// 		//

// 		auto actorValue = mgef->data.primaryAV;

// 		std::string avName(
// 			magic_enum::enum_name(actorValue));

// 		if (avName.empty()) {
// 			continue;
// 		}

// 		// kHealth -> Health
// 		avName.erase(0, 1);

// 		auto jsonValue =
// 			settings->JSONSettings["Magic Effects"]
// 								  ["Actor Values"]
// 								  ["Damage"]
// 								  [avName];

// 		if (jsonValue == nullptr ||
// 			static_cast<float>(jsonValue) <= 0.0f) {
// 			continue;
// 		}

// 		const float jsonMultiplier =
// 			static_cast<float>(jsonValue);

// 		//
// 		// Trap EditorID / Keyword lookup
// 		//

// 		bool isTrap = false;

// 		auto editorID =
// 			clib_util::editorID::get_editorID(mgef);

// 		if (!editorID.empty() &&
// 			containsTrap(editorID)) {
// 			isTrap = true;
// 		}

// 		if (!isTrap) {
// 			for (std::uint32_t i = 0; i < mgef->numKeywords; i++) {
// 				auto* keyword = mgef->keywords[i];

// 				if (!keyword) {
// 					continue;
// 				}

// 				auto keywordID =
// 					clib_util::editorID::get_editorID(keyword);

// 				if (!keywordID.empty() &&
// 					containsTrap(keywordID)) {
// 					isTrap = true;
// 					break;
// 				}
// 			}
// 		}

// 		if (!isTrap) {
// 			continue;
// 		}

// 		logger::info(
// 			"[TRAP MGEF] Name={} EditorID={} FormID={:08X} JSON_AV={} JSON_Mult={}",
// 			mgef->GetName(),
// 			editorID.empty() ? "NULL" : editorID.c_str(),
// 			mgef->GetFormID(),
// 			avName,
// 			jsonMultiplier);

// 		for (std::uint32_t i = 0; i < mgef->numKeywords; i++) {
// 			auto* keyword = mgef->keywords[i];

// 			if (!keyword) {
// 				continue;
// 			}

// 			auto keywordID =
// 				clib_util::editorID::get_editorID(keyword);

// 			logger::info(
// 				"    Keyword={} EditorID={} FormID={:08X}",
// 				keyword->GetName(),
// 				keywordID.empty() ? "NULL" : keywordID.c_str(),
// 				keyword->GetFormID());
// 		}
// 	}

// 	logger::info("========== FILTERED TRAP MGEF DUMP END ==========");
// }