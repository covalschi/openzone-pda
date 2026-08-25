// Сторінка «Карта»: своя позиція та чужі маячки.
//
// КАРТА Й ТРАНСПОНДЕР -- РІЗНІ РЕЧІ, і залізо в них різне:
//
//   карту показує будь-який КПК -- це просто карта, і антена їй не потрібна;
//   маячки (свій і чужі) потребують АНТЕНИ, і в обидва боки однаково.
//
// Без антени сторінка чесно каже, що приймача немає, а не малює порожню карту
// з виглядом «нікого немає». Це різні відповіді, і плутати їх не можна.
//
// Радіус дає сам модуль антени (RangeM). Нуль означає «покриття задає щось
// інше» -- наприклад стаціонарна вежа з мода рації; тоді маячків не буде
// доти, доки те інше не з'явиться.

class OZ_PdaHandlerMap : OZ_PageHandler
{
    // Той самий прийом, що й у записках: час у секундах зіткнувся б на двох
    // мітках в одну секунду, тому до нього додається лічильник.
    private static int s_Seq = 0;

    override string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;
        error = "STR_OZ_ERR_UNKNOWN_OP";

        if (op == "state")
            return State(sender, ok, error);

        if (op == "transponder")
            return Transponder(json, sender, ok, error);

        if (op == "marker_add")
            return MarkerAdd(json, sender, ok, error);

        if (op == "marker_del")
            return MarkerDel(json, sender, ok, error);

