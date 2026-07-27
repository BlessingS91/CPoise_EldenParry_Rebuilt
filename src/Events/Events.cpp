#include "Events/Events.h"

#include "ActorValues/AVManager.h"
#include "Hooks/HitEventHandler.h"
#include "Hooks/PoiseAV.h"

void Events::Register()
{
	cellLoadEventHandler::Register();
	fastTravelEventHandler::Register();
	waitEventHandler::Register();
}

bool cellLoadEventHandler::Register()
{
	static cellLoadEventHandler singleton;
	auto                        ScriptEventSource = RE::ScriptEventSourceHolder::GetSingleton();

	if (!ScriptEventSource) {
		logger::error("Script event source not found");
		return false;
	}

	ScriptEventSource->AddEventSink(&singleton);

	logger::info("Registered {}", typeid(singleton).name());

	return true;
}

RE::BSEventNotifyControl cellLoadEventHandler::ProcessEvent(
	const RE::TESCellFullyLoadedEvent*,
	RE::BSTEventSource<RE::TESCellFullyLoadedEvent>*)
{
	auto poiseAV = PoiseAV::GetSingleton();
	poiseAV->GarbageCollection();

	static bool initialized = false;

	if (!initialized) {
		logger::info("Initializing HitEventHandler weapon cache...");
		HitEventHandler::GetSingleton()->InitializeWeapons();
		initialized = true;
	}

	return RE::BSEventNotifyControl::kContinue;
}

bool fastTravelEventHandler::Register()
{
	static fastTravelEventHandler singleton;
	auto                          ScriptEventSource = RE::ScriptEventSourceHolder::GetSingleton();

	if (!ScriptEventSource) {
		logger::error("Script event source not found");
		return false;
	}

	ScriptEventSource->AddEventSink(&singleton);

	logger::info("Registered {}", typeid(singleton).name());

	return true;
}

RE::BSEventNotifyControl fastTravelEventHandler::ProcessEvent(const RE::TESFastTravelEndEvent*, RE::BSTEventSource<RE::TESFastTravelEndEvent>*)
{
	auto avManager = AVManager::GetSingleton();
	avManager->Revert();
	return RE::BSEventNotifyControl::kContinue;
}

bool waitEventHandler::Register()
{
	static waitEventHandler singleton;
	auto                    ScriptEventSource = RE::ScriptEventSourceHolder::GetSingleton();

	if (!ScriptEventSource) {
		logger::error("Script event source not found");
		return false;
	}

	ScriptEventSource->AddEventSink(&singleton);

	logger::info("Registered {}", typeid(singleton).name());

	return true;
}

RE::BSEventNotifyControl waitEventHandler::ProcessEvent(const RE::TESWaitStopEvent*, RE::BSTEventSource<RE::TESWaitStopEvent>*)
{
	auto avManager = AVManager::GetSingleton();
	avManager->Revert();
	return RE::BSEventNotifyControl::kContinue;
}
