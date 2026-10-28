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

## Sword And Shield Pack, from Mixamo (Adobe)

- **What we use:** sword slashes and attacks, the kick (guard break), block, impacts and two deaths,
  retargeted onto the UE5 mannequin by `Tools/build_sword.py` and onto the Meshy characters by
  `Tools/build_characters.py`.
- **License:** Mixamo's terms: characters and animations are royalty free for personal, commercial and
  non-profit projects, including games. They must not be redistributed or sold as standalone files (raw or
  as an asset pack). **This repo is public, so none of it is in git**: the raw FBX files and every
  Mixamo-derived asset live in gitignored `Mixamo/` folders on the PC that imported the pack. The game
  prefers those montages when they exist (`LocalAnimOverrides` in `Config/DefaultGame.ini`) and otherwise
  plays the public CC0 set from Quaternius.
- **Source:** mixamo.com, "Sword And Shield Pack" (FBX Binary, 30 fps), downloaded 2026-10-27 with
  Ryan's Adobe account.
- **To rebuild:** put the zip in `Art/Mixamo/`, extract it there, run `Tools/merge_mixamo.py` in Blender,
  then `Tools/build_sword.py` and `Tools/build_anims.py`.

## Meshy models (Wren, the king's soldier, Sir Garrick Vane, sword, shield)

- **What we have:** generated on meshy.ai on 2026-10-27 with Ryan's Premium plan, visibility **Private**:
  Wren and Garrick by image-to-3D (Meshy 7.1) from our own concept art, the soldier, sword and shield by
  text-to-3D (Meshy 6, textured with Meshy 7). Characters remeshed to ~30K triangles and auto-rigged on the
  UE5 Mannequin skeleton template; props remeshed to ~10K. Imported by `Tools/import_meshy.py` into
  `Content/Characters/Conquer/` and `Content/Props/`; previews in `Art/Previews/`. The sword was regenerated
  as a single blade on 2026-10-27 (the first try fused two crossed swords). Credits used: 140 of 5,100.
- **Pipeline:** `Tools/fix_meshy_rigs.py` (Meshy's FBX puts a x100 scale on the armature root, which breaks
  retargeting), `Tools/prep_props.py` (sword and shield sized and pivoted for the hands),
  `Tools/import_meshy.py` (with a PBR material per model), then `Tools/build_characters.py`.
- **License:** Meshy Terms of Use, section 3.2: on a paid plan the customer owns their outputs (to the extent
  the law allows), and commercial use is fine; no attribution required. Restrictions: don't strip AI
  identifiers from outputs, and don't use them to train competing AI models. AI output may not be
  protectable as IP. (Free-plan outputs would instead be Meshy-owned under CC BY 4.0, credit required.)
  Meshy may train on non-Enterprise users' content unless you opt out by emailing support@meshy.ai.
- **Raw exports** (`Art/Meshy/`) stay out of git.

## Third Person mannequin pack, by Epic Games

- **What we use:** `Content/Characters` (Manny and Quinn, their animations) and three of its deaths.
- **License:** Unreal Engine content, usable in projects made with Unreal Engine under the Unreal Engine EULA.
- **Source:** the UE 5.8 Third Person template (`Templates/TemplateResources/High/Characters`).

## Our own

- `Art/Weapons/SM_Sword.fbx`: the placeholder longsword, made by `Tools/make_sword.py`.
- `Art/UI`: the logo and title screen art.
