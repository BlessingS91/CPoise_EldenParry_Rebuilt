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
		auto dataHandler = RE::TESDataHandler::GetSingleton();
		if (dataHandler) {
			auto minWeap = RE::TESForm::LookupByEditorID<RE::TESObjectWEAP>("IronDagger");
			auto maxWeap = RE::TESForm::LookupByEditorID<RE::TESObjectWEAP>("DaedricWarhammer");

			auto instance = GetSingleton();
			instance->_minWeapon = minWeap;
			instance->_maxWeapon = maxWeap;

			if (!minWeap || !maxWeap) {
				logger::error("Failed to resolve baseline weapons (IronDagger or DaedricWarhammer)!");
			} else {
				logger::info("Baseline weapons successfully cached for scaling curve.");
			}
		}
		Hooks::Install();
	}

	float GetWeaponDamage(RE::TESObjectWEAP* a_weapon);
	float GetUnarmedDamage(RE::Actor* a_actor);
	float GetShieldDamage(RE::TESObjectARMO* a_shield);
	float GetMiscDamage();
	float ModActorBashMult(RE::Actor* aggressor);

	float RecalculateStagger(RE::Actor* target, RE::Actor* aggressor, RE::HitData* hitData);

	void PreProcessHit(RE::Actor* target, RE::HitData* hitData);

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
