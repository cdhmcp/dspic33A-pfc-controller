#!/usr/bin/env python3
"""Merge peripheral modules from the LVPFC .mc3 into the pfc-controller .mc3.

Reads both files, merges the peripheral configuration (packages, classes,
tokenMap JSON), and writes the result back to the pfc-controller .mc3.
A backup (.mc3.bak) is created before overwriting.
"""

import json
import shutil
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

SOURCE = Path(r"c:\repo\customer\collins\dspic33A-LVPFC-MCC\dspic33A-LVPFC-MCC.X\dspic33A-LVPFC-MCC.mc3")
TARGET = Path(r"c:\repo\customer\collins\dspic33A-pfc-controller\dspic33A-pfc-controller.X\dspic33A-pfc-controller.mc3")


def parse_mc3(path):
    """Parse an .mc3 file returning its components."""
    tree = ET.parse(path)
    root = tree.getroot()

    # Root attributes
    attribs = dict(root.attrib)

    # Packages
    packages = []
    for pkg in root.find("usedPackages"):
        packages.append({
            "packageName": pkg.get("packageName"),
            "version": pkg.get("version"),
            "contentType": pkg.get("contentType"),
        })

    # Classes (TreeMap entries)
    classes = []
    for entry in root.find("usedClasses"):
        strings = entry.findall("string")
        classes.append((strings[0].text, strings[1].text))

    # Library version
    lib_elem = root.find("usedLibraries").find("ILibraryFile")
    library_version = lib_elem.get("version")

    # TokenMap JSON
    token_entry = root.find("tokenMap").find("entry")
    value_elem = token_entry.find("value")
    json_data = json.loads(value_elem.text)

    return attribs, packages, classes, library_version, json_data


def merge_packages(source_pkgs, target_pkgs):
    """Return sorted union of packages (by packageName)."""
    by_name = {p["packageName"]: p for p in target_pkgs}
    for p in source_pkgs:
        if p["packageName"] not in by_name:
            by_name[p["packageName"]] = p
    return sorted(by_name.values(), key=lambda p: p["packageName"])


def merge_classes(source_classes, target_classes):
    """Return sorted union of class entries."""
    by_key = {k: v for k, v in target_classes}
    for k, v in source_classes:
        if k not in by_key:
            by_key[k] = v
    return sorted(by_key.items(), key=lambda x: x[0])


def merge_json(source_json, target_json):
    """Merge tokenMap JSON: modules, userAddedModules, content."""
    merged = dict(target_json)

    # userAddedModules: union preserving target order, append source additions
    target_modules_set = set(merged["userAddedModules"])
    added = list(merged["userAddedModules"])
    for m in source_json["userAddedModules"]:
        if m not in target_modules_set:
            added.append(m)
            target_modules_set.add(m)
    merged["userAddedModules"] = added

    # modules: for shared modules take source version (has payloads); add source-only modules
    merged_modules = dict(merged["modules"])
    for mod_id, mod_def in source_json["modules"].items():
        merged_modules[mod_id] = mod_def
    merged["modules"] = merged_modules

    # content: union
    merged_content = dict(merged.get("content", {}))
    for k, v in source_json.get("content", {}).items():
        if k not in merged_content:
            merged_content[k] = v
    merged["content"] = merged_content

    return merged


