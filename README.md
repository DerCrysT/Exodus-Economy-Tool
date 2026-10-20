# Exodus Economy Tool

Zwei Teile:

1. **`mod/ExodusEconomyTracker`** – Server-Mod fuer DayZ, die Spielerdaten fuer die Economy aufzeichnet (Stand: erster Entwurf, noch nicht in DayZ kompiliert/getestet).
2. **Economy-Tool** – (folgt) liest die aufgezeichneten JSONs plus die Konfigurationen des Servers (Items/Tiers, Trader, Rezepte, Quests) und berechnet Preise, Craft-Kosten und Quest-Belohnungen.

## Mod: ExodusEconomyTracker

Haengt sich hart an `ExodusQuests`, `XCP_Trader_V2` und `XCP_Craftbenches` (siehe `requiredAddons` in `config.cpp`). Nur Server – kein Client-Teil noetig.

Aufgezeichnet wird pro Spieler (`$profile:EconomyTracker/Players/<SteamId>.json`):

| Was | Wie |
|---|---|
| Spielzeit, Strecke | Messung alle `SampleIntervalSec` (10 s); Spruenge > `MaxJumpMeters` zaehlen nicht; je Zone getrennt |
| Umsatz | Jeder erfolgreiche Trader-Kauf/-Verkauf/-Tausch (Trader, Item, Menge, Preis); Quest-Geldbelohnungen; gefundenes Geld |
| Loot / Lootpool | Item-Aufnahme vom Boden, aus Containern, von Leichen – je Zone und Quelle. Selbst abgelegte Items und frisch erzeugte (Trader, Quest, Craft) zaehlen nicht |
| Stashes | Quest-Stashes (ExodusQuests, LootStash) und `ActionXCP_LootSearch` (XCP Loot-System) |
| Schaden | Treffer (Quelle, Zone, HP) plus "environment" = Lebensverlust ohne Treffer (Anomalien, Strahlung, Blutung, Hunger) |
| Kills, Haeuten | Kills je Classname/Zone, `ActionXCPSkinning` je Mutant |
| Craften | Bench-Crafts mit Zutaten (XCP_Craftbenches), Vanilla-Crafts, Harvest, Fischen |
| Verbrauch | Essen/Trinken/Medizin/Tabletten ueber die Aktionen (`use`) |

Dazu `Events/<Datum>.jsonl` mit Einzelereignissen (trade, craft, quest, stash, money_found, death, join, leave).

### Konfiguration (wird beim ersten Start angelegt)

- `Config.json` – Intervalle, Schalter, **Waehrungs-Classnames mit Wert** (vorbelegt mit `Exodus_Geld_Rbl_*`).
- `Zones.json` – Gebiete als Kreise `{ZoneId, Name, Tier, X, Z, Radius}`. Innere (kleinere) Zonen haben Vorrang, ausserhalb gilt `none`. **Hier muessen die echten Gebiete mit Tier eingetragen werden.**

### Statistik-Tabelle (`Stats` in der Spielerdatei)

Jede Zeile: `Cat, K1, K2, K3, Count, Sum`. Bedeutung siehe Kommentar in `mod/.../scripts/3_Game/EET_Data.c`.
Beispiele: `loot | zone | ground | Classname | 3`, `trade_sell | trader | | Classname | 2 | 400`, `damage | zone | Quelle | | 14 | 230.5`.

### Bekannte Grenzen

- Nicht kompiliert/getestet (kein DayZ-Tools-Build hier) – bitte zuerst auf einem Testserver laden und die Script-Logs pruefen.
- Items, die per Skript auf den Boden gelegt werden (Haeuten, Trader mit `drop_feet`), zaehlen beim Aufheben als Bodenloot. Mutantenteile erkennt man in der Auswertung ueber die `skin`-Zeilen / die Skinning-Config.
- Munitionsverbrauch (Schuesse) wird noch nicht erfasst.
- Strahlung hat keinen eigenen Hook, solange kein Strahlungs-Mod mit bekannter API vorliegt; ein Fremd-Mod kann `EET_Tracker.Get().Add(player, cat, k1, k2, k3, count, sum)` aufrufen.
