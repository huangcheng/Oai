# Changelog

All notable changes to Seelie are recorded here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

Initial release.

### Added
- `SEELIE_INCLUDE_NSFW` CMake option (default `OFF`) gates bundling of packs
  whose `manifest.json` `tags` array contains `"nsfw"`. Default builds ship a
  store-safe pack lineup; opt in with `-DSEELIE_INCLUDE_NSFW=ON`.
  Classification is per-pack — to clear a single pack, remove the `nsfw` tag
  from its manifest. Runtime is unaffected; users can still install any
  `.spk` into their user pack dir.
