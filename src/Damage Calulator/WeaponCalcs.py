# WeaponCalcs.py


def calculate_weapon_poise(
    weapon_damage,
    weapon_weight,
    weapon_mult=1.0,
    min_damage=4.0,
    max_damage=27.0,
    max_damage_multiplier=5.0,
    weight_contribution=0.005,
    curve_strength=2.5,
    min_poise=25.0,
    max_poise=75.0,
):

    max_damage *= max_damage_multiplier

    damage_range = max_damage - min_damage

    if damage_range <= 0:
        return min_poise

    normalized_damage = (weapon_damage - min_damage) / damage_range

    normalized_damage = max(0.0, min(1.0, normalized_damage))

    # ARR curve
    r1 = normalized_damage * curve_strength

    r2 = r1 / (1.0 + r1)

    # Weapon weight
    weight_factor = weapon_weight * weight_contribution

    base_factor = r2 + weight_factor

    base_factor = max(0.0, min(1.0, base_factor))

    poise = min_poise + (base_factor * (max_poise - min_poise))

    return max(0.0, min(200.0, poise * weapon_mult))
