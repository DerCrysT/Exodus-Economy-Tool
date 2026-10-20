class CfgPatches {
	class ExodusEconomyTracker {
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		// Hart abhaengig von Quest- und Trader-Mod, weil Quests/Handel direkt eingehakt werden.
		requiredAddons[] = {"DZ_Data","DZ_Scripts","ExodusQuests","XCP_Trader_V2","XCP_Craftbenches"};
	};
};
class CfgMods {
	class ExodusEconomyTracker {
		dir = "ExodusEconomyTracker";
		name = "Exodus Economy Tracker";
		credits = "Exodus Team";
		author = "Exodus Team";
		type = "mod";
		dependencies[] = {"Game","World","Mission"};
		class defs {
			class gameScriptModule {
				value = "";
				files[] = {"ExodusEconomyTracker/scripts/3_Game"};
			};
			class worldScriptModule {
				value = "";
				files[] = {"ExodusEconomyTracker/scripts/4_World"};
			};
			class missionScriptModule {
				value = "";
				files[] = {"ExodusEconomyTracker/scripts/5_Mission"};
			};
		};
	};
};
