#pragma once
#include "EldenParry.h"
#include "Storage/Settings.h"
#include <unordered_map>
// #include "lib/PrecisionAPI.h"

class HitEventHandler
{
	// friend EldenParry;
public:
	[[nodiscard]] static HitEventHandler* GetSingleton()
	{
		static HitEventHandler singleton;
		return std::addressof(singleton);
	}

	static void InstallHooks()
	{
		logger::info("Installing HitEventHandler hooks...");
		Hooks::Install();
	}

	void InitializeEquipmentCache()
	{
		logger::info("Initializing combat equipment cache...");

		auto instance = GetSingleton();

		instance->_minWeapon =
			RE::TESForm::LookupByEditorID<RE::TESObjectWEAP>("IronDagger");

		instance->_maxWeapon =
			RE::TESForm::LookupByEditorID<RE::TESObjectWEAP>("DaedricWarhammer");

		instance->_minGauntlet =
			RE::TESForm::LookupByEditorID<RE::TESObjectARMO>("ArmorHideGauntlets");

		instance->_maxGauntlet =
			RE::TESForm::LookupByEditorID<RE::TESObjectARMO>("ArmorDaedricGauntlets");

		instance->_minShield =
			RE::TESForm::LookupByEditorID<RE::TESObjectARMO>("ArmorHideShield");

		instance->_maxShield =
			RE::TESForm::LookupByEditorID<RE::TESObjectARMO>("ArmorDaedricShield");

		instance->_maxArmorHelmet =
			RE::TESForm::LookupByEditorID<RE::TESObjectARMO>("ArmorDaedricHelmet");

		instance->_maxArmorCuirass =
			RE::TESForm::LookupByEditorID<RE::TESObjectARMO>("ArmorDaedricCuirass");

		instance->_maxArmorBoots =
			RE::TESForm::LookupByEditorID<RE::TESObjectARMO>("ArmorDaedricBoots");

		instance->_maxArmorGauntlets =
			RE::TESForm::LookupByEditorID<RE::TESObjectARMO>("ArmorDaedricGauntlets");

		instance->_maxArmorShield =
			RE::TESForm::LookupByEditorID<RE::TESObjectARMO>("ArmorDaedricShield");

		instance->_maxArmorRating =
			(instance->_maxArmorHelmet ? instance->_maxArmorHelmet->GetArmorRating() : 0.0f) +
			(instance->_maxArmorCuirass ? instance->_maxArmorCuirass->GetArmorRating() : 0.0f) +
			(instance->_maxArmorBoots ? instance->_maxArmorBoots->GetArmorRating() : 0.0f) +
			(instance->_maxArmorGauntlets ? instance->_maxArmorGauntlets->GetArmorRating() : 0.0f) +
			(instance->_maxArmorShield ? instance->_maxArmorShield->GetArmorRating() : 0.0f);

		// Weapon Cache Logging
		if (instance->_minWeapon) {
			logger::info(
				"[Weapon Cache Min] Name={} Damage={} Weight={}",
				instance->_minWeapon->GetName(),
				instance->_minWeapon->GetAttackDamage(),
				instance->_minWeapon->GetWeight());
		} else {
			logger::error("[Weapon Cache Min] NULL");
		}

		if (instance->_maxWeapon) {
			logger::info(
				"[Weapon Cache Max] Name={} Damage={} Weight={}",
				instance->_maxWeapon->GetName(),
				instance->_maxWeapon->GetAttackDamage(),
				instance->_maxWeapon->GetWeight());
		} else {
			logger::error("[Weapon Cache Max] NULL");
		}

		// Gauntlet Cache Logging
		if (instance->_minGauntlet) {
			logger::info(
				"[Gauntlet Cache Min] Name={} ArmorRating={} Weight={}",
				instance->_minGauntlet->GetName(),
				instance->_minGauntlet->GetArmorRating(),
				instance->_minGauntlet->GetWeight());
		} else {
			logger::error("[Gauntlet Cache Min] NULL");
		}

		if (instance->_maxGauntlet) {
			logger::info(
				"[Gauntlet Cache Max] Name={} ArmorRating={} Weight={}",
				instance->_maxGauntlet->GetName(),
				instance->_maxGauntlet->GetArmorRating(),
				instance->_maxGauntlet->GetWeight());
		} else {
			logger::error("[Gauntlet Cache Max] NULL");
		}
		// Shield Cache Logging
		if (instance->_minShield) {
			logger::info(
				"[Shield Cache Min] Name={} ArmorRating={} Weight={}",
				instance->_minShield->GetName(),
				instance->_minShield->GetArmorRating(),
				instance->_minShield->GetWeight());
		} else {
			logger::error("[Shield Cache Min] NULL");
		}

		if (instance->_maxShield) {
			logger::info(
				"[Shield Cache Max] Name={} ArmorRating={} Weight={}",
				instance->_maxShield->GetName(),
				instance->_maxShield->GetArmorRating(),
				instance->_maxShield->GetWeight());
		} else {
			logger::error("[Shield Cache Max] NULL");
		}

		// Armor Cache Logging
		auto logArmor = [](const char* slot, RE::TESObjectARMO* armor) {
			if (armor) {
				logger::info(
					"[Armor Cache] Slot={} Name={} ArmorRating={} Weight={}",
					slot,
					armor->GetName(),
					armor->GetArmorRating(),
					armor->GetWeight());
			} else {
				logger::error(
					"[Armor Cache] Slot={} NULL",
					slot);
			}
		};

		logArmor("Helmet", instance->_maxArmorHelmet);
		logArmor("Cuirass", instance->_maxArmorCuirass);
		logArmor("Boots", instance->_maxArmorBoots);
		logArmor("Gauntlets", instance->_maxArmorGauntlets);
		logArmor("Shield", instance->_maxArmorShield);

		logger::info(
			"[Equipment Reference] MaxArmorRating={} ReferenceMultiplier={} EffectiveReference={}",
			instance->_maxArmorRating,
			Settings::GetSingleton()->Global.EquipmentReferenceMultiplier,
			instance->_maxArmorRating * Settings::GetSingleton()->Global.EquipmentReferenceMultiplier);

		// ==========================
		// Weapon Multiplier Cache
		// ==========================

		_weaponMultiplierCache.clear();

		auto settings = Settings::GetSingleton();

		auto& multipliers = settings->JSONSettings["Weapons"]["Multipliers"];

		if (multipliers.is_object()) {
			for (auto& [name, value] : multipliers.items()) {
				float mult = value.get<float>();

				_weaponMultiplierCache.emplace(name, mult);

				logger::info(
					"[Weapon Mult Cache] {} = {}",
					name,
					mult);
			}
		} else {
			logger::error("[Weapon Mult Cache] JSON entry Weapons->Multipliers missing!");
		}

		logger::info(
			"Cached {} weapon multipliers.",
			_weaponMultiplierCache.size());
	}

