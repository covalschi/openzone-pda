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

// Пуш маячків: список плюс два клієнтські числа з Tuning.json -- окремий
// канал для них був би дорожчий за два поля в конверті, який і так їде.
class OZ_BeaconPush
{
    ref array<ref OZ_MapBeacon> Beacons;
    int AdvanceM = 30;
    int ToastS   = 8;

    void OZ_BeaconPush()
    {
        Beacons = new array<ref OZ_MapBeacon>();
    }
}

class OZ_PdaHandlerMap : OZ_PageHandler
{
    // Один живий обробник: пуш маячків іде через нього, бо антену, шпигунську
    // плату й приватність рахує саме цей код.
    private static OZ_PdaHandlerMap s_Inst;

    void OZ_PdaHandlerMap()
    {
        s_Inst = this;
    }

    // Що кому пішло минулого разу: порожньому за порожнім не шлемо.
    private ref map<string, string> m_BeaconSig = new map<string, string>();

    static void PushBeacons()
    {
        if (s_Inst)
            s_Inst.PushBeaconsNow();
    }

    private void PushBeaconsNow()
    {
        array<Man> players = new array<Man>();
        GetGame().GetPlayers(players);

        for (int i = 0; i < players.Count(); i++)
        {
            PlayerBase pl = PlayerBase.Cast(players[i]);
            if (!pl)
                continue;
            PlayerIdentity id = pl.GetIdentity();
            if (!id)
                continue;

            string uid = id.GetPlainId();

            OZ_PDA_Base pda = OZ_PdaLookup.HeldByPlayer(pl);
            string sig = "";
            OZ_BeaconPush push = new OZ_BeaconPush();

            if (pda && pda.OZ_IsOn() && pda.OZ_IsUnlocked())
            {
                float range = AntennaRange(pda);
                if (range > 0)
                {
                    FillBeacons(id, pl, pda, range, push.Beacons);
                    for (int b = 0; b < push.Beacons.Count(); b++)
                        sig += push.Beacons[b].Name + "|" + push.Beacons[b].Pos + ";";
                }
            }

            // Порожньо і минулого разу було порожньо -- мовчимо. Один
            // порожній пуш на переході все ж їде: клієнт мусить стерти
            // маячки, коли антена вимкнулась чи всі зникли.
            string last;
            if (!m_BeaconSig.Find(uid, last))
                last = "";
            if (sig == "" && last == "")
                continue;
            m_BeaconSig.Set(uid, sig);

            push.AdvanceM = OZ_PdaTune.RouteAdvanceM();
            push.ToastS   = OZ_PdaTune.ToastSeconds();

            string json;
            string err;
            if (!JsonFileLoader<OZ_BeaconPush>.MakeData(push, json, err, false))
                continue;

            OZ_Rpc.Respond(id, OZ_PdaConst.PAGE_MAP, "beacons", true, json, "");
        }
    }

    // Той самий прийом, що й у записках: час у секундах зіткнувся б на двох
    // мітках в одну секунду, тому до нього додається лічильник.
    private static int s_Seq = 0;

    override string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;
        error = "STR_OZ_ERR_UNKNOWN_OP";

        // КАПСУЛА -- лише читання: власна пам'ять пристрою (мітки)
        // показується, а мінятись їй більше нема куди. Живого чужого це
        // не стосується: він працює як пристрій власника.
        if (op == "transponder" || op == "marker_add" || op == "marker_del" || op == "marker_edit" || op == "route_add" || op == "route_clear" || op == "route_take")
        {
            if (OZ_PdaCapsule.IsFrozen(OZ_PdaLookup.HeldBy(sender)))
            {
                error = "STR_OZ_ERR_FROZEN";
                return "";
            }
        }

        if (op == "state")
            return State(sender, ok, error);

        if (op == "transponder")
            return Transponder(json, sender, ok, error);

        if (op == "marker_add")
            return MarkerAdd(json, sender, ok, error);

        if (op == "marker_del")
            return MarkerDel(json, sender, ok, error);

        if (op == "marker_edit")
            return MarkerEdit(json, sender, ok, error);

        if (op == "carrier_add")
            return CarrierAdd(json, sender, ok, error);

        if (op == "route_add")
            return RouteAdd(json, sender, ok, error);

        if (op == "route_clear")
            return RouteClear(sender, ok, error);

        if (op == "route_write")
            return RouteWrite(sender, ok, error);

        if (op == "route_take")
            return RouteTake(sender, ok, error);

