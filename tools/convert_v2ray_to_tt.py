#!/usr/bin/env python3
"""Convert v2ray/v2raya routing config to TrustTunnel TOML config."""

import re
from collections import OrderedDict

INPUT = "/opt/trusttunnel_client/tmp_v2ray.conf"
OUTPUT = "/opt/trusttunnel_client/tmp_tt.conf"


def parse_v2ray_conf(path):
    """Parse v2ray config and extract proxy rules, preserving section context."""

    current_category = "general"

    with open(path, "r") as f:
        lines = f.readlines()

    entries = []  # list of (category, type, value)

    for line in lines:
        line = line.strip()

        if not line:
            continue

        # Commented lines
        if line.startswith("#"):
            stripped_comment = line.lstrip("#").strip()
            # Section label: a comment that isn't a commented-out rule
            if stripped_comment and "->" not in stripped_comment and "domain(" not in stripped_comment and "ip(" not in stripped_comment and "geosite" not in stripped_comment and "geoip" not in stripped_comment:
                current_category = stripped_comment.lower()
            continue

        # Skip direct rules, default, geoip/geosite
        if "->direct" in line or line.startswith("default:") or "geoip:" in line or "geosite:" in line:
            continue

        if "->proxy" not in line:
            continue

        # domain(domain:X)->proxy
        m = re.match(r'domain\(domain:(.+?)\)\s*->\s*proxy', line)
        if m:
            entries.append((current_category, "domain", m.group(1).strip()))
            continue

        # domain(contains:X)->proxy
        m = re.match(r'domain\(contains:(.+?)\)\s*->\s*proxy', line)
        if m:
            entries.append((current_category, "contains", m.group(1).strip()))
            continue

        # ip(X)->proxy
        m = re.match(r'ip\((.+?)\)\s*->\s*proxy', line)
        if m:
            entries.append((current_category, "ip", m.group(1).strip()))
            continue

    return entries


def categorize_entries(entries):
    """Group entries into well-defined categories using exact domain matching."""

    # Exact domain -> category mappings for well-known services
    domain_to_category = {}

    discord_domains = [
        "discord.com", "dis.gd", "discord.co", "discord.dev", "discord.gg",
        "discord.media", "discord.tools", "discordapp.com", "discordapp.net",
        "discordstatus.com", "discordpartygames.com", "discordactivities.com",
        "discordsays.com", "discord-activities.com",
    ]
    for d in discord_domains:
        domain_to_category[d] = "discord"

    whatsapp_domains = [
        "whatsapp.net", "whatsapp.com", "wa.me", "wl.co", "whatsappbrand.com",
        "whatsapp.cc", "whatsapp.org", "whatsapp-plus.me", "whatsapp-plus.net",
        "whatsapp-plus.info", "whatsapp.tv", "whatsapp.info",
    ]
    for d in whatsapp_domains:
        domain_to_category[d] = "whatsapp"

    youtube_domains = ["youtube.com", "youtu.be", "googlevideo.com", "ytimg.com", "ggpht.com"]
    for d in youtube_domains:
        domain_to_category[d] = "youtube"

    meta_domains = [
        "facebook.com", "fbcdn.net", "meta.com", "instagram.com", "cdninstagram.com",
    ]
    for d in meta_domains:
        domain_to_category[d] = "instagram/meta"

    twitter_domains = ["x.com", "twitter.com", "twimg.com", "t.com", "t.co"]
    for d in twitter_domains:
        domain_to_category[d] = "twitter/x"

    ai_domains = [
        "openai.com", "auth.openai.com", "platform.openai.com", "chatgpt.com",
        "chat.com", "oaistatic.com", "claude.com", "claude.ai", "anthropic.com",
        "grok.com", "x.ai", "ollama.com",
    ]
    for d in ai_domains:
        domain_to_category[d] = "openai/ai"

    atlassian_domains = [
        "atlassian.net", "atlassian.com", "atl-paas.net", "ss-inf.net",
        "jira.com", "bitbucket.org", "trello.com", "pndsn.com",
    ]
    for d in atlassian_domains:
        domain_to_category[d] = "atlassian"

    dev_domains = [
        "github.com", "docker.io", "terraform.io", "npmjs.org", "replit.com",
        "sentry.io", "sentry-cdn.com", "notion.so", "linear.app",
        "client-api.linear.app", "grafana.dev.coffeepoint.games", "openh264.org",
        "debian.org", "fedorapeople.org", "sublimetext.com", "globalping.io",
    ]
    for d in dev_domains:
        domain_to_category[d] = "dev tools"

    vpn_domains = ["letsvpn.world", "mullvad.net", "astrill.com"]
    for d in vpn_domains:
        domain_to_category[d] = "vpn services"

    gaming_domains = [
        "unity.com", "bethesda.net", "crazygames.com", "stake.com",
        "stake-engine.com", "jetton.games", "battle.net", "itch.io",
        "whale.io", "coffeestain.com", "bustabit.com",
    ]
    for d in gaming_domains:
        domain_to_category[d] = "gaming"

    adult_domains = [
        "fapello.com", "fapello.su", "xhamster.com", "undress.app",
        "phncdn.com", "xvideos.com", "xvideos-cdn.com", "xv-ru.com",
        "spankbang.com", "onlyfans.com", "aznude.com", "notfans.com",
    ]
    for d in adult_domains:
        domain_to_category[d] = "adult"

    envato_domains = ["envato.com", "audiojungle.net", "envato-static.com", "envatousercontent.com"]
    for d in envato_domains:
        domain_to_category[d] = "envato"

    # Contains pattern -> category
    contains_to_category = {
        "chatgpt": "openai/ai",
        "youtube": "youtube",
        "googlevideo": "youtube",
        "linear.app": "dev tools",
        "sentry.io": "dev tools",
        "porn": "adult",
        "thothub": "adult",
        "redtube": "adult",
        "rutracker": "general",
        "apmex": "general",
        "thepiratebay": "general",
        "ashoo": "general",
        "alcatel": "general",
    }

    # IP categorization by context from original file
    # Discord IPs: 66.22.x.x
    # Instagram IPs: the block after #instagram comment
    # General IPs: 157.240.205.174, 157.240.205.63, 160.79.104.0/23, 209.249.57.0/24

    grouped = OrderedDict()

    # Desired category ordering
    category_order = [
        "general", "dev tools", "openai/ai", "twitter/x", "instagram/meta",
        "atlassian", "gaming", "youtube", "vpn services", "envato", "adult",
        "discord", "whatsapp", "instagram ips", "discord ips",
    ]
    for c in category_order:
        grouped[c] = {"domains": [], "contains": [], "ips": []}

    all_domains_set = set()  # for dedup

    for orig_cat, typ, val in entries:
        if typ == "domain":
            cat = domain_to_category.get(val, "general")
            if val not in all_domains_set:
                all_domains_set.add(val)
                if cat not in grouped:
                    grouped[cat] = {"domains": [], "contains": [], "ips": []}
                grouped[cat]["domains"].append(val)

        elif typ == "contains":
            cat = contains_to_category.get(val, "general")
            if cat not in grouped:
                grouped[cat] = {"domains": [], "contains": [], "ips": []}
            if val not in grouped[cat]["contains"]:
                grouped[cat]["contains"].append(val)

        elif typ == "ip":
            # Categorize IPs based on original file context
            if orig_cat == "instagram":
                cat = "instagram ips"
            elif val.startswith("66.22."):
                cat = "discord ips"
            elif val in ("157.240.205.174", "157.240.205.63"):
                cat = "instagram/meta"
            elif val in ("160.79.104.0/23", "209.249.57.0/24"):
                cat = "openai/ai"
            else:
                cat = "general"
            if cat not in grouped:
                grouped[cat] = {"domains": [], "contains": [], "ips": []}
            if val not in grouped[cat]["ips"]:
                grouped[cat]["ips"].append(val)

    return grouped


