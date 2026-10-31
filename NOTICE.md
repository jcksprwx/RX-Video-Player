# Third-Party Licenses & Software Notices

RX Media Player utilizes and dynamically links with the following open-source software libraries:

## 1. libmpv & FFmpeg
- **License:** GNU Lesser General Public License version 2.1 or later (LGPL v2.1+)
- **Purpose:** Multimedia demuxing, video and audio decoding, and Direct3D 11 hardware presentation.
- **Source & Project:** [https://mpv.io](https://mpv.io) / [https://ffmpeg.org](https://ffmpeg.org)
- **Notice:** libmpv is dynamically linked as `libmpv-2.dll` and `mpv-2.dll`. Under the LGPL v2.1, users are free to modify or replace the dynamic library without invalidating the application license.

## 2. SQLite Database Engine
- **License:** Public Domain ([https://sqlite.org/copyright.html](https://sqlite.org/copyright.html))
- **Purpose:** Persistent media library, playback history, resume position memory, and bookmarks.
- **Source:** SQLite 3.49 amalgamation (`src/library/sqlite/sqlite3.c`).

## 3. Microsoft Windows SDK
- **Copyright:** Copyright (c) Microsoft Corporation. All rights reserved.
- **Components:** Direct3D 11, DXVA2, D3D11VA, Windows Audio Session API (WASAPI), Desktop Window Manager (DWM), Shell.