        return "";
    }

    // --- мітки ---
    //
    // Читаються й пишуться на самому ПРИСТРОЇ. Через це межа в профілі щось
    // означає, а вкрадений КПК віддає чужі схованки -- обидва навмисно.

    // ------------------------------------------------------------ маршрут
    //
    // Маршрут -- впорядковані КОПІЇ міток пристрою. Копії навмисно: мітку
    // можна правити чи видаляти, а нитка маршруту лишається такою, якою її
    // склали. Стеля -- та сама, що в міток.

    private OZ_MarkerList LoadRouteOf(OZ_PDA_Base pda)
    {
        OZ_MarkerList r = new OZ_MarkerList();
        if (pda.OZ_RouteJson() == "")
            return r;

        string err;
        OZ_MarkerList parsed;
        if (JsonFileLoader<OZ_MarkerList>.LoadData(pda.OZ_RouteJson(), parsed, err) && parsed && parsed.Items)
            return parsed;
        return r;
    }

    private bool FlushRoute(OZ_PDA_Base pda, OZ_MarkerList r, out string error)
    {
        string outJson;
        string err;
        if (!JsonFileLoader<OZ_MarkerList>.MakeData(r, outJson, err, false))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return false;
        }
        pda.OZ_SetRouteJson(outJson);
        return true;
    }

    private string RouteAdd(string json, PlayerIdentity sender, out bool ok, out string error)
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
        if (!JsonFileLoader<OZ_MarkerRef>.LoadData(json, r, err) || !r || r.Id == "")
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_MarkerList mine = LoadMarkers(pda);
        OZ_MapMarker src = null;
        for (int i = 0; i < mine.Items.Count(); i++)
        {
            if (mine.Items[i].Id == r.Id)
            {
                src = mine.Items[i];
                break;
            }
        }
        if (!src)
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_MarkerList route = LoadRouteOf(pda);

        for (int d = 0; d < route.Items.Count(); d++)
        {
            if (route.Items[d].Id == src.Id)
            {
                error = "STR_OZ_ERR_CARRIER_DUP";
                return "";
            }
        }

        int limit = 0;
        OZ_PdaProfile prof = OZ_PdaProfiles.ForClass(pda.GetType());
        if (prof)
            limit = MarkerLimit(prof);
        if (limit <= 0 || route.Items.Count() >= limit)
        {
            error = "STR_OZ_ERR_MARKERS_FULL";
            return "";
        }

        OZ_MapMarker cp = new OZ_MapMarker();
        cp.Id   = src.Id;
        cp.Name = src.Name;
        cp.Desc = src.Desc;
        cp.Pos  = src.Pos;
        route.Items.Insert(cp);

        if (!FlushRoute(pda, route, error))
            return "";

        ok = true;
        error = "";
        return "";
    }

    private string RouteClear(PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
        if (!pda)
        {
            error = "STR_OZ_ERR_NO_DEVICE";
            return "";
        }

        pda.OZ_SetRouteJson("");
        ok = true;
        error = "";
        return "";
    }

    // Маршрут на чип: НИТКА цілком, стеля -- ліміт міток класу чипа
    // (рішення власника 2026-08-29). Запис дозволений і капсулі: чип --
    // фізичний носій, не пам'ять пристрою.
    private string RouteWrite(PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
        if (!pda)
        {
            error = "STR_OZ_ERR_NO_DEVICE";
            return "";
        }

        OZ_MarkerList route = LoadRouteOf(pda);
        if (route.Items.Count() == 0)
        {
            error = "STR_OZ_ERR_ROUTE_EMPTY";
            return "";
        }

        OZ_DataCarrier_Base c = OZ_CarrierOps.ResolveWritable(sender, error);
        if (!c)
            return "";

        // Місце питаємо в САМОГО носія: він знає і свою стелю, і скільки на
        // ньому вже лежить чужих родів. Точка маршруту -- такий самий запис,
        // як мітка чи нотатка, і платить так само.
        int room = c.OZ_RoomFor(OZ_DataCarrier_Base.KIND_ROUTE);
        if (room >= 0 && route.Items.Count() > room)
        {
            error = "STR_OZ_ERR_CARRIER_FULL";
            return "";
        }

        string outJson;
        string err;
        if (!JsonFileLoader<OZ_MarkerList>.MakeData(route, outJson, err, false))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        c.OZ_WriteRoute(outJson, route.Items.Count());
        ok = true;
        error = "";
        return "";
    }

    // З чипа в пристрій: маршрут ЗАМІНЮЄТЬСЯ, а не зливається -- нитка
    // або твоя, або чужа, половинка від кожної не веде нікуди.
    private string RouteTake(PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
        if (!pda)
        {
            error = "STR_OZ_ERR_NO_DEVICE";
            return "";
        }

        OZ_DataCarrier_Base c = OZ_CarrierOps.Resolve(sender, error);
        if (!c)
            return "";

        if (c.OZ_Route() == "")
        {
            error = "STR_OZ_ERR_ROUTE_EMPTY";
            return "";
        }

        OZ_MarkerList incoming;
        string err;
        if (!JsonFileLoader<OZ_MarkerList>.LoadData(c.OZ_Route(), incoming, err) || !incoming || !incoming.Items)
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        int limit = 0;
        OZ_PdaProfile prof = OZ_PdaProfiles.ForClass(pda.GetType());
        if (prof)
            limit = MarkerLimit(prof);
        if (limit <= 0 || incoming.Items.Count() > limit)
        {
            error = "STR_OZ_ERR_MARKERS_FULL";
            return "";
        }

        pda.OZ_SetRouteJson(c.OZ_Route());
        ok = true;
        error = "";
        return "";
    }

    // Експорт ОДНІЄЇ мітки на носій -- вибір гравця, а не «все гуртом».
    // Повторний експорт тієї самої мітки ОНОВЛЮЄ її на чипі за Id, а не
    // плодить копію: чип -- збірка, яку складають свідомо.
    private string CarrierAdd(string json, PlayerIdentity sender, out bool ok, out string error)
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
        if (!JsonFileLoader<OZ_MarkerRef>.LoadData(json, r, err) || !r)
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_MarkerList mine = LoadMarkers(pda);
        OZ_MapMarker found;
        for (int i = 0; i < mine.Items.Count(); i++)
        {
            if (mine.Items[i].Id == r.Id)
            {
                found = mine.Items[i];
                break;
            }
        }

        if (!found)
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_DataCarrier_Base c = OZ_CarrierOps.ResolveWritable(sender, error);
        if (!c)
            return "";

        OZ_MarkerList carried = new OZ_MarkerList();
        if (c.OZ_Marks() != "")
        {
            OZ_MarkerList parsed;
            if (JsonFileLoader<OZ_MarkerList>.LoadData(c.OZ_Marks(), parsed, err) && parsed && parsed.Items)
                carried = parsed;
            else
                // Як і в нотатках: свіжий список поверх нечитної секції,
                // але зі слідом у лозі -- на чипі щось було.
                OZ_Log.Warn("carrier: unreadable markers payload on " + c.GetType() + ", replacing (" + err + ")");
        }

        // Дедуп за ВМІСТОМ (назва + місце), а не за Id. Id мітки не переживає
        // імпорту -- при завантаженні з чипа він карбується заново, щоб чужі
        // id не зіткнулися з нашими. Тож повторний експорт тієї самої мітки
        // після циклу експорт->імпорт мав би вже інший Id і плодив би копію.
        // Однакова назва в одній точці -- це та сама мітка, хоч би звідки.
        int at = -1;
        for (int k = 0; k < carried.Items.Count(); k++)
        {
            if (carried.Items[k].Name == found.Name && carried.Items[k].Pos == found.Pos)
            {
                at = k;
                break;
            }
        }

        if (at >= 0)
        {
            carried.Items.Set(at, found);
        }
        else
        {
            int room = c.OZ_RoomFor(OZ_DataCarrier_Base.KIND_MARKS);
            if (room >= 0 && carried.Items.Count() >= room)
            {
                error = "STR_OZ_ERR_CARRIER_FULL";
                return "";
            }
            carried.Items.Insert(found);
        }

        string outJson;
        if (!JsonFileLoader<OZ_MarkerList>.MakeData(carried, outJson, err, false))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        c.OZ_WriteMarks(outJson, carried.Items.Count());

        ok = true;
        error = "";
        return "";
    }

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
        if (!JsonFileLoader<OZ_MapMarker>.LoadData(json, incoming, err) || !incoming)
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
        Scrub(incoming);

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
        if (!JsonFileLoader<OZ_MarkerRef>.LoadData(json, r, err) || !r)
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

    // Переназвати або переописати. Позиція навмисно НЕ редагується: мітка --
    // це місце, і «посунути мітку» означає поставити нову там, де стоїш або
    // куди клікнув. Інакше з'явився б спосіб виправляти координати наосліп.
    private string MarkerEdit(string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
        if (!pda)
        {
            error = "STR_OZ_ERR_NO_DEVICE";
            return "";
        }

        OZ_MapMarker incoming;
        string err;
        if (!JsonFileLoader<OZ_MapMarker>.LoadData(json, incoming, err) || !incoming)
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_MarkerList list = LoadMarkers(pda);

        OZ_MapMarker found;
        for (int i = 0; i < list.Items.Count(); i++)
        {
            if (list.Items[i].Id == incoming.Id)
            {
                found = list.Items[i];
                break;
            }
        }

        if (!found)
        {
            error = "STR_OZ_ERR_NO_MARKER";
            return "";
        }

        Scrub(incoming);
        found.Name = incoming.Name;
        found.Desc = incoming.Desc;

        if (!SaveMarkers(pda, list))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        ok = true;
        error = "";
        return "";
    }

    // Текст із клієнта чиститься завжди -- він поїде в JSON, на карту й у
    // список. Одне місце на обидва шляхи (add і edit), щоб межі не розійшлись.
    private void Scrub(OZ_MapMarker m)
    {
        m.Name = MiscGameplayFunctions.SanitizeString(m.Name);
        m.Name = OZ_Text.Clip(m.Name, OZ_PdaTune.MarkerNameMax());

        m.Desc = MiscGameplayFunctions.SanitizeString(m.Desc);
        m.Desc = OZ_Text.Clip(m.Desc, OZ_PdaTune.MarkerDescMax());
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
            st.Route   = LoadRouteOf(pda).Items;

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

        FillBeacons(sender, me, pda, range, st.Beacons);

        return Serialise(st, ok, error);
    }

    // Маячки для ОДНОГО глядача. Спільна для відповіді сторінки і серверного
    // пуша: антена, шпигунська плата і приватність рахуються однаково.
    private void FillBeacons(PlayerIdentity sender, PlayerBase me, OZ_PDA_Base pda, float range, array<ref OZ_MapBeacon> outBeacons)
    {
        string myUid = sender.GetPlainId();

        array<Man> near = new array<Man>();
        OZ_Spatial.PlayersInRadius(me.GetPosition(), range, near);

        // ШПИГУНСЬКА плата ламає приватність: бачить усіх, у кого
        // транспондер узагалі не "off". Ціна -- лічені хвилини ресурсу,
        // який плата списує сама (OZ_SpyAntennaBehaviour).
        bool spyEye = SpyActive(pda);

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
            {
                if (!spyEye || od.TransponderMode == "off" || od.TransponderMode == "")
                    continue;
            }

            // Вести маячок теж потрібна АНТЕНА, і саме в того, хто веде.
            // Інакше вийшло б, що чужий пристрій світить позицію тим, чим
            // світити не може.
            OZ_PDA_Base theirs = OZ_PdaLookup.HeldBy(oid);
            if (!theirs || AntennaRange(theirs) <= 0)
                continue;

            OZ_MapBeacon b = new OZ_MapBeacon();
            b.Name = oid.GetName();
            b.Pos  = other.GetPosition().ToString(false);
            outBeacons.Insert(b);
        }
    }

    // Чи в КПК стоїть жива шпигунська плата.
    private bool SpyActive(OZ_PDA_Base pda)
    {
        if (!pda)
            return false;

        for (int i = 0; i < OZ_PdaConst.MODULE_SLOTS_MAX; i++)
        {
            string cls = pda.OZ_ModuleClass(i);
            if (cls == "")
                continue;

            OZ_ModuleSpec spec = OZ_PdaHardware.ModuleFor(cls);
            if (!spec || spec.SpyMinutes <= 0)
                continue;

            OZ_Module_SpyAntenna plate = OZ_Module_SpyAntenna.Cast(pda.OZ_Attached(OZ_PdaConst.ModuleSlot(i)));
            if (plate && plate.OZ_SpyAlive())
                return true;
        }
        return false;
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
            // Списки тримають КЛЮЧІ ПЕРСОНАЖІВ: маячок, дозволений колишньому
            // життю цього акаунта, новому не дістається.
            return them.TransponderTo.Find(OZ_PlayerStore.KeyOf(toUid)) != -1;
        }

        if (them.TransponderMode == "friends")
        {
            if (!them.Friends)
                return false;
            return them.Friends.Find(OZ_PlayerStore.KeyOf(toUid)) != -1;
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
        if (!JsonFileLoader<OZ_TransponderOp>.LoadData(json, t, err) || !t)
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        // Перелік режимів закритий. Чуже слово в TransponderMode згодом
        // прочитав би Broadcasts() і не впізнав -- тобто маячок мовчав би, а
        // гравець вважав би, що веде.
        // "contacts" не приймається, поки список TransponderTo нічим вести:
        // див. NextMode() на сторінці карти.
        if (t.Mode != "off" && t.Mode != "public" && t.Mode != "friends" && t.Mode != "faction")
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
