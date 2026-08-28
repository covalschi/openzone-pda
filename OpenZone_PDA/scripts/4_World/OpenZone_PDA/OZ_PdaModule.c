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

        if (op == "crack")
            return Crack(sender, ok, error);

        if (op == "sealed")
            return Sealed(sender, ok, error);

        if (op == "carrier_write")
            return CarrierWrite(json, sender, ok, error);

        if (op == "carrier_read")
            return CarrierRead(sender, ok, error);

        if (op == "carrier_import")
            return CarrierImport(sender, ok, error);

        if (op == "carrier_erase")
            return CarrierErase(sender, ok, error);

        return "";
    }

    // ------------------------------------------------------------ носій
    //
    // Чип -- фізична річ для фізичного обміну: записав мітки, віддав у руки,
    // той вставив і забрав собі. Пейлоад живе НА ПРЕДМЕТІ (CF ModStorage),
    // переживає рестарти і їде з чипом у кишені, у сховку, на трупі.
    //
    // Ворота доступу безкоштовні: ці опи не входять у винятки живлення й
    // замка, тож OZ_PdaAccess вже вимагає ввімкнений і відімкнений пристрій.

    private OZ_DataCarrier_Base CarrierOf(PlayerIdentity sender, out string error)
    {
        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
        if (!pda)
        {
            error = "STR_OZ_ERR_NO_DEVICE";
            return null;
        }

        OZ_DataCarrier_Base c = OZ_DataCarrier_Base.Cast(pda.OZ_Attached(OZ_PdaConst.SLOT_CARRIER));
        if (!c)
        {
            error = "STR_OZ_ERR_NO_CARRIER";
            return null;
        }

        return c;
    }

    private string CarrierWrite(string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_CarrierWriteOp opw;
        string err;
        if (!JsonFileLoader<OZ_CarrierWriteOp>.LoadData(json, opw, err) || !opw)
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_DataCarrier_Base c = CarrierOf(sender, error);
        if (!c)
            return "";

        // Перше справжнє застосування Writable з Hardware.json: чип, який
        // конфіг оголосив лише читаним, не перезаписується ніколи. Клас БЕЗ
        // запису в таблиці -- теж замок: клієнт такому кнопок не малює, і
        // підроблений запит не має права пройти там, де чесний не пройде.
        OZ_CarrierSpec spec = OZ_PdaHardware.CarrierFor(c.GetType());
        if (!spec || !spec.Writable)
        {
            error = "STR_OZ_ERR_CARRIER_LOCKED";
            return "";
        }

        // Перезапис ІНШИМ родом -- лише через явне стирання: мітки живуть
        // тільки на пристрої й чипі, і мовчазна заміна книжкою нотаток
        // губила б їх безповоротно. Свій род поверх себе -- звичайне
        // оновлення, воно лишається в один дотик.
        if (c.OZ_IsWritten() && c.OZ_Kind() != opw.Kind)
        {
            error = "STR_OZ_ERR_CARRIER_KIND";
            return "";
        }

        if (opw.Kind == "markers")
        {
            OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
            string payload = pda.OZ_MarkersJson();
            if (payload == "")
                payload = "{\"Version\":1,\"Items\":[]}";

            int cnt = 0;
            OZ_MarkerList pl;
            string perr;
            if (JsonFileLoader<OZ_MarkerList>.LoadData(payload, pl, perr) && pl && pl.Items)
                cnt = pl.Items.Count();

            c.OZ_Write("markers", payload, cnt);
            ok = true;
            error = "";
            return "";
        }

        if (opw.Kind == "notes")
        {
            // Записки живуть у Discord -- їх спершу треба ПРИНЕСТИ. Відповідь
            // відкладена, як усе, що ходить через міст. Alive, не IsRunning:
            // друге означає лише «опит увімкнено в налаштуваннях», і мертвий
            // міст висів би мовчки замість чесної відмови.
            if (!OZ_BridgeClient.Alive())
            {
                error = "STR_OZ_ERR_NO_BRIDGE";
                return "";
            }

            string uid = sender.GetPlainId();
            OZ_NotesAskList a = new OZ_NotesAskList();
            a.Uid = uid;

            string letter;
            if (!JsonFileLoader<OZ_NotesAskList>.MakeData(a, letter, err, false))
            {
                error = "STR_OZ_ERR_INTERNAL";
                return "";
            }

            OZ_BridgeClient.Call("v1/notes/list", letter, new OZ_CarrierNotesReply(uid, c));
            error = OZ_Const.DEFER;
            return "";
        }

        error = "STR_OZ_ERR_INTERNAL";
        return "";
    }

    private string CarrierRead(PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_DataCarrier_Base c = CarrierOf(sender, error);
        if (!c)
            return "";

        if (!c.OZ_IsWritten())
        {
            error = "STR_OZ_ERR_CARRIER_BLANK";
            return "";
        }

        // Тіло як є: клієнт розбере за Kind. Що на чипі -- те й видно, і
        // саме тому крадений КПК з чужим чипом читає чужі мітки: така ціна
        // фізичного носія, і вона навмисна.
        OZ_CarrierView v = new OZ_CarrierView();
        v.Kind    = c.OZ_Kind();
        v.Payload = c.OZ_Payload();

        string outJson;
        string err;
        if (!JsonFileLoader<OZ_CarrierView>.MakeData(v, outJson, err, false))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        ok = true;
        error = "";
        return outJson;
    }

    private string CarrierImport(PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_DataCarrier_Base c = CarrierOf(sender, error);
        if (!c)
            return "";

        if (!c.OZ_IsWritten())
        {
            error = "STR_OZ_ERR_CARRIER_BLANK";
            return "";
        }

        if (c.OZ_Kind() == "markers")
        {
            OZ_MarkerList incoming;
            string err;
            if (!JsonFileLoader<OZ_MarkerList>.LoadData(c.OZ_Payload(), incoming, err) || !incoming || !incoming.Items)
            {
                error = "STR_OZ_ERR_INTERNAL";
                return "";
            }

            OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
            OZ_PdaProfile prof = OZ_PdaProfiles.ForClass(pda.GetType());

            OZ_MarkerList mine = new OZ_MarkerList();
            string raw = pda.OZ_MarkersJson();
            if (raw != "")
            {
                OZ_MarkerList parsed;
                if (JsonFileLoader<OZ_MarkerList>.LoadData(raw, parsed, err) && parsed && parsed.Items)
                    mine = parsed;
            }

            int limit = 0;
            if (prof && prof.Limits)
                limit = prof.Limits.Markers;

            // limit <= 0 -- зіпсований конфіг. Сторінка карти в цьому стані
            // відмовляє СТАВИТИ, тож імпорт поводиться так само, а не читає
            // той самий нуль як «безліміт».
            if (limit <= 0 || mine.Items.Count() >= limit)
            {
                error = "STR_OZ_ERR_MARKERS_FULL";
                return "";
            }

            int total = incoming.Items.Count();
            int taken = 0;
            for (int i = 0; i < total; i++)
            {
                if (mine.Items.Count() >= limit)
                    break;

                OZ_MapMarker m = incoming.Items[i];

                // Той самий санітар, що й у marker_add: чуже походження --
                // не привілей, а межі в чипа ніхто не питав.
                m.Name = OZ_Text.Clip(m.Name, OZ_PdaConst.MARKER_NAME_MAX);
                m.Desc = OZ_Text.Clip(m.Desc, OZ_PdaConst.MARKER_DESC_MAX);

                // Id карбуємо ЗАНОВО: чужі id зіткнулись би з нашими, і
                // видалення по id зносило б не ту мітку.
                s_CarrierSeq++;
                m.Id = OZ_Time.NowUtc() + "#c" + s_CarrierSeq.ToString();
                mine.Items.Insert(m);
                taken++;
            }

            string outJson;
            if (!JsonFileLoader<OZ_MarkerList>.MakeData(mine, outJson, err, false))
            {
                error = "STR_OZ_ERR_INTERNAL";
                return "";
            }

            pda.OZ_SetMarkersJson(outJson);
            OZ_Log.Info("carrier: imported " + taken.ToString() + "/" + total.ToString() + " marker(s) for " + sender.GetPlainId());

            // Скільки влізло -- у відповідь: «Done.» при 25 з 40 було б
            // брехнею, а гравець мусить знати, що хвіст лишився на чипі.
            OZ_CarrierTaken t = new OZ_CarrierTaken();
            t.Taken = taken;
            t.Total = total;

            string tj;
            if (!JsonFileLoader<OZ_CarrierTaken>.MakeData(t, tj, err, false))
                tj = "";

            ok = true;
            error = "";
            return tj;
        }

        if (c.OZ_Kind() == "notes")
        {
            if (!OZ_BridgeClient.Alive())
            {
                error = "STR_OZ_ERR_NO_BRIDGE";
                return "";
            }

            OZ_NoteBook book;
            string err2;
            if (!JsonFileLoader<OZ_NoteBook>.LoadData(c.OZ_Payload(), book, err2) || !book || !book.Notes)
            {
                error = "STR_OZ_ERR_INTERNAL";
                return "";
            }

            // Кожна записка -- окремий лист мостові з ПОРОЖНІМ Id: це
            // «створи мені таку саму», а не «редагуй чужу». Стеля та сама,
            // що й у книжки (NOTES_MAX): один дотик не має права висипати в
            // Discord більше, ніж книжка взагалі вміщає.
            string uid = sender.GetPlainId();
            int wanted = book.Notes.Count();
            int cap = wanted;
            if (cap > OZ_PdaConst.NOTES_MAX)
                cap = OZ_PdaConst.NOTES_MAX;

            int sent = 0;
            for (int k = 0; k < cap; k++)
            {
                OZ_NotesAskSave a = new OZ_NotesAskSave();
                a.Uid   = uid;
                a.Id    = "";
                a.Title = OZ_Text.Clip(book.Notes[k].Title, OZ_PdaConst.NOTE_TITLE_MAX);
                a.Body  = OZ_Text.Clip(book.Notes[k].Body, OZ_PdaConst.NOTE_BODY_MAX);
                a.Name  = sender.GetName();

                string letter;
                if (!JsonFileLoader<OZ_NotesAskSave>.MakeData(a, letter, err2, false))
                    continue;

                OZ_BridgeClient.Call("v1/notes/save", letter, new OZ_NotesReply(uid, "carrier_note", false));
                sent++;
            }

            OZ_Log.Info("carrier: importing " + sent.ToString() + "/" + wanted.ToString() + " note(s) for " + uid);

            OZ_CarrierTaken tn = new OZ_CarrierTaken();
            tn.Taken = sent;
            tn.Total = wanted;

            string tnj;
            if (!JsonFileLoader<OZ_CarrierTaken>.MakeData(tn, tnj, err2, false))
                tnj = "";

            ok = true;
            error = "";
            return tnj;
        }

        error = "STR_OZ_ERR_INTERNAL";
        return "";
    }

    private string CarrierErase(PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_DataCarrier_Base c = CarrierOf(sender, error);
        if (!c)
            return "";

        OZ_CarrierSpec spec = OZ_PdaHardware.CarrierFor(c.GetType());
        if (!spec || !spec.Writable)
        {
            error = "STR_OZ_ERR_CARRIER_LOCKED";
            return "";
        }

        // Симетрія з імпортом і читанням: порожній чип стирати нема чого,
        // і «зроблено» на ніщо було б звітом про неіснуючу роботу.
        if (!c.OZ_IsWritten())
        {
            error = "STR_OZ_ERR_CARRIER_BLANK";
            return "";
        }

        c.OZ_Erase();
        ok = true;
        error = "";
        return "";
    }

    private static int s_CarrierSeq = 0;

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

        // Замок тут НЕ рахуємо: OZ_PdaAccess.Check уже покликав
        // OZ_EvaluateLock мікросекундами раніше, на цьому ж самому запиті
        // (OZ_PdaAccess.c:42) -- і робить це для КОЖНОЇ сторінки, а не лише
        // для цієї. Другий виклик тут був мертвою роботою, і він же створював
        // хибне враження, ніби автозамок тримається на секундному опитуванні
        // сторінки «Пристрій». Не тримається: він у воротах.

        // Ліниво, як і замок: рахунок дешифратора добігає саме тоді, коли на
        // пристрій дивляться.
        pda.OZ_EvaluateCrack();

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

        // І сторінки, які приносить ЗАЛІЗО.
        //
        // Профіль описує ПРИСТРІЙ, а не те, що в нього вставили, тож без
        // цього договір EnablesPages лишався б обіцянкою, якої КПК не
        // виконує. Саме так і сталося з рацією: гейт операцій уже питав
        // модулі (OZ_PdaAccess.ModuleEnables), а перелік вкладок -- ні, і
        // вставлена плата працювала б, якби до неї було як дійти.
        for (int m = 0; m < OZ_PdaConst.MODULE_SLOTS_MAX; m++)
        {
            string mcls = pda.OZ_ModuleClass(m);
            if (mcls == "")
                continue;

            OZ_ModuleSpec mspec = OZ_PdaHardware.ModuleFor(mcls);
            if (!mspec || !mspec.EnablesPages)
                continue;

            for (int e = 0; e < mspec.EnablesPages.Count(); e++)
            {
                string extra = mspec.EnablesPages[e];
                if (!OZ_PageRegistry.Has(extra))
                    continue;
                if (st.Pages.Find(extra) != -1)
                    continue;
                st.Pages.Insert(extra);
            }
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
            {
                st.CarrierKind     = cs.DefaultKind;
                st.CarrierWritable = cs.Writable;
                st.CarrierDisplay  = cs.DisplayName;
            }

            OZ_DataCarrier_Base carrier = OZ_DataCarrier_Base.Cast(pda.OZ_Attached(OZ_PdaConst.SLOT_CARRIER));
            if (carrier)
            {
                st.CarrierWritten = carrier.OZ_IsWritten();
                st.CarrierCount   = carrier.OZ_Count();
                if (carrier.OZ_Kind() != "")
                    st.CarrierKind = carrier.OZ_Kind();
            }
        }

        st.HasPin    = pda.OZ_HasPin();
        st.Unlocked  = pda.OZ_IsUnlocked();
        st.AutoLock  = pda.OZ_AutoLock();
        st.ForceAutoLock = prof.ForceAutoLock;
        st.LockedOut = pda.OZ_IsLockedOut(sender.GetPlainId());

        st.Sealed       = pda.OZ_IsSealed();
        st.HasDecryptor = pda.OZ_HasDecryptor();
        st.Cracking     = pda.OZ_IsCracking();
        st.CrackLeftSec = pda.OZ_CrackLeftSec();

        st.Online      = pda.OZ_IsOnline(pd.SessionEpoch);
        st.SessionMine = pda.OZ_HasSession(sender.GetPlainId(), pd.SessionEpoch);
        if (!st.Online)
            st.SnapshotAt = pda.OZ_SnapshotAt();

        st.DiscordLinked = (pd.DiscordId != "");
        st.FirstSeen     = pd.FirstSeen;

        // Радіацію питаємо ЛИШЕ якщо є чим міряти. Питати те, чого нема чим
        // виміряти, і малювати відповідь -- це вигадувати цифри.
        // ...і лише якщо пристрій УВІМКНЕНО. Замірник живиться від нього, і
        // вимкнений КПК, який показує поточний фон, -- це не прилад.
        bool wantAmbient = st.Powered && pda.OZ_HasModuleKind(OZ_PdaConst.MOD_RADIOMETER);
        bool wantDose    = st.Powered && pda.OZ_HasModuleKind(OZ_PdaConst.MOD_DOSIMETER);
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

    // Що можна сказати про ЗАМКНЕНИЙ пристрій, не відмикаючи його.
    //
    // Рівно чотири речі, і жодна з них нічого не видає: чи він запечатаний,
    // чи є чим його зламати, чи вже ламають і скільки лишилось. Усе решта --
    // ім'я, профіль, вміст -- лишається за замком, бо саме за цим замок і є.
    private string Sealed(PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
        if (!pda)
        {
            error = "STR_OZ_ERR_NO_DEVICE";
            return "";
        }

        pda.OZ_EvaluateCrack();

        OZ_PdaDeviceStatus st = new OZ_PdaDeviceStatus();
        st.Sealed       = pda.OZ_IsSealed();
        st.HasDecryptor = pda.OZ_HasDecryptor();
        st.Cracking     = pda.OZ_IsCracking();
        st.CrackLeftSec = pda.OZ_CrackLeftSec();

        string outJson;
        string err;
        if (!JsonFileLoader<OZ_PdaDeviceStatus>.MakeData(st, outJson, err, false))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        ok = true;
        error = "";
        return outJson;
    }

    // Почати злам запечатаного пристрою.
    private string Crack(PlayerIdentity sender, out bool ok, out string error)
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

        string why = pda.OZ_StartCrack(prof.CrackSeconds);
        if (why != "")
        {
            error = why;
            return "";
        }

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

        // Дерево каталогів профілю -- ПЕРШИМ рядком, до будь-якого читання
        // чи запису. Ядро будує його у своєму OnMissionStart, але порядок
        // CF-модулів не гарантований, і на цьому ж стенді він уже підводив:
        // рація відпрацювала раніше за КПК. EnsureTree ідемпотентна, тож
        // зайвий виклик коштує нічого, а відсутній коштує конфігів.
        OZ_Json.EnsureTree();

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

        OZ_PageRegistry.Register(OZ_PdaConst.PAGE_CONTACTS,
                                 "#STR_OZ_PAGE_CONTACTS",
                                 "set:oz_pda image:contacts",
                                 new OZ_PdaHandlerContacts());

        OZ_PageRegistry.Register(OZ_PdaConst.PAGE_NOTES,
                                 "#STR_OZ_PAGE_NOTES",
                                 "set:oz_pda image:notes",
                                 new OZ_PdaHandlerNotes());

        OZ_PageRegistry.Register(OZ_PdaConst.PAGE_MAP,
                                 "#STR_OZ_PAGE_MAP",
                                 "set:oz_pda image:map",
                                 new OZ_PdaHandlerMap());

        OZ_PageRegistry.Register(OZ_PdaConst.PAGE_CHAT,
                                 "#STR_OZ_PAGE_CHAT",
                                 "set:oz_pda image:chat",
                                 new OZ_PdaHandlerChat());

        OZ_PageRegistry.Register(OZ_PdaConst.PAGE_NEWS,
                                 "#STR_OZ_PAGE_NEWS",
                                 "set:oz_pda image:news",
                                 new OZ_PdaHandlerNews());

        // Розмови живуть у Discord, тож на диску їм каталогу не треба -- а
        // ось вухо для вхідних рядків треба. Підписка не залежить від того,
        // чи вже стартував міст: порядок модулів CF не гарантований, а мапа
        // приймачів однаково питається на кожну пачку.
        OZ_BridgeClient.Subscribe("chat", new OZ_ChatSink());

        OZ_PdaProfiles.ServerLoad();
        OZ_PdaHardware.ServerLoad();
        OZ_Factions.ServerLoad();

        // Ядро пускало всі сторінки, бо пристроїв не має. Тепер вирішує той,
        // хто їх приносить.
        OZ_PageAccess.Bind(new OZ_PdaAccess());

        CheckSlots();

        string summary = "pda loaded: profiles=" + OZ_PdaProfiles.Count().ToString();
        summary += " pages=" + OZ_PageRegistry.Count().ToString();
        summary += " modules=" + OZ_PdaHardware.ModuleCount().ToString();
        summary += " factions=" + OZ_Factions.Count().ToString();
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
