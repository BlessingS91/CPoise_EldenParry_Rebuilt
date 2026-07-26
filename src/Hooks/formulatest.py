def calculate_weapon_poise(
    weapon_damage,
    weapon_weight,
    weapon_mult=1.0,
    min_damage=4.0,
    max_damage=27.0,
    max_multiplier=7.0,
    weight_contrib=0.005,
    min_poise=15.0,
    max_poise=70.0,
):
    """
    Matches current C++ GetWeaponDamage()
    """

    max_damage *= max_multiplier

    damage_range = max_damage - min_damage

    if damage_range <= 0:
        return min_poise

    normalized_damage = (weapon_damage - min_damage) / damage_range

    normalized_damage = max(0.0, min(1.0, normalized_damage))

    # ARR style curve
    r1 = normalized_damage * 5.0
    r2 = r1 / (1.0 + r1)

    weight_factor = weapon_weight * weight_contrib

    base_poise_factor = r2 + weight_factor

    base_poise_factor = max(0.0, min(1.0, base_poise_factor))

    final_value = min_poise + (base_poise_factor * (max_poise - min_poise))

    return max(0.0, min(200.0, final_value * weapon_mult))


def calculate_armor_reduction(
    armor_rating,
    armor_mult=0.040,
):
    """
    Matches current C++:

    r1 = (AR / 100) * ArmorMult * 5
    reduction = r1/(1+r1)
    """

    r1 = (armor_rating / 100.0) * armor_mult * 5.0

    return r1 / (1.0 + r1)


def apply_armor(
    poise_damage,
    armor_rating,
    armor_mult_min=0.20,
):

    reduction = calculate_armor_reduction(armor_rating)

    armor_multiplier = 1.0 - reduction

    if armor_multiplier < armor_mult_min:
        armor_multiplier = armor_mult_min

    return poise_damage * armor_multiplier


weapons = [
    ("Iron Dagger", 4, 2, 0.95),
    ("Steel Sword", 7, 10, 1.00),
    ("Steel Greatsword", 17, 17, 1.08),
    ("Legendary Steel Greatsword", 60, 17, 1.08),
    ("Daedric Sword", 16, 16, 1.00),
    ("Daedric Warhammer", 27, 31, 1.12),
    ("Legendary Daedric Warhammer", 80, 31, 1.12),
    ("Legendary Daedric Dagger", 55, 6, 0.95),
]


armor_values = [
    0,
    100,
    200,
    300,
    500,
    600,
    800,
    1000,
]


print("Normalized 100 Poise System")
print("==========================")

for name, damage, weight, mult in weapons:

    raw = calculate_weapon_poise(
        weapon_damage=damage, weapon_weight=weight, weapon_mult=mult
    )

    print(f"\n{name}")
    print(f" Raw Poise Damage: {raw:.2f}")

    for armor in armor_values:

        reduction = calculate_armor_reduction(armor)

        final = apply_armor(raw, armor)

        print(f" {armor:4} AR " f"({reduction*100:5.1f}% reduction): " f"{final:.2f}")
