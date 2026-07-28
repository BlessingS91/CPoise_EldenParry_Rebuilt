# UnarmedCalcs.py


def calculate_unarmed_poise(
    unarmed_damage,
    gauntlet_armor=0,
    gauntlet_weight=0,
    skill=0,
    armor_contribution=1.0,
    weight_contribution=0.15,
    skill_contribution=0.40,
    heavy_gauntlet_multiplier=1.0,
    light_gauntlet_multiplier=0.8,
    is_heavy=False,
    min_damage=4.0,
    max_damage=27.0,
    max_damage_multiplier=7.0,
):

    max_damage *= max_damage_multiplier

    damage_range = max_damage - min_damage

    if damage_range <= 0:
        return 0.0

    normalized_damage = (unarmed_damage - min_damage) / damage_range

    normalized_damage = max(0.0, min(1.0, normalized_damage))

    # Same weapon curve
    r1 = normalized_damage * 2.5

    r2 = r1 / (1 + r1)

    poise = 20.0 + (r2 * 55.0)

    #
    # Gauntlet scaling
    #

    armor_factor = gauntlet_armor / 100.0

    armor_factor *= armor_contribution

    weight_factor = gauntlet_weight * weight_contribution

    gauntlet_factor = armor_factor + weight_factor

    gauntlet_factor = max(0.0, min(1.0, gauntlet_factor))

    gauntlet_bonus = gauntlet_factor * 25.0

    if is_heavy:
        gauntlet_bonus *= heavy_gauntlet_multiplier
    else:
        gauntlet_bonus *= light_gauntlet_multiplier

    poise += gauntlet_bonus

    #
    # Skill scaling
    #

    skill_multiplier = 1.0 + (skill / 100.0) * skill_contribution

    poise *= skill_multiplier

    return max(0.0, min(200.0, poise))
