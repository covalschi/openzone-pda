// Серверна половина КПК: реєструє свої сторінки, читає профілі пристроїв і
// підміняє ядерну заглушку доступу справжньою перевіркою.

class OZ_PdaHandlerDevice : OZ_PageHandler
{
    override string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;
        error = "STR_OZ_ERR_UNKNOWN_OP";

        if (op == "status")
        {
            OZ_PlayerData d = OZ_PlayerStore.Load(sender.GetPlainId());

            OZ_PdaDeviceStatus st = new OZ_PdaDeviceStatus();
            st.DiscordLinked = (d.DiscordId != "");
            st.FirstSeen     = d.FirstSeen;

            string outJson;
            string err;
            if (JsonFileLoader<OZ_PdaDeviceStatus>.MakeData(st, outJson, err, false))
            {
                ok = true;
                error = "";
                return outJson;
            }

            OZ_Log.Error("device status serialise failed: " + err);
            error = "STR_OZ_ERR_INTERNAL";
        }

        return "";
    }
}

class OZ_PdaHandlerQuests : OZ_PageHandler
{
    override string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;
        error = "STR_OZ_ERR_UNKNOWN_OP";

        if (op == "journal")
        {
            // Порожній журнал і ВІДСУТНІЙ журнал -- різні повідомлення для
            // гравця. HasProvider розрізняє «завдань немає» і «на цьому
            // сервері квестового мода взагалі немає».
            OZ_QuestJournal j = OZ_PdaQuests.Collect(sender);

            string outJson;
            string err;
            if (JsonFileLoader<OZ_QuestJournal>.MakeData(j, outJson, err, false))
            {
                ok = true;
                error = "";
                return outJson;
            }

            OZ_Log.Error("quest journal serialise failed: " + err);
            error = "STR_OZ_ERR_INTERNAL";
        }

        return "";
    }
}

[CF_RegisterModule(OZ_PdaModule)]
class OZ_PdaModule : CF_ModuleWorld
{
    override void OnInit()
    {
        super.OnInit();
        EnableMissionStart();
    }

    override void OnMissionStart(Class sender, CF_EventArgs args)
    {
        super.OnMissionStart(sender, args);

        if (!GetGame().IsServer())
            return;

        // Спочатку СТОРІНКИ, потім профілі: Validate() профілів звіряє свій
        // список Pages з реєстром, і на порожньому реєстрі виплюнув би
        // попередження на кожен рядок.
        OZ_PageRegistry.Register(OZ_PdaConst.PAGE_DEVICE,
                                 "#STR_OZ_PAGE_DEVICE",
                                 "set:oz_pda image:device",
                                 new OZ_PdaHandlerDevice());

        OZ_PageRegistry.Register(OZ_PdaConst.PAGE_QUESTS,
                                 "#STR_OZ_PAGE_QUESTS",
                                 "set:oz_pda image:quests",
                                 new OZ_PdaHandlerQuests());

        OZ_PdaProfiles.ServerLoad();
        OZ_PdaHardware.ServerLoad();

        // Ядро пускало всі сторінки, бо пристроїв не має. Тепер вирішує той,
        // хто їх приносить.
        OZ_PageAccess.Bind(new OZ_PdaAccess());

        CheckSlots();

        string summary = "pda loaded: profiles=" + OZ_PdaProfiles.Count().ToString();
        summary += " pages=" + OZ_PageRegistry.Count().ToString();
        summary += " modules=" + OZ_PdaHardware.ModuleCount().ToString();
        summary += " carriers=" + OZ_PdaHardware.CarrierCount().ToString();
        OZ_Log.Info(summary);
    }

    // Слот з друкарською помилкою в імені -- класична мовчазна поломка: конфіг
    // парситься, предмет спавниться, а вкласти в нього нічого не можна, і в
    // лозі про це ані слова. Ловимо на буті, а не в грі.
    private void CheckSlots()
    {
        CheckSlot(OZ_PdaConst.SLOT_BATTERY);
        CheckSlot(OZ_PdaConst.SLOT_CARRIER);
        for (int i = 0; i < OZ_PdaConst.MODULE_SLOTS_MAX; i++)
            CheckSlot(OZ_PdaConst.ModuleSlot(i));
    }

    private void CheckSlot(string name)
    {
        int id = InventorySlots.GetSlotIdFromString(name);
        if (id == -1)
        {
            OZ_Log.Warn("slot \"" + name + "\" does not resolve - check CfgSlots and attachments[]");
            return;
        }
        OZ_Log.Dbg("slot " + name + " -> id " + id.ToString());
    }
}
