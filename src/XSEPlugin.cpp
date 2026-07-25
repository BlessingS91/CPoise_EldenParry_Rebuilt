#include "ActorValues/AVManager.h"
#include "AnimEventHandler.h"
#include "EldenParry.h"
#include "Events/Events.h"
#include "Hooks.h"
#include "Hooks/Hooks.h"
#include "Hooks/PoiseAV.h"
#include "Storage/ActorCache.h"
#include "Storage/Serialization.h"
#include "Storage/Settings.h"
#include "UI/PoiseAVHUD.h"
#include "Utils.hpp"

void OnDataLoaded()
{
}

void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
{
	switch (a_msg->type) {
	case SKSE::MessagingInterface::kDataLoaded:
		{
			auto poiseAV = PoiseAV::GetSingleton();
			poiseAV->RetrieveFullBodyStaggerFaction();
			auto settings = Settings::GetSingleton();
			settings->LoadSettings();
			EldenParry::GetSingleton()->init();
			animEventHandler::Register(true, EldenSettings::bEnableNPCParry);
			break;
		}
	case SKSE::MessagingInterface::kPostLoad:
		{
			auto poiseAVHUD = PoiseAVHUD::GetSingleton();
			poiseAVHUD->trueHUDInterface = reinterpret_cast<TRUEHUD_API::IVTrueHUD3*>(TRUEHUD_API::RequestPluginAPI(TRUEHUD_API::InterfaceVersion::V3));
			if (poiseAVHUD->trueHUDInterface) {
				logger::info("Obtained TrueHUD API");
			} else {
				logger::warn("Failed to obtain TrueHUD API");
			}
			break;
		}
	case SKSE::MessagingInterface::kPreLoadGame:
		{
			auto settings = Settings::GetSingleton();
			settings->LoadSettings();
			ActorCache::GetSingleton()->Revert();
			break;
		}
	case SKSE::MessagingInterface::kNewGame:
		{
			auto settings = Settings::GetSingleton();
			settings->LoadSettings();
			ActorCache::GetSingleton()->Revert();
			break;
		}
	default:
		break;
	}
}

void Init()
{
	auto messaging = SKSE::GetMessagingInterface();
	messaging->RegisterListener("SKSE", MessageHandler);
}

void onSKSEInit()
{
	auto poiseAV = PoiseAV::GetSingleton();
	auto avManager = AVManager::GetSingleton();
	avManager->RegisterActorValue(PoiseAV::g_avName, poiseAV);

	auto messaging = SKSE::GetMessagingInterface();
	messaging->RegisterListener("SKSE", MessageHandler);  // Keep only one listener registration

	auto serialization = SKSE::GetSerializationInterface();
	serialization->SetUniqueID(Serialization::kUniqueID);
	serialization->SetSaveCallback(Serialization::SaveCallback);
	serialization->SetLoadCallback(Serialization::LoadCallback);
	serialization->SetRevertCallback(Serialization::RevertCallback);

	Hooks::Install();
	Events::Register();
	ActorCache::RegisterEvents();
	EldenSettings::readSettings();
	EldenHooks::install();

	// Verify if Milf::GetSingleton()->Load() depends on game data
	// that isn't loaded yet. If so, move it to the kDataLoaded event instead.
}

void InitializeLog()
{
#ifndef NDEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		util::report_and_fail("Failed to find standard logging directory"sv);
	}

	*path /= fmt::format(FMT_STRING("{}.log"), Plugin::NAME);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

#ifndef NDEBUG
	const auto level = spdlog::level::trace;
#else
	const auto level = spdlog::level::info;
#endif

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
	log->set_level(level);
	log->flush_on(level);

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%l] %v"s);
}

EXTERN_C [[maybe_unused]] __declspec(dllexport) bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	// Remove or comment out this block:
	/*
    #ifndef NDEBUG
        while (!IsDebuggerPresent()) {
        };
    #endif
    */

	InitializeLog();

	logger::info("Loaded plugin");

	SKSE::Init(a_skse);

	Init();

	onSKSEInit();

	return true;
}

EXTERN_C [[maybe_unused]] __declspec(dllexport) constinit auto SKSEPlugin_Version = []() noexcept {
	SKSE::PluginVersionData v;
	v.PluginName("PluginName");
	v.PluginVersion({ 1, 0, 0, 0 });
	v.UsesAddressLibrary(true);
	v.HasNoStructUse(true);
	return v;
}();

EXTERN_C [[maybe_unused]] __declspec(dllexport) bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface*, SKSE::PluginInfo* pluginInfo)
{
	pluginInfo->name = SKSEPlugin_Version.pluginName;
	pluginInfo->infoVersion = SKSE::PluginInfo::kVersion;
	pluginInfo->version = SKSEPlugin_Version.pluginVersion;
	return true;
}