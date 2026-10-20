/**
 * EET_Tracker (nur Server)
 * Zentrale Sammelstelle: haelt pro Spieler eine Statistik-Tabelle (siehe EET_Stat) im Speicher, schreibt sie periodisch
 * als JSON nach $profile:EconomyTracker/Players/<SteamId>.json und haengt Einzelereignisse an Events/<Datum>.jsonl an.
 * Die Hooks (EET_Hooks.c, EET_Integrations.c) rufen nur die Record*-Methoden auf und enthalten keine Logik.
 */
class EET_Session {
	string PlayerId;
	ref EET_PlayerStats Data;
	ref map<string, ref EET_Stat> Index = new map<string, ref EET_Stat>();
	vector LastPos;
	bool   HasLastPos;
	int    LastMs;
	float  LastHealth;
	bool   HasLastHealth;
	float  HitSinceTick;
}

class EET_Tracker {
	protected static ref EET_Tracker s_Instance;
	protected ref EET_ConfigData m_Config;
	protected ref array<ref EET_Zone> m_Zones = new array<ref EET_Zone>();
	protected ref map<string, int> m_CurrencyValue = new map<string, int>();
	protected ref map<string, ref EET_Session> m_Sessions = new map<string, ref EET_Session>();
	protected bool  m_Ready;
	protected float m_SinceSave;

	static EET_Tracker Get() {
		if (!s_Instance)
			s_Instance = new EET_Tracker();
		return s_Instance;
	}

	// ------------------------------------------------------------------ Start / Ende

