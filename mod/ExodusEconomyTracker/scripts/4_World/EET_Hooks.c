/**
 * EET_Hooks
 * Alle Vanilla-Einhakpunkte des Economy-Trackers. Die Hooks melden nur an EET_Tracker (keine Logik).
 */
modded class ItemBase {
	/** true, wenn ein Spieler das Item zuletzt abgelegt/verstaut hat: erneutes Aufheben zaehlt nicht als Loot. */
	bool m_EET_Handled;

	override void EEItemLocationChanged(notnull InventoryLocation oldLoc, notnull InventoryLocation newLoc) {
		super.EEItemLocationChanged(oldLoc, newLoc);
		if (GetGame().IsServer())
			EET_Tracker.Get().OnItemMoved(this, oldLoc, newLoc);
	}
}

modded class PlayerBase {
	override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef) {
		super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
		if (!GetGame().IsServer() || !damageResult)
			return;
		float amount = damageResult.GetDamage(dmgZone, "Health");
		if (amount > 0)
			EET_Tracker.Get().RecordHit(this, source, ammo, amount);
	}

	/** Bench-Crafting (XCP_Craftbenches): XCP_GiveResults wird nach erfolgreichem Verbrauch der Zutaten aufgerufen. */
	override void XCP_GiveResults(CraftItem itemData) {
		super.XCP_GiveResults(itemData);
		if (!GetGame().IsServer() || !itemData)
			return;
		string bench = "unknown";
		if (m_Workbench)
			bench = m_Workbench.GetType();
		EET_Tracker.Get().RecordBenchCraft(this, bench, itemData.RecipeName, itemData.Result, itemData.ResultCount, itemData.CraftComponents);
	}
}

modded class AnimatedActionBase {
	override void OnAnimationEvent(ActionData action_data) {
		bool wasExecuted = action_data.m_WasExecuted;
		super.OnAnimationEvent(action_data);
		if (!wasExecuted && action_data.m_WasExecuted && GetGame().IsServer() && !ActionContinuousBase.Cast(this))
			EET_ActionHook.Fire(this, action_data);
	}
}

modded class ActionContinuousBase {
	override void OnFinishProgress(ActionData action_data) {
		super.OnFinishProgress(action_data);
		EET_ActionHook.Fire(this, action_data);
	}
}

class EET_ActionHook {
	static void Fire(ActionBase action, ActionData action_data) {
		if (!GetGame().IsServer() || !action_data || !action_data.m_Player)
			return;
		Object target = null;
		if (action_data.m_Target)
			target = action_data.m_Target.GetObject();
		EET_Tracker.Get().RecordAction(action_data.m_Player, action.ClassName(), target, action_data.m_MainItem);
	}
}