	float GetWeaponDamage(RE::TESObjectWEAP* a_weapon, bool ignoreWeight = false);
	float CalculateWeaponStagger(RE::Actor* aggressor, RE::TESObjectWEAP* weapon);
	float CalculateProjectileStagger(RE::Actor* aggressor, RE::Projectile* projectile);
	float ApplyArmorReduction(RE::Actor* target, float stagger);

	float GetShieldDamage(RE::Actor* a_actor);
	bool  IsShieldStrike(RE::Actor* actor, RE::HitData* hitData);

	float GetUnarmedDamage(RE::Actor* a_actor);
	float GetBashDamage(RE::TESObjectARMO* a_shield);
	float GetMiscDamage();
	float ApplyAttackMultiplier(RE::HitData* hitData, float stagger);
	float CalculateBashStagger(RE::Actor* aggressor);
	float RecalculateStagger(RE::Actor* target, RE::Actor* aggressor, RE::HitData* hitData);
	float ApplyBlockingMultiplier(RE::HitData* hitData, RE::Actor* target, float stagger);
	bool  IsCreature(RE::Actor* actor);
	void  PreProcessHit(RE::Actor* target, RE::HitData* hitData);

protected:
	struct Hooks
	{
		struct ProcessHitEvent
		{
			static void thunk(RE::Actor* target, RE::HitData* hitData)
			{
				auto handler = GetSingleton();
				handler->PreProcessHit(target, hitData);
				func(target, hitData);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		static void Install()
		{
			stl::write_thunk_call<ProcessHitEvent>(REL::RelocationID(37673, 38627).address() + REL::Relocate(0x3C0, 0x4A8, 0x3C0));  // 1.5.97 140628C20
		}
	};

	// static void PoiseCallback_Post(const PRECISION_API::PrecisionHitData& a_precisionHitData, const RE::HitData& hitData);

private:
	// Weapon scaling baseline
	RE::TESObjectWEAP* _minWeapon{ nullptr };
	RE::TESObjectWEAP* _maxWeapon{ nullptr };

	// Unarmed gauntlet scaling baseline
	RE::TESObjectARMO* _minGauntlet{ nullptr };
	RE::TESObjectARMO* _maxGauntlet{ nullptr };

	// Shield bash scaling baseline
	RE::TESObjectARMO* _minShield{ nullptr };
	RE::TESObjectARMO* _maxShield{ nullptr };

	// Armor poise scaling baseline
	RE::TESObjectARMO* _maxArmorHelmet{ nullptr };
	RE::TESObjectARMO* _maxArmorCuirass{ nullptr };
	RE::TESObjectARMO* _maxArmorBoots{ nullptr };
	RE::TESObjectARMO* _maxArmorGauntlets{ nullptr };
	RE::TESObjectARMO* _maxArmorShield{ nullptr };

	float _maxArmorRating{ 0.0f };

	// Cached JSON weapon multipliers
	std::unordered_map<std::string, float> _weaponMultiplierCache;

	HitEventHandler() noexcept = default;
	HitEventHandler(const HitEventHandler&) = delete;
	HitEventHandler(HitEventHandler&&) = delete;

	~HitEventHandler() = default;

	HitEventHandler& operator=(const HitEventHandler&) = delete;
	HitEventHandler& operator=(HitEventHandler&&) = delete;
};
