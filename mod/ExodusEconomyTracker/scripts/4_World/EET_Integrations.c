/**
 * EET_Integrations
 * Einhakpunkte in die Mods des Servers: XCP_Trader_V2 (Kaufen/Verkaufen/Tauschen) und ExodusQuests (Quest-Belohnungen, Stashes).
 */
modded class XCPTraderTransactionService {
	protected static ref XCPTraderTransactionResponse ExecuteBuy(PlayerBase player, XCPTraderConfigEntry traderConfig, XCPTraderTransactionRequest request) {
		ref XCPTraderTransactionResponse response = super.ExecuteBuy(player, traderConfig, request);
		EET_TraderHook.Report(player, traderConfig, "buy", request, response);
		return response;
	}

	protected static ref XCPTraderTransactionResponse ExecuteSell(PlayerBase player, XCPTraderConfigEntry traderConfig, XCPTraderTransactionRequest request) {
		ref XCPTraderTransactionResponse response = super.ExecuteSell(player, traderConfig, request);
		EET_TraderHook.Report(player, traderConfig, "sell", request, response);
		return response;
	}

	protected static ref XCPTraderTransactionResponse ExecuteBarter(PlayerBase player, XCPTraderConfigEntry traderConfig, XCPTraderTransactionRequest request) {
		ref XCPTraderTransactionResponse response = super.ExecuteBarter(player, traderConfig, request);
		EET_TraderHook.Report(player, traderConfig, "barter", request, response);
		return response;
	}
}

class EET_TraderHook {
	static void Report(PlayerBase player, XCPTraderConfigEntry traderConfig, string mode, XCPTraderTransactionRequest request, XCPTraderTransactionResponse response) {
		if (!response || !response.success || !traderConfig)
			return;
		EET_Tracker.Get().RecordTrade(player, traderConfig.trader_id, mode, request);
	}
}

modded class EQ_QuestManager {
	override void CompleteQuest(PlayerBase player, EQ_QuestBase quest) {
		bool alreadyClosed = quest.IsClosed();
		EQ_QuestConfig cfg = quest.GetConfig();
		super.CompleteQuest(player, quest);
		if (!alreadyClosed)
			EET_Tracker.Get().RecordQuestFinished(player, cfg);
	}
}

modded class EQ_Objective_LootStash {
	protected override void HandleItemTaken(PlayerBase player, ItemBase item) {
		bool isStashItem = item && m_StashItems.Find(item) != -1 && player == m_Player;
		super.HandleItemTaken(player, item);
		if (isStashItem) {
			string stashId = GetParam("StashId");
			EET_Tracker.Get().Add(player, "stash", stashId, "", "", 1, 0);
			EET_Tracker.Get().LogEvent("stash", player, EET_Tracker.J("stash", stashId) + "," + EET_Tracker.J("item", item.GetType()));
		}
	}
}