def build_xml(attribs, packages, classes, library_version, json_data):
    """Construct the .mc3 XML string with proper &quot; encoding."""
    lines = []

    # <config> opening tag - use target attributes
    attr_parts = []
    attr_order = ["configVersion", "device", "deviceLibraryClass", "coreVersion", "spaHostVersion"]
    for key in attr_order:
        if key in attribs:
            attr_parts.append(f'{key}="{attribs[key]}"')
    lines.append(f'<config {" ".join(attr_parts)}>')

    # <usedPackages>
    lines.append('   <usedPackages class="java.util.ArrayList">')
    for pkg in packages:
        lines.append(f'      <package packageName="{pkg["packageName"]}" version="{pkg["version"]}" contentType="{pkg["contentType"]}">')
        lines.append('         <modules class="java.util.ArrayList"/>')
        lines.append('      </package>')
    lines.append('   </usedPackages>')

    # <usedClasses>
    lines.append('   <usedClasses class="java.util.TreeMap">')
    for key, value in classes:
        lines.append('      <entry>')
        lines.append(f'         <string>{key}</string>')
        lines.append(f'         <string>{value}</string>')
        lines.append('      </entry>')
    lines.append('   </usedClasses>')

    # <usedLibraries>
    lines.append('   <usedLibraries class="java.util.ArrayList">')
    lines.append(f'      <ILibraryFile class="com.microchip.mcc.core.library.BaseLibraryFile" libraryClass="com.microchip.mcc.melody.Library" version="{library_version}"/>')
    lines.append('   </usedLibraries>')

    # <tokenMap>
    json_str = json.dumps(json_data, separators=(',', ':'))
    encoded_value = json_str.replace('"', '&quot;')
    lines.append('   <tokenMap class="java.util.TreeMap">')
    lines.append('      <entry>')
    lines.append('         <key class="com.microchip.mcc.core.tokenManager.CustomKey" moduleName="Application Builder" name="state"/>')
    lines.append(f'         <value>{encoded_value}</value>')
    lines.append('      </entry>')
    lines.append('   </tokenMap>')

    # <generatedFileHashHistoryMap> - empty
    lines.append('   <generatedFileHashHistoryMap class="java.util.TreeMap"/>')

    lines.append('</config>')
    return '\n'.join(lines)


def validate_output(path):
    """Re-parse output to verify valid XML and JSON."""
    tree = ET.parse(path)
    root = tree.getroot()
    token_entry = root.find("tokenMap").find("entry")
    value_elem = token_entry.find("value")
    data = json.loads(value_elem.text)

    pkg_count = len(root.find("usedPackages"))
    class_count = len(root.find("usedClasses"))
    module_count = len(data["modules"])
    content_count = len(data["content"])

    print(f"Validation passed:")
    print(f"  Packages: {pkg_count}")
    print(f"  Classes: {class_count}")
    print(f"  Modules in JSON: {module_count}")
    print(f"  Content entries: {content_count}")
    print(f"  userAddedModules: {len(data['userAddedModules'])}")
    return True


def main():
    if not SOURCE.exists():
        print(f"ERROR: Source file not found: {SOURCE}", file=sys.stderr)
        return 1
    if not TARGET.exists():
        print(f"ERROR: Target file not found: {TARGET}", file=sys.stderr)
        return 1

    print(f"Source: {SOURCE}")
    print(f"Target: {TARGET}")
    print()

    # Parse both files
    print("Parsing source...")
    src_attribs, src_packages, src_classes, src_lib, src_json = parse_mc3(SOURCE)
    print(f"  {len(src_packages)} packages, {len(src_classes)} classes, {len(src_json['modules'])} modules")

    print("Parsing target...")
    tgt_attribs, tgt_packages, tgt_classes, tgt_lib, tgt_json = parse_mc3(TARGET)
    print(f"  {len(tgt_packages)} packages, {len(tgt_classes)} classes, {len(tgt_json['modules'])} modules")
    print()

    # Merge
    print("Merging...")
    merged_packages = merge_packages(src_packages, tgt_packages)
    merged_classes = merge_classes(src_classes, tgt_classes)
    merged_json = merge_json(src_json, tgt_json)
    print(f"  Merged packages: {len(merged_packages)}")
    print(f"  Merged classes: {len(merged_classes)}")
    print(f"  Merged modules: {len(merged_json['modules'])}")
    print()

    # Build output XML using target's attributes
    output = build_xml(tgt_attribs, merged_packages, merged_classes, tgt_lib, merged_json)

    # Backup and write
    backup_path = TARGET.with_suffix('.mc3.bak')
    shutil.copy2(TARGET, backup_path)
    print(f"Backup created: {backup_path}")

    TARGET.write_text(output, encoding='utf-8')
    print(f"Merged file written: {TARGET}")
    print()

    # Validate
    validate_output(TARGET)
    return 0


if __name__ == '__main__':
    sys.exit(main())
