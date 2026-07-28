# MagicCalcs.py


def calculate_magic_damage(magnitude, effect_multiplier):
    """
    Matches ActiveEffectHandler:

        float poiseDamage = effectMultiplier * a_magnitudeDelta;

    JSON Setting:
        Effect Multiplier

    Input:
        magnitude:
            Spell magnitude / actor value change

        effect_multiplier:
            JSON magic effect multiplier

    Returns:
        Pre-softcap magic poise damage
    """

    return magnitude * effect_multiplier


def apply_magic_softcap(damage, softcap=50.0, reduction=0.5):
    """
    Matches:

        if (poiseDamage > 50.0f)
        {
            float excessDamage = poiseDamage - 50.0f;
            poiseDamage = 50.0f + (excessDamage * 0.5f);
        }


    Prevents extremely large magic effects
    from instantly destroying poise.

    INI:
        No current setting
    """

    if damage > softcap:

        excess = damage - softcap

        damage = softcap + (excess * reduction)

    return damage


def calculate_effective_magic_resistance(
    magic_resist, fire_resist, frost_resist, shock_resist
):
    """
    Matches:

        float effectiveResist =
            (magicResist * 0.65f) +
            (elementalAverage * 0.35f);


    Magic resistance:
        65%

    Elemental resistance:
        35%
    """

    elemental_average = (fire_resist + frost_resist + shock_resist) / 3.0

    effective_resist = magic_resist * 0.65 + elemental_average * 0.35

    return max(-100, min(100, effective_resist))


def apply_magic_resistance(
    damage, magic_resist, fire_resist, frost_resist, shock_resist, resistance_mult=1.0
):
    """
    Matches ApplyMagicPoiseResistance()

    Positive Resistance:
        Uses diminishing return curve

    Negative Resistance:
        Increases incoming poise damage


    INI:

        [Magic]

        ResistanceMult = 1.0
    """

    effective_resist = calculate_effective_magic_resistance(
        magic_resist, fire_resist, frost_resist, shock_resist
    )

    if effective_resist >= 0:

        normalized = effective_resist / 100.0

        curve_strength = 1.5 * resistance_mult

        r1 = normalized * curve_strength

        reduction = r1 / (1 + r1)

        multiplier = 1 - reduction

    else:

        weakness = abs(effective_resist) / 100.0

        multiplier = 1 + weakness

    return damage * multiplier
