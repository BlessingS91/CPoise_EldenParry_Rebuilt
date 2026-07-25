#pragma once
#include "ActorValues/AVManager.h"
#include "Hooks/PoiseAV.h"

class Utils
{
private:
#define PI 3.1415926535897932384626f

	static int soundHelper_a(void* manager, RE::BSSoundHandle* a2, int a3, int a4)  //sub_140BEEE70
	{
		using func_t = decltype(&soundHelper_a);
		REL::Relocation<func_t> func{ RELOCATION_ID(66401, 67663) };
		return func(manager, a2, a3, a4);
	}

	static void soundHelper_b(RE::BSSoundHandle* a1, RE::NiAVObject* source_node)  //sub_140BEDB10
	{
		using func_t = decltype(&soundHelper_b);
		REL::Relocation<func_t> func{ RELOCATION_ID(66375, 67636) };
		return func(a1, source_node);
	}

	static char __fastcall soundHelper_c(RE::BSSoundHandle* a1)  //sub_140BED530
	{
		using func_t = decltype(&soundHelper_c);
		REL::Relocation<func_t> func{ RELOCATION_ID(66355, 67616) };
		return func(a1);
	}

	static char set_sound_position(RE::BSSoundHandle* a1, float x, float y, float z)
	{
		using func_t = decltype(&set_sound_position);
		REL::Relocation<func_t> func{ RELOCATION_ID(66370, 67631) };
		return func(a1, x, y, z);
	}

	static inline const RE::BSFixedString poise_largest = "poise_largest_start";
	static inline const RE::BSFixedString poise_largest_fwd = "poise_largest_start_fwd";
	static inline const RE::BSFixedString poise_large = "poise_large_start";
	static inline const RE::BSFixedString poise_large_fwd = "poise_large_start_fwd";
	static inline const RE::BSFixedString poise_med = "poise_med_start";
	static inline const RE::BSFixedString poise_med_fwd = "poise_med_start_fwd";
	static inline const RE::BSFixedString poise_small = "poise_small_start";
	static inline const RE::BSFixedString poise_small_fwd = "poise_small_start_fwd";

	static inline const RE::BSFixedString staggerDirection = "staggerDirection";
	static inline const RE::BSFixedString StaggerMagnitude = "StaggerMagnitude";
	static inline const RE::BSFixedString staggerStart = "staggerStart";
	static inline const RE::BSFixedString staggerStop = "staggerStop";
	static inline const RE::BSFixedString bleedOutStart = "BleedoutStart";
	static inline const RE::BSFixedString bleedOutStop = "BleedOutStop";
	static inline const RE::BSFixedString bleedOutGraphBool = "IsBleedingOut";

	static inline const RE::BSFixedString recoilLargeStart = "recoilLargeStart";
	static inline const RE::BSFixedString recoilStart = "recoilStart";
	static inline const RE::BSFixedString attackStop = "attackStop";

	static inline bool ApproximatelyEqual(float A, float B)
	{
		return ((A - B) < FLT_EPSILON) && ((B - A) < FLT_EPSILON);
	}

	static inline void SetRotationMatrix(RE::NiMatrix3& a_matrix, float sacb, float cacb, float sb)
	{
		float cb = std::sqrtf(1 - sb * sb);
		float ca = cacb / cb;
		float sa = sacb / cb;
		a_matrix.entry[0][0] = ca;
		a_matrix.entry[0][1] = -sacb;
		a_matrix.entry[0][2] = sa * sb;
		a_matrix.entry[1][0] = sa;
		a_matrix.entry[1][1] = cacb;
		a_matrix.entry[1][2] = -ca * sb;
		a_matrix.entry[2][0] = 0.0;
		a_matrix.entry[2][1] = sb;
		a_matrix.entry[2][2] = cb;
	}

