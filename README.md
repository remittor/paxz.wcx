# PAXZ - packer plugin for Total Commander

[![Build status](https://ci.appveyor.com/api/projects/status/n5xro5gaih99j3op/branch/master?svg=true)](https://ci.appveyor.com/project/remittor/paxz-wcx/branch/master)
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/ed11cd2ac20f45f9b2c93ec7de2e34e4)](https://www.codacy.com/manual/remittor/paxz.wcx?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=remittor/paxz.wcx&amp;utm_campaign=Badge_Grade)
[![License: MIT](https://img.shields.io/badge/License-MIT-informational.svg)](https://github.com/remittor/paxz.wcx/blob/master/License.txt)
![Platforms](https://img.shields.io/badge/platform-windows-9cf)
[![Github All Releases](https://img.shields.io/github/downloads/remittor/paxz.wcx/total.svg)](https://www.github.com/remittor/paxz.wcx/releases/latest)
[![GitHub release (latest by date including pre-releases)](https://img.shields.io/github/v/release/remittor/paxz.wcx?include_prereleases)](https://www.github.com/remittor/paxz.wcx/releases)

Supported formats for decompression: `pax`, `lz4`, `pax.lz4`, `zst`, `paxz`

Supported formats for compression: `pax`, `pax.lz4`, `paxz`

File format `paxz` equivalent for `pax.zst`.

All files created by this plugin can be unpacked with free cross-platform utilities: `lz4`/`zstd`, `pax`/`gnutar`.

## Tasks list
- [ ] Zstandard and custom format PAXZ.
- [ ] Inserting and deleting files.
- [ ] Cipher ChaCha20.
