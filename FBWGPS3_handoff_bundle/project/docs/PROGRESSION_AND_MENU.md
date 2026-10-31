# Progression, saves and menu focus

The following modules are platform-independent. They implement the rules and input behaviour needed by the future menu/game loop; they do not create the rendered menu scenes or call the storage API of the PS3 SDK.

## Load the Forest Temple catalogue

Use `fw::progression::loadForestCatalog(gameDir, catalog, &error)`. It reads `data/forest/temple.json`, loads all 32 Tiled maps and counts ordinary diamonds in each `Chars` layer. Silver diamonds are excluded from the normal-diamond target and are tracked separately during a run.

The tree links in `temple.json` refer to each level's `_id` menu node, not its public `id`. `CatalogLevel` retains both values. Do not use the public level number to walk `ForestCatalog::edges`.

## Record a run

`calculateResult(rules, run, singlePlayer, output, &error)` applies the original rules:

- normal/speed: one base star, one for collecting every ordinary diamond, and one for meeting the time limit;
- puzzle: three stars only when at least one silver diamond is collected;
- dark: three stars within the time limit, otherwise two;
- single-player mode uses `mobileTime` when that value is positive.

Call `markPlayed(progress, levelId)` when the level is entered. Call `mergeRun(progress, result)` when a result is available. This separation matches the source model's independent `setPlayed()` and `update()` paths. Only positive-star runs can become a best result; comparison order is stars, ordinary diamonds, then faster time.

`calculateMenuProgress(catalog, progress, ownedKeys)` returns the tree node states, best stars, total stars, game completion, soft-lock state, and `is_new` flags. In this Forest Temple data set there are 32 levels, 31 edges, six speed levels, nine puzzle levels and 320 ordinary diamonds.

## Save file

`saveProgressFile(path, gameId, progress, &error)` writes a versioned JSON document. `loadProgressFile()` checks the version/game identity, duplicate level IDs, boolean `played` flags, and all best-run statistics before replacing the caller's `Progress` object.

The current format is:

```json
{
  "version": 1,
  "game": "forest",
  "levels": [
    {
      "id": 1,
      "played": true,
      "best": {
        "stars": 3,
        "time": 28.4,
        "diamonds": 7,
        "silverDiamond": 0
      }
    }
  ]
}
```

The test suite writes only into the build directory and removes the test save after checking it. The runtime save path is not yet connected. The intended PS3 path is under `/dev_hdd0/game/<TITLEID>/USRDIR/`; Windows desktop prototype path selection remains an integration task.

## Menu focus navigation

Build a list of `MenuItemLayout` entries from the current visible menu buttons and pass it to `MenuFocus::setItems()`. Feed `Input::menuEvent()` into `MenuFocus::update()` each frame. The helper finds the nearest selectable item in the requested direction, skips hidden/disabled entries, and returns `Activate` or `Back` actions for Cross/Circle. `focusRect()` gives the renderer the rectangle for a highlight outline.

The helper intentionally does not own button callbacks, transition logic, layout generation or focus visuals. Those remain responsibilities of the future menu scene layer.