	static bool PredictAimProjectile(RE::NiPoint3 a_projectilePos, RE::NiPoint3 a_targetPosition, RE::NiPoint3 a_targetVelocity, float a_gravity, RE::NiPoint3& a_projectileVelocity)
	{
		// http://ringofblades.com/Blades/Code/PredictiveAim.cs

		float projectileSpeedSquared = a_projectileVelocity.SqrLength();
		float projectileSpeed = std::sqrtf(projectileSpeedSquared);

		if (projectileSpeed <= 0.f || a_projectilePos == a_targetPosition) {
			return false;
		}

		float        targetSpeedSquared = a_targetVelocity.SqrLength();
		float        targetSpeed = std::sqrtf(targetSpeedSquared);
		RE::NiPoint3 targetToProjectile = a_projectilePos - a_targetPosition;
		float        distanceSquared = targetToProjectile.SqrLength();
		float        distance = std::sqrtf(distanceSquared);
		RE::NiPoint3 direction = targetToProjectile;
		direction.Unitize();
		RE::NiPoint3 targetVelocityDirection = a_targetVelocity;
		targetVelocityDirection.Unitize();

		float cosTheta = (targetSpeedSquared > 0) ? direction.Dot(targetVelocityDirection) : 1.0f;

		bool  bValidSolutionFound = true;
		float t;

		if (ApproximatelyEqual(projectileSpeedSquared, targetSpeedSquared)) {
			// We want to avoid div/0 that can result from target and projectile traveling at the same speed
			//We know that cos(theta) of zero or less means there is no solution, since that would mean B goes backwards or leads to div/0 (infinity)
			if (cosTheta > 0) {
				t = 0.5f * distance / (targetSpeed * cosTheta);
			} else {
				bValidSolutionFound = false;
				t = 1;
			}
		} else {
			float a = projectileSpeedSquared - targetSpeedSquared;
			float b = 2.0f * distance * targetSpeed * cosTheta;
			float c = -distanceSquared;
			float discriminant = b * b - 4.0f * a * c;

			if (discriminant < 0) {
				// NaN
				bValidSolutionFound = false;
				t = 1;
			} else {
				// a will never be zero
				float uglyNumber = sqrtf(discriminant);
				float t0 = 0.5f * (-b + uglyNumber) / a;
				float t1 = 0.5f * (-b - uglyNumber) / a;

				// Assign the lowest positive time to t to aim at the earliest hit
				t = min(t0, t1);
				if (t < FLT_EPSILON) {
					t = max(t0, t1);
				}

				if (t < FLT_EPSILON) {
					// Time can't flow backwards when it comes to aiming.
					// No real solution was found, take a wild shot at the target's future location
					bValidSolutionFound = false;
					t = 1;
				}
			}
		}

		a_projectileVelocity = a_targetVelocity + (-targetToProjectile / t);

		if (!bValidSolutionFound) {
			a_projectileVelocity.Unitize();
			a_projectileVelocity *= projectileSpeed;
		}

		if (!ApproximatelyEqual(a_gravity, 0.f)) {
			float netFallDistance = (a_projectileVelocity * t).z;
			float gravityCompensationSpeed = (netFallDistance + 0.5f * a_gravity * t * t) / t;
			a_projectileVelocity.z = gravityCompensationSpeed;
		}

		return bValidSolutionFound;
	}

public:
	static void triggerStagger(RE::Actor* a_defender, RE::Actor* a_aggressor)
	{
		const auto caster = a_defender->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
		float      a_reprisal = (EldenParry::GetSingleton()->AttackerBeatsParry(a_aggressor, a_defender));

		auto bHasEldenParryPerk2 = false;
		auto bHasEldenParryPerk1 = false;
		auto bHasDragonsTail = false;
		auto bHasDeliverance = false;

		if (const auto perk = RE::TESForm::LookupByEditorID<RE::BGSPerk>(Milf::GetSingleton()->perks.EldenParry_Perk2); perk) {
			bHasEldenParryPerk2 = a_defender->HasPerk(perk);
		}
		if (const auto perk = RE::TESForm::LookupByEditorID<RE::BGSPerk>(Milf::GetSingleton()->perks.EldenParry_Perk1); perk) {
			bHasEldenParryPerk1 = a_defender->HasPerk(perk);
		}

		if (const auto perk = RE::TESForm::LookupByEditorID<RE::BGSPerk>(Milf::GetSingleton()->perks.DragonsTail_Perk); perk) {
			bHasDragonsTail = a_defender->HasPerk(perk);
		}

		if (const auto perk = RE::TESForm::LookupByEditorID<RE::BGSPerk>(Milf::GetSingleton()->perks.Deliverance_Perk); perk) {
			bHasDeliverance = a_defender->HasPerk(perk);
		}

		if (bHasEldenParryPerk2 || bHasEldenParryPerk1) {
			RE::MagicItem* eldenArmorSpell = nullptr;

			if (bHasEldenParryPerk2) {
				if (const auto spell = RE::TESForm::LookupByEditorID<RE::MagicItem>(Milf::GetSingleton()->perks.EldenParry_Spell2); spell) {
					eldenArmorSpell = spell;
				}

			} else if (bHasEldenParryPerk1) {
				if (const auto spell = RE::TESForm::LookupByEditorID<RE::MagicItem>(Milf::GetSingleton()->perks.EldenParry_Spell1); spell) {
					eldenArmorSpell = spell;
				}
			}

			if (eldenArmorSpell && (bHasEldenParryPerk2 || bHasEldenParryPerk1)) {
				caster->CastSpellImmediate(eldenArmorSpell, true, a_defender, 1, false, 45, a_defender);
			}
		}

		auto bDefenderHasShield = isEquippedShield(a_defender);
		auto defender_weaponType = UGetAttackWeapon(a_defender);

		// auto aggressor_weaponType = UGetAttackWeapon(a_aggressor->GetActorRuntimeData().currentProcess);
		auto weaponAI = a_aggressor->GetActorRuntimeData().currentProcess;

		if (weaponAI && weaponAI->high && weaponAI->high->attackData) {
			const RE::TESForm* weaponL = weaponAI->high->attackData.get()->IsLeftAttack() ? weaponAI->GetEquippedLeftHand() : weaponAI->GetEquippedRightHand();

			// if the weapon is underdetermined//
			if (!weaponL) {
				if (!isHumanoid(a_aggressor)) {
					//and attacker is not humanoid/
					if (defender_weaponType && defender_weaponType->GetWeaponType() == RE::WEAPON_TYPE::kHandToHandMelee) {
						//Defender parries with hand//
						if (PoiseAV::GetSingleton()->Score_GetBaseActorValue(a_aggressor) <= 11.0f) {
							//it's a tiny creature//
							if (bHasEldenParryPerk2) {
								if (a_reprisal >= 10.0f) {
									a_aggressor->NotifyAnimationGraph(recoilLargeStart);
									if (bHasDragonsTail) {
										PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
									}
									if (bHasDeliverance) {
										AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
									}
									return;
								} else if (a_reprisal < 10.0f) {
									a_aggressor->NotifyAnimationGraph(recoilStart);
									if (!(a_reprisal <= 0.0f)) {
										if (bHasDragonsTail) {
											PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
										}
										if (bHasDeliverance) {
											AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
										}
									}
									return;
								}
							} else if (bHasEldenParryPerk1) {
								if (a_reprisal >= 25.0f) {
									a_aggressor->NotifyAnimationGraph(recoilLargeStart);
									if (bHasDragonsTail) {
										PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
									}
									if (bHasDeliverance) {
										AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
									}
									return;
								} else if (a_reprisal >= 5.0f && a_reprisal < 25.0f) {
									a_aggressor->NotifyAnimationGraph(recoilStart);
									if (bHasDragonsTail) {
										PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
									}
									if (bHasDeliverance) {
										AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
									}
									return;
								} else if (a_reprisal < 5.0f) {
									a_defender->NotifyAnimationGraph(recoilStart);
									a_aggressor->NotifyAnimationGraph(recoilStart);
									return;
								}
							} else {
								if (a_reprisal >= 20.0f) {
									a_aggressor->NotifyAnimationGraph(recoilLargeStart);
									if (bHasDragonsTail) {
										PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
									}
									if (bHasDeliverance) {
										AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
									}
									return;
								} else if (a_reprisal >= 10.0f && a_reprisal < 20.0f) {
									a_aggressor->NotifyAnimationGraph(recoilStart);
									if (bHasDragonsTail) {
										PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
									}
									if (bHasDeliverance) {
										AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
									}
									return;
								} else if (a_reprisal >= 0.0f && a_reprisal < 10.0f) {
									a_defender->NotifyAnimationGraph(recoilStart);
									a_aggressor->NotifyAnimationGraph(recoilStart);
									return;
								} else if (a_reprisal < 0.0f) {
									a_defender->NotifyAnimationGraph(recoilLargeStart);
									return;
								}
							}

						} else {
							//is mid to massive creature== punish defender
							a_defender->NotifyAnimationGraph(recoilLargeStart);
							if (a_reprisal <= 0.0f) {
								a_reprisal += 100.0f;
							}
							PoiseAV::GetSingleton()->DamageAndCheckPoise(a_defender, a_aggressor, a_reprisal);
							return;
						}

					} else {
						//Defender parries with shield//
						if (bDefenderHasShield) {
							if (bHasEldenParryPerk2) {
								if (a_reprisal >= 20.0f) {
									a_aggressor->NotifyAnimationGraph(recoilLargeStart);
									if (bHasDragonsTail) {
										PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
									}
									if (bHasDeliverance) {
										AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
									}
									return;
								} else if (a_reprisal < 20.0f) {
									a_aggressor->NotifyAnimationGraph(recoilStart);
									if (!(a_reprisal <= 0.0f)) {
										if (bHasDragonsTail) {
											PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
										}
										if (bHasDeliverance) {
											AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
										}
									} else {
										PoiseAV::GetSingleton()->DamageAndCheckPoise(a_defender, a_aggressor, -(a_reprisal));
									}
									return;
								}
							} else if (bHasEldenParryPerk1) {
								if (a_reprisal >= 15.0f) {
									a_aggressor->NotifyAnimationGraph(recoilLargeStart);
									if (bHasDragonsTail) {
										PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
									}
									if (bHasDeliverance) {
										AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
									}
									return;
								} else if (a_reprisal < 15.0f) {
									a_aggressor->NotifyAnimationGraph(recoilStart);
									if (!(a_reprisal <= 0.0f)) {
										if (bHasDragonsTail) {
											PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
										}
										if (bHasDeliverance) {
											AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
										}
									} else {
										PoiseAV::GetSingleton()->DamageAndCheckPoise(a_defender, a_aggressor, -(a_reprisal));
									}
									return;
								}
							} else {
								if (a_reprisal >= 10.0f) {
									a_aggressor->NotifyAnimationGraph(recoilLargeStart);
									if (bHasDragonsTail) {
										PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
									}
									if (bHasDeliverance) {
										AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
									}
									return;
								} else if (a_reprisal < 10.0f) {
									a_aggressor->NotifyAnimationGraph(recoilStart);
									if (!(a_reprisal <= 0.0f)) {
										if (bHasDragonsTail) {
											PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
										}
										if (bHasDeliverance) {
											AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
										}
									} else {
										PoiseAV::GetSingleton()->DamageAndCheckPoise(a_defender, a_aggressor, -(a_reprisal));
									}
									return;
								}
							}
						}
						//Defender parries with weapon//
						if (bHasEldenParryPerk2) {
							if (a_reprisal >= 10.0f) {
								a_aggressor->NotifyAnimationGraph(recoilLargeStart);
								if (bHasDragonsTail) {
									PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
								}
								if (bHasDeliverance) {
									AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
								}
								return;
							} else if (a_reprisal < 10.0f) {
								a_aggressor->NotifyAnimationGraph(recoilStart);
								if (!(a_reprisal <= 0.0f)) {
									if (bHasDragonsTail) {
										PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
									}
									if (bHasDeliverance) {
										AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
									}
								} else {
									PoiseAV::GetSingleton()->DamageAndCheckPoise(a_defender, a_aggressor, -(a_reprisal));
								}
								return;
							}
						} else if (bHasEldenParryPerk1) {
							if (a_reprisal >= 25.0f) {
								a_aggressor->NotifyAnimationGraph(recoilLargeStart);
								if (bHasDragonsTail) {
									PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
								}
								if (bHasDeliverance) {
									AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
								}
								return;
							} else if (a_reprisal >= 5.0f && a_reprisal < 25.0f) {
								a_aggressor->NotifyAnimationGraph(recoilStart);
								if (bHasDragonsTail) {
									PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
								}
								if (bHasDeliverance) {
									AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
								}
								return;
							} else if (a_reprisal < 5.0f) {
								a_defender->NotifyAnimationGraph(recoilStart);
								a_aggressor->NotifyAnimationGraph(recoilStart);
								if (a_reprisal < 0.0) {
									PoiseAV::GetSingleton()->DamageAndCheckPoise(a_defender, a_aggressor, -(a_reprisal));
								}
								return;
							}
						} else {
							if (a_reprisal >= 20.0f) {
								a_aggressor->NotifyAnimationGraph(recoilLargeStart);
								if (bHasDragonsTail) {
									PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
								}
								if (bHasDeliverance) {
									AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
								}
								return;
							} else if (a_reprisal >= 10.0f && a_reprisal < 20.0f) {
								a_aggressor->NotifyAnimationGraph(recoilStart);
								if (bHasDragonsTail) {
									PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
								}
								if (bHasDeliverance) {
									AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
								}
								return;
							} else if (a_reprisal >= 0.0f && a_reprisal < 10.0f) {
								a_defender->NotifyAnimationGraph(recoilStart);
								a_aggressor->NotifyAnimationGraph(recoilStart);
								return;
							} else if (a_reprisal < 0.0f) {
								a_defender->NotifyAnimationGraph(recoilLargeStart);
								PoiseAV::GetSingleton()->DamageAndCheckPoise(a_defender, a_aggressor, -(a_reprisal));
								return;
							}
						}
					}
				} else {
					//Attacker is humanoid//
					if (defender_weaponType && defender_weaponType->GetWeaponType() == RE::WEAPON_TYPE::kHandToHandMelee) {
						//Dedender parrries with hand//
						if (bHasEldenParryPerk2) {
							if (a_reprisal >= 10.0f) {
								a_aggressor->NotifyAnimationGraph(recoilLargeStart);
								if (bHasDragonsTail) {
									PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
								}
								if (bHasDeliverance) {
									AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
								}
								return;
							} else if (a_reprisal < 10.0f) {
								a_aggressor->NotifyAnimationGraph(recoilStart);
								if (!(a_reprisal <= 0.0f)) {
									if (bHasDragonsTail) {
										PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
									}
									if (bHasDeliverance) {
										AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
									}
								}
								return;
							}
						} else if (bHasEldenParryPerk1) {
							if (a_reprisal >= 25.0f) {
								a_aggressor->NotifyAnimationGraph(recoilLargeStart);
								if (bHasDragonsTail) {
									PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
								}
								if (bHasDeliverance) {
									AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
								}
								return;
							} else if (a_reprisal >= 5.0f && a_reprisal < 25.0f) {
								a_aggressor->NotifyAnimationGraph(recoilStart);
								if (bHasDragonsTail) {
									PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
								}
								if (bHasDeliverance) {
									AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
								}
								return;
							} else if (a_reprisal < 5.0f) {
								a_defender->NotifyAnimationGraph(recoilStart);
								a_aggressor->NotifyAnimationGraph(recoilStart);
								return;
							}
						} else {
							if (a_reprisal >= 20.0f) {
								a_aggressor->NotifyAnimationGraph(recoilLargeStart);
								if (bHasDragonsTail) {
									PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
								}
								if (bHasDeliverance) {
									AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
								}
								return;
							} else if (a_reprisal >= 10.0f && a_reprisal < 20.0f) {
								a_aggressor->NotifyAnimationGraph(recoilStart);
								if (bHasDragonsTail) {
									PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
								}
								if (bHasDeliverance) {
									AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
								}
								return;
							} else if (a_reprisal >= 0.0f && a_reprisal < 10.0f) {
								a_defender->NotifyAnimationGraph(recoilStart);
								a_aggressor->NotifyAnimationGraph(recoilStart);
								return;
							} else if (a_reprisal < 0.0f) {
								a_defender->NotifyAnimationGraph(recoilLargeStart);
								a_aggressor->NotifyAnimationGraph(recoilStart);
								return;
							}
						}
					} else {
						//Dedender parrries with weapon or shield//
						a_aggressor->NotifyAnimationGraph(recoilLargeStart);
						if (a_reprisal <= 0.0f) {
							a_reprisal += 50.0f;
						}
						PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, a_reprisal);
						if (bHasDeliverance) {
							AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, a_reprisal);
						}
						return;
					}
				}
			}

