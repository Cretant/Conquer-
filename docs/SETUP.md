# Setting up Conquer on your PC

You only need to do this once. It takes about an hour, most of it waiting for downloads.

## 1. Install the tools

1. **Epic Games Launcher:** download it from https://store.epicgames.com and sign in.
2. **Unreal Engine 5:** in the launcher, go to **Unreal Engine > Library**, click the **+** next to *Engine Versions*, pick **5.6** (or the newest 5.x) and click **Install**. It's a large download (about 40 to 60 GB).
3. **Visual Studio 2022 Community** (free): download it from https://visualstudio.microsoft.com. In the installer, tick the **Game development with C++** workload. On the right, make sure **Unreal Engine installer** and a **Windows 11 SDK** are ticked too. Unreal needs this to compile the game's code.

## 2. Open the project

1. Put the `Conquer` folder somewhere simple, like `Documents\Conquer`.
2. If you installed a version other than 5.6, right-click `Conquer.uproject` and choose **Switch Unreal Engine version**, then pick yours.
3. Double-click `Conquer.uproject`. Unreal will say the **Conquer module is missing or built with a different engine version** and ask to rebuild it. Click **Yes**. The first build takes a few minutes.
   - If the build fails, copy the error message and send it to Claude. That's the fastest way to fix it.

## 3. Add placeholder characters (one time)

The prototype borrows Epic's free mannequins until we have real models of Wren and the soldiers.

1. In the editor, open the **Content Browser** (Ctrl+Space).
2. Click **+ Add** > **Add Feature or Content Pack**.
3. Choose **Third Person**, then **Add to Project**.

## 4. Make the test arena (one time)

1. **File > New Level > Basic**, then **Create**.
2. **File > Save Current Level As**, create a folder called `Maps`, and save the level as `Arena`.
3. If the floor looks small, click it, and in the **Details** panel set **Scale** to `20, 20, 1`.

## 5. Play

Press **Play** (or Alt+P). Wren spawns, then three soldiers come at you after a second and a half. Kill them and the gate captain arrives. If you die, the arena restarts.

The controls are in the [README](../README.md) and on screen in the top left.

## If something looks wrong

- **The build fails:** send Claude the error text from the window, or from **Window > Output Log** in the editor.
- **Characters are invisible, or T-posing:** the placeholder mannequins weren't found. Check step 3 was done. If it was, open **Window > Output Log**, search for `placeholder body`, and send Claude what it says.
- **Nothing spawns:** make sure you pressed Play in the `Arena` level and not another map.

## What to look for when you play

This is a feel test, not a looking-good test. Things worth telling Claude about:

- Do attacks feel too fast or too slow to react to?
- Is blocking and parrying readable from the guard indicators?
- Does Rage feel powerful enough?
- Is the gate captain too easy or too hard?
