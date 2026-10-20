modded class MissionServer {
	override void OnInit() {
		super.OnInit();
		EET_Tracker.Get().Init();
	}

	override void OnClientDisconnectedEvent(PlayerIdentity identity, PlayerBase player, int logoutTime, bool authFailed) {
		EET_Tracker.Get().OnDisconnect(player, identity);
		super.OnClientDisconnectedEvent(identity, player, logoutTime, authFailed);
	}

	override void OnMissionFinish() {
		EET_Tracker.Get().SaveAll();
		super.OnMissionFinish();
	}
}