			/// Filtered out undefined weapon, now define the weapon//

			auto aggressor_weaponType = weaponL->As<RE::TESObjectWEAP>();

			//Hand to Hand //

			if (defender_weaponType && defender_weaponType->GetWeaponType() == RE::WEAPON_TYPE::kHandToHandMelee && aggressor_weaponType->GetWeaponType() == RE::WEAPON_TYPE::kHandToHandMelee) {
				// and atacker is humanoid//
				if (bHasEldenParryPerk2) {
					if (a_reprisal >= 10.0f) {
						a_aggressor->NotifyAnimationGraph(recoilLargeStart);
						if (bHasDragonsTail) {
							PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
						}
						if (bHasDeliverance) {
							AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
						}
						return;
					} else if (a_reprisal < 10.0f) {
						a_aggressor->NotifyAnimationGraph(recoilStart);
						if (!(a_reprisal <= 0.0f)) {
							if (bHasDragonsTail) {
								PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
							}
							if (bHasDeliverance) {
								AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
							}
						}
						return;
					}
				} else if (bHasEldenParryPerk1) {
					if (a_reprisal >= 25.0f) {
						a_aggressor->NotifyAnimationGraph(recoilLargeStart);
						if (bHasDragonsTail) {
							PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
						}
						if (bHasDeliverance) {
							AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
						}
						return;
					} else if (a_reprisal >= 5.0f && a_reprisal < 25.0f) {
						a_aggressor->NotifyAnimationGraph(recoilStart);
						if (bHasDragonsTail) {
							PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
						}
						if (bHasDeliverance) {
							AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
						}
						return;
					} else if (a_reprisal < 5.0f) {
						a_defender->NotifyAnimationGraph(recoilStart);
						a_aggressor->NotifyAnimationGraph(recoilStart);
						return;
					}
				} else {
					if (a_reprisal >= 20.0f) {
						a_aggressor->NotifyAnimationGraph(recoilLargeStart);
						if (bHasDragonsTail) {
							PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
						}
						if (bHasDeliverance) {
							AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
						}
						return;
					} else if (a_reprisal >= 10.0f && a_reprisal < 20.0f) {
						a_aggressor->NotifyAnimationGraph(recoilStart);
						if (bHasDragonsTail) {
							PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
						}
						if (bHasDeliverance) {
							AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
						}
						return;
					} else if (a_reprisal >= 0.0f && a_reprisal < 10.0f) {
						a_defender->NotifyAnimationGraph(recoilStart);
						a_aggressor->NotifyAnimationGraph(recoilStart);
						return;
					} else if (a_reprisal < 0.0f) {
						a_defender->NotifyAnimationGraph(recoilLargeStart);
						a_aggressor->NotifyAnimationGraph(recoilStart);
						return;
					}
				}
			}

