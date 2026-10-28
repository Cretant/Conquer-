# Conquer

A third-person medieval revenge game in Unreal Engine 5. A silent peasant woman, driven by rage, cuts her way through a corrupt king's army, church and court to reach the king himself.

- **Design doc:** https://claude.ai/artifact/QASkT9uDv8aSPArHhVro3b
- **First-time setup on your PC:** [docs/SETUP.md](docs/SETUP.md)

## What's in this first version

This is the **combat prototype** for the opening fight. It uses gray-box art: placeholder mannequins, and debug lines in place of attack animations.

| System | Where | What it does |
| --- | --- | --- |
| Duel combat | `CombatComponent` | Left / top / right guards, light and heavy attacks with windup, active and recovery phases, block, parry, guard break, feint, dodge, stagger |
| Rage | `RageComponent` | Fills from fighting and taking hits. Unleashed, she is faster, hits harder, can't be flinched and her heavies are unblockable. Fills on its own near death, once per life |
| Health and stamina | `AttributesComponent` | Every attack, block and dodge costs stamina |
| Wren | `WrenCharacter` | Hack-and-slash out of lock-on; lock on to duel with directional guards; executions on staggered, wounded enemies |
| Enemies | `EnemySoldier`, `EnemyAIController` | Soldiers that read your attacks and guard against them, and the gate captain mini-boss who swings only from the top and right |
| Arena | `ConquerGameMode` | Spawns three soldiers, then the captain. Limits how many enemies attack at once |
| HUD | `ConquerHUD` | Health, stamina and Rage bars, plus guard indicators over enemies (red = incoming attack, orange = unblockable) |

All the combat numbers (timings, damage, stamina costs, AI reaction times) are editable properties, so they can be tuned in the editor without touching code.

## Marrowford, the first level

Any key on the title screen starts a new game in Marrowford (1 and 2 open the Arena and Keep Hall test fights). It is a greybox blockout built by `Tools/build_marrowford.py` and run by `MarrowfordGameMode`:

- **Opening:** a short skippable cutscene at the farm (any key skips). Soldiers come up the lane, a cut to black, the farm burns, and Wren gets up and takes the pitchfork.
- **The lane:** three soldiers, one at a time.
- **The burning square:** four soldiers, then a shield guard, two archers and one more soldier. A side path leads to the smithy's whetstone, which makes her hits stronger.
- **The mill bridge:** a checkpoint, then waves from the far bank: soldiers, two archers and a shield guard. The way back burns behind her.
- **The village gate:** a checkpoint, then the gate captain. Rage stays locked until he has her at low health. She takes his sword when he falls, the portcullis rises, and the level ends past the gate.
- **Checkpoints:** dying brings her back at the last checkpoint with that fight reset.

Headless check: `-ConquerMarrowfordTest` walks through every fight, a death and respawn, and the ending, logging each step. Previews are in `Art/Previews/Marrowford_*.png`.

## Combat

- **Parry:** guard on the blow's side no more than 0.15 s before it lands (tap F / LB, or flick the guard over while locked on). The attacker staggers, Wren gains Rage, and the hit freezes for a moment with a gold flash. Guarding earlier just blocks, at a cost in stamina. Arrows are deflected the same way. Unblockable (orange) attacks can't be parried.
- **Guard break:** E / X throws a guarding enemy open for a moment. Enemies' guard breaks flash blue: press your own after theirs has started to counter it, which leaves them open instead.
- **Dodge:** short i-frames at the start, and a stamina cost. It cuts short a light's recovery, or a heavy before 60% of its windup (after that the heavy is committed). Light straight out of a dodge is a lunging dodge attack.
- **Shield guards** block everything from the front, so guard break them or get round to their side (they turn slowly). **Archers** keep their distance and loose burning arrows after a glowing draw.
- **Garrick** grabs for a raised guard (counter it), and a parried swing leaves him open for 1.4 s.
- Each mechanic gets a one-line hint the first time it matters in a level.
- Headless check: `-ConquerCombatTest` in the Arena runs all of this and logs PASS or FAIL for each.

## Controls

| Action | Keyboard and mouse | Controller |
| --- | --- | --- |
| Move | WASD | Left stick |
| Camera | Mouse | Right stick |
| Lock on (toggle) | Tab or middle mouse | LT |
| Choose guard side (while locked on; a flick to the blow's side as it lands parries) | Move the mouse left, up or right | Right stick |
| Guard: hold to block (out of lock-on it turns to the next blow), tap as a blow lands to parry | F | LB |
| Light attack (straight out of a dodge: a lunging dodge attack) | Left mouse | RB |
| Heavy attack | Right mouse | RT |
| Guard break (pressed as an enemy's comes in, it counters it) | E | X |
| Feint a heavy | Q | B |
| Dodge (cancels a light's follow-through, or a heavy before it commits) | Space | A |
| Unleash Rage | R | Y |
| Execute | Heavy on a staggered enemy below 30% health | RT |
