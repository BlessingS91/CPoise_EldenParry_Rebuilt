import customtkinter as ctk

from MagicCalcs import (
    calculate_magic_damage,
    apply_magic_softcap,
    apply_magic_resistance,
)

from WeaponCalcs import calculate_weapon_poise
from UnarmedCalcs import calculate_unarmed_poise


class PoiseApp(ctk.CTk):

    def __init__(self):
        super().__init__()

        self.title("Chocolate Poise Balance Tool")
        self.geometry("900x700")

        self.create_navigation()

        self.content_frame = ctk.CTkFrame(self)
        self.content_frame.pack(
            side="right", padx=20, pady=20, fill="both", expand=True
        )

        self.show_home()

    def create_navigation(self):

        nav = ctk.CTkFrame(self)
        nav.pack(side="left", padx=10, pady=10, fill="y")

        ctk.CTkLabel(nav, text="Calculators", font=("Arial", 18)).pack(pady=20)

        ctk.CTkButton(nav, text="Magic", command=self.show_magic).pack(pady=5, padx=10)

        ctk.CTkButton(nav, text="Damage", command=self.show_damage).pack(
            pady=5, padx=10
        )

        ctk.CTkButton(nav, text="Defence", command=self.show_defence).pack(
            pady=5, padx=10
        )

    def clear_content(self):

        for widget in self.content_frame.winfo_children():
            widget.destroy()

    def create_input(self, label, value):

        frame = ctk.CTkFrame(self.content_frame)

        frame.pack(fill="x", pady=3)

        ctk.CTkLabel(frame, text=label, width=200).pack(side="left", padx=10)

        entry = ctk.CTkEntry(frame)

        entry.insert(0, value)

        entry.pack(side="right", padx=10)

        return entry

    def show_home(self):

        self.clear_content()

        ctk.CTkLabel(
            self.content_frame, text="Select a Calculator", font=("Arial", 24)
        ).pack(pady=50)

    def show_magic(self):

        self.clear_content()

        ctk.CTkLabel(
            self.content_frame, text="Magic Calculator", font=("Arial", 24)
        ).pack(pady=20)

        #
        # SPELL
        #

        ctk.CTkLabel(self.content_frame, text="Spell", font=("Arial", 18)).pack(pady=5)

        self.magic_magnitude = self.create_input("Spell Magnitude", "100")

        #
        # RESISTANCE
        #

        ctk.CTkLabel(self.content_frame, text="Resistance", font=("Arial", 18)).pack(
            pady=5
        )

        self.magic_resist = self.create_input("Magic Resistance", "0")

        self.fire_resist = self.create_input("Fire Resistance", "0")

        self.frost_resist = self.create_input("Frost Resistance", "0")

        self.shock_resist = self.create_input("Shock Resistance", "0")

        #
        # JSON SETTINGS
        #

        ctk.CTkLabel(self.content_frame, text="JSON Settings", font=("Arial", 18)).pack(
            pady=5
        )

        self.magic_effect_multiplier = self.create_input("Effect Multiplier", "1.0")

        #
        # INI
        #

        ctk.CTkLabel(self.content_frame, text="INI Settings", font=("Arial", 18)).pack(
            pady=5
        )

        self.magic_resistance_mult = self.create_input("ResistanceMult", "1.0")

        self.magic_result = ctk.CTkLabel(self.content_frame, text="Result: 0")

        self.magic_result.pack(pady=20)

        self.magic_button = ctk.CTkButton(
            self.content_frame, text="Calculate", command=self.calculate_magic
        )

        self.magic_button.pack()

    def calculate_magic(self):

        try:

            magnitude = float(self.magic_magnitude.get())

            magic_resist = float(self.magic_resist.get())

            fire = float(self.fire_resist.get())

            frost = float(self.frost_resist.get())

            shock = float(self.shock_resist.get())

            effect_multiplier = float(self.magic_effect_multiplier.get())

            resistance_mult = float(self.magic_resistance_mult.get())

            # C++
            # poiseDamage = effectMultiplier * magnitude

            damage = calculate_magic_damage(magnitude, effect_multiplier)

            # C++
            # high-end magic scaling

            damage_after_softcap = apply_magic_softcap(damage)

            # C++
            # ApplyMagicPoiseResistance

            final = apply_magic_resistance(
                damage_after_softcap, magic_resist, fire, frost, shock, resistance_mult
            )

            self.magic_result.configure(text=f"""
Spell Magnitude:
{magnitude:.2f}

JSON Effect Multiplier:
{effect_multiplier:.2f}

Before Softcap:
{damage:.2f}

After Softcap:
{damage_after_softcap:.2f}

Final Poise Damage:
{final:.2f}
""")

        except Exception as e:

            self.magic_result.configure(text=f"Error: {e}")

    def show_damage(self):

        self.clear_content()

        ctk.CTkLabel(
            self.content_frame, text="Weapon Poise Calculator", font=("Arial", 24)
        ).pack(pady=20)

        #
        # WEAPON DATA
        #

        ctk.CTkLabel(self.content_frame, text="Weapon Data", font=("Arial", 18)).pack(
            pady=5
        )

        self.weapon_damage = self.create_input("Attack Damage", "20")

        self.weapon_weight = self.create_input("Weapon Weight", "10")

        #
        # JSON SETTINGS
        #

        ctk.CTkLabel(self.content_frame, text="JSON Settings", font=("Arial", 18)).pack(
            pady=5
        )

        self.weapon_mult = self.create_input("Weapon Multiplier", "1.0")

        #
        # INI SETTINGS
        #

        ctk.CTkLabel(self.content_frame, text="INI Settings", font=("Arial", 18)).pack(
            pady=5
        )

        self.weapon_min_damage = self.create_input("Min Damage", "4.0")

        self.weapon_max_damage = self.create_input("Max Damage", "27.0")

        self.weapon_max_multiplier = self.create_input("Max Damage Multiplier", "5.0")

        self.weapon_weight_contribution = self.create_input(
            "Weight Contribution", "0.005"
        )

        self.weapon_curve_strength = self.create_input("Curve Strength", "2.5")

        self.weapon_min_poise = self.create_input("Minimum Poise", "25.0")

        self.weapon_max_poise = self.create_input("Maximum Poise", "75.0")

        self.weapon_result = ctk.CTkLabel(self.content_frame, text="Result: 0")

        self.weapon_result.pack(pady=20)

        ctk.CTkButton(
            self.content_frame, text="Calculate", command=self.calculate_weapon
        ).pack()

    def calculate_weapon(self):

        try:

            result = calculate_weapon_poise(
                weapon_damage=float(self.weapon_damage.get()),
                weapon_weight=float(self.weapon_weight.get()),
                weapon_mult=float(self.weapon_mult.get()),
                min_damage=float(self.weapon_min_damage.get()),
                max_damage=float(self.weapon_max_damage.get()),
                max_damage_multiplier=float(self.weapon_max_multiplier.get()),
                weight_contribution=float(self.weapon_weight_contribution.get()),
                curve_strength=float(self.weapon_curve_strength.get()),
                min_poise=float(self.weapon_min_poise.get()),
                max_poise=float(self.weapon_max_poise.get()),
            )

            self.weapon_result.configure(text=f"""
    Weapon Damage:
    {self.weapon_damage.get()}

    Weight:
    {self.weapon_weight.get()}

    Final Weapon Poise:
    {result:.2f}
    """)

        except Exception as e:

            self.weapon_result.configure(text=f"Error: {e}")

    def show_defence(self):

        self.clear_content()

        ctk.CTkLabel(
            self.content_frame, text="Defence Calculator", font=("Arial", 24)
        ).pack(pady=50)


if __name__ == "__main__":

    app = PoiseApp()

    app.mainloop()