        return "";
    }

    // --- мітки ---
    //
    // Читаються й пишуться на самому ПРИСТРОЇ. Через це межа в профілі щось
    // означає, а вкрадений КПК віддає чужі схованки -- обидва навмисно.

    private OZ_MarkerList LoadMarkers(OZ_PDA_Base pda)
    {
        OZ_MarkerList list = new OZ_MarkerList();

        string raw = pda.OZ_MarkersJson();
        if (raw == "")
            return list;

        string err;
        OZ_MarkerList parsed;
        if (!JsonFileLoader<OZ_MarkerList>.LoadData(raw, parsed, err) || !parsed)
        {
            // Зіпсований запис НЕ мовчимо й НЕ затираємо: гравець має знати,
            // що мітки не читаються, а адмін -- побачити це в лозі.
            OZ_Log.Warn("markers on " + pda.GetType() + " unreadable: " + err);
            return list;
        }

        if (!parsed.Items)
            parsed.Items = new array<ref OZ_MapMarker>();
        return parsed;
    }

    private bool SaveMarkers(OZ_PDA_Base pda, OZ_MarkerList list)
    {
        string json;
        string err;
        if (!JsonFileLoader<OZ_MarkerList>.MakeData(list, json, err, false))
        {
            OZ_Log.Error("markers serialise failed: " + err);
            return false;
        }

        pda.OZ_SetMarkersJson(json);
        return true;
    }

    private string MarkerAdd(string json, PlayerIdentity sender, out bool ok, out string error)
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

        OZ_MapMarker incoming;
        string err;
        if (!JsonFileLoader<OZ_MapMarker>.LoadData(json, incoming, err))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_MarkerList list = LoadMarkers(pda);

        int limit = MarkerLimit(prof);
        if (list.Items.Count() >= limit)
        {
            error = "STR_OZ_ERR_MARKERS_FULL";
            return "";
        }

        // Ім'я з клієнта чиститься завжди -- воно поїде в JSON і на карту.
        incoming.Name = MiscGameplayFunctions.SanitizeString(incoming.Name);
        if (incoming.Name.Length() > OZ_PdaConst.MARKER_NAME_MAX)
            incoming.Name = incoming.Name.Substring(0, OZ_PdaConst.MARKER_NAME_MAX);

        // Позицію бере СЕРВЕР з того, що прислав клієнт, але межі світу
        // перевіряє сам: мітка за краєм карти -- це не мітка.
        vector at = incoming.Pos.ToVector();
        if (!InsideWorld(at))
        {
            error = "STR_OZ_ERR_REFUSED";
            return "";
        }

        s_Seq++;
        incoming.Id  = OZ_Time.NowUtc();
        incoming.Id += "#" + s_Seq.ToString();
        incoming.Pos = at.ToString(false);

        list.Items.Insert(incoming);

        if (!SaveMarkers(pda, list))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        ok = true;
        error = "";
        return "";
    }

    private string MarkerDel(string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
        if (!pda)
        {
            error = "STR_OZ_ERR_NO_DEVICE";
            return "";
        }

        OZ_MarkerRef r;
        string err;
        if (!JsonFileLoader<OZ_MarkerRef>.LoadData(json, r, err))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_MarkerList list = LoadMarkers(pda);

        int at = -1;
        for (int i = 0; i < list.Items.Count(); i++)
        {
            if (list.Items[i].Id == r.Id)
            {
                at = i;
                break;
            }
        }

        if (at == -1)
        {
            error = "STR_OZ_ERR_NO_MARKER";
            return "";
        }

        list.Items.Remove(at);

        if (!SaveMarkers(pda, list))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        ok = true;
        error = "";
        return "";
    }

    private int MarkerLimit(OZ_PdaProfile prof)
    {
        if (!prof.Limits || prof.Limits.Markers <= 0)
            return 0;
        return prof.Limits.Markers;
    }

    // Межі світу питаємо в рушія, а не вписуємо число: карти бувають різні,
    // і 15360 вірне лише для Чернорусі.
    private bool InsideWorld(vector at)
    {
        if (at[0] <= 0 || at[2] <= 0)
            return false;

        int size = GetGame().GetWorld().GetWorldSize();
        if (size <= 0)
            return true;   // рушій не відповів -- не вигадуємо межі

        return at[0] <= size && at[2] <= size;
    }

    private string State(PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        PlayerBase me = OZ_PdaLookup.PlayerOf(sender);
        if (!me)
        {
            error = "STR_OZ_ERR_NO_DEVICE";
            return "";
        }

        string myUid = sender.GetPlainId();
        OZ_PlayerData mine = OZ_PlayerStore.Load(myUid);

        OZ_MapState st = new OZ_MapState();
        st.SelfPos         = me.GetPosition().ToString(false);
        st.TransponderMode = mine.TransponderMode;

        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
        float range = 0;
        if (pda)
        {
            range = AntennaRange(pda);
            st.HasAntenna    = (range > 0);
            st.AntennaRangeM = range;

            st.Markers = LoadMarkers(pda).Items;

            OZ_PdaProfile prof = OZ_PdaProfiles.ForClass(pda.GetType());
            if (prof)
                st.MarkerLimit = MarkerLimit(prof);
        }

        // Без антени слухати нема чим -- і це не порожній список, а окремий
        // стан, який сторінка показує словами.
        if (!st.HasAntenna)
        {
            return Serialise(st, ok, error);
        }

        array<Man> near = new array<Man>();
        OZ_Spatial.PlayersInRadius(me.GetPosition(), range, near);

        for (int i = 0; i < near.Count(); i++)
        {
            PlayerBase other = PlayerBase.Cast(near[i]);
            if (!other || other == me)
                continue;

            PlayerIdentity oid = other.GetIdentity();
            if (!oid)
                continue;

            string otherUid = oid.GetPlainId();
            OZ_PlayerData od = OZ_PlayerStore.Load(otherUid);

            if (!Broadcasts(od, myUid))
                continue;

            // Вести маячок теж потрібна АНТЕНА, і саме в того, хто веде.
            // Інакше вийшло б, що чужий пристрій світить позицію тим, чим
            // світити не може.
            OZ_PDA_Base theirs = OZ_PdaLookup.HeldBy(oid);
            if (!theirs || AntennaRange(theirs) <= 0)
                continue;

            OZ_MapBeacon b = new OZ_MapBeacon();
            b.Name = oid.GetName();
            b.Pos  = other.GetPosition().ToString(false);
            st.Beacons.Insert(b);
        }

        return Serialise(st, ok, error);
    }

    // Кому цей гравець показує свою позицію.
    private bool Broadcasts(OZ_PlayerData them, string toUid)
    {
        if (them.TransponderMode == "public")
            return true;

        if (them.TransponderMode == "contacts")
        {
            if (!them.TransponderTo)
                return false;
            return them.TransponderTo.Find(toUid) != -1;
        }

        if (them.TransponderMode == "friends")
        {
            if (!them.Friends)
                return false;
            return them.Friends.Find(toUid) != -1;
        }

        if (them.TransponderMode == "faction")
        {
            // Своїм по фракції. Одинакам цей режим не дає нічого, і це
            // правильно: одинак -- не фракція, а її відсутність.
            string theirs = OZ_Factions.Of(null, them.SteamId);
            if (theirs == "")
                return false;
            return OZ_Factions.Of(null, toUid) == theirs;
        }

        // "off" і будь-що незнайоме -- нікому.
        return false;
    }

    private float AntennaRange(OZ_PDA_Base pda)
    {
        for (int i = 0; i < OZ_PdaConst.MODULE_SLOTS_MAX; i++)
        {
            string cls = pda.OZ_ModuleClass(i);
            if (cls == "")
                continue;

            OZ_ModuleSpec spec = OZ_PdaHardware.ModuleFor(cls);
            if (!spec || spec.Kind != OZ_PdaConst.MOD_ANTENNA)
                continue;

            if (spec.RangeM > 0)
                return spec.RangeM;
        }
        return 0;
    }

    private string Serialise(OZ_MapState st, out bool ok, out string error)
    {
        string outJson;
        string err;
        if (!JsonFileLoader<OZ_MapState>.MakeData(st, outJson, err, false))
        {
            OZ_Log.Error("map state serialise failed: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        ok = true;
        error = "";
        return outJson;
    }

    private string Transponder(string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_TransponderOp t;
        string err;
        if (!JsonFileLoader<OZ_TransponderOp>.LoadData(json, t, err))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        // Перелік режимів закритий. Чуже слово в TransponderMode згодом
        // прочитав би Broadcasts() і не впізнав -- тобто маячок мовчав би, а
        // гравець вважав би, що веде.
        if (t.Mode != "off" && t.Mode != "public" && t.Mode != "friends" && t.Mode != "contacts" && t.Mode != "faction")
        {
            error = "STR_OZ_ERR_REFUSED";
            return "";
        }

        string uid = sender.GetPlainId();
        OZ_PlayerData d = OZ_PlayerStore.Load(uid);
        d.TransponderMode = t.Mode;
        OZ_PlayerStore.MarkDirty(uid);

        ok = true;
        error = "";
        return "";
    }
}
