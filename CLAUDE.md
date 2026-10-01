# CLAUDE.md

Stable Audio 3 inference on eacp. Code style, build conventions and git rules
are eacp's: see `CLAUDE.md` in the eacp checkout (Allman, 85 columns,
clang-format every edited file, `-G Ninja -DEACP_UNITY_BUILD=OFF`, never
commit or push without being asked).

eacp comes through CPM (`CMake/Findeacp.cmake`) from the
`jp/stable-audio-infrence` branch of `jamierpond/eacp`, until that library
work merges into `eyalamirmusic/eacp`. Library changes go in eacp, not here.
To build against a local eacp checkout:

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release -DEACP_UNITY_BUILD=OFF \
      -DCPM_eacp_SOURCE=$HOME/eacp
```

`$HOME`, not `~`: CMake does not expand `~`.

`ctest --test-dir build -R '^SA3'`; golden cases skip without cached
checkpoints (`Tests/SkipWithoutCheckpoint.h`), `EACP_REQUIRE_CHECKPOINTS=1`
makes that a failure.
