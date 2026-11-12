# Attorney Online

![CI build](https://github.com/AttorneyOnline/AO2-Client/actions/workflows/build.yml/badge.svg?event=push) ![license](https://img.shields.io/github/license/AttorneyOnline/AO2-Client?color=blue) ![contributors](https://img.shields.io/github/contributors/AttorneyOnline/AO2-Client)<br>

[Attorney Online](https://aceattorneyonline.com) is an online version of the world-renowned courtroom drama simulator that allows you to create and play out cases in an off-the-cuff format.

**[Refer to the docs](https://github.com/AttorneyOnline/docs/blob/master/docs/index.md) for more information.**

Linux users will need to install the system dependencies related to Qt. These are the commands to run on a terminal for some distributions:
* Ubuntu 24.04 LTS:
```
$ sudo apt-get install qt6-base-dev libqt6websockets6-dev qt6-image-formats-plugins libqt6svg6-dev qt6-multimedia-dev
```
* Arch:
```
$ sudo pacman -Syu qt6-base qt6-websockets qt6-imageformats qt6-svg qt6-multimedia
```
* Fedora:
```
$ sudo dnf install qt6-qtbase qt6-qtwebsockets qt6-qtimageformats qt6-qtsvg qt6-qtmultimedia
```

## Credits

This is a open-source remake of Attorney Online written by OmniTroid. The original Attorney Online client was written by FanatSors in Delphi.

The logo (`logo_kfo.png` and `logo_kfo.ico`) was designed by the Killing Fever Online community.

The server browser interface was designed by Ellym (Konoe). The characters depicted on it belong to the Ace Attorney series by Capcom.

### Project

The project is dual-licensed; you are free to copy, modify and distribute AO2 under the GPLv3 or the MIT license.

Copyright (c) 2016-2018 David "OmniTroid" Skoland

Modifications copyright (c) 2017-2018 oldmud0

Case Café additions copyright (c) 2018 Cerapter

Killing Fever Online additions copyright (c) 2019 Crystalwarrior

### Qt

This project uses Qt 6, which is licensed under the [GNU Lesser General Public License](https://www.gnu.org/licenses/lgpl-3.0.txt) with [certain licensing restrictions and exceptions](https://www.qt.io/qt-licensing-terms/). To comply with licensing requirements for static linking, object code is available if you would like to relink with an alternative version of Qt, and the source code for Qt may be found at https://github.com/qt/qtbase, http://code.qt.io/cgit/, or at https://qt.io.

Copyright (c) 2016 The Qt Company Ltd.

### BASS

This project depends on the BASS shared library. Get it here: http://www.un4seen.com/

Copyright (c) 1999-2016 Un4seen Developments Ltd. All rights reserved.

### Noto Emoji

This project bundles the Noto Color Emoji font (`resource/fonts/NotoColorEmoji.ttf`), licensed under the [SIL Open Font License 1.1](https://scripts.sil.org/OFL). The source is available at https://github.com/googlefonts/noto-emoji.

Copyright (c) 2013-2017 Google Inc. and Arjen Nienhuis