def generate_toml(grouped):
    """Generate the TOML config string."""

    lines = []
    lines.append("# TrustTunnel Client Configuration")
    lines.append("# Converted from v2ray/v2raya format")
    lines.append("")
    lines.append('loglevel = "info"')
    lines.append('vpn_mode = "selective"')
    lines.append("")
    lines.append("# In selective mode, only these domains/IPs are routed through VPN")
    lines.append("exclusions = [")

    first_category = True
    for cat, data in grouped.items():
        domains = data["domains"]
        contains = data["contains"]
        ips = data["ips"]

        if not domains and not contains and not ips:
            continue

        if not first_category:
            lines.append("")
        first_category = False

        lines.append(f"  # --- {cat} ---")

        for d in domains:
            lines.append(f'  "{d}",')

        # Contains rules as comments
        for c in contains:
            # Check if covered by an explicit domain entry anywhere
            covered = False
            for ocat, odata in grouped.items():
                for d in odata["domains"]:
                    if c.lower() in d.lower():
                        covered = True
                        break
                if covered:
                    break

            if covered:
                lines.append(f'  # NOTE: v2ray "contains:{c}" rule is covered by explicit domain entries above.')
            else:
                lines.append(f'  # NOTE: v2ray "contains:{c}" rule cannot be exactly represented in TrustTunnel.')

        if ips and domains:
            lines.append(f"  # {cat} IPs")
        for ip in ips:
            lines.append(f'  "{ip}",')

    lines.append("]")
    lines.append("")

    return "\n".join(lines)


def main():
    entries = parse_v2ray_conf(INPUT)
    grouped = categorize_entries(entries)
    toml_content = generate_toml(grouped)

    with open(OUTPUT, "w") as f:
        f.write(toml_content)

    total_domains = sum(len(v["domains"]) for v in grouped.values())
    total_ips = sum(len(v["ips"]) for v in grouped.values())
    total_contains = sum(len(v["contains"]) for v in grouped.values())
    print(f"Conversion complete: {OUTPUT}")
    print(f"  Categories: {len([c for c, v in grouped.items() if v['domains'] or v['ips'] or v['contains']])}")
    print(f"  Domain exclusions: {total_domains}")
    print(f"  IP/CIDR exclusions: {total_ips}")
    print(f"  Contains rules (as comments): {total_contains}")


if __name__ == "__main__":
    main()
