#!/usr/bin/env python3
"""
Generate resources/arcaderoms.xml by combining two sources:

  1. AntoPISA MAME_(arcade).dat  — authoritative source for all entries
                                   (games, devices, BIOS, cloneof)
  2. `mame -listxml`             — only used to add vert="true" on existing games

Run as:  python3 generate_arcaderoms.py [/path/to/mame]
"""

import io
import subprocess
import sys
import os
import urllib.request
import xml.etree.ElementTree as ET

OUTPUT  = os.path.join(os.path.dirname(os.path.abspath(__file__)), "resources", "arcaderoms.xml")
MAME    = sys.argv[1] if len(sys.argv) > 1 else "mame"
DAT_URL = "https://raw.githubusercontent.com/AntoPISA/MAME_Dats/refs/heads/main/MAME_dat/MAME_(arcade).dat"


def fetch_dat(url):
    """Return all entries from the DAT as a list of dicts, preserving order."""
    print(f"Fetching {url} ...", flush=True)
    with urllib.request.urlopen(url) as r:
        data = r.read()

    entries = []
    for event, elem in ET.iterparse(io.BytesIO(data), events=("end",)):
        if elem.tag == "machine":
            mid     = elem.get("name", "")
            is_dev  = elem.get("isdevice", "no") == "yes"
            is_bios = elem.get("isbios",   "no") == "yes"
            if mid:
                desc = elem.find("description")
                entries.append({
                    "id":      mid,
                    "name":    (desc.text or "").strip() if desc is not None else "",
                    "cloneof": elem.get("cloneof", ""),
                    "device":  is_dev,
                    "bios":    is_bios,
                })
            elem.clear()

    games   = sum(1 for e in entries if not e["device"] and not e["bios"])
    devices = sum(1 for e in entries if e["device"])
    bioses  = sum(1 for e in entries if e["bios"])
    print(f"  {games} games, {devices} devices, {bioses} BIOS loaded from DAT.", flush=True)
    return entries


def fetch_vert_info(mame_bin):
    """Return {id} set of game IDs that have vertical display in mame -listxml."""
    print(f"Running: {mame_bin} -listxml (vert info only) ...", flush=True)
    proc = subprocess.Popen(
        [mame_bin, "-listxml"],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
    )

    vert_ids = set()
    mid = None
    is_vert = False

    for event, elem in ET.iterparse(proc.stdout, events=("start", "end")):
        if event == "start" and elem.tag == "machine":
            mid     = elem.get("name", "")
            is_vert = False

        elif event == "end":
            if elem.tag == "display" and mid is not None:
                if elem.get("rotate", "0") in ("90", "270"):
                    is_vert = True
                elem.clear()

            elif elem.tag == "machine":
                if mid and is_vert:
                    vert_ids.add(mid)
                mid = None
                elem.clear()

    proc.wait()
    print(f"  {len(vert_ids)} vertical games found.", flush=True)
    return vert_ids


def build_xml(entries, vert_ids):
    lines = ['<?xml version="1.0" encoding="utf-8"?>', "<roms>"]

    for e in entries:
        esc_id = e["id"].replace("&", "&amp;").replace('"', "&quot;")

        if e["device"]:
            lines.append(f'  <rom id="{esc_id}" device="true" />')
        elif e["bios"]:
            lines.append(f'  <rom id="{esc_id}" bios="true" />')
        else:
            cloneof    = e["cloneof"]
            vert_attr  = ' vert="true"' if e["id"] in vert_ids else ""
            clone_attr = f' cloneof="{cloneof}"' if cloneof else ""
            esc_name   = e["name"].replace("&", "&amp;").replace('"', "&quot;")
            lines.append(f'  <rom id="{esc_id}"{vert_attr}{clone_attr} name="{esc_name}" />')

    lines.append("</roms>")
    return "\n".join(lines) + "\n"


def main():
    entries  = fetch_dat(DAT_URL)
    vert_ids = fetch_vert_info(MAME)

    xml = build_xml(entries, vert_ids)

    os.makedirs(os.path.dirname(OUTPUT), exist_ok=True)
    with open(OUTPUT, "w", encoding="utf-8") as f:
        f.write(xml)

    games   = xml.count(' name="')
    verts   = xml.count('vert="true"')
    devices = xml.count('device="true"')
    bioses  = xml.count('bios="true"')
    print(f"Written to {OUTPUT}: {games} games ({verts} vertical), {devices} devices, {bioses} BIOS")


if __name__ == "__main__":
    main()
