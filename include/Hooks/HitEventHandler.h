#pragma once
#include "EldenParry.h"
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

	void InitializeWeapons()
	{
		logger::info("Initializing weapon cache...");

		auto instance = GetSingleton();

		instance->_minWeapon =
			RE::TESForm::LookupByEditorID<RE::TESObjectWEAP>("IronDagger");

		instance->_maxWeapon =
			RE::TESForm::LookupByEditorID<RE::TESObjectWEAP>("DaedricWarhammer");

		if (instance->_minWeapon) {
			logger::info(
				"[Weapon Cache Min] Name={} Damage={} Weight={}",
				instance->_minWeapon->GetName(),
				instance->_minWeapon->GetAttackDamage(),
				instance->_minWeapon->GetWeight());
		} else {
			logger::info("[Weapon Cache Min] NULL");
		}

		if (instance->_maxWeapon) {
			logger::info(
				"[Weapon Cache Max] Name={} Damage={} Weight={}",
				instance->_maxWeapon->GetName(),
				instance->_maxWeapon->GetAttackDamage(),
				instance->_maxWeapon->GetWeight());
		} else {
			logger::info("[Weapon Cache Max] NULL");
		}
	}

	float GetWeaponDamage(RE::TESObjectWEAP* a_weapon, bool ignoreWeight = false);
	float CalculateWeaponStagger(RE::Actor* aggressor, RE::TESObjectWEAP* weapon);
	float CalculateProjectileStagger(RE::Actor* aggressor, RE::Projectile* projectile);
	float ApplyArmorReduction(RE::Actor* target, float stagger);
	float GetUnarmedDamage(RE::Actor* a_actor);
	float GetShieldDamage(RE::TESObjectARMO* a_shield);
	float GetMiscDamage();
	float ApplyAttackMultiplier(RE::HitData* hitData, float stagger);
	float CalculateBashStagger(RE::Actor* aggressor);
	float RecalculateStagger(RE::Actor* target, RE::Actor* aggressor, RE::HitData* hitData);
	float ApplyBlockingMultiplier(RE::HitData* hitData, RE::Actor* aggressor, RE::Actor* target, float stagger);
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

private:
	RE::TESObjectWEAP* _minWeapon{ nullptr };
	RE::TESObjectWEAP* _maxWeapon{ nullptr };
	// static void PoiseCallback_Post(const PRECISION_API::PrecisionHitData& a_precisionHitData, const RE::HitData& hitData);
	constexpr HitEventHandler() noexcept = default;
	HitEventHandler(const HitEventHandler&) = delete;
	HitEventHandler(HitEventHandler&&) = delete;

	~HitEventHandler() = default;

	HitEventHandler& operator=(const HitEventHandler&) = delete;
	HitEventHandler& operator=(HitEventHandler&&) = delete;
};