			// defender parries with hand against a weapon or sheild = punish defender)

			if (defender_weaponType && defender_weaponType->GetWeaponType() == RE::WEAPON_TYPE::kHandToHandMelee) {
				if (!(aggressor_weaponType->GetWeaponType() == RE::WEAPON_TYPE::kHandToHandMelee)) {
					a_defender->NotifyAnimationGraph(recoilLargeStart);
					if (a_reprisal <= 0.0f) {
						a_reprisal += 50.0f;
					}
					PoiseAV::GetSingleton()->DamageAndCheckPoise(a_defender, a_aggressor, a_reprisal);
					return;
				}
			}

			// defender parries with weapon/sheild against hand attack and attacker is humanoid = punish attacker) //&& isHumanoid(a_aggressor)

			if (!(defender_weaponType && defender_weaponType->GetWeaponType() == RE::WEAPON_TYPE::kHandToHandMelee)) {
				if (aggressor_weaponType->GetWeaponType() == RE::WEAPON_TYPE::kHandToHandMelee) {
					a_aggressor->NotifyAnimationGraph(recoilLargeStart);
					if (a_reprisal <= 0.0f) {
						a_reprisal += 50.0f;
					}
					PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, a_reprisal);
					if (bHasDeliverance) {
						AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, a_reprisal);
					}
					return;
				}
			}

			// Normal parries weapons/ sheilds

			if (bDefenderHasShield) {
				if (bHasEldenParryPerk2) {
					if (a_reprisal >= 20.0f) {
						a_aggressor->NotifyAnimationGraph(recoilLargeStart);
						if (bHasDragonsTail) {
							PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
						}
						if (bHasDeliverance) {
							AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
						}
						return;
					} else if (a_reprisal < 20.0f) {
						a_aggressor->NotifyAnimationGraph(recoilStart);
						if (!(a_reprisal <= 0.0f)) {
							if (bHasDragonsTail) {
								PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
							}
							if (bHasDeliverance) {
								AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
							}
						}
						return;
					}
				} else if (bHasEldenParryPerk1) {
					if (a_reprisal >= 15.0f) {
						a_aggressor->NotifyAnimationGraph(recoilLargeStart);
						if (bHasDragonsTail) {
							PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
						}
						if (bHasDeliverance) {
							AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
						}
						return;
					} else if (a_reprisal < 15.0f) {
						a_aggressor->NotifyAnimationGraph(recoilStart);
						if (!(a_reprisal <= 0.0f)) {
							if (bHasDragonsTail) {
								PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
							}
							if (bHasDeliverance) {
								AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
							}
						}
						return;
					}
				} else {
					if (a_reprisal >= 10.0f) {
						a_aggressor->NotifyAnimationGraph(recoilLargeStart);
						if (bHasDragonsTail) {
							PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
						}
						if (bHasDeliverance) {
							AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
						}
						return;
					} else if (a_reprisal < 10.0f) {
						a_aggressor->NotifyAnimationGraph(recoilStart);
						if (!(a_reprisal <= 0.0f)) {
							if (bHasDragonsTail) {
								PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
							}
							if (bHasDeliverance) {
								AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
							}
						}
						return;
					}
				}
			} else {
				if (bHasEldenParryPerk2) {
					if (a_reprisal >= 10.0f) {
						a_aggressor->NotifyAnimationGraph(recoilLargeStart);
						if (bHasDragonsTail) {
							PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
						}
						if (bHasDeliverance) {
							AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
						}
						return;
					} else if (a_reprisal < 10.0f) {
						a_aggressor->NotifyAnimationGraph(recoilStart);
						if (!(a_reprisal <= 0.0f)) {
							if (bHasDragonsTail) {
								PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
							}
							if (bHasDeliverance) {
								AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
							}
						}
						return;
					}
				} else if (bHasEldenParryPerk1) {
					if (a_reprisal >= 25.0f) {
						a_aggressor->NotifyAnimationGraph(recoilLargeStart);
						if (bHasDragonsTail) {
							PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
						}
						if (bHasDeliverance) {
							AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
						}
						return;
					} else if (a_reprisal >= 5.0f && a_reprisal < 25.0f) {
						a_aggressor->NotifyAnimationGraph(recoilStart);
						if (bHasDragonsTail) {
							PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
						}
						if (bHasDeliverance) {
							AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
						}
						return;
					} else if (a_reprisal < 5.0f) {
						a_defender->NotifyAnimationGraph(recoilStart);
						a_aggressor->NotifyAnimationGraph(recoilStart);
						return;
					}
				} else {
					if (a_reprisal >= 20.0f) {
						a_aggressor->NotifyAnimationGraph(recoilLargeStart);
						if (bHasDragonsTail) {
							PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
						}
						if (bHasDeliverance) {
							AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
						}
						return;
					} else if (a_reprisal >= 10.0f && a_reprisal < 20.0f) {
						a_aggressor->NotifyAnimationGraph(recoilStart);
						if (bHasDragonsTail) {
							PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
						}
						if (bHasDeliverance) {
							AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
						}
						return;
					} else if (a_reprisal >= 0.0f && a_reprisal < 10.0f) {
						a_defender->NotifyAnimationGraph(recoilStart);
						a_aggressor->NotifyAnimationGraph(recoilStart);
						return;
					} else if (a_reprisal < 0.0f) {
						a_defender->NotifyAnimationGraph(recoilLargeStart);
						a_aggressor->NotifyAnimationGraph(recoilStart);
						return;
					}
				}
			}

		} else {
			//WeaponAI only//
			if (!isHumanoid(a_aggressor)) {
				//and attacker is not humanoid/
				if (defender_weaponType && defender_weaponType->GetWeaponType() == RE::WEAPON_TYPE::kHandToHandMelee) {
					//Defender parries with hand//
					if (PoiseAV::GetSingleton()->Score_GetBaseActorValue(a_aggressor) <= 11.0f) {
						//it's a tiny creature//
						if (bHasEldenParryPerk2) {
							if (a_reprisal >= 10.0f) {
								a_aggressor->NotifyAnimationGraph(recoilLargeStart);
								if (bHasDragonsTail) {
									PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
								}
								if (bHasDeliverance) {
									AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
								}
								return;
							} else if (a_reprisal < 10.0f) {
								a_aggressor->NotifyAnimationGraph(recoilStart);
								if (!(a_reprisal <= 0.0f)) {
									if (bHasDragonsTail) {
										PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
									}
									if (bHasDeliverance) {
										AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
									}
								}
								return;
							}
						} else if (bHasEldenParryPerk1) {
							if (a_reprisal >= 25.0f) {
								a_aggressor->NotifyAnimationGraph(recoilLargeStart);
								if (bHasDragonsTail) {
									PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
								}
								if (bHasDeliverance) {
									AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
								}
								return;
							} else if (a_reprisal >= 5.0f && a_reprisal < 25.0f) {
								a_aggressor->NotifyAnimationGraph(recoilStart);
								if (bHasDragonsTail) {
									PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
								}
								if (bHasDeliverance) {
									AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
								}
								return;
							} else if (a_reprisal < 5.0f) {
								a_defender->NotifyAnimationGraph(recoilStart);
								a_aggressor->NotifyAnimationGraph(recoilStart);
								return;
							}
						} else {
							if (a_reprisal >= 20.0f) {
								a_aggressor->NotifyAnimationGraph(recoilLargeStart);
								if (bHasDragonsTail) {
									PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
								}
								if (bHasDeliverance) {
									AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
								}
								return;
							} else if (a_reprisal >= 10.0f && a_reprisal < 20.0f) {
								a_aggressor->NotifyAnimationGraph(recoilStart);
								if (bHasDragonsTail) {
									PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
								}
								if (bHasDeliverance) {
									AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
								}
								return;
							} else if (a_reprisal >= 0.0f && a_reprisal < 10.0f) {
								a_defender->NotifyAnimationGraph(recoilStart);
								a_aggressor->NotifyAnimationGraph(recoilStart);
								return;
							} else if (a_reprisal < 0.0f) {
								a_defender->NotifyAnimationGraph(recoilLargeStart);
								return;
							}
						}

					} else {
						//is mid to massive creature== punish defender
						a_defender->NotifyAnimationGraph(recoilLargeStart);
						if (a_reprisal <= 0.0f) {
							a_reprisal += 100.0f;
						}
						PoiseAV::GetSingleton()->DamageAndCheckPoise(a_defender, a_aggressor, a_reprisal);
						return;
					}

				} else {
					//Defender parries with shield//
					if (bDefenderHasShield) {
						if (bHasEldenParryPerk2) {
							if (a_reprisal >= 20.0f) {
								a_aggressor->NotifyAnimationGraph(recoilLargeStart);
								if (bHasDragonsTail) {
									PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
								}
								if (bHasDeliverance) {
									AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
								}
								return;
							} else if (a_reprisal < 20.0f) {
								a_aggressor->NotifyAnimationGraph(recoilStart);
								if (!(a_reprisal <= 0.0f)) {
									if (bHasDragonsTail) {
										PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
									}
									if (bHasDeliverance) {
										AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
									}
								} else {
									PoiseAV::GetSingleton()->DamageAndCheckPoise(a_defender, a_aggressor, -(a_reprisal));
								}
								return;
							}
						} else if (bHasEldenParryPerk1) {
							if (a_reprisal >= 15.0f) {
								a_aggressor->NotifyAnimationGraph(recoilLargeStart);
								if (bHasDragonsTail) {
									PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
								}
								if (bHasDeliverance) {
									AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
								}
								return;
							} else if (a_reprisal < 15.0f) {
								a_aggressor->NotifyAnimationGraph(recoilStart);
								if (!(a_reprisal <= 0.0f)) {
									if (bHasDragonsTail) {
										PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
									}
									if (bHasDeliverance) {
										AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
									}
								} else {
									PoiseAV::GetSingleton()->DamageAndCheckPoise(a_defender, a_aggressor, -(a_reprisal));
								}
								return;
							}
						} else {
							if (a_reprisal >= 10.0f) {
								a_aggressor->NotifyAnimationGraph(recoilLargeStart);
								if (bHasDragonsTail) {
									PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
								}
								if (bHasDeliverance) {
									AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
								}
								return;
							} else if (a_reprisal < 10.0f) {
								a_aggressor->NotifyAnimationGraph(recoilStart);
								if (!(a_reprisal <= 0.0f)) {
									if (bHasDragonsTail) {
										PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
									}
									if (bHasDeliverance) {
										AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
									}
								} else {
									PoiseAV::GetSingleton()->DamageAndCheckPoise(a_defender, a_aggressor, -(a_reprisal));
								}
								return;
							}
						}
					}
					//Defender parries with weapon//
					if (bHasEldenParryPerk2) {
						if (a_reprisal >= 10.0f) {
							a_aggressor->NotifyAnimationGraph(recoilLargeStart);
							if (bHasDragonsTail) {
								PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
							}
							if (bHasDeliverance) {
								AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
							}
							return;
						} else if (a_reprisal < 10.0f) {
							a_aggressor->NotifyAnimationGraph(recoilStart);
							if (!(a_reprisal <= 0.0f)) {
								if (bHasDragonsTail) {
									PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
								}
								if (bHasDeliverance) {
									AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
								}
							} else {
								PoiseAV::GetSingleton()->DamageAndCheckPoise(a_defender, a_aggressor, -(a_reprisal));
							}
							return;
						}
					} else if (bHasEldenParryPerk1) {
						if (a_reprisal >= 25.0f) {
							a_aggressor->NotifyAnimationGraph(recoilLargeStart);
							if (bHasDragonsTail) {
								PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
							}
							if (bHasDeliverance) {
								AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
							}
							return;
						} else if (a_reprisal >= 5.0f && a_reprisal < 25.0f) {
							a_aggressor->NotifyAnimationGraph(recoilStart);
							if (bHasDragonsTail) {
								PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
							}
							if (bHasDeliverance) {
								AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
							}
							return;
						} else if (a_reprisal < 5.0f) {
							a_defender->NotifyAnimationGraph(recoilStart);
							a_aggressor->NotifyAnimationGraph(recoilStart);
							if (a_reprisal < 0.0) {
								PoiseAV::GetSingleton()->DamageAndCheckPoise(a_defender, a_aggressor, -(a_reprisal));
							}
							return;
						}
					} else {
						if (a_reprisal >= 20.0f) {
							a_aggressor->NotifyAnimationGraph(recoilLargeStart);
							if (bHasDragonsTail) {
								PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
							}
							if (bHasDeliverance) {
								AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
							}
							return;
						} else if (a_reprisal >= 10.0f && a_reprisal < 20.0f) {
							a_aggressor->NotifyAnimationGraph(recoilStart);
							if (bHasDragonsTail) {
								PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
							}
							if (bHasDeliverance) {
								AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
							}
							return;
						} else if (a_reprisal >= 0.0f && a_reprisal < 10.0f) {
							a_defender->NotifyAnimationGraph(recoilStart);
							a_aggressor->NotifyAnimationGraph(recoilStart);
							return;
						} else if (a_reprisal < 0.0f) {
							a_defender->NotifyAnimationGraph(recoilLargeStart);
							a_aggressor->NotifyAnimationGraph(recoilStart);
							PoiseAV::GetSingleton()->DamageAndCheckPoise(a_defender, a_aggressor, -(a_reprisal));
							return;
						}
					}
				}
			} else {
				//Attacker is humanoid//
				if (defender_weaponType && defender_weaponType->GetWeaponType() == RE::WEAPON_TYPE::kHandToHandMelee) {
					//Dedender parrries with hand//
					if (bHasEldenParryPerk2) {
						if (a_reprisal >= 10.0f) {
							a_aggressor->NotifyAnimationGraph(recoilLargeStart);
							if (bHasDragonsTail) {
								PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
							}
							if (bHasDeliverance) {
								AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
							}
							return;
						} else if (a_reprisal < 10.0f) {
							a_aggressor->NotifyAnimationGraph(recoilStart);
							if (!(a_reprisal <= 0.0f)) {
								if (bHasDragonsTail) {
									PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
								}
								if (bHasDeliverance) {
									AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
								}
							}
							return;
						}
					} else if (bHasEldenParryPerk1) {
						if (a_reprisal >= 25.0f) {
							a_aggressor->NotifyAnimationGraph(recoilLargeStart);
							if (bHasDragonsTail) {
								PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
							}
							if (bHasDeliverance) {
								AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
							}
							return;
						} else if (a_reprisal >= 5.0f && a_reprisal < 25.0f) {
							a_aggressor->NotifyAnimationGraph(recoilStart);
							if (bHasDragonsTail) {
								PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
							}
							if (bHasDeliverance) {
								AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
							}
							return;
						} else if (a_reprisal < 5.0f) {
							a_defender->NotifyAnimationGraph(recoilStart);
							a_aggressor->NotifyAnimationGraph(recoilStart);
							return;
						}
					} else {
						if (a_reprisal >= 20.0f) {
							a_aggressor->NotifyAnimationGraph(recoilLargeStart);
							if (bHasDragonsTail) {
								PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
							}
							if (bHasDeliverance) {
								AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
							}
							return;
						} else if (a_reprisal >= 10.0f && a_reprisal < 20.0f) {
							a_aggressor->NotifyAnimationGraph(recoilStart);
							if (bHasDragonsTail) {
								PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, (a_reprisal / 3.0f));
							}
							if (bHasDeliverance) {
								AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, (a_reprisal / 3.0f));
							}
							return;
						} else if (a_reprisal >= 0.0f && a_reprisal < 10.0f) {
							a_defender->NotifyAnimationGraph(recoilStart);
							a_aggressor->NotifyAnimationGraph(recoilStart);
							return;
						} else if (a_reprisal < 0.0f) {
							a_defender->NotifyAnimationGraph(recoilLargeStart);
							a_aggressor->NotifyAnimationGraph(recoilStart);
							return;
						}
					}
				} else {
					//Dedender parrries with weapon or shield//
					a_aggressor->NotifyAnimationGraph(recoilLargeStart);
					if (a_reprisal <= 0.0f) {
						a_reprisal += 50.0f;
					}
					PoiseAV::GetSingleton()->DamageAndCheckPoise(a_aggressor, a_defender, a_reprisal);
					if (bHasDeliverance) {
						AVManager::GetSingleton()->RestoreActorValue(PoiseAV::g_avName, a_defender, a_reprisal);
					}
					return;
				}
			}
		}
	};

	static bool isEquippedShield(RE::Actor* a_actor)
	{
		auto lhs = a_actor->GetEquippedObject(true);
		return lhs && lhs->IsArmor();
	}

	static bool isHumanoid(RE::Actor* a_actor)
	{
		auto bodyPartData = a_actor->GetRace() ? a_actor->GetRace()->bodyPartData : nullptr;
		return bodyPartData && bodyPartData->GetFormID() == 0x1d;
	}

	static RE::TESObjectWEAP* UGetAttackWeapon(RE::Actor* a_actor)
	{
		RE::TESObjectWEAP* weapon = nullptr;

		if (auto aiProcess = a_actor->GetActorRuntimeData().currentProcess; aiProcess) {
			if (aiProcess->high && aiProcess->high->attackData && aiProcess->high->attackData.get()) {
				auto equipped = aiProcess->high->attackData.get()->IsLeftAttack() ? aiProcess->GetEquippedLeftHand() : aiProcess->GetEquippedRightHand();

				if (equipped && equipped->Is(RE::FormType::Weapon)) {
					weapon = equipped->As<RE::TESObjectWEAP>();
				}
			}
		}
		return weapon;
	}

	static void resetProjectileOwner(RE::Projectile* a_projectile, RE::Actor* a_actor, RE::hkpCollidable* a_projectile_collidable)
	{
		a_projectile->SetActorCause(a_actor->GetActorCause());
		a_projectile->GetProjectileRuntimeData().shooter = a_actor->GetHandle();
		uint32_t a_collisionFilterInfo = a_actor->GetCollisionFilterInfo(a_collisionFilterInfo);
		a_projectile_collidable->broadPhaseHandle.collisionFilterInfo &= (0x0000FFFF);
		a_projectile_collidable->broadPhaseHandle.collisionFilterInfo |= (a_collisionFilterInfo << 16);
	}

	/*Play sound with formid at a certain actor's position.
	@param a: actor on which to play sonud.
	@param formid: formid of the sound descriptor.*/
	static void playSound(RE::Actor* a, RE::BGSSoundDescriptorForm* a_descriptor)
	{
		if (!a) {
			logger::error("playSound failed: actor is null");
			return;
		}

		if (!a_descriptor) {
			logger::error("playSound failed: sound descriptor is null for actor {}", a->GetName());
			return;
		}

		logger::debug("Playing sound formID {:X} on actor {}",
			a_descriptor->GetFormID(),
			a->GetName());

		RE::BSSoundHandle handle;
		handle.soundID = static_cast<uint32_t>(-1);
		handle.assumeSuccess = false;
		*(uint32_t*)&handle.state = 0;

		soundHelper_a(RE::BSAudioManager::GetSingleton(), &handle, a_descriptor->GetFormID(), 16);
		if (set_sound_position(&handle, a->data.location.x, a->data.location.y, a->data.location.z)) {
			soundHelper_b(&handle, a->Get3D());
			soundHelper_c(&handle);
		}
	}

	static void ReflectProjectile(RE::Projectile* a_projectile)
	{
		a_projectile->GetProjectileRuntimeData().linearVelocity *= -1.f;

		// rotate model
		auto projectileNode = a_projectile->Get3D2();
		if (projectileNode) {
			RE::NiPoint3 direction = a_projectile->GetProjectileRuntimeData().linearVelocity;
			direction.Unitize();

			a_projectile->data.angle.x = asin(direction.z);
			a_projectile->data.angle.z = atan2(direction.x, direction.y);

			if (a_projectile->data.angle.z < 0.0) {
				a_projectile->data.angle.z += PI;
			}

			if (direction.x < 0.0) {
				a_projectile->data.angle.z += PI;
			}

			Utils::SetRotationMatrix(projectileNode->local.rotate, -direction.x, direction.y, direction.z);
		}
	}

	/*Get the body position of this actor.*/
	static void getBodyPos(RE::Actor* a_actor, RE::NiPoint3& pos)
	{
		if (!a_actor->GetActorRuntimeData().race) {
			return;
		}
		RE::BGSBodyPart* bodyPart = a_actor->GetActorRuntimeData().race->bodyPartData->parts[0];
		if (!bodyPart) {
			return;
		}
		auto targetPoint = a_actor->GetNodeByName(bodyPart->targetName.c_str());
		if (!targetPoint) {
			return;
		}

		pos = targetPoint->world.translate;
	}

	/*retarget this projectile to a_target.*/
	static void RetargetProjectile(RE::Projectile* a_projectile, RE::TESObjectREFR* a_target)
	{
		a_projectile->GetProjectileRuntimeData().desiredTarget = a_target;

		auto projectileNode = a_projectile->Get3D2();
		auto targetHandle = a_target->GetHandle();

		RE::NiPoint3 targetPos = a_target->GetPosition();
		if (a_target->GetFormType() == RE::FormType::ActorCharacter) {
			getBodyPos(a_target->As<RE::Actor>(), targetPos);
		}

		RE::NiPoint3 targetVelocity;
		targetHandle.get()->GetLinearVelocity(targetVelocity);

		float projectileGravity = 0.f;
		if (auto ammo = a_projectile->GetProjectileRuntimeData().ammoSource) {
			if (auto bgsProjectile = ammo->data.projectile) {
				projectileGravity = bgsProjectile->data.gravity;
				if (auto bhkWorld = a_projectile->parentCell->GetbhkWorld()) {
					if (auto hkpWorld = bhkWorld->GetWorld1()) {
						auto  vec4 = hkpWorld->gravity;
						float quad[4];
						_mm_store_ps(quad, vec4.quad);
						float gravity = -quad[2] * RE::bhkWorld::GetWorldScaleInverse();
						projectileGravity *= gravity;
					}
				}
			}
		}

		PredictAimProjectile(a_projectile->data.location, targetPos, targetVelocity, projectileGravity, a_projectile->GetProjectileRuntimeData().linearVelocity);

		// rotate
		RE::NiPoint3 direction = a_projectile->GetProjectileRuntimeData().linearVelocity;
		direction.Unitize();

		a_projectile->data.angle.x = asin(direction.z);
		a_projectile->data.angle.z = atan2(direction.x, direction.y);

		if (a_projectile->data.angle.z < 0.0) {
			a_projectile->data.angle.z += PI;
		}

		if (direction.x < 0.0) {
			a_projectile->data.angle.z += PI;
		}

		SetRotationMatrix(projectileNode->local.rotate, -direction.x, direction.y, direction.z);
	}

	static RE::BSTimer* BSTimer_GetSingleton()
	{
		REL::Relocation<RE::BSTimer**> singleton{ REL::RelocationID(523657, 410196) };
		return *singleton;
	}

	static RE::BSTempEffectParticle* BSTimer_SetGlobalTimeMultiplier(RE::BSTimer* This, float a_percentage, bool a_unk = true)
	{
		using func_t = decltype(&BSTimer_SetGlobalTimeMultiplier);
		REL::Relocation<func_t> func{ RELOCATION_ID(66988, 68245) };
		return func(This, a_percentage, a_unk);
	}

	/*Slow down game time for a set period.
	@param a_duration: duration of the slow time.
	@param a_percentage: relative time speed to normal time(1).*/
	static void slowTime(float a_duration, float a_percentage)
	{
		int duration_milisec = static_cast<int>(a_duration * 1000);
		BSTimer_SetGlobalTimeMultiplier(BSTimer_GetSingleton(), a_percentage);
		/*Reset time here*/
		auto resetSlowTime = [](int a_duration) {
			std::this_thread::sleep_for(std::chrono::milliseconds(a_duration));
			BSTimer_SetGlobalTimeMultiplier(BSTimer_GetSingleton(), 1);
		};
		std::jthread resetThread(resetSlowTime, duration_milisec);
		resetThread.detach();
	}
};