	void Init() {
		if (m_Ready)
			return;
		if (!FileExist(EET_Constants.ROOT_DIR))
			MakeDirectory(EET_Constants.ROOT_DIR);
		if (!FileExist(EET_Constants.PLAYER_DIR))
			MakeDirectory(EET_Constants.PLAYER_DIR);
		if (!FileExist(EET_Constants.EVENT_DIR))
			MakeDirectory(EET_Constants.EVENT_DIR);
		LoadConfig();
		LoadZones();
		if (!m_Config.Enabled) {
			Print("[EET] Aufzeichnung ist in Config.json deaktiviert.");
			return;
		}
		m_Ready = true;
		SubscribeQuestEvents();
		int intervalMs = Math.Max(2, m_Config.SampleIntervalSec) * 1000;
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Tick, intervalMs, true);
		Print("[EET] Economy-Tracker gestartet. Zonen: " + m_Zones.Count().ToString());
	}

	bool IsReady() {
		return m_Ready;
	}

	EET_ConfigData GetConfig() {
		return m_Config;
	}

	protected void LoadConfig() {
		m_Config = new EET_ConfigData();
		if (FileExist(EET_Constants.CONFIG_FILE)) {
			JsonFileLoader<EET_ConfigData>.JsonLoadFile(EET_Constants.CONFIG_FILE, m_Config);
		}
		else {
			// Bekannte Waehrung des Trader-Mods vorbelegen; bei Bedarf in Config.json anpassen.
			m_Config.Currency.Insert(new EET_CurrencyDef("Exodus_Geld_Rbl_1", 1));
			m_Config.Currency.Insert(new EET_CurrencyDef("Exodus_Geld_Rbl_2", 2));
			m_Config.Currency.Insert(new EET_CurrencyDef("Exodus_Geld_Rbl_5", 5));
			m_Config.Currency.Insert(new EET_CurrencyDef("Exodus_Geld_Rbl_10", 10));
			m_Config.Currency.Insert(new EET_CurrencyDef("Exodus_Geld_Rbl_50", 50));
			m_Config.Currency.Insert(new EET_CurrencyDef("Exodus_Geld_Rbl_100", 100));
			m_Config.Currency.Insert(new EET_CurrencyDef("Exodus_Geld_Rbl_200", 200));
			m_Config.Currency.Insert(new EET_CurrencyDef("Exodus_Geld_Rbl_500", 500));
			m_Config.Currency.Insert(new EET_CurrencyDef("Exodus_Geld_Rbl_1000", 1000));
			m_Config.Currency.Insert(new EET_CurrencyDef("Exodus_Geld_Rbl_2000", 2000));
			m_Config.Currency.Insert(new EET_CurrencyDef("Exodus_Geld_Rbl_5000", 5000));
			JsonFileLoader<EET_ConfigData>.JsonSaveFile(EET_Constants.CONFIG_FILE, m_Config);
		}
		m_CurrencyValue.Clear();
		foreach (EET_CurrencyDef def : m_Config.Currency) {
			if (def && def.ClassName != "")
				m_CurrencyValue.Insert(def.ClassName, def.Value);
		}
	}

	protected void LoadZones() {
		m_Zones.Clear();
		ref EET_ZoneFile file = new EET_ZoneFile();
		if (FileExist(EET_Constants.ZONES_FILE)) {
			JsonFileLoader<EET_ZoneFile>.JsonLoadFile(EET_Constants.ZONES_FILE, file);
		}
		else {
			// Beispiel-Zone zum Anpassen. Ausserhalb aller Zonen wird "none" erfasst.
			EET_Zone example = new EET_Zone();
			example.ZoneId = "example";
			example.Name = "Beispielzone (bitte anpassen)";
			example.Tier = 1;
			example.X = 7500;
			example.Z = 7500;
			example.Radius = 500;
			file.Zones.Insert(example);
			JsonFileLoader<EET_ZoneFile>.JsonSaveFile(EET_Constants.ZONES_FILE, file);
		}
		foreach (EET_Zone zone : file.Zones) {
			if (zone && zone.ZoneId != "")
				m_Zones.Insert(zone);
		}
	}

	/** Kleinste Zone, die die Position enthaelt (innere Zonen haben Vorrang), sonst "none". */
	string GetZoneId(vector pos) {
		string best = "none";
		float bestRadius = 99999999;
		foreach (EET_Zone zone : m_Zones) {
			float dx = pos[0] - zone.X;
			float dz = pos[2] - zone.Z;
			if (dx * dx + dz * dz <= zone.Radius * zone.Radius && zone.Radius < bestRadius) {
				best = zone.ZoneId;
				bestRadius = zone.Radius;
			}
		}
		return best;
	}

	string ZoneOf(PlayerBase player) {
		if (!player)
			return "none";
		return GetZoneId(player.GetPosition());
	}

	// ------------------------------------------------------------------ Sessions

	EET_Session GetSession(PlayerBase player) {
		if (!m_Ready || !player)
			return null;
		PlayerIdentity identity = player.GetIdentity();
		if (!identity)
			return null;
		string id = identity.GetId();
		EET_Session session;
		if (m_Sessions.Find(id, session)) {
			session.Data.Name = identity.GetName();
			return session;
		}
		session = new EET_Session();
		session.PlayerId = id;
		session.Data = new EET_PlayerStats();
		string path = PlayerFile(id);
		if (FileExist(path))
			JsonFileLoader<EET_PlayerStats>.JsonLoadFile(path, session.Data);
		session.Data.PlayerId = id;
		session.Data.Name = identity.GetName();
		int now = EQ_TimeUtils.Now();
		if (session.Data.FirstSeen == 0)
			session.Data.FirstSeen = now;
		session.Data.LastSeen = now;
		session.Data.Sessions++;
		foreach (EET_Stat stat : session.Data.Stats)
			session.Index.Insert(StatKey(stat.Cat, stat.K1, stat.K2, stat.K3), stat);
		session.LastMs = GetGame().GetTime();
		m_Sessions.Insert(id, session);
		LogEvent("join", player, "");
		return session;
	}

	protected string PlayerFile(string id) {
		return EET_Constants.PLAYER_DIR + id + ".json";
	}

	protected string StatKey(string cat, string k1, string k2, string k3) {
		return cat + "|" + k1 + "|" + k2 + "|" + k3;
	}

	void OnDisconnect(PlayerBase player, PlayerIdentity identity) {
		if (!m_Ready || !identity)
			return;
		string id = identity.GetId();
		EET_Session session;
		if (!m_Sessions.Find(id, session))
			return;
		LogEvent("leave", player, "");
		SaveSession(session);
		m_Sessions.Remove(id);
	}

	void SaveSession(EET_Session session) {
		if (!session || !session.Data)
			return;
		session.Data.LastSeen = EQ_TimeUtils.Now();
		JsonFileLoader<EET_PlayerStats>.JsonSaveFile(PlayerFile(session.PlayerId), session.Data);
	}

	void SaveAll() {
		if (!m_Ready)
			return;
		for (int i = 0; i < m_Sessions.Count(); i++)
			SaveSession(m_Sessions.GetElement(i));
		m_SinceSave = 0;
	}

	// ------------------------------------------------------------------ Zyklische Messung

	protected void Tick() {
		if (!m_Ready)
			return;
		int nowMs = GetGame().GetTime();
		array<Man> men = new array<Man>();
		GetGame().GetPlayers(men);
		foreach (Man man : men) {
			PlayerBase player = PlayerBase.Cast(man);
			if (!player || !player.GetIdentity())
				continue;
			EET_Session s = GetSession(player);
			if (!s)
				continue;
			float dt = (nowMs - s.LastMs) / 1000.0;
			s.LastMs = nowMs;
			if (dt < 0 || dt > m_Config.SampleIntervalSec * 4)
				dt = m_Config.SampleIntervalSec;
			if (!player.IsAlive()) {
				s.HasLastPos = false;
				s.HasLastHealth = false;
				continue;
			}
			vector pos = player.GetPosition();
			string zone = GetZoneId(pos);
			s.Data.PlaytimeSec += dt;
			Add(player, "zone_time", zone, "", "", 1, dt);
			if (s.HasLastPos) {
				vector a = Vector(pos[0], 0, pos[2]);
				vector b = Vector(s.LastPos[0], 0, s.LastPos[2]);
				float dist = vector.Distance(a, b);
				if (dist <= m_Config.MaxJumpMeters) {
					s.Data.DistanceM += dist;
					Add(player, "zone_dist", zone, "", "", 0, dist);
				}
			}
			s.LastPos = pos;
			s.HasLastPos = true;
			if (m_Config.TrackDamage)
				MeasureEnvironmentDamage(player, s, zone);
		}
		m_SinceSave += m_Config.SampleIntervalSec;
		if (m_SinceSave >= m_Config.SaveIntervalSec)
			SaveAll();
	}

	/**
	 * Lebensverlust ohne Treffer (Anomalien, Strahlung, Blutung, Hunger ...): Differenz der Gesundheit zwischen zwei
	 * Messungen abzueglich der in EEHitBy gemeldeten Treffer. Heilung im selben Intervall verdeckt Verluste (Naeherung).
	 */
	protected void MeasureEnvironmentDamage(PlayerBase player, EET_Session s, string zone) {
		float health = player.GetHealth("", "Health");
		if (s.HasLastHealth) {
			float other = (s.LastHealth - health) - s.HitSinceTick;
			if (other > 0.5) {
				s.Data.DamageTaken += other;
				Add(player, "damage", zone, "environment", "", 1, other);
			}
		}
		s.LastHealth = health;
		s.HasLastHealth = true;
		s.HitSinceTick = 0;
	}

	// ------------------------------------------------------------------ Aufzeichnen

	/** Allgemeiner Zaehler. Auch fuer Erweiterungen/andere Mods gedacht (z. B. Strahlung): EET_Tracker.Get().Add(...). */
	void Add(PlayerBase player, string cat, string k1, string k2, string k3, int count, float sum) {
		EET_Session s = GetSession(player);
		if (!s)
			return;
		string key = StatKey(cat, k1, k2, k3);
		EET_Stat stat;
		if (!s.Index.Find(key, stat)) {
			stat = new EET_Stat();
			stat.Cat = cat;
			stat.K1 = k1;
			stat.K2 = k2;
			stat.K3 = k3;
			s.Index.Insert(key, stat);
			s.Data.Stats.Insert(stat);
		}
		stat.Count += count;
		stat.Sum += sum;
	}

	void RecordDeath(PlayerBase player) {
		EET_Session s = GetSession(player);
		if (!s)
			return;
		s.Data.Deaths++;
		s.HasLastPos = false;
		s.HasLastHealth = false;
		LogEvent("death", player, "");
	}

	void RecordKill(PlayerBase killer, EntityAI victim, Object source) {
		if (!killer || !victim)
			return;
		string victimType = victim.GetType();
		if (PlayerBase.Cast(victim))
			victimType = "player";
		Add(killer, "kill", ZoneOf(killer), "", victimType, 1, 0);
		string weapon = "";
		if (source)
			weapon = source.GetType();
		LogEvent("kill", killer, J("victim", victimType) + "," + J("weapon", weapon));
	}

	void RecordHit(PlayerBase victim, EntityAI source, string ammo, float amount) {
		if (!m_Config || !m_Config.TrackDamage || amount <= 0)
			return;
		EET_Session s = GetSession(victim);
		if (!s)
			return;
		string src = ammo;
		if (source) {
			PlayerBase attacker = PlayerBase.Cast(source);
			if (attacker)
				src = "player";
			else if (source.GetHierarchyRootPlayer())
				src = "player:" + source.GetType();
			else
				src = source.GetType();
		}
		if (src == "")
			src = "unknown";
		s.HitSinceTick += amount;
		s.Data.DamageTaken += amount;
		Add(victim, "damage", ZoneOf(victim), src, "", 1, amount);
	}

	/** Item ist neu in das Inventar eines Spielers gewandert. oldLoc bestimmt die Quelle. */
	void OnItemMoved(ItemBase item, InventoryLocation oldLoc, InventoryLocation newLoc) {
		if (!m_Ready || !item)
			return;
		PlayerBase newRoot = PlayerBase.Cast(item.GetHierarchyRootPlayer());
		EntityAI oldParent = oldLoc.GetParent();
		PlayerBase oldRoot;
		if (oldParent)
			oldRoot = PlayerBase.Cast(oldParent.GetHierarchyRootPlayer());
		if (oldLoc.GetType() == InventoryLocationType.HANDS && oldLoc.GetParent())
			oldRoot = PlayerBase.Cast(oldLoc.GetParent());

		// Spieler gibt etwas ab (Boden, Container, anderer Spieler): spaeteres Aufheben zaehlt nicht als Loot.
		if (oldRoot && !newRoot)
			item.m_EET_Handled = true;
		if (!newRoot || newRoot == oldRoot || !m_Config.TrackLoot)
			return;

		string source = "";
		InventoryLocationType oldType = oldLoc.GetType();
		if (oldType == InventoryLocationType.GROUND) {
			source = "ground";
		}
		else if (oldParent && !oldRoot) {
			if (ZombieBase.Cast(oldParent) || AnimalBase.Cast(oldParent) || PlayerBase.Cast(oldParent))
				source = "corpse/" + oldParent.GetType();
			else
				source = "container/" + oldParent.GetType();
		}
		else if (oldRoot && oldRoot != newRoot && !oldRoot.IsAlive()) {
			source = "corpse/player";
		}
		// oldType UNKNOWN = neu erzeugt (Trader, Quest-Belohnung, Craft) -> kein Loot.
		if (source == "")
			return;
		if (item.m_EET_Handled) {
			item.m_EET_Handled = false;
			return;
		}
		int amount = 1;
		if (item.HasQuantity() && item.GetQuantityMax() > 1)
			amount = Math.Max(1, Math.Round(item.GetQuantity()));
		string zone = ZoneOf(newRoot);
		string cls = item.GetType();
		int value;
		if (m_CurrencyValue.Find(cls, value)) {
			float found = value * amount;
			GetSession(newRoot).Data.MoneyFound += found;
			Add(newRoot, "money_found", zone, source, "", amount, found);
			LogEvent("money_found", newRoot, J("source", source) + "," + JN("value", found));
			return;
		}
		Add(newRoot, "loot", zone, source, cls, amount, 0);
	}

	void RecordTrade(PlayerBase player, string traderId, string mode, XCPTraderTransactionRequest request) {
		if (!player || !request || !request.items)
			return;
		string items = "";
		float total = 0;
		foreach (XCPTraderTransactionEntry entry : request.items) {
			if (!entry)
				continue;
			string cls = entry.classname;
			if (entry.selected_classname != "")
				cls = entry.selected_classname;
			string cat = "trade_" + mode;
			if (mode == "buy" || mode == "sell") {
				Add(player, cat, traderId, "", cls, entry.count, entry.price);
				total += entry.price;
			}
			else {
				Add(player, cat, traderId, "", cls, entry.count, 0);
			}
			if (items != "")
				items = items + ",";
			items = items + "{" + J("class", cls) + "," + JN("count", entry.count) + "," + JN("price", entry.price) + "}";
		}
		EET_Session s = GetSession(player);
		if (s) {
			if (mode == "buy")
				s.Data.MoneySpentBuy += total;
			else if (mode == "sell")
				s.Data.MoneyEarnedSell += total;
		}
		LogEvent("trade", player, J("trader", traderId) + "," + J("mode", mode) + "," + JN("total", total) + ",\"items\":[" + items + "]");
	}

	/** Bench-Crafting (XCP_Craftbenches). */
	void RecordBenchCraft(PlayerBase player, string benchType, string recipeName, string result, int resultCount, array<ref CraftComponent> components) {
		if (!player)
			return;
		Add(player, "craft", benchType, recipeName, result, Math.Max(1, resultCount), 0);
		string comps = "";
		if (components) {
			foreach (CraftComponent comp : components) {
				if (!comp)
					continue;
				if (comp.Destroy)
					Add(player, "craft_in", benchType, result, comp.Classname, comp.Amount, 0);
				if (comps != "")
					comps = comps + ",";
				comps = comps + "{" + J("class", comp.Classname) + "," + JN("amount", comp.Amount) + "}";
			}
		}
		LogEvent("craft", player, J("bench", benchType) + "," + J("recipe", recipeName) + "," + J("result", result) + "," + JN("count", resultCount) + ",\"ingredients\":[" + comps + "]");
	}

	void RecordQuestFinished(PlayerBase player, EQ_QuestConfig cfg) {
		if (!player || !cfg)
			return;
		float money = 0;
		string rewards = "";
		foreach (EQ_RewardConfig reward : cfg.Rewards) {
			if (!reward || reward.ClassName == "")
				continue;
			int amount = reward.Amount;
			if (reward.Quantity > 0)
				amount = amount * reward.Quantity;
			int value;
			if (m_CurrencyValue.Find(reward.ClassName, value))
				money += value * amount;
			else
				Add(player, "quest_item", cfg.QuestId, "", reward.ClassName, amount, 0);
			if (rewards != "")
				rewards = rewards + ",";
			rewards = rewards + "{" + J("class", reward.ClassName) + "," + JN("amount", amount) + "}";
		}
		EET_Session s = GetSession(player);
		if (s)
			s.Data.MoneyQuestRewards += money;
		Add(player, "quest", cfg.QuestId, "", "", 1, money);
		LogEvent("quest", player, J("quest", cfg.QuestId) + "," + J("category", cfg.Category) + "," + JN("money", money) + ",\"rewards\":[" + rewards + "]");
	}

	void RecordAction(PlayerBase player, string actionClass, Object target, ItemBase mainItem) {
		if (!m_Ready || !player)
			return;
		string zone = ZoneOf(player);
		string targetType = "";
		if (target && !PlayerBase.Cast(target))
			targetType = target.GetType();
		// Spezielle Aktionen mit Ziel: Stash/Loot-Objekte durchsuchen, Mutanten haeuten.
		if (actionClass == "ActionXCP_LootSearch") {
			Add(player, "stash_search", zone, "", targetType, 1, 0);
			LogEvent("stash_search", player, J("object", targetType));
			return;
		}
		if (actionClass == "ActionXCPSkinning") {
			Add(player, "skin", zone, "", targetType, 1, 0);
			return;
		}
		if (!m_Config.TrackUsage || !mainItem)
			return;
		if (IsUseAction(actionClass))
			Add(player, "use", "", mainItem.GetType(), actionClass, 1, 0);
	}

	protected bool IsUseAction(string actionClass) {
		return actionClass.IndexOf("ActionEat") == 0 || actionClass.IndexOf("ActionDrink") == 0 || actionClass.IndexOf("ActionConsume") == 0
			|| actionClass.IndexOf("ActionForceConsume") == 0 || actionClass.IndexOf("ActionForceFeed") == 0 || actionClass.IndexOf("ActionInject") == 0
			|| actionClass.IndexOf("ActionBandage") == 0 || actionClass.IndexOf("ActionApply") == 0 || actionClass.IndexOf("ActionTreat") == 0
			|| actionClass.IndexOf("ActionSplint") == 0 || actionClass.IndexOf("ActionTakePills") == 0 || actionClass.IndexOf("ActionHeal") == 0;
	}

	// ------------------------------------------------------------------ Quest-Events (ExodusQuests)

	protected void SubscribeQuestEvents() {
		EQ_EventManager em = EQ_EventManager.Get();
		em.OnKillEx.Insert(HandleKill);
		em.OnPlayerDeath.Insert(HandlePlayerDeath);
		em.OnCraft.Insert(HandleCraft);
		em.OnHarvest.Insert(HandleHarvest);
		em.OnFish.Insert(HandleFish);
	}

	protected void HandleKill(PlayerBase killer, EntityAI victim, Object source) {
		RecordKill(killer, victim, source);
	}

	protected void HandlePlayerDeath(PlayerBase victim) {
		RecordDeath(victim);
	}

	protected void HandleCraft(PlayerBase player, ItemBase result) {
		if (result)
			Add(player, "craft", "vanilla", "", result.GetType(), 1, 0);
	}

	protected void HandleHarvest(PlayerBase player, string className, int amount) {
		Add(player, "harvest", ZoneOf(player), "", className, amount, 0);
	}

	protected void HandleFish(PlayerBase player, ItemBase fish) {
		if (fish)
			Add(player, "fish", ZoneOf(player), "", fish.GetType(), 1, 0);
	}

	// ------------------------------------------------------------------ Ereignisprotokoll (JSON Lines)

	void LogEvent(string type, PlayerBase player, string jsonFields) {
		if (!m_Ready || !m_Config.WriteEventLog)
			return;
		string playerId = "";
		string name = "";
		string zone = "none";
		if (player) {
			if (player.GetIdentity()) {
				playerId = player.GetIdentity().GetId();
				name = player.GetIdentity().GetName();
			}
			zone = GetZoneId(player.GetPosition());
		}
		string line = "{" + JN("t", EQ_TimeUtils.Now()) + "," + J("type", type) + "," + J("player", playerId) + "," + J("name", name) + "," + J("zone", zone);
		if (jsonFields != "")
			line = line + "," + jsonFields;
		line = line + "}";
		int year, month, day;
		GetYearMonthDayUTC(year, month, day);
		string m = month.ToString();
		if (month < 10)
			m = "0" + m;
		string d = day.ToString();
		if (day < 10)
			d = "0" + d;
		FileHandle fh = OpenFile(EET_Constants.EVENT_DIR + year.ToString() + "-" + m + "-" + d + ".jsonl", FileMode.APPEND);
		if (fh) {
			FPrintln(fh, line);
			CloseFile(fh);
		}
	}

	// ------------------------------------------------------------------ JSON-Helfer

	static string Esc(string value) {
		string s = value;
		s.Replace("\\", "\\\\");
		s.Replace("\"", "\\\"");
		return s;
	}

	/** "key":"value" */
	static string J(string key, string value) {
		return "\"" + key + "\":\"" + Esc(value) + "\"";
	}

	/** "key":123 */
	static string JN(string key, float value) {
		return "\"" + key + "\":" + value.ToString();
	}
}
