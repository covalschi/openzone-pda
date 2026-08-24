// Серверна половина КПК: реєструє свої сторінки, читає профілі пристроїв і
// підміняє ядерну заглушку доступу справжньою перевіркою.

class OZ_PdaHandlerDevice : OZ_PageHandler
{
    override string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;
        error = "STR_OZ_ERR_UNKNOWN_OP";

        if (op == "status")
            return Status(sender, ok, error);

        if (op == "unlock")
            return Unlock(json, sender, ok, error);

        if (op == "autolock")
            return AutoLock(json, sender, ok, error);

        if (op == "power")
            return Power(json, sender, ok, error);

        if (op == "setpin")
            return SetPin(json, sender, ok, error);

        return "";
    }

    private string Status(PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
        if (!pda)
        {
            error = "STR_OZ_ERR_NO_DEVICE";
            return "";
        }

        OZ_PdaProfile prof = OZ_PdaProfiles.ForClass(pda.GetType());
        if (!prof)
        {
            // Пристрій є, а профілю під нього немає: адмін прибрав його з
            // Profiles.json або переплутав класнейм. Кажемо про це прямо.
            OZ_Log.Warn("no device profile for class " + pda.GetType());
            error = "STR_OZ_ERR_NO_PROFILE";
            return "";
        }

        PlayerBase player = OZ_PdaLookup.PlayerOf(sender);
        OZ_PlayerData pd = OZ_PlayerStore.Load(sender.GetPlainId());
        pda.OZ_EvaluateLock(prof.LockAfterMinutes);

        // ЧИСТИЙ пристрій прив'язується до того, хто перший його відкрив.
        // Без цього КПК без піна назавжди лишався б офлайном: сесію відкривав
        // тільки успішний ввід коду, а вводити на ньому нічого.
        //
        // Умова саме «сесії НЕМАЄ ЖОДНОЇ», а не «сесія не моя»: інакше чужий
        // відімкнений КПК ставав би твоїм від одного погляду, і «капсула
        // часу» перетворилась би на просту передачу власності.
        if (pda.OZ_IsUnlocked() && !pda.OZ_HasAnySession())
            pda.OZ_OpenSession(sender.GetPlainId(), pd.SessionEpoch);

        OZ_PdaDeviceStatus st = new OZ_PdaDeviceStatus();

        st.ClassName   = pda.GetType();
        st.ProfileId   = prof.Id;
        st.DisplayName = prof.DisplayName;
        st.ModuleSlots = prof.ModuleSlots;
        st.LockAfterMinutes = prof.LockAfterMinutes;

        // Адреса пристрою для клієнта. GetNetworkID віддає id двома int'ами,
        // і клієнт піднімає по них ту саму сутність через GetObjectByNetworkId.
        int netLow;
        int netHigh;
        pda.GetNetworkID(netLow, netHigh);
        st.NetLow  = netLow;
        st.NetHigh = netHigh;
        st.InHands = (player != null && player.GetItemInHands() == pda);

        for (int i = 0; i < prof.Pages.Count(); i++)
        {
            // На клієнт їдуть лише ті сторінки, які СПРАВДІ зареєстровані:
            // намалювати вкладку, за якою нікого немає, гірше, ніж не
            // намалювати її зовсім.
            if (OZ_PageRegistry.Has(prof.Pages[i]))
                st.Pages.Insert(prof.Pages[i]);
        }

        st.Powered    = pda.OZ_IsOn();
        st.HasBattery = pda.OZ_HasBattery();
        st.Charge01   = pda.OZ_Charge01();

        for (int b = 0; b < OZ_PdaConst.MODULE_SLOTS_MAX; b++)
        {
            OZ_BayInfo bay = new OZ_BayInfo();
            bay.Index   = b;
            bay.Visible = (b < prof.ModuleSlots);

            string cls = pda.OZ_ModuleClass(b);
            if (cls != "")
            {
                bay.ClassName = cls;
                OZ_ModuleSpec spec = OZ_PdaHardware.ModuleFor(cls);
                if (spec)
                {
                    bay.Display = spec.DisplayName;
                    bay.Kind    = spec.Kind;
                }
            }
            st.Bays.Insert(bay);
        }

        st.CarrierClass = pda.OZ_CarrierClass();
        if (st.CarrierClass != "")
        {
            OZ_CarrierSpec cs = OZ_PdaHardware.CarrierFor(st.CarrierClass);
            if (cs)
                st.CarrierKind = cs.DefaultKind;

            OZ_DataCarrier_Base carrier = OZ_DataCarrier_Base.Cast(pda.OZ_Attached(OZ_PdaConst.SLOT_CARRIER));
            if (carrier)
            {
                st.CarrierWritten = carrier.OZ_IsWritten();
                if (carrier.OZ_Kind() != "")
                    st.CarrierKind = carrier.OZ_Kind();
            }
        }

        st.HasPin    = pda.OZ_HasPin();
        st.Unlocked  = pda.OZ_IsUnlocked();
        st.AutoLock  = pda.OZ_AutoLock();
        st.ForceAutoLock = prof.ForceAutoLock;
        st.LockedOut = pda.OZ_IsLockedOut(sender.GetPlainId());

        st.Online      = pda.OZ_IsOnline(pd.SessionEpoch);
        st.SessionMine = pda.OZ_HasSession(sender.GetPlainId(), pd.SessionEpoch);
        if (!st.Online)
            st.SnapshotAt = pda.OZ_SnapshotAt();

        st.DiscordLinked = (pd.DiscordId != "");
        st.FirstSeen     = pd.FirstSeen;

        // Радіацію питаємо ЛИШЕ якщо є чим міряти. Питати те, чого нема чим
        // виміряти, і малювати відповідь -- це вигадувати цифри.
        bool wantAmbient = pda.OZ_HasModuleKind(OZ_PdaConst.MOD_RADIOMETER);
        bool wantDose    = pda.OZ_HasModuleKind(OZ_PdaConst.MOD_DOSIMETER);
        if (wantAmbient || wantDose)
        {
            OZ_RadiationReading rr = OZ_PdaRadiation.Read(player, wantAmbient, wantDose);
            st.HasRadiationProvider = rr.HasProvider;
            st.AmbientUSvH = rr.AmbientUSvH;
            st.DoseUSv     = rr.DoseUSv;
            st.DoseWarnUSv = rr.DoseWarnUSv;
        }

        string outJson;
        string err;
        if (!JsonFileLoader<OZ_PdaDeviceStatus>.MakeData(st, outJson, err, false))
        {
            OZ_Log.Error("device status serialise failed: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        ok = true;
        error = "";
        return outJson;
    }

    private string Unlock(string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
        if (!pda)
        {
            error = "STR_OZ_ERR_NO_DEVICE";
            return "";
        }

        OZ_PdaPinAttempt att;
        string err;
        if (!JsonFileLoader<OZ_PdaPinAttempt>.LoadData(json, att, err))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        if (!pda.OZ_TryUnlock(sender.GetPlainId(), att.Pin))
        {
            // Скільки спроб лишилось -- НЕ кажемо. Це підказка тому, хто
            // підбирає, і жодної користі власнику.
            error = "STR_OZ_ERR_BAD_PIN";
            return "";
        }

        // Відімкнув -- значить сесія на цьому пристрої тепер його.
        OZ_PlayerData pd = OZ_PlayerStore.Load(sender.GetPlainId());
        pda.OZ_OpenSession(sender.GetPlainId(), pd.SessionEpoch);

        ok = true;
        error = "";
        return "";
    }

    // Зміна коду. Щоб змінити пін, його треба ЗНАТИ -- пристрій не питає,
    // хто ти, він питає старий код. Порожній новий код означає «зняти пін».
    private string SetPin(string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
        if (!pda)
        {
            error = "STR_OZ_ERR_NO_DEVICE";
            return "";
        }

        OZ_PdaPinChange ch;
        string err;
        if (!JsonFileLoader<OZ_PdaPinChange>.LoadData(json, ch, err))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        if (!pda.OZ_SetPin(sender.GetPlainId(), ch.OldPin, ch.NewPin))
        {
            // Скільки спроб лишилось -- НЕ кажемо, з тієї ж причини, що й при
            // відмиканні: це підказка тому, хто підбирає.
            error = "STR_OZ_ERR_BAD_PIN";
            return "";
        }

        // Задав код -- значить сесія на цьому пристрої тепер його.
        OZ_PlayerData pd = OZ_PlayerStore.Load(sender.GetPlainId());
        if (!pda.OZ_HasAnySession())
            pda.OZ_OpenSession(sender.GetPlainId(), pd.SessionEpoch);

        ok = true;
        error = "";
        return "";
    }

    // Живлення. Той самий важіль, що й ванільна дія з рук -- але через сервер:
    // клієнт просить, сервер вирішує й називає причину відмови.
    private string Power(string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
        if (!pda)
        {
            error = "STR_OZ_ERR_NO_DEVICE";
            return "";
        }

        OZ_PdaFlagOp flag;
        string err;
        if (!JsonFileLoader<OZ_PdaFlagOp>.LoadData(json, flag, err))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        string why = pda.OZ_SetPower(flag.Value);
        if (why != "")
        {
            error = why;
            return "";
        }

        ok = true;
        error = "";
        return "";
    }

    private string AutoLock(string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
        if (!pda)
        {
            error = "STR_OZ_ERR_NO_DEVICE";
            return "";
        }

        OZ_PdaProfile prof = OZ_PdaProfiles.ForClass(pda.GetType());
        if (!prof)
        {
            error = "STR_OZ_ERR_NO_PROFILE";
            return "";
        }

        OZ_PdaFlagOp flag;
        string err;
        if (!JsonFileLoader<OZ_PdaFlagOp>.LoadData(json, flag, err))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        if (!pda.OZ_SetAutoLock(flag.Value, prof.ForceAutoLock))
        {
            error = "STR_OZ_ERR_REFUSED";
            return "";
        }

        ok = true;
        error = "";
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