class blockSpark
{
	friend class EldenParry;

private:
	static auto getBipedIndex(RE::TESForm* parryEquipment, bool rightHand)
	{
		if (!parryEquipment)
			return RE::BIPED_OBJECT::kNone;

		if (parryEquipment->As<RE::TESObjectWEAP>()) {
			switch (parryEquipment->As<RE::TESObjectWEAP>()->GetWeaponType()) {
			case RE::WEAPON_TYPE::kOneHandSword:
				return rightHand ? RE::BIPED_OBJECT::kOneHandSword : RE::BIPED_OBJECT::kShield;
			case RE::WEAPON_TYPE::kOneHandAxe:
				return rightHand ? RE::BIPED_OBJECT::kOneHandAxe : RE::BIPED_OBJECT::kShield;
			case RE::WEAPON_TYPE::kOneHandMace:
				return rightHand ? RE::BIPED_OBJECT::kOneHandMace : RE::BIPED_OBJECT::kShield;
			case RE::WEAPON_TYPE::kOneHandDagger:
				return rightHand ? RE::BIPED_OBJECT::kOneHandDagger : RE::BIPED_OBJECT::kShield;
			case RE::WEAPON_TYPE::kTwoHandAxe:
			case RE::WEAPON_TYPE::kTwoHandSword:
			case RE::WEAPON_TYPE::kHandToHandMelee:
				return RE::BIPED_OBJECT::kTwoHandMelee;
			}
		} else if (parryEquipment->IsArmor())
			return RE::BIPED_OBJECT::kShield;

		return RE::BIPED_OBJECT::kNone;
	}

public:
	static RE::BSTempEffectParticle* TESObjectCELL_PlaceParticleEffect(RE::TESObjectCELL* a_cell, float a_lifetime, const char* a_modelName, const RE::NiMatrix3& a_normal, const RE::NiPoint3& a_pos, float a_scale, std::uint32_t a_flags, RE::NiAVObject* a_target)
	{
		using func_t = decltype(&TESObjectCELL_PlaceParticleEffect);
		REL::Relocation<func_t> func{ RELOCATION_ID(29219, 30072) };
		return func(a_cell, a_lifetime, a_modelName, a_normal, a_pos, a_scale, a_flags, a_target);
	}

