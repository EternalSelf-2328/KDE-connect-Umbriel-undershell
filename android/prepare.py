#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prepare an explicit, minimum-permission variant; no runtime feature download."""
from pathlib import Path
import shutil
import sys
import xml.etree.ElementTree as ET

source=Path(__file__).resolve().parent
variant=sys.argv[1]
if variant not in {"desktop","phone-control"}:
    raise SystemExit("Variant must be desktop or phone-control")
out=Path(sys.argv[2]).resolve()
expected=(source/"../build/android"/variant).resolve()
if out != expected:
    raise SystemExit("Refusing to clean outside the variant build directory")
for name in ["classes","dex","generated","res"]:
    path=out/name
    if path.exists(): shutil.rmtree(path)
    path.mkdir(parents=True)
shutil.copytree(source/"res",out/"res",dirs_exist_ok=True)
android="{http://schemas.android.com/apk/res/android}"
ET.register_namespace("android",android[1:-1])
manifest=ET.parse(source/"AndroidManifest.xml")
if variant=="phone-control":
    manifest.getroot().find("application").append(ET.parse(source/"variants/phone-control/service.xml").getroot())
    manifest.getroot().set(android+"versionName","0.4.1-phone-control-experimental")
else:
    (out/"res/xml/accessibility.xml").unlink()
manifest.write(out/"AndroidManifest.xml",encoding="utf-8",xml_declaration=True)
package=out/"generated/org/eternalself/umbrielremote"
package.mkdir(parents=True,exist_ok=True)
(package/"BuildFeatures.java").write_text("package org.eternalself.umbrielremote;\nfinal class BuildFeatures { static final boolean PHONE_CONTROL = "+str(variant=="phone-control").lower()+"; }\n")
