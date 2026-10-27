# Third-party art

Everything here may be used in a commercial game. Raw downloads stay out of git (see `.gitignore`);
the `Tools/` scripts rebuild the game assets from them.

## Universal Animation Library 1 and 2, by Quaternius

- **What we use:** sword attacks, block, dash, hit reactions, a death and a few spares, retargeted onto the
  UE5 mannequin by `Tools/build_sword.py` (into `Content/Conquer/Animations/Sword`).
- **License:** CC0 1.0 Universal (public domain dedication),
  https://creativecommons.org/publicdomain/zero/1.0/. Free for personal, educational and commercial
  projects; no attribution required (credit given anyway: thank you, Quaternius).
- **Source:** the free Standard versions, downloaded 2026-10-27 with no account:
  - https://quaternius.itch.io/universal-animation-library ("Universal Animation Library[Standard].zip")
  - https://quaternius.itch.io/universal-animation-library-2 ("Universal Animation Library 2[Standard].zip")
- **To rebuild:** put both zips in `Art/Quaternius/` as `UAL1_Standard.zip` and `UAL2_Standard.zip`, extract
  them there, then run `Tools/build_sword.py` and `Tools/build_anims.py` (commands in each script).

## Third Person mannequin pack, by Epic Games

- **What we use:** `Content/Characters` (Manny and Quinn, their animations) and three of its deaths.
- **License:** Unreal Engine content, usable in projects made with Unreal Engine under the Unreal Engine EULA.
- **Source:** the UE 5.8 Third Person template (`Templates/TemplateResources/High/Characters`).

## Our own

- `Art/Weapons/SM_Sword.fbx`: the placeholder longsword, made by `Tools/make_sword.py`.
- `Art/UI`: the logo and title screen art.