	static void playBlockSpark(RE::Actor* a_actor)
	{
		if (!a_actor || !a_actor->GetActorRuntimeData().currentProcess || !a_actor->GetActorRuntimeData().currentProcess->high || !a_actor->Get3D()) {
			return;
		}
		RE::BIPED_OBJECT BipeObjIndex;
		auto             defenderLeftEquipped = a_actor->GetEquippedObject(true);

		if (defenderLeftEquipped && (defenderLeftEquipped->IsWeapon() || defenderLeftEquipped->IsArmor())) {
			BipeObjIndex = getBipedIndex(defenderLeftEquipped, false);
		} else {
			BipeObjIndex = getBipedIndex(a_actor->GetEquippedObject(false), true);
		}

		if (BipeObjIndex == RE::BIPED_OBJECT::kNone) {
			return;
		}

		auto defenderNode = a_actor->GetCurrentBiped()->objects[BipeObjIndex].partClone;
		if (!defenderNode || !defenderNode.get()) {
			return;
		}
		const char* modelName;
		if (BipeObjIndex == RE::BIPED_OBJECT::kShield && defenderLeftEquipped && defenderLeftEquipped->IsArmor()) {
			if (EldenSettings::facts::isValhallaCombatAPIObtained) {
				modelName = "ValhallaCombat\\impactShieldRoot.nif";
			} else {
				modelName = "EldenParry\\impactShieldRoot.nif";
			}

		} else {
			if (EldenSettings::facts::isValhallaCombatAPIObtained) {
				modelName = "ValhallaCombat\\impactWeaponRoot.nif";
			} else {
				modelName = "EldenParry\\impactWeaponRoot.nif";
			}
		}
		//DEBUG("Get Weapon Spark Position!");
		TESObjectCELL_PlaceParticleEffect(a_actor->GetParentCell(), 0.0f, modelName, defenderNode->world.rotate, defenderNode->worldBound.center, 1.0f, 4U, defenderNode.get());
	}
};

class inlineUtils
{
public:
	static bool isPowerAttacking(RE::Actor* a_actor)
	{
		if (a_actor->GetActorRuntimeData().currentProcess && a_actor->GetActorRuntimeData().currentProcess->high) {
			auto atkData = a_actor->GetActorRuntimeData().currentProcess->high->attackData.get();
			if (atkData) {
				return atkData->data.flags.any(RE::AttackData::AttackFlag::kPowerAttack);
			}
		}
		return false;
	}

	static void restoreAv(RE::Actor* a_actor, RE::ActorValue a_actorValue, float a_val)
	{
		if (a_val == 0) {
			return;
		}
		if (a_actor) {
			a_actor->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, a_actorValue, a_val);
		}
	}

	static void damageAv(RE::Actor* a_actor, RE::ActorValue a_actorValue, float a_val)
	{
		if (a_val == 0) {
			return;
		}
		if (a_actor) {
			a_actor->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, a_actorValue, -a_val);
		}
	}

	static void shakeCamera(float strength, RE::NiPoint3 source, float duration)
	{
		using func_t = decltype(&shakeCamera);
		REL::Relocation<func_t> func{ RELOCATION_ID(32275, 33012) };
		func(strength, source, duration);
	}
};
