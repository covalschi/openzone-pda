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
    override string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;
        error = "STR_OZ_ERR_UNKNOWN_OP";

        if (op == "state")
            return State(sender, ok, error);

        if (op == "transponder")
            return Transponder(json, sender, ok, error);

        return "";
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

        // "friends" -- список друзів ще не існує, і поки його немає, режим
        // чесно не показує нікому. Мовчазне «як public» тут було б витоком.
        //
        // "off" і будь-що незнайоме -- теж нікому.
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
        if (t.Mode != "off" && t.Mode != "public" && t.Mode != "friends" && t.Mode != "contacts")
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
