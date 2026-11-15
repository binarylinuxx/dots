# openSUSE packaging (community, unmaintained)

These specs are **not maintained** by blxshell's author and are up for adoption
by the community.

**Do not use `home:binarylinuxx:blxshell` on OBS.** The account behind it was
compromised and is no longer under the author's control, so nothing published
there can be trusted, including anything published after November 2026.

## Taking it over

The specs here are updated for Vellum (blxshell 4, on vw) but have never been
built on OBS:

- `vw/` builds vw and vw-devel, pinned to a vw commit and a zwwm commit (`_fetch`).
- `blxshell/` builds the shell against vw-devel. Set `_commit` in
  `blxshell.spec` and `_fetch` to a commit of `main` first (`PIN_AFTER_PUSH`).
- The `blxshell-*` metapackages and fonts are as before.
- A few BuildRequires names are unchecked: `quickjs-devel`, `stb-devel`,
  `nlohmann_json-devel`, `shaderc`.

`publish-to-obs.sh` publishes to an OBS project; set `OBS_PROJECT` to your own.
Open an issue or a pull request on github.com/binarylinuxx/dots if you maintain
these, and the docs will point to your repository.
