/**
 * EET_Data
 * Konfigurations- und Datenklassen des Economy-Trackers (alle als JSON im Server-Profil).
 * Struktur der Ausgabedateien: siehe docs/DATA_FORMAT.md
 */
class EET_Constants {
	static const string ROOT_DIR    = "$profile:EconomyTracker/";
	static const string PLAYER_DIR  = ROOT_DIR + "Players/";
	static const string EVENT_DIR   = ROOT_DIR + "Events/";
	static const string CONFIG_FILE = ROOT_DIR + "Config.json";
	static const string ZONES_FILE  = ROOT_DIR + "Zones.json";
}

class EET_CurrencyDef {
	string ClassName;
	int    Value = 1;
	void EET_CurrencyDef(string className = "", int value = 1) {
		ClassName = className;
		Value = value;
	}
}

class EET_ConfigData {
	/** Abstand (s) der Messung von Spielzeit/Strecke/Zone. */
	int   SampleIntervalSec = 10;
	/** Abstand (s), in dem die Spielerdateien geschrieben werden (zusaetzlich beim Disconnect/Shutdown). */
	int   SaveIntervalSec = 300;
	/** Spruenge groesser als dieser Wert (m) zwischen zwei Messungen zaehlen nicht als Strecke (Teleport, Respawn). */
	float MaxJumpMeters = 250;
	/** Gesamte Aufzeichnung an/aus. */
	bool  Enabled = true;
	/** Einzelne Ereignisse zusaetzlich als Zeilen in Events/<Datum>.jsonl schreiben (Trades, Quests, Stashes, Tode). */
	bool  WriteEventLog = true;
	/** Loot-Aufnahmen vom Boden/Leichen/Containern zaehlen (Lootpool-Analyse). */
	bool  TrackLoot = true;
	/** Verbrauch (Essen, Medizin, Munition) zaehlen. */
	bool  TrackUsage = true;
	/** Schaden zaehlen. */
	bool  TrackDamage = true;
	/** Waehrungs-Items (Classname -> Wert). Wird fuer "Geld gefunden" und zum Ausschluss aus dem Lootpool benoetigt. */
	ref array<ref EET_CurrencyDef> Currency = new array<ref EET_CurrencyDef>();
}

class EET_Zone {
	string ZoneId;
	string Name;
	/** Tier des Gebiets (frei vergeben, z. B. 1-5). Bestimmt Loot und Mutanten. */
	int    Tier = 1;
	float  X;
	float  Z;
	float  Radius = 100;
}

class EET_ZoneFile {
	ref array<ref EET_Zone> Zones = new array<ref EET_Zone>();
}

/**
 * Eine Zeile der Statistik-Tabelle. Cat bestimmt die Bedeutung von K1..K3:
 *   zone_time   K1=Zone                          Count=Messungen  Sum=Sekunden
 *   zone_dist   K1=Zone                          Sum=Meter
 *   loot        K1=Zone K2=Quelle K3=Classname   Count=Stueck (Quelle: ground|corpse|container)
 *   money_found K1=Zone K2=Quelle                Sum=Geldwert
 *   trade_buy   K1=TraderId K2=-  K3=Classname   Count=Stueck  Sum=bezahlt
 *   trade_sell  K1=TraderId K2=-  K3=Classname   Count=Stueck  Sum=erhalten
 *   trade_barter K1=TraderId K3=Classname        Count=Stueck
 *   kill        K1=Zone K3=Classname             Count=Kills
 *   damage      K1=Zone K2=Quelle(Classname)     Count=Treffer Sum=Schaden (HP)
 *   consume     K3=Classname                     Count=Verbrauch
 *   ammo        K3=Munitionstyp                  Count=Schuesse
 *   stash       K1=StashId                       Count=Items entnommen
 *   quest       K1=QuestId                       Count=abgeschlossen  Sum=Geld-Belohnung
 *   quest_item  K1=QuestId K3=Classname          Count=Stueck als Belohnung
 *   craft       K3=Classname                     Count=hergestellt
 *   harvest/fish K3=Classname                    Count=Stueck
 */
class EET_Stat {
	string Cat;
	string K1;
	string K2;
	string K3;
	int    Count;
	float  Sum;
}

class EET_PlayerStats {
	string PlayerId;
	string Name;
	int    FirstSeen;
	int    LastSeen;
	int    Sessions;
	int    Deaths;
	float  PlaytimeSec;
	float  DistanceM;
	float  DamageTaken;
	float  MoneyEarnedSell;
	float  MoneySpentBuy;
	float  MoneyQuestRewards;
	float  MoneyFound;
	ref array<ref EET_Stat> Stats = new array<ref EET_Stat>();
}
