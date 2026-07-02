#!/usr/bin/env python3
"""Generate Steam/Dota "go direct" exclusions for a TrustTunnel client profile.

In vpn_mode = "general" everything is routed through the VPN except entries in
`exclusions`. To keep Steam and Dota 2 traffic direct (not through TrustTunnel)
we add two kinds of entries:

  1. Steam/Dota DOMAINS — covers the client (store/community/api) and CDN/game
     downloads, which are name-based HTTPS traffic.
  2. Valve IP ranges (ASN AS32590) — covers actual Dota 2 GAMEPLAY, which is
     connectionless UDP to Valve's game servers and Steam Datagram Relay (SDR)
     pops. This traffic is by IP, so domain exclusions do not catch it.

TrustTunnel has no per-process split tunneling; routing is by destination only
(domain / IP / CIDR, optionally with port). Valve's game-server/relay IPs live
on AS32590, so excluding that ASN's prefixes is the robust way to send gameplay
direct. The CIDR list is fetched live from RIPEstat so it stays current.

Usage:
  update_steam_direct.py                  # print the exclusions block
  update_steam_direct.py --apply PROFILE  # inject/refresh a managed block in PROFILE
  update_steam_direct.py --no-ipv6        # omit IPv6 Valve prefixes

The managed block is wrapped in marker comments so re-running --apply refreshes
Valve's CIDRs in place without touching the rest of your `exclusions`.
"""

import argparse
import json
import re
import shutil
import sys
import urllib.request
from datetime import datetime, timezone

RIPESTAT_URL = "https://stat.ripe.net/data/announced-prefixes/data.json?resource=AS32590"
VALVE_AS = "AS32590"

# Steam/Dota domains to route directly. Add both the bare domain (matches it and
# its www. variant) and the *. wildcard (matches subdomains) for full coverage,
# per TrustTunnel's domain-matching rules.
STEAM_DOMAINS = [
    # Steam client: store / community / api / login / checkout
    "steamcommunity.com", "*.steamcommunity.com",
    "steampowered.com", "*.steampowered.com",
    "steamstatic.com", "*.steamstatic.com",
    # Steam CDN / depot downloads
    "steamcontent.com", "*.steamcontent.com",
    # Steam misc
    "steamchat.com", "*.steamchat.com",
    "steamserver.net", "*.steamserver.net",
    "steamgames.com", "*.steamgames.com",
    # Dota 2 web
    "dota2.com", "*.dota2.com",
]

BEGIN_MARKER = "# >>> steam-direct (managed by update_steam_direct.py) >>>"
END_MARKER = "# <<< steam-direct <<<"


def fetch_valve_prefixes(include_ipv6=True):
    """Fetch current Valve (AS32590) prefixes from RIPEstat."""
    with urllib.request.urlopen(RIPESTAT_URL, timeout=30) as r:
        data = json.load(r)
    prefixes = [p["prefix"] for p in data["data"]["prefixes"]]
    v4 = sorted(p for p in prefixes if ":" not in p and "." in p)
    out = list(v4)
    if include_ipv6:
        out += sorted(p for p in prefixes if ":" in p)
    return v4, [p for p in out if ":" in p], out


def build_block(include_ipv6=True):
    """Build the managed exclusions block text (without the marker lines)."""
    try:
        v4, v6, allpfx = fetch_valve_prefixes(include_ipv6)
        src = f"live from RIPEstat ({VALVE_AS})"
    except Exception as e:
        print(f"warning: could not fetch RIPEstat ({e}); emitting domains only", file=sys.stderr)
        allpfx = []
        src = "UNAVAILABLE (Valve CIDR fetch failed)"

    lines = []
    lines.append(f"  {BEGIN_MARKER}")
    lines.append(f"  # Valve {VALVE_AS} CIDRs: {src}, fetched {datetime.now(timezone.utc):%Y-%m-%d}Z")
    lines.append("  # --- Steam/Dota domains (client, CDN, downloads) ---")
    for d in STEAM_DOMAINS:
        lines.append(f'  "{d}",')
    if allpfx:
        lines.append(f"  # --- Valve {VALVE_AS} IPv4 (game servers / SDR relays / CM) ---")
        for p in v4:
            lines.append(f'  "{p}",')
        if include_ipv6 and v6:
            lines.append(f"  # --- Valve {VALVE_AS} IPv6 ---")
            for p in v6:
                lines.append(f'  "{p}",')
    lines.append(f"  {END_MARKER}")
    return "\n".join(lines)


def apply_to_profile(path, include_ipv6=True):
    """Insert/refresh the managed block inside the profile's `exclusions` array."""
    text = open(path).read()
    block = build_block(include_ipv6)

    # Replace an existing managed block in place (preserve everything else).
    pattern = re.compile(r"^[ \t]*# >>> steam-direct.*?^[ \t]*# <<< steam-direct <<<",
                         flags=re.DOTALL | re.MULTILINE)
    if pattern.search(text):
        new = pattern.sub(block, text, count=1)
    else:
        # First time: insert the block just before the closing ']' of the
        # `exclusions = [ ... ]` array, preserving all existing entries.
        m = re.search(r"^(\s*exclusions\s*=\s*\[)", text, flags=re.MULTILINE)
        if not m:
            # No exclusions array at all — append a minimal one.
            new = text.rstrip() + f"\n\nexclusions = [\n{block}\n]\n"
        else:
            start = m.end()
            close = text.find("]", start)
            if close == -1:
                print("error: could not find closing ']' of exclusions array", file=sys.stderr)
                sys.exit(1)
            new = text[:close] + block + "\n" + text[close:]

    shutil.copyfile(path, path + ".bak")
    open(path, "w").write(new)
    print(f"updated {path} (backup: {path}.bak)")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--apply", metavar="PROFILE", help="inject/refresh the managed block in this profile")
    ap.add_argument("--no-ipv6", action="store_true", help="omit IPv6 Valve prefixes")
    args = ap.parse_args()

    if args.apply:
        apply_to_profile(args.apply, include_ipv6=not args.no_ipv6)
    else:
        print(build_block(include_ipv6=not args.no_ipv6))


if __name__ == "__main__":
    main()
