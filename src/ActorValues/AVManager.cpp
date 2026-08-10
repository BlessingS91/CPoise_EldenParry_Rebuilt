#include "ActorValues/AVManager.h"

#include "Storage/Serialization.h"

bool AVManager::SerializeSave(SKSE::SerializationInterface* a_intfc)
{
	if (!Serialization::Save(a_intfc, this->avStorage)) {
		logger::error("Failed to write actor values");
		return false;
	}
	return true;
}

bool AVManager::SerializeSave(SKSE::SerializationInterface* a_intfc, uint32_t a_type, uint32_t a_version)
{
	if (!a_intfc->OpenRecord(a_type, a_version)) {
		logger::error("Failed to open actor values record!");
		;
		return false;
	}

	return SerializeSave(a_intfc);
}

bool AVManager::DeserializeLoad(SKSE::SerializationInterface* a_intfc)
{
	if (!Serialization::Load(a_intfc, this->avStorage)) {
		logger::info("Failed to load actor values!");
		return false;
	}
	return true;
}

void AVManager::Revert()
{
	auto avManager = AVManager::GetSingleton();
	avManager->mtx.lock();
	avStorage.clear();
	avManager->mtx.unlock();
}

auto AVManager::ProcessEvent(const RE::TESFormDeleteEvent* a_event, RE::BSTEventSource<RE::TESFormDeleteEvent>*) -> RE::BSEventNotifyControl
{
	std::string formID = std::to_string(a_event->formID);
	mtx.lock();
	avStorage.erase(formID);
	mtx.unlock();
	return RE::BSEventNotifyControl::kContinue;
}

bool AVManager::RegisterActorValue(std::string avName, AVInterface* avInterface)
{
	if (registeredInterfaces[avName]) {
		return false;
	}
	registeredInterfaces[avName] = avInterface;
	return true;
}

float AVManager::GetBaseActorValue(std::string a_actorValue, RE::Actor* a_actor)
{
	std::string formID = std::to_string(a_actor->formID);
	if (avStorage[formID][a_actorValue] == nullptr)
		avStorage[formID][a_actorValue] = { 0.0f, 0.f, 0.0f };
	auto avInterface = registeredInterfaces.at(a_actorValue);
	auto value = avInterface->GetBaseActorValue(a_actor);
	return value;
}

float AVManager::GetActorValueMax(std::string a_actorValue, RE::Actor* a_actor)
{
	std::string formID = std::to_string(a_actor->formID);
	if (avStorage[formID][a_actorValue] == nullptr)
		avStorage[formID][a_actorValue] = { 0.0f, 0.f, 0.0f };
	auto avInterface = registeredInterfaces.at(a_actorValue);
	auto value = avInterface->GetActorValueMax(a_actor);
	return value;
}

void AVManager::DamageActorValue(
	std::string a_actorValue,
	RE::Actor*  a_actor,
	float       a_damage)
{
	if (!a_actor || a_damage == 0.0f) {
		return;
	}

	std::string formID = std::to_string(a_actor->formID);

	if (avStorage[formID][a_actorValue] == nullptr) {
		avStorage[formID][a_actorValue] = { 0.0f, 0.0f, 0.0f };
	}

	auto avInterface = registeredInterfaces.at(a_actorValue);

	float damage = avStorage[formID][a_actorValue][2];
	float newDamage = damage + a_damage;

	const float maxValue = avInterface->GetActorValueMax(a_actor);

	newDamage = std::clamp(newDamage, 0.0f, maxValue);

	avStorage[formID][a_actorValue][2] = newDamage;
}

float AVManager::GetActorValue(std::string a_actorValue, RE::Actor* a_actor)
{
	std::string formID = std::to_string(a_actor->formID);
	if (avStorage[formID][a_actorValue] == nullptr)
		avStorage[formID][a_actorValue] = { 0.0f, 0.f, 0.0f };
	auto  avInterface = registeredInterfaces.at(a_actorValue);
	float value = avInterface->GetBaseActorValue(a_actor);
	value += (float)avStorage[formID][a_actorValue][0];
	value += (float)avStorage[formID][a_actorValue][1];
	value -= (float)avStorage[formID][a_actorValue][2];
	return value;
}

float AVManager::GetActorValuePercentage(
	std::string a_actorValue,
	RE::Actor*  a_actor)
{
	const float max =
		GetActorValueMax(a_actorValue, a_actor);

	if (max <= 0.0f) {
		return 0.0f;
	}

	return GetActorValue(a_actorValue, a_actor) / max;
}

void AVManager::RestoreActorValue(
	std::string a_actorValue,
	RE::Actor*  a_actor,
	float       a_restore)
{
	if (!a_actor || a_restore <= 0.0f) {
		return;
	}

	std::string formID = std::to_string(a_actor->formID);

	if (avStorage[formID][a_actorValue] == nullptr) {
		avStorage[formID][a_actorValue] = { 0.0f, 0.0f, 0.0f };
	}

	float currentValue = GetActorValue(a_actorValue, a_actor);
	float maxValue = GetActorValueMax(a_actorValue, a_actor);

	if (currentValue >= maxValue) {
		return;
	}

	float restoreAmount = (a_restore < (maxValue - currentValue)) ? a_restore : (maxValue - currentValue);

	// Restoration means reducing the accumulated damage.
	DamageActorValue(a_actorValue, a_actor, -restoreAmount);
}

void AVManager::RestoreActorValueToMax(
	std::string a_actorValue,
	RE::Actor*  a_actor)
{
	if (!a_actor) {
		return;
	}

	float currentValue = GetActorValue(a_actorValue, a_actor);
	float maxValue = GetActorValueMax(a_actorValue, a_actor);

	float restoreAmount = maxValue - currentValue;

	if (restoreAmount <= 0.0f) {
		return;
	}

	DamageActorValue(a_actorValue, a_actor, -restoreAmount);
}
